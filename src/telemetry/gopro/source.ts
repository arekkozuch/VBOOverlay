import { execFile } from 'node:child_process';
import { open } from 'node:fs/promises';
import { promisify } from 'node:util';
import { GoProTelemetry as parseGoProTelemetry } from 'gopro-telemetry';
import type { GoProTelemetryInfo, TelemetryAlias } from '../../shared/models.js';
import type { TelemetryChannelData, TelemetrySession } from '../core/session.js';

const execFileAsync = promisify(execFile);

interface ProbeStream {
  index: number;
  codec_tag_string?: string;
  codec_type?: string;
  avg_frame_rate?: string;
}

interface ProbePacket {
  pts_time?: string;
  duration_time?: string;
  size?: string;
  pos?: string;
}

interface GpmfProbe {
  streams?: ProbeStream[];
  format?: { duration?: string; tags?: Record<string, string> };
}

interface PacketProbe {
  packets?: ProbePacket[];
}

export interface GoProRawTelemetry {
  rawData: Buffer;
  timing: {
    videoDuration: number;
    frameDuration: number;
    start: Date;
    samples: Array<{ cts: number; duration: number }>;
  };
  packetCount: number;
}

interface ParsedSample {
  cts?: unknown;
  value?: unknown;
}

interface ParsedStream {
  samples?: unknown;
}

interface ParsedDevice {
  streams?: unknown;
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return typeof value === 'object' && value !== null
    ? (value as Record<string, unknown>)
    : undefined;
}

async function jsonCommand<T>(arguments_: string[]): Promise<T> {
  const { stdout } = await execFileAsync('ffprobe', arguments_, {
    maxBuffer: 64 * 1024 * 1024,
  });
  return JSON.parse(stdout) as T;
}

export async function extractGoProRaw(path: string): Promise<GoProRawTelemetry> {
  const probe = await jsonCommand<GpmfProbe>([
    '-v',
    'error',
    '-show_format',
    '-show_streams',
    '-of',
    'json',
    path,
  ]);
  const stream = probe.streams?.find(
    (candidate) => candidate.codec_type === 'data' && candidate.codec_tag_string === 'gpmd',
  );
  if (!stream) throw new Error('No GoPro GPMF telemetry track was found.');
  const videoStream = probe.streams?.find((candidate) => candidate.codec_type === 'video');
  const rateParts = videoStream?.avg_frame_rate?.split('/').map(Number) ?? [];
  const frameRate = rateParts[1] ? rateParts[0]! / rateParts[1] : rateParts[0];

  const packetProbe = await jsonCommand<PacketProbe>([
    '-v',
    'error',
    '-select_streams',
    String(stream.index),
    '-show_packets',
    '-show_entries',
    'packet=pts_time,duration_time,size,pos',
    '-of',
    'json',
    path,
  ]);
  const packets = (packetProbe.packets ?? []).map((packet) => ({
    cts: Number(packet.pts_time) * 1000,
    duration: Number(packet.duration_time) * 1000,
    size: Number(packet.size),
    position: Number(packet.pos),
  }));
  if (
    !packets.length ||
    packets.some(
      ({ cts, duration, size, position }) =>
        ![cts, duration, size, position].every(Number.isFinite) || size <= 0 || position < 0,
    )
  )
    throw new Error('The GPMF packet index is missing or invalid.');

  const totalSize = packets.reduce((sum, packet) => sum + packet.size, 0);
  if (totalSize > 512 * 1024 * 1024)
    throw new Error('The GPMF metadata track is unexpectedly large.');
  const rawData = Buffer.allocUnsafe(totalSize);
  const file = await open(path, 'r');
  try {
    let destinationOffset = 0;
    for (const packet of packets) {
      const { bytesRead } = await file.read(
        rawData,
        destinationOffset,
        packet.size,
        packet.position,
      );
      if (bytesRead !== packet.size) throw new Error('Unexpected end of file in GPMF packet.');
      destinationOffset += packet.size;
    }
  } finally {
    await file.close();
  }

  const startValue = probe.format?.tags?.creation_time;
  const start = startValue ? new Date(startValue) : new Date(0);
  return {
    rawData,
    timing: {
      videoDuration: Number(probe.format?.duration ?? 0),
      frameDuration: frameRate && frameRate > 0 ? 1 / frameRate : 0,
      start: Number.isNaN(start.getTime()) ? new Date(0) : start,
      samples: packets.map(({ cts, duration }) => ({ cts, duration })),
    },
    packetCount: packets.length,
  };
}

