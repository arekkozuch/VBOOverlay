import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { basename, join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { parseVbo } from '../../src/telemetry/vbo/parser';

const samples = join(process.cwd(), 'samples');
const samplePath = existsSync(samples)
  ? readdirSync(samples)
      .find((file) => file.toLowerCase().endsWith('.vbo'))
      ?.replace(/^/, `${samples}/`)
  : undefined;
const rootPath = readdirSync(process.cwd())
  .find((file) => file.toLowerCase().endsWith('.vbo'))
  ?.replace(/^/, `${process.cwd()}/`);
const realVbo = samplePath ?? rootPath;
describe.skipIf(!realVbo)('real VBO integration', () => {
  it('parses the supplied real sample', () => {
    const session = parseVbo(readFileSync(realVbo!, 'utf8'));
    expect(basename(realVbo!)).toMatch(/\.vbo$/i);
    expect(session.sampleCount).toBeGreaterThan(30_000);
    expect(session.channels.size).toBeGreaterThan(40);
    expect(session.duration).toBeGreaterThan(0);
    expect(session.aliases).toMatchObject({
      speed: 'velocity',
      rpm: 'rpm-obd',
      throttle: 'throttle_pos-obd',
      brake: 'brake_pos-obd',
      heartRate: 'heart_rate-hrm',
      latitude: 'lat',
      longitude: 'long',
      lateralAcceleration: 'latacc',
      longitudinalAcceleration: 'longacc',
    });
    expect(session.channels.get('lat')!.values[0]).toBeCloseTo(50.01345, 4);
    expect(session.channels.get('long')!.values[0]).toBeCloseTo(-19.89034, 4);
    expect(session.channels.has('x_acc-acc (2)')).toBe(true);
  });
});
