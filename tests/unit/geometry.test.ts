import { describe, expect, it } from 'vitest';
import { latLonToLocal, normalizeTrack } from '../../src/telemetry/geometry/track';

describe('GPS geometry', () => {
  it('converts longitude using latitude-dependent metric scale', () => {
    const equator = latLonToLocal([
      { latitude: 0, longitude: 0 },
      { latitude: 0, longitude: 0.001 },
    ]);
    const highLatitude = latLonToLocal([
      { latitude: 60, longitude: 0 },
      { latitude: 60, longitude: 0.001 },
    ]);
    expect(equator[1]!.x).toBeCloseTo(111.2, 0);
    expect(highLatitude[1]!.x).toBeCloseTo(55.6, 0);
  });
  it('filters invalid GPS and normalizes bounds', () => {
    const local = latLonToLocal([
      { latitude: 100, longitude: 0 },
      { latitude: 52, longitude: 21 },
      { latitude: 52.001, longitude: 21.002 },
    ]);
    const track = normalizeTrack(local);
    expect(track.points).toHaveLength(2);
    expect(track.points[1]).toEqual({ x: 1, y: 0 });
    expect(track.bounds.maxX).toBeGreaterThan(0);
    expect(track.bounds.minY).toBeLessThan(0);
  });
});
