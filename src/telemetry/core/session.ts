import type { SerializedTelemetrySession, TelemetryAlias } from '../../shared/models.js';

export type InterpolationMode = 'nearest' | 'previous' | 'linear';

export interface TelemetryChannelData {
  name: string;
  unit?: string;
  timestamps: Float64Array;
  values: Float32Array;
}

export interface TelemetrySession {
  duration: number;
  startTime: number;
  metadata: Record<string, string>;
  channels: Map<string, TelemetryChannelData>;
  aliases: Partial<Record<TelemetryAlias, string>>;
  warnings: string[];
  sampleCount: number;
}

export function hydrateSession(input: SerializedTelemetrySession): TelemetrySession {
  return {
    ...input,
    channels: new Map(
      Object.entries(input.channels).map(([key, channel]) => [
        key,
        {
          ...channel,
          timestamps: new Float64Array(channel.timestamps),
          values: new Float32Array(channel.values),
        },
      ]),
    ),
  };
}

export function serializeSession(input: TelemetrySession): SerializedTelemetrySession {
  return {
    ...input,
    channels: Object.fromEntries(
      [...input.channels].map(([key, channel]) => [
        key,
        {
          ...channel,
          timestamps: Array.from(channel.timestamps),
          values: Array.from(channel.values),
        },
      ]),
    ),
  };
}

export function videoToTelemetryTime(
  videoTime: number,
  transform: { offset: number; timeScale: number },
): number {
  return videoTime * transform.timeScale + transform.offset;
}

export function valueAt(
  session: TelemetrySession,
  channelName: string | TelemetryAlias,
  time: number,
  mode: InterpolationMode = 'linear',
): number | undefined {
  const resolved = session.aliases[channelName as TelemetryAlias] ?? channelName;
  const channel = session.channels.get(resolved);
  if (!channel || channel.timestamps.length === 0 || !Number.isFinite(time)) return undefined;
  const { timestamps, values } = channel;
  if (time <= timestamps[0]!) return values[0];
  const last = timestamps.length - 1;
  if (time >= timestamps[last]!) return values[last];
  let low = 0;
  let high = last;
  while (low <= high) {
    const middle = (low + high) >>> 1;
    const sampleTime = timestamps[middle]!;
    if (sampleTime === time) return values[middle];
    if (sampleTime < time) low = middle + 1;
    else high = middle - 1;
  }
  const previous = low - 1;
  const next = low;
  if (mode === 'previous') return values[previous];
  if (mode === 'nearest') {
    return time - timestamps[previous]! <= timestamps[next]! - time
      ? values[previous]
      : values[next];
  }
  const span = timestamps[next]! - timestamps[previous]!;
  const ratio = span === 0 ? 0 : (time - timestamps[previous]!) / span;
  return values[previous]! + (values[next]! - values[previous]!) * ratio;
}
