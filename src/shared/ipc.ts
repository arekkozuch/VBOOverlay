import type {
  EnvironmentInfo,
  MenuAction,
  MediaInfo,
  ProjectFile,
  SerializedTelemetrySession,
  SyncResult,
} from './models.js';

export interface OpenVboResult {
  path: string;
  session: SerializedTelemetrySession;
}

export interface ProjectOpenResult {
  path: string;
  project: ProjectFile;
  media?: MediaInfo;
  telemetry?: OpenVboResult;
  warnings: string[];
}

export interface FlappedEarApi {
  openVideo(): Promise<MediaInfo | null>;
  openVbo(): Promise<OpenVboResult | null>;
  inspectEnvironment(): Promise<EnvironmentInfo>;
  openProject(): Promise<ProjectOpenResult | null>;
  saveProject(project: ProjectFile, path?: string): Promise<string | null>;
  autoSync(): Promise<SyncResult>;
  onMenuAction(listener: (action: MenuAction) => void): () => void;
}

export const IPC = {
  openVideo: 'media:open-video',
  openVbo: 'telemetry:open-vbo',
  environment: 'system:environment',
  openProject: 'project:open',
  saveProject: 'project:save',
  autoSync: 'telemetry:auto-sync',
  menuAction: 'menu:action',
} as const;
