import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import type { MediaInfo, VideoStreamInfo } from '../../shared/models.js';

const execFileAsync = promisify(execFile);

interface ProbeStream {
  index: number;
  codec_name?: string;
  codec_type?: string;
  width?: number;
  height?: number;
  pix_fmt?: string;
  r_frame_rate?: string;
  avg_frame_rate?: string;
  time_base?: string;
  tags?: Record<string, string>;
}
interface ProbeOutput {
  format?: { duration?: string; format_name?: string; bit_rate?: string };
  streams?: ProbeStream[];
}

function fraction(value?: string): number | undefined {
  if (!value) return undefined;
  const parts = value.split('/');
  const numerator = Number(parts[0]);
  const denominator = Number(parts[1] ?? '1');
  return Number.isFinite(numerator) && Number.isFinite(denominator) && denominator !== 0
    ? numerator! / denominator!
    : undefined;
}

function streamInfo(stream: ProbeStream): VideoStreamInfo {
  return {
    index: stream.index,
    codec: stream.codec_name ?? 'unknown',
    codecType: stream.codec_type ?? 'unknown',
    width: stream.width,
    height: stream.height,
    pixelFormat: stream.pix_fmt,
    frameRate: fraction(stream.r_frame_rate),
    averageFrameRate: fraction(stream.avg_frame_rate),
    timeBase: stream.time_base,
    tags: stream.tags ?? {},
  };
}

export async function probeMedia(path: string, mediaUrl: string): Promise<MediaInfo> {
  const { stdout } = await execFileAsync(
    'ffprobe',
    ['-v', 'error', '-show_format', '-show_streams', '-of', 'json', path],
    { maxBuffer: 20 * 1024 * 1024 },
  );
  const probe = JSON.parse(stdout) as ProbeOutput;
  const streams = (probe.streams ?? []).map(streamInfo);
  const video = streams.find((stream) => stream.codecType === 'video');
  if (!video) throw new Error('The selected file contains no video stream.');
  const metadata = streams.filter((stream) => stream.codecType === 'data');
  const rateDifference = Math.abs((video.frameRate ?? 0) - (video.averageFrameRate ?? 0));
  const goProPattern = /gopro|gpmd|gpmf/i;
  return {
    path,
    mediaUrl,
    duration: Number(probe.format?.duration ?? 0),
    format: probe.format?.format_name ?? 'unknown',
    bitRate: probe.format?.bit_rate ? Number(probe.format.bit_rate) : undefined,
    streams,
    video,
    audio: streams.filter((stream) => stream.codecType === 'audio'),
    metadata,
    variableFrameRate: rateDifference > 0.01,
    hasGoProTelemetry: metadata.some((stream) =>
      goProPattern.test(`${stream.codec} ${Object.values(stream.tags).join(' ')}`),
    ),
  };
}
