import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import type { EncoderInfo, EnvironmentInfo } from '../../shared/models.js';

const execFileAsync = promisify(execFile);
const candidates: EncoderInfo[] = [
  { name: 'hevc_videotoolbox', hardware: true, label: 'Apple VideoToolbox' },
  { name: 'hevc_nvenc', hardware: true, label: 'NVIDIA NVENC' },
  { name: 'hevc_qsv', hardware: true, label: 'Intel Quick Sync' },
  { name: 'hevc_amf', hardware: true, label: 'AMD AMF' },
  { name: 'libx265', hardware: false, label: 'x265 software' },
];

async function version(command: string): Promise<string> {
  try {
    return (await execFileAsync(command, ['-version'])).stdout.split('\n')[0] ?? 'unknown';
  } catch {
    return 'not found';
  }
}

export async function inspectEnvironment(): Promise<EnvironmentInfo> {
  let listing = '';
  try {
    listing = (
      await execFileAsync('ffmpeg', ['-hide_banner', '-encoders'], { maxBuffer: 10 * 1024 * 1024 })
    ).stdout;
  } catch {
    /* reported by versions */
  }
  return {
    ffmpegVersion: await version('ffmpeg'),
    ffprobeVersion: await version('ffprobe'),
    encoders: candidates.filter(({ name }) => new RegExp(`\\b${name}\\b`).test(listing)),
  };
}
