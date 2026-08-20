import electron from 'electron';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { randomUUID } from 'node:crypto';
import { join } from 'node:path';
import { IPC } from '../shared/ipc.js';
import type { MenuAction, ProjectFile } from '../shared/models.js';
import { serializeSession } from '../telemetry/core/session.js';
import { loadGoProTelemetry } from '../telemetry/gopro/source.js';
import { TelemetrySyncEngine } from '../telemetry/sync/sync.js';
import { parseVbo } from '../telemetry/vbo/parser.js';
import { inspectEnvironment } from './export/encoders.js';
import { probeMedia } from './media/ffprobe.js';
import { serveMedia } from './media/protocol.js';

const { app, BrowserWindow, dialog, ipcMain, Menu, protocol } = electron;

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

interface WindowState {
  width: number;
  height: number;
  x?: number;
  y?: number;
  maximized?: boolean;
}

function sendMenuAction(action: MenuAction): void {
  BrowserWindow.getFocusedWindow()?.webContents.send(IPC.menuAction, action);
}

function installApplicationMenu(): void {
  const fileItems: Electron.MenuItemConstructorOptions[] = [
    {
      label: 'New Project',
      accelerator: 'CmdOrCtrl+N',
      click: () => sendMenuAction('new-project'),
    },
    {
      label: 'Open Project…',
      accelerator: 'CmdOrCtrl+O',
      click: () => sendMenuAction('open-project'),
    },
    { type: 'separator' },
    {
      label: 'Open Video…',
      accelerator: 'CmdOrCtrl+Shift+V',
      click: () => sendMenuAction('open-video'),
    },
    {
      label: 'Open VBO…',
      accelerator: 'CmdOrCtrl+Shift+T',
      click: () => sendMenuAction('open-vbo'),
    },
    { type: 'separator' },
    { label: 'Save', accelerator: 'CmdOrCtrl+S', click: () => sendMenuAction('save-project') },
    {
      label: 'Save As…',
      accelerator: 'CmdOrCtrl+Shift+S',
      click: () => sendMenuAction('save-project-as'),
    },
    { type: 'separator' },
    process.platform === 'darwin' ? { role: 'close' } : { role: 'quit' },
  ];
  const template: Electron.MenuItemConstructorOptions[] = [
    ...(process.platform === 'darwin'
      ? [
          {
            label: app.name,
            submenu: [
              { role: 'about' as const },
              { type: 'separator' as const },
              { role: 'quit' as const },
            ],
          },
        ]
      : []),
    { label: 'File', submenu: fileItems },
    {
      label: 'Edit',
      submenu: [
        { role: 'undo' },
        { role: 'redo' },
        { type: 'separator' },
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'selectAll' },
      ],
    },
    {
      label: 'View',
      submenu: [
        { role: 'togglefullscreen' },
        { type: 'separator' },
        { role: 'resetZoom' },
        { role: 'zoomIn' },
        { role: 'zoomOut' },
      ],
    },
    { label: 'Window', submenu: [{ role: 'minimize' }, { role: 'zoom' }] },
  ];
  Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

async function loadWindowState(): Promise<WindowState | undefined> {
  try {
    return JSON.parse(
      await readFile(join(app.getPath('userData'), 'window-state.json'), 'utf8'),
    ) as WindowState;
  } catch {
    return undefined;
  }
}

async function saveWindowState(window: Electron.BrowserWindow): Promise<void> {
  const bounds = window.getNormalBounds();
  const state: WindowState = { ...bounds, maximized: window.isMaximized() };
  const directory = app.getPath('userData');
  await mkdir(directory, { recursive: true });
  await writeFile(join(directory, 'window-state.json'), JSON.stringify(state), 'utf8');
}

function registerMediaPath(path: string): string {
  const token = randomUUID();
  mediaPaths.set(token, path);
  return `fet-media://${token}/video`;
}

function createWindow(state?: WindowState): void {
  const window = new BrowserWindow({
    width: state?.width ?? 1440,
    height: state?.height ?? 900,
    ...(state?.x === undefined ? {} : { x: state.x }),
    ...(state?.y === undefined ? {} : { y: state.y }),
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
  if (state?.maximized) window.maximize();
  window.on('close', () => void saveWindowState(window));
  if (process.argv.includes('--dev')) void window.loadURL('http://localhost:5173');
  else void window.loadFile(join(import.meta.dirname, '../../dist/index.html'));
}

function validProject(value: unknown): value is ProjectFile {
  return (
    typeof value === 'object' && value !== null && (value as { version?: unknown }).version === 1
  );
}

app.whenReady().then(async () => {
  app.setName('FlappedEar Telemetry');
  installApplicationMenu();
  protocol.handle('fet-media', async (request) => {
    const token = new URL(request.url).hostname;
    const path = mediaPaths.get(token);
    if (!path) return new Response('Not found', { status: 404 });
    return serveMedia(request, path);
  });
  ipcMain.handle(IPC.openVideo, async () => {
    const result = await dialog.showOpenDialog({
      properties: ['openFile'],
      filters: [{ name: 'Video', extensions: ['mp4', 'mov'] }],
    });
    if (result.canceled || !result.filePaths[0]) return null;
    currentVideoPath = result.filePaths[0];
    goProCache = undefined;
    return probeMedia(result.filePaths[0], registerMediaPath(result.filePaths[0]));
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
    const warnings: string[] = [];
    let media;
    let telemetry;
    if (project.videoPath) {
      try {
        media = await probeMedia(project.videoPath, registerMediaPath(project.videoPath));
        currentVideoPath = project.videoPath;
        goProCache = undefined;
      } catch (error) {
        warnings.push(`Could not reopen video: ${String(error)}`);
      }
    }
    if (project.vboPath) {
      try {
        telemetry = {
          path: project.vboPath,
          session: serializeSession(parseVbo(await readFile(project.vboPath, 'utf8'))),
        };
        currentVboPath = project.vboPath;
      } catch (error) {
        warnings.push(`Could not reopen VBO: ${String(error)}`);
      }
    }
    return { path, project, media, telemetry, warnings };
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
  createWindow(await loadWindowState());
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
