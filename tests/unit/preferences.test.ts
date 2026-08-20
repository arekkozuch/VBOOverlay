import { describe, expect, it } from 'vitest';
import {
  parseEditorPreferences,
  serializeEditorPreferences,
  type EditorPreferences,
} from '../../src/renderer/editor/preferences';

const preferences: EditorPreferences = {
  version: 1,
  scene: {
    widgets: [
      {
        id: 'speed-1',
        type: 'speed',
        x: 0.1,
        y: 0.2,
        width: 0.15,
        height: 0.16,
        scale: 1,
        rotation: 0,
        opacity: 1,
        visible: true,
        settings: { source: 'acceleratorpos', textColor: '#ffffff' },
      },
    ],
  },
  sync: { offset: 2.374, timeScale: 1 },
  liveTelemetry: [{ id: 'oil', channel: 'oiltemp', label: 'Oil', unit: '°C', decimals: 1 }],
};

describe('editor preferences', () => {
  it('round trips widget, sync, and live telemetry choices', () => {
    expect(parseEditorPreferences(serializeEditorPreferences(preferences))).toEqual(preferences);
  });

  it('ignores malformed or unsupported data', () => {
    expect(parseEditorPreferences(null)).toBeUndefined();
    expect(parseEditorPreferences('{broken')).toBeUndefined();
    expect(parseEditorPreferences('{"version":2}')).toBeUndefined();
  });
});
