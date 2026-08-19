import { unlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { serveMedia } from '../../src/main/media/protocol';

const fixture = join(tmpdir(), `flappedear-media-range-${process.pid}.mp4`);
beforeAll(() => writeFile(fixture, Buffer.from('0123456789')));
afterAll(() => unlink(fixture));

describe('local media protocol', () => {
  it('serves complete files with seekable media headers', async () => {
    const response = await serveMedia(new Request('https://media.test/video'), fixture);
    expect(response.status).toBe(200);
    expect(response.headers.get('accept-ranges')).toBe('bytes');
    expect(response.headers.get('content-length')).toBe('10');
    expect(await response.text()).toBe('0123456789');
  });

  it('serves explicit and suffix byte ranges', async () => {
    const partial = await serveMedia(
      new Request('https://media.test/video', { headers: { Range: 'bytes=3-6' } }),
      fixture,
    );
    expect(partial.status).toBe(206);
    expect(partial.headers.get('content-range')).toBe('bytes 3-6/10');
    expect(await partial.text()).toBe('3456');

    const suffix = await serveMedia(
      new Request('https://media.test/video', { headers: { Range: 'bytes=-3' } }),
      fixture,
    );
    expect(await suffix.text()).toBe('789');
  });

  it('rejects invalid or unsatisfiable ranges', async () => {
    const response = await serveMedia(
      new Request('https://media.test/video', { headers: { Range: 'bytes=20-30' } }),
      fixture,
    );
    expect(response.status).toBe(416);
    expect(response.headers.get('content-range')).toBe('bytes */10');
  });
});
