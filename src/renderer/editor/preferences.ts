import type { LiveTelemetryItem, SyncTransform, WidgetScene } from '../../shared/models';

export interface EditorPreferences {
  version: 1;
  scene: WidgetScene;
  sync: SyncTransform;
  liveTelemetry: LiveTelemetryItem[];
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null;
}

export function parseEditorPreferences(raw: string | null): EditorPreferences | undefined {
  if (!raw) return undefined;
  try {
    const value: unknown = JSON.parse(raw);
    if (
      !isRecord(value) ||
      value.version !== 1 ||
      !isRecord(value.scene) ||
      !Array.isArray(value.scene.widgets) ||
      !isRecord(value.sync) ||
      typeof value.sync.offset !== 'number' ||
      typeof value.sync.timeScale !== 'number' ||
      !Array.isArray(value.liveTelemetry)
    )
      return undefined;
    return value as unknown as EditorPreferences;
  } catch {
    return undefined;
  }
}

export function serializeEditorPreferences(preferences: EditorPreferences): string {
  return JSON.stringify(preferences);
}
