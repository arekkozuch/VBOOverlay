export const TELEMETRY_ALIASES = [
  'speed',
  'rpm',
  'throttle',
  'brake',
  'heartRate',
  'latitude',
  'longitude',
  'lateralAcceleration',
  'longitudinalAcceleration',
] as const;
export type TelemetryAlias = (typeof TELEMETRY_ALIASES)[number];

export interface SerializedTelemetryChannel {
  name: string;
  unit?: string;
  timestamps: number[];
  values: number[];
}

export interface SerializedTelemetrySession {
  duration: number;
  startTime: number;
  metadata: Record<string, string>;
  channels: Record<string, SerializedTelemetryChannel>;
  aliases: Partial<Record<TelemetryAlias, string>>;
  warnings: string[];
  sampleCount: number;
}

export interface VideoStreamInfo {
  index: number;
  codec: string;
  codecType: string;
  width?: number;
  height?: number;
  pixelFormat?: string;
  frameRate?: number;
  averageFrameRate?: number;
  timeBase?: string;
  tags: Record<string, string>;
}

export interface MediaInfo {
  path: string;
  mediaUrl: string;
  duration: number;
  format: string;
  bitRate?: number;
  streams: VideoStreamInfo[];
  video: VideoStreamInfo;
  audio: VideoStreamInfo[];
  metadata: VideoStreamInfo[];
  variableFrameRate: boolean;
  hasGoProTelemetry: boolean;
}

export interface SyncTransform {
  offset: number;
  timeScale: number;
}

export type WidgetType =
  'speed' | 'rpm' | 'heartRate' | 'pedals' | 'gForce' | 'track' | 'customValue';

export interface WidgetInstance {
  id: string;
  type: WidgetType;
  x: number;
  y: number;
  width: number;
  height: number;
  scale: number;
  rotation: number;
  opacity: number;
  visible: boolean;
  settings: Record<string, string | number | boolean>;
}

export interface WidgetScene {
  widgets: WidgetInstance[];
}

export interface ProjectFile {
  version: 1;
  videoPath?: string;
  vboPath?: string;
  sync: SyncTransform;
  scene: WidgetScene;
  mapSettings: { providerId: string; styleUrl?: string };
  exportSettings: { quality: 'fast' | 'high' | 'maximum' };
}

export interface EncoderInfo {
  name: string;
  hardware: boolean;
  label: string;
}

export interface EnvironmentInfo {
  ffmpegVersion: string;
  ffprobeVersion: string;
  encoders: EncoderInfo[];
}

export interface GoProTelemetryInfo {
  availableChannels: string[];
  sampleCounts: Record<string, number>;
  duration: number;
  packetCount: number;
}

export interface SyncResult {
  offset: number;
  timeScale: number;
  confidence: number;
  strategy: string;
  diagnostics: {
    correlation: number;
    peakUniqueness: number;
    validSamples: number;
    sampleRate: number;
    coarseOffset?: number;
  };
}
