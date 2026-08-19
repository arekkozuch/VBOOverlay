import type {
  EnvironmentInfo,
  MediaInfo,
  ProjectFile,
  SerializedTelemetrySession,
} from './models.js';

export interface OpenVboResult {
  path: string;
  session: SerializedTelemetrySession;
}

export interface ProjectOpenResult {
  path: string;
  project: ProjectFile;
}

export interface FlappedEarApi {
  openVideo(): Promise<MediaInfo | null>;
  openVbo(): Promise<OpenVboResult | null>;
  inspectEnvironment(): Promise<EnvironmentInfo>;
  openProject(): Promise<ProjectOpenResult | null>;
  saveProject(project: ProjectFile, path?: string): Promise<string | null>;
}

export const IPC = {
  openVideo: 'media:open-video',
  openVbo: 'telemetry:open-vbo',
  environment: 'system:environment',
  openProject: 'project:open',
  saveProject: 'project:save',
} as const;
