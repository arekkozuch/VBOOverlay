import { contextBridge, ipcRenderer } from 'electron';
import { IPC, type FlappedEarApi } from '../shared/ipc.js';
import type { ProjectFile } from '../shared/models.js';

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
};
contextBridge.exposeInMainWorld('flappedEar', api);
