import { createReadStream } from 'node:fs';
import { stat } from 'node:fs/promises';
import { extname } from 'node:path';
import { Readable } from 'node:stream';

function mediaType(path: string): string {
  return extname(path).toLowerCase() === '.mov' ? 'video/quicktime' : 'video/mp4';
}

function rangeNotSatisfiable(size: number): Response {
  return new Response(null, {
    status: 416,
    headers: { 'Content-Range': `bytes */${size}` },
  });
}

export async function serveMedia(request: Request, path: string): Promise<Response> {
  const { size } = await stat(path);
  const range = request.headers.get('range');
  let start = 0;
  let end = size - 1;
  let status = 200;

  if (range) {
    const match = /^bytes=(\d*)-(\d*)$/.exec(range);
    if (!match) return rangeNotSatisfiable(size);
    if (match[1]) start = Number(match[1]);
    if (match[2]) end = Number(match[2]);
    if (!match[1] && match[2]) {
      const suffixLength = Number(match[2]);
      start = Math.max(0, size - suffixLength);
      end = size - 1;
    }
    if (
      !Number.isSafeInteger(start) ||
      !Number.isSafeInteger(end) ||
      start < 0 ||
      end < start ||
      start >= size
    )
      return rangeNotSatisfiable(size);
    end = Math.min(end, size - 1);
    status = 206;
  }

  const headers = new Headers({
    'Accept-Ranges': 'bytes',
    'Content-Length': String(end - start + 1),
    'Content-Type': mediaType(path),
    'Cache-Control': 'no-store',
  });
  if (status === 206) headers.set('Content-Range', `bytes ${start}-${end}/${size}`);
  if (request.method === 'HEAD') return new Response(null, { status, headers });
  const body = Readable.toWeb(createReadStream(path, { start, end })) as ReadableStream<Uint8Array>;
  return new Response(body, { status, headers });
}
