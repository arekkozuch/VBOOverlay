import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { parseVbo } from '../../src/telemetry/vbo/parser';

const fixture = readFileSync(new URL('../fixtures/basic.vbo', import.meta.url), 'utf8');

describe('VBO parser', () => {
  it('parses sections, metadata, dynamic columns, clock timestamps and aliases', () => {
    const session = parseVbo(fixture);
    expect(session.sampleCount).toBe(3);
    expect(session.duration).toBe(1);
    expect(session.metadata.vehicle).toBe('Test Car');
    expect(session.channels.has('mystery')).toBe(true);
    expect(session.aliases).toMatchObject({
      speed: 'velocity',
      rpm: 'rpm',
      throttle: 'throttle',
      brake: 'brake',
      heartRate: 'heart_rate',
    });
    expect(Array.from(session.channels.get('velocity')!.values)).toEqual([0, 50, 100]);
  });

  it('tolerates whitespace, missing values, malformed values, and extra values', () => {
    const text = `[column names]\ntime speed unknown\n[data]\n0   10  1\n1 bad\n2 30 3 extra`;
    const session = parseVbo(text);
    expect(session.sampleCount).toBe(3);
    expect(Number.isNaN(session.channels.get('speed')!.values[1])).toBe(true);
    expect(session.warnings).toHaveLength(2);
  });

  it('rejects files without required sections', () => {
    expect(() => parseVbo('[header]\nfoo=bar')).toThrow(/column names/i);
  });
});
