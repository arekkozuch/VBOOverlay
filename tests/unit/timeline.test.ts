import { describe, expect, it } from 'vitest';
import { parseVbo } from '../../src/telemetry/vbo/parser';
import { valueAt, videoToTelemetryTime } from '../../src/telemetry/core/session';

const session = parseVbo('[column names]\ntime speed\n[data]\n0 0\n1 10\n2 30');
describe('telemetry timeline', () => {
  it('handles exact samples and interpolation modes', () => {
    expect(valueAt(session, 'speed', 1)).toBe(10);
    expect(valueAt(session, 'speed', 1.5)).toBe(20);
    expect(valueAt(session, 'speed', 1.6, 'previous')).toBe(10);
    expect(valueAt(session, 'speed', 1.6, 'nearest')).toBe(30);
  });
  it('clamps outside the timeline and reports missing channels', () => {
    expect(valueAt(session, 'speed', -1)).toBe(0);
    expect(valueAt(session, 'speed', 9)).toBe(30);
    expect(valueAt(session, 'rpm', 1)).toBeUndefined();
  });
  it('applies offset and time scale centrally', () => {
    expect(videoToTelemetryTime(10, { offset: 2.5, timeScale: 1.01 })).toBe(12.6);
  });
});