function findStreams(parsed: unknown): Record<string, ParsedStream> {
  const result: Record<string, ParsedStream> = {};
  const devices = asRecord(parsed) ?? {};
  for (const value of Object.values(devices)) {
    const device = asRecord(value) as ParsedDevice | undefined;
    const streams = asRecord(device?.streams);
    if (!streams) continue;
    for (const [name, stream] of Object.entries(streams)) {
      const record = asRecord(stream);
      if (record) result[name] = record;
    }
  }
  return result;
}

function samplesOf(stream?: ParsedStream): ParsedSample[] {
  return Array.isArray(stream?.samples) ? (stream.samples as ParsedSample[]) : [];
}

function channelFrom(
  name: string,
  samples: ParsedSample[],
  valueAtIndex: (values: number[]) => number,
  isValid: (values: number[]) => boolean = () => true,
): TelemetryChannelData {
  const valid = samples
    .map((sample) => ({
      time: Number(sample.cts) / 1000,
      values: Array.isArray(sample.value) ? sample.value.map(Number) : [],
    }))
    .filter(
      ({ time, values }) =>
        Number.isFinite(time) && values.every(Number.isFinite) && isValid(values),
    );
  return {
    name,
    timestamps: Float64Array.from(valid, ({ time }) => time),
    values: Float32Array.from(valid, ({ values }) => valueAtIndex(values)),
  };
}

export async function loadGoProTelemetry(path: string): Promise<{
  session: TelemetrySession;
  info: GoProTelemetryInfo;
}> {
  const extracted = await extractGoProRaw(path);
  const parsed: unknown = await parseGoProTelemetry(extracted, {
    stream: ['GPS5', 'GPS9', 'ACCL', 'GYRO'],
    timeOut: 'cts',
    tolerant: true,
  });
  const streams = findStreams(parsed);
  const channels = new Map<string, TelemetryChannelData>();
  const aliases: Partial<Record<TelemetryAlias, string>> = {};
  const gpsName = streams.GPS9 ? 'GPS9' : streams.GPS5 ? 'GPS5' : undefined;
  if (gpsName) {
    const gps = samplesOf(streams[gpsName]);
    const validGps = (values: number[]) =>
      Math.abs(values[0] ?? Infinity) <= 90 &&
      Math.abs(values[1] ?? Infinity) <= 180 &&
      !((values[0] ?? 0) === 0 && (values[1] ?? 0) === 0) &&
      (gpsName !== 'GPS9' || (values[8] ?? 0) >= 2);
    channels.set(
      'GoPro latitude',
      channelFrom('GoPro latitude', gps, (values) => values[0]!, validGps),
    );
    channels.set(
      'GoPro longitude',
      channelFrom('GoPro longitude', gps, (values) => values[1]!, validGps),
    );
    channels.set(
      'GoPro GPS speed',
      channelFrom('GoPro GPS speed', gps, (values) => values[3]! * 3.6, validGps),
    );
    aliases.latitude = 'GoPro latitude';
    aliases.longitude = 'GoPro longitude';
    aliases.speed = 'GoPro GPS speed';
  }
  for (const [streamName, channelNames] of [
    ['ACCL', ['GoPro acceleration X', 'GoPro acceleration Y', 'GoPro acceleration Z']],
    ['GYRO', ['GoPro gyroscope X', 'GoPro gyroscope Y', 'GoPro gyroscope Z']],
  ] as const) {
    const samples = samplesOf(streams[streamName]);
    if (!samples.length) continue;
    channelNames.forEach((name, index) =>
      channels.set(
        name,
        channelFrom(name, samples, (values) => values[index]!),
      ),
    );
  }
  if (!channels.size) throw new Error('The GPMF track contains no usable GPS or motion channels.');
  const firstChannel = channels.values().next().value as TelemetryChannelData | undefined;
  const lastTime = firstChannel?.timestamps.at(-1) ?? 0;
  const session: TelemetrySession = {
    duration: extracted.timing.videoDuration || lastTime,
    startTime: 0,
    metadata: { source: 'GoPro GPMF', start: extracted.timing.start.toISOString() },
    channels,
    aliases,
    warnings: [],
    sampleCount: firstChannel?.values.length ?? 0,
  };
  return {
    session,
    info: {
      availableChannels: [...channels.keys()],
      sampleCounts: Object.fromEntries(
        [...channels].map(([name, channel]) => [name, channel.values.length]),
      ),
      duration: session.duration,
      packetCount: extracted.packetCount,
    },
  };
}
