import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { probeMedia } from '../../src/main/media/ffprobe';
import { loadGoProTelemetry } from '../../src/telemetry/gopro/source';
import { TelemetrySyncEngine } from '../../src/telemetry/sync/sync';
import { parseVbo } from '../../src/telemetry/vbo/parser';

const root = process.cwd();
const videoPath = readdirSync(root)
  .find((file) => /\.(?:mp4|mov)$/i.test(file))
  ?.replace(/^/, `${root}/`);
const samples = join(root, 'samples');
const vboPath = [root, ...(existsSync(samples) ? [samples] : [])]
  .flatMap((directory) =>
    readdirSync(directory)
      .filter((file) => file.toLowerCase().endsWith('.vbo'))
      .map((file) => join(directory, file)),
  )
  .at(0);

describe.skipIf(!videoPath || !vboPath)('real GoPro integration', () => {
  it('detects GPMF, extracts useful telemetry, and synchronizes it with the VBO', async () => {
    const media = await probeMedia(videoPath!, 'fet-media://test/video');
    expect(media.hasGoProTelemetry).toBe(true);
    expect(media.video.codec).toBe('hevc');
    expect(media.video.averageFrameRate).toBeCloseTo(59.94, 2);
    expect(media.variableFrameRate).toBe(false);

    const goPro = await loadGoProTelemetry(videoPath!);
    expect(goPro.info.packetCount).toBe(1536);
    expect(goPro.info.sampleCounts).toMatchObject({
      'GoPro GPS speed': 14796,
      'GoPro acceleration X': 309862,
      'GoPro gyroscope X': 309862,
    });
    expect(goPro.session.channels.get('GoPro latitude')!.values[0]).toBeCloseTo(50.013134, 5);

    const vbo = parseVbo(readFileSync(vboPath!, 'utf8'));
    const sync = new TelemetrySyncEngine().synchronize(goPro.session, vbo);
    expect(sync.offset).toBeCloseTo(90.2, 1);
    expect(sync.timeScale).toBe(1);
    expect(sync.diagnostics.correlation).toBeGreaterThan(0.97);
    expect(sync.confidence).toBeGreaterThan(0.5);
  }, 30_000);
});
