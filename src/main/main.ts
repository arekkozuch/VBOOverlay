import electron from 'electron';
import { readFile, writeFile } from 'node:fs/promises';
import { randomUUID } from 'node:crypto';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { IPC } from '../shared/ipc.js';
import type { ProjectFile } from '../shared/models.js';
import { serializeSession } from '../telemetry/core/session.js';
import { loadGoProTelemetry } from '../telemetry/gopro/source.js';
import { TelemetrySyncEngine } from '../telemetry/sync/sync.js';
import { parseVbo } from '../telemetry/vbo/parser.js';
import { inspectEnvironment } from './export/encoders.js';
import { probeMedia } from './media/ffprobe.js';

const { app, BrowserWindow, dialog, ipcMain, net, protocol } = electron;

protocol.registerSchemesAsPrivileged([
  {
    scheme: 'fet-media',
    privileges: { secure: true, standard: true, stream: true, supportFetchAPI: true },
  },
]);
const mediaPaths = new Map<string, string>();
let currentVideoPath: string | undefined;
let currentVboPath: string | undefined;
let goProCache: { path: string; result: ReturnType<typeof loadGoProTelemetry> } | undefined;

function createWindow(): void {
  const window = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 1024,
    minHeight: 680,
    backgroundColor: '#101216',
    webPreferences: {
      preload: join(import.meta.dirname, '../preload/preload.cjs'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });
  if (process.argv.includes('--dev')) void window.loadURL('http://localhost:5173');
  else void window.loadFile(join(import.meta.dirname, '../../dist/index.html'));
}

function validProject(value: unknown): value is ProjectFile {
  return (
    typeof value === 'object' && value !== null && (value as { version?: unknown }).version === 1
  );
}

app.whenReady().then(() => {
  protocol.handle('fet-media', (request) => {
    const token = new URL(request.url).hostname;
    const path = mediaPaths.get(token);
    if (!path) return new Response('Not found', { status: 404 });
    // Chromium issues byte-range requests for seeking and may re-request the
    // current range when its video surface changes size. Dropping the Range
    // header makes a large local video fall back to a full 200 response and
    // leaves the media element unable to resume after a window resize.
    return net.fetch(pathToFileURL(path).href, {
      method: request.method,
      headers: request.headers,
    });
  });
  ipcMain.handle(IPC.openVideo, async () => {
    const result = await dialog.showOpenDialog({
      properties: ['openFile'],
      filters: [{ name: 'Video', extensions: ['mp4', 'mov'] }],
    });
    if (result.canceled || !result.filePaths[0]) return null;
    const token = randomUUID();
    mediaPaths.set(token, result.filePaths[0]);
    currentVideoPath = result.filePaths[0];
    goProCache = undefined;
    return probeMedia(result.filePaths[0], `fet-media://${token}/video`);
  });
  ipcMain.handle(IPC.openVbo, async () => {
    const result = await dialog.showOpenDialog({
      properties: ['openFile'],
      filters: [{ name: 'VBOX telemetry', extensions: ['vbo'] }],
    });
    if (result.canceled || !result.filePaths[0]) return null;
    const path = result.filePaths[0];
    currentVboPath = path;
    return { path, session: serializeSession(parseVbo(await readFile(path, 'utf8'))) };
  });
  ipcMain.handle(IPC.autoSync, async () => {
    if (!currentVideoPath || !currentVboPath)
      throw new Error('Open both a GoPro video and VBO before automatic synchronization.');
    console.info('[sync] GPS speed synchronization started');
    const vbo = parseVbo(await readFile(currentVboPath, 'utf8'));
    if (!goProCache || goProCache.path !== currentVideoPath)
      goProCache = { path: currentVideoPath, result: loadGoProTelemetry(currentVideoPath) };
    const goPro = await goProCache.result;
    console.info(
      `[gopro] ${goPro.info.packetCount} packets, channels: ${goPro.info.availableChannels.join(', ')}`,
    );
    const result = new TelemetrySyncEngine().synchronize(goPro.session, vbo);
    console.info(
      `[sync] ${result.strategy}, offset ${result.offset.toFixed(3)}s, confidence ${(result.confidence * 100).toFixed(0)}%`,
    );
    return result;
  });
  ipcMain.handle(IPC.environment, inspectEnvironment);
  ipcMain.handle(IPC.openProject, async () => {
    const result = await dialog.showOpenDialog({
      properties: ['openFile'],
      filters: [{ name: 'FlappedEar project', extensions: ['fetproject'] }],
    });
    if (result.canceled || !result.filePaths[0]) return null;
    const path = result.filePaths[0];
    const project: unknown = JSON.parse(await readFile(path, 'utf8'));
    if (!validProject(project)) throw new Error('Unsupported or invalid project file.');
    return { path, project };
  });
  ipcMain.handle(IPC.saveProject, async (_event, project: ProjectFile, requestedPath?: string) => {
    if (!validProject(project)) throw new Error('Cannot save an invalid project.');
    let path = requestedPath;
    if (!path) {
      const result = await dialog.showSaveDialog({
        defaultPath: 'Untitled.fetproject',
        filters: [{ name: 'FlappedEar project', extensions: ['fetproject'] }],
      });
      if (result.canceled || !result.filePath) return null;
      path = result.filePath;
    }
    await writeFile(path, `${JSON.stringify(project, null, 2)}\n`, 'utf8');
    return path;
  });
  createWindow();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
