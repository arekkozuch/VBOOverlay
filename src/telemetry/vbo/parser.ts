import type { TelemetryAlias } from '../../shared/models.js';
import type { TelemetryChannelData, TelemetrySession } from '../core/session.js';

const ALIAS_PATTERNS: Array<[TelemetryAlias, RegExp[]]> = [
  ['speed', [/^velocity$/i, /^gps.?speed/i, /^speed$/i, /^obd.?speed/i]],
  ['rpm', [/^rpm(?:[-_].*)?$/i, /engine.?speed/i]],
  ['throttle', [/throttle/i, /accelerator.?pedal/i]],
  ['brake', [/^brake(?:[-_].*)?$/i, /brake.?pressure/i, /brake.?pedal/i]],
  ['heartRate', [/heart.?rate/i, /^hr$/i, /^bpm$/i]],
  ['latitude', [/^latitude$/i, /^lat$/i]],
  ['longitude', [/^longitude$/i, /^lon(?:g)?$/i]],
  ['lateralAcceleration', [/^latacc$/i, /lat(?:eral)?.?(?:accel|acceleration|g)/i, /^g.?x$/i]],
  [
    'longitudinalAcceleration',
    [/^longacc$/i, /long(?:itudinal)?.?(?:accel|acceleration|g)/i, /^g.?y$/i],
  ],
];

const TIME_NAMES = /^(?:time|timestamp|utc.?time)$/i;

function normalizeName(name: string): string {
  return name
    .trim()
    .replace(/^['"]|['"]$/g, '')
    .replace(/\s+/g, ' ');
}

function uniqueNames(input: string[]): string[] {
  const counts = new Map<string, number>();
  return input.map((name) => {
    const count = (counts.get(name) ?? 0) + 1;
    counts.set(name, count);
    return count === 1 ? name : `${name} (${count})`;
  });
}

function normalizeCoordinate(name: string, value: number): number {
  // RaceChrono/VBOX stores signed arc-minutes; other producers may emit
  // decimal degrees. Values outside the valid degree range distinguish them.
  if (/^(?:lat|latitude)$/i.test(name) && Math.abs(value) > 90 && Math.abs(value) <= 5400)
    return value / 60;
  if (/^(?:lon|long|longitude)$/i.test(name) && Math.abs(value) > 180 && Math.abs(value) <= 10800)
    return value / 60;
  return value;
}

function parseClockTime(value: string): number | undefined {
  const numeric = Number(value);
  if (!Number.isFinite(numeric)) return undefined;
  if (!value.includes(':') && numeric >= 10000 && numeric < 240000) {
    const hours = Math.floor(numeric / 10000);
    const minutes = Math.floor((numeric - hours * 10000) / 100);
    return hours * 3600 + minutes * 60 + (numeric % 100);
  }
  if (value.includes(':')) {
    const pieces = value.split(':').map(Number);
    if (pieces.length === 3 && pieces.every(Number.isFinite))
      return pieces[0]! * 3600 + pieces[1]! * 60 + pieces[2]!;
  }
  return numeric;
}

function splitRow(line: string): string[] {
  const trimmed = line.trim();
  if (trimmed.includes(',')) return trimmed.split(',').map((part) => part.trim());
  return trimmed.split(/\s+/);
}

function resolveAliases(names: string[]): Partial<Record<TelemetryAlias, string>> {
  const aliases: Partial<Record<TelemetryAlias, string>> = {};
  for (const [alias, patterns] of ALIAS_PATTERNS) {
    const name = names.find((candidate) => patterns.some((pattern) => pattern.test(candidate)));
    if (name) aliases[alias] = name;
  }
  return aliases;
}

export function parseVbo(text: string): TelemetrySession {
  const lines = text.replace(/^\uFEFF/, '').split(/\r?\n/);
  const sections = new Map<string, string[]>();
  let section = '';
  for (const raw of lines) {
    const line = raw.trim();
    if (!line || line.startsWith(';') || line.startsWith('#')) continue;
    const match = /^\[([^\]]+)\]$/.exec(line);
    if (match) {
      section = match[1]!.trim().toLowerCase();
      if (!sections.has(section)) sections.set(section, []);
    } else {
      const entries = sections.get(section) ?? [];
      entries.push(line);
      sections.set(section, entries);
    }
  }

  const metadata: Record<string, string> = {};
  for (const [name, entries] of sections) {
    if (/column|data/.test(name)) continue;
    for (const entry of entries) {
      const parts = entry.split(/[:=]/, 2);
      if (parts.length === 2) metadata[normalizeName(parts[0]!)] = parts[1]!.trim();
      else metadata[`${name}.${Object.keys(metadata).length}`] = entry;
    }
  }

  const columnSection = [...sections].find(([name]) => /column/.test(name))?.[1];
  const dataSection = [...sections].find(([name]) => /^data/.test(name))?.[1];
  if (!columnSection?.length) throw new Error('VBO has no [column names] section.');
  if (!dataSection?.length) throw new Error('VBO has no [data] rows.');
  const names = uniqueNames(splitRow(columnSection.join(' ')).map(normalizeName));
  const timeIndex = names.findIndex((name) => TIME_NAMES.test(name));
  const warnings: string[] = [];
  const rawValues = names.map(() => [] as number[]);
  const rawTimes: number[] = [];
  let origin: number | undefined;

  for (let rowIndex = 0; rowIndex < dataSection.length; rowIndex += 1) {
    const cells = splitRow(dataSection[rowIndex]!);
    if (cells.length < names.length)
      warnings.push(`Row ${rowIndex + 1}: missing ${names.length - cells.length} value(s).`);
    if (cells.length > names.length)
      warnings.push(`Row ${rowIndex + 1}: ignored ${cells.length - names.length} extra value(s).`);
    const parsedTime = timeIndex >= 0 ? parseClockTime(cells[timeIndex] ?? '') : rowIndex;
    if (parsedTime === undefined) {
      warnings.push(`Row ${rowIndex + 1}: invalid timestamp; row skipped.`);
      continue;
    }
    origin ??= parsedTime;
    let timestamp = parsedTime - origin;
    if (timestamp < 0) timestamp += 24 * 3600;
    rawTimes.push(timestamp);
    names.forEach((_, column) => {
      const value = Number(cells[column]);
      rawValues[column]!.push(
        Number.isFinite(value) ? normalizeCoordinate(names[column]!, value) : Number.NaN,
      );
    });
  }
  if (rawTimes.length === 0) throw new Error('VBO contains no valid timestamped data rows.');

  const channels = new Map<string, TelemetryChannelData>();
  names.forEach((name, index) => {
    if (index === timeIndex) return;
    const values = rawValues[index]!;
    if (!values.some(Number.isFinite)) return;
    channels.set(name, {
      name,
      timestamps: new Float64Array(rawTimes),
      values: new Float32Array(values),
    });
  });
  return {
    duration: rawTimes.at(-1)! - rawTimes[0]!,
    startTime: origin ?? 0,
    metadata,
    channels,
    aliases: resolveAliases([...channels.keys()]),
    warnings,
    sampleCount: rawTimes.length,
  };
}
