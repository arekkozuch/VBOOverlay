import type { TelemetrySession } from '../core/session.js';

export interface SampledSignal {
  timestamps: Float64Array;
  values: Float32Array;
}
export interface SyncCandidate {
  offset: number;
  timeScale: number;
  confidence: number;
  strategy: string;
  diagnostics: {
    correlation: number;
    peakUniqueness: number;
    validSamples: number;
    sampleRate: number;
    coarseOffset?: number;
  };
}
export interface SyncStrategy {
  canRun(a: SampledSignal, b: SampledSignal): boolean;
  calculate(a: SampledSignal, b: SampledSignal): SyncCandidate;
}

function interpolate(signal: SampledSignal, time: number): number | undefined {
  const { timestamps, values } = signal;
  if (!timestamps.length || time < timestamps[0]! || time > timestamps.at(-1)!) return undefined;
  let low = 0,
    high = timestamps.length - 1;
  while (low <= high) {
    const mid = (low + high) >>> 1;
    if (timestamps[mid]! < time) low = mid + 1;
    else high = mid - 1;
  }
  if (low === 0) return values[0];
  const before = low - 1;
  const ratio = (time - timestamps[before]!) / (timestamps[low]! - timestamps[before]! || 1);
  return values[before]! + (values[low]! - values[before]!) * ratio;
}

function correlation(a: number[], b: number[]): number {
  if (a.length < 2 || a.length !== b.length) return -1;
  const meanA = a.reduce((sum, value) => sum + value, 0) / a.length;
  const meanB = b.reduce((sum, value) => sum + value, 0) / b.length;
  let numerator = 0,
    varianceA = 0,
    varianceB = 0;
  for (let index = 0; index < a.length; index += 1) {
    const da = a[index]! - meanA,
      db = b[index]! - meanB;
    numerator += da * db;
    varianceA += da * da;
    varianceB += db * db;
  }
  return varianceA > 0 && varianceB > 0 ? numerator / Math.sqrt(varianceA * varianceB) : -1;
}

export class GpsSpeedSync implements SyncStrategy {
  constructor(
    private readonly searchWindow = 30,
    private readonly sampleRate = 10,
    private readonly centerOffset = 0,
  ) {}
  canRun(a: SampledSignal, b: SampledSignal): boolean {
    return a.values.length >= 20 && b.values.length >= 20;
  }
  calculate(video: SampledSignal, telemetry: SampledSignal): SyncCandidate {
    if (!this.canRun(video, telemetry))
      throw new Error('Insufficient usable GPS speed samples for synchronization.');
    const step = 1 / this.sampleRate;
    const results: Array<{ offset: number; score: number; samples: number }> = [];
    for (
      let offset = this.centerOffset - this.searchWindow;
      offset <= this.centerOffset + this.searchWindow + step / 2;
      offset += step
    ) {
      const a: number[] = [],
        b: number[] = [];
      for (let time = video.timestamps[0]!; time <= video.timestamps.at(-1)!; time += step) {
        const av = interpolate(video, time),
          bv = interpolate(telemetry, time + offset);
        if (av !== undefined && bv !== undefined && Number.isFinite(av) && Number.isFinite(bv)) {
          a.push(av);
          b.push(bv);
        }
      }
      results.push({ offset, score: correlation(a, b), samples: a.length });
    }
    results.sort((left, right) => right.score - left.score);
    const best = results[0]!;
    // Exclude the broad autocorrelation shoulder around the winning peak.
    // Smooth vehicle-speed signals naturally score similarly one second apart.
    const separated = results.filter((item) => Math.abs(item.offset - best.offset) >= 5);
    const second = separated[0]?.score ?? -1;
    const uniqueness = Math.max(0, Math.min(1, (best.score - second) / 0.25));
    const durationScore = Math.min(1, best.samples / (this.sampleRate * 20));
    const strength = Math.max(0, Math.min(1, (best.score + 1) / 2));
    return {
      offset: Math.round(best.offset * 1000) / 1000,
      timeScale: 1,
      confidence:
        Math.round(100 * strength * (0.35 + 0.4 * uniqueness + 0.25 * durationScore)) / 100,
      strategy: 'GPS speed',
      diagnostics: {
        correlation: best.score,
        peakUniqueness: uniqueness,
        validSamples: best.samples,
        sampleRate: this.sampleRate,
      },
    };
  }
}

export class TelemetrySyncEngine {
  synchronize(video: TelemetrySession, telemetry: TelemetrySession): SyncCandidate {
    const videoName = video.aliases.speed;
    const telemetryName = telemetry.aliases.speed;
    const videoSpeed = videoName ? video.channels.get(videoName) : undefined;
    const telemetrySpeed = telemetryName ? telemetry.channels.get(telemetryName) : undefined;
    if (!videoSpeed || !telemetrySpeed)
      throw new Error('GPS speed is not available in both telemetry sources.');
    const videoSignal = { timestamps: videoSpeed.timestamps, values: videoSpeed.values };
    const telemetrySignal = {
      timestamps: telemetrySpeed.timestamps,
      values: telemetrySpeed.values,
    };

    // Prefer offsets that keep the shorter signal fully inside the longer one.
    // Fall back to all overlapping offsets when neither timeline contains the other.
    let minimum = telemetrySpeed.timestamps[0]! - videoSpeed.timestamps[0]!;
    let maximum = telemetrySpeed.timestamps.at(-1)! - videoSpeed.timestamps.at(-1)!;
    if (maximum < minimum) {
      minimum = telemetrySpeed.timestamps[0]! - videoSpeed.timestamps.at(-1)!;
      maximum = telemetrySpeed.timestamps.at(-1)! - videoSpeed.timestamps[0]!;
    }
    const center = (minimum + maximum) / 2;
    const window = Math.max(1, (maximum - minimum) / 2);
    const coarse = new GpsSpeedSync(window, 1, center).calculate(videoSignal, telemetrySignal);
    const fine = new GpsSpeedSync(5, 10, coarse.offset).calculate(videoSignal, telemetrySignal);
    return {
      ...fine,
      diagnostics: { ...fine.diagnostics, coarseOffset: coarse.offset },
    };
  }
}
