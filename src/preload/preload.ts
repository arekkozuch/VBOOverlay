import { contextBridge, ipcRenderer } from 'electron';
import { IPC, type FlappedEarApi } from '../shared/ipc.js';
import type { MenuAction, ProjectFile } from '../shared/models.js';

const api: FlappedEarApi = {
  openVideo: () => ipcRenderer.invoke(IPC.openVideo) as ReturnType<FlappedEarApi['openVideo']>,
  openVbo: () => ipcRenderer.invoke(IPC.openVbo) as ReturnType<FlappedEarApi['openVbo']>,
  inspectEnvironment: () =>
    ipcRenderer.invoke(IPC.environment) as ReturnType<FlappedEarApi['inspectEnvironment']>,
  openProject: () =>
    ipcRenderer.invoke(IPC.openProject) as ReturnType<FlappedEarApi['openProject']>,
  saveProject: (project: ProjectFile, path?: string) =>
    ipcRenderer.invoke(IPC.saveProject, project, path) as ReturnType<FlappedEarApi['saveProject']>,
  autoSync: () => ipcRenderer.invoke(IPC.autoSync) as ReturnType<FlappedEarApi['autoSync']>,
  onMenuAction: (listener: (action: MenuAction) => void) => {
    const handler = (_event: Electron.IpcRendererEvent, action: MenuAction) => listener(action);
    ipcRenderer.on(IPC.menuAction, handler);
    return () => ipcRenderer.removeListener(IPC.menuAction, handler);
  },
};
contextBridge.exposeInMainWorld('flappedEar', api);
