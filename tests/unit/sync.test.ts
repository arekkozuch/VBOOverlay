import { describe, expect, it } from 'vitest';
import { GpsSpeedSync, type SampledSignal } from '../../src/telemetry/sync/sync';

function signal(shift: number, duration = 30): SampledSignal {
  const timestamps = Float64Array.from({ length: duration * 10 }, (_, index) => index / 10 + shift);
  const values = Float32Array.from(timestamps, (timestamp) => {
    const time = timestamp - shift;
    return 40 + 12 * Math.sin(time * 0.73) + 5 * Math.sin(time * 1.91) + (time > 14 ? 8 : 0);
  });
  return { timestamps, values };
}
describe('GPS speed synchronization', () => {
  it('recovers a deterministic offset', () => {
    const result = new GpsSpeedSync(5, 10).calculate(signal(0), signal(2.3));
    expect(result.offset).toBeCloseTo(2.3, 1);
    expect(result.diagnostics.correlation).toBeGreaterThan(0.99);
  });
  it('rejects insufficient input', () => {
    const short = signal(0, 1);
    expect(() => new GpsSpeedSync().calculate(short, short)).toThrow(/insufficient/i);
  });
  it('does not assign high confidence to an ambiguous constant signal', () => {
    const timestamps = Float64Array.from({ length: 300 }, (_, index) => index / 10);
    const values = new Float32Array(300).fill(10);
    expect(
      new GpsSpeedSync(3).calculate({ timestamps, values }, { timestamps, values }).confidence,
    ).toBeLessThan(0.5);
  });
});
