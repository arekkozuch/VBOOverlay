import type { TelemetrySession } from '../core/session.js';
import { valueAt } from '../core/session.js';

export interface Point {
  x: number;
  y: number;
}
export interface TrackGeometry {
  points: Point[];
  bounds: { minX: number; minY: number; maxX: number; maxY: number };
}

export function latLonToLocal(points: Array<{ latitude: number; longitude: number }>): Point[] {
  const valid = points.filter(
    ({ latitude, longitude }) =>
      Number.isFinite(latitude) &&
      Number.isFinite(longitude) &&
      Math.abs(latitude) <= 90 &&
      Math.abs(longitude) <= 180,
  );
  if (!valid.length) return [];
  const origin = valid[0]!;
  const earthRadius = 6_371_000;
  const latitudeScale = Math.PI / 180;
  return valid.map(({ latitude, longitude }) => ({
    x:
      (longitude - origin.longitude) *
      latitudeScale *
      earthRadius *
      Math.cos(origin.latitude * latitudeScale),
    y: -(latitude - origin.latitude) * latitudeScale * earthRadius,
  }));
}

export function normalizeTrack(points: Point[]): TrackGeometry {
  if (!points.length) return { points: [], bounds: { minX: 0, minY: 0, maxX: 0, maxY: 0 } };
  const xs = points.map(({ x }) => x);
  const ys = points.map(({ y }) => y);
  const bounds = {
    minX: Math.min(...xs),
    minY: Math.min(...ys),
    maxX: Math.max(...xs),
    maxY: Math.max(...ys),
  };
  const width = bounds.maxX - bounds.minX || 1;
  const height = bounds.maxY - bounds.minY || 1;
  return {
    points: points.map(({ x, y }) => ({
      x: (x - bounds.minX) / width,
      y: (y - bounds.minY) / height,
    })),
    bounds,
  };
}

export function buildTrackGeometry(session: TelemetrySession): TrackGeometry {
  const latitudeName = session.aliases.latitude;
  const longitudeName = session.aliases.longitude;
  if (!latitudeName || !longitudeName) return normalizeTrack([]);
  const latitudes = session.channels.get(latitudeName);
  const longitudes = session.channels.get(longitudeName);
  if (!latitudes || !longitudes) return normalizeTrack([]);
  const count = Math.min(latitudes.values.length, longitudes.values.length);
  const source = Array.from({ length: count }, (_, index) => ({
    latitude: latitudes.values[index]!,
    longitude: longitudes.values[index]!,
  }));
  return normalizeTrack(latLonToLocal(source));
}

export function currentTrackPoint(
  session: TelemetrySession,
  time: number,
  geometry: TrackGeometry,
): Point | undefined {
  const latitude = valueAt(session, 'latitude', time);
  const longitude = valueAt(session, 'longitude', time);
  const latitudeName = session.aliases.latitude;
  const longitudeName = session.aliases.longitude;
  if (latitude === undefined || longitude === undefined || !latitudeName || !longitudeName)
    return undefined;
  const originLatitude = session.channels.get(latitudeName)?.values[0];
  const originLongitude = session.channels.get(longitudeName)?.values[0];
  if (originLatitude === undefined || originLongitude === undefined) return undefined;
  const local = latLonToLocal([
    { latitude: originLatitude, longitude: originLongitude },
    { latitude, longitude },
  ])[1];
  if (!local) return undefined;
  const width = geometry.bounds.maxX - geometry.bounds.minX || 1;
  const height = geometry.bounds.maxY - geometry.bounds.minY || 1;
  return {
    x: (local.x - geometry.bounds.minX) / width,
    y: (local.y - geometry.bounds.minY) / height,
  };
}
