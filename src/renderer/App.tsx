import { useEffect, useMemo, useRef, useState } from 'react';
import type {
  EnvironmentInfo,
  MediaInfo,
  ProjectFile,
  SyncTransform,
  SyncResult,
  WidgetInstance,
  WidgetScene,
  WidgetType,
} from '../shared/models';
import {
  hydrateSession,
  valueAt,
  videoToTelemetryTime,
  type TelemetrySession,
} from '../telemetry/core/session';
import { OverlayCanvas } from './widgets/OverlayCanvas';

const WIDGET_LABELS: Record<WidgetType, string> = {
  speed: 'FE Speed',
  rpm: 'FE RPM',
  heartRate: 'FE Heart Rate',
  pedals: 'FE Pedals',
  gForce: 'FE G-Force',
  track: 'FE Track',
};
const DEFAULT_SIZES: Record<WidgetType, [number, number]> = {
  speed: [0.15, 0.16],
  rpm: [0.2, 0.09],
  heartRate: [0.2, 0.09],
  pedals: [0.25, 0.13],
  gForce: [0.14, 0.19],
  track: [0.2, 0.28],
};

function newWidget(type: WidgetType, index: number): WidgetInstance {
  const [width, height] = DEFAULT_SIZES[type];
  return {
    id: crypto.randomUUID(),
    type,
    x: 0.04 + (index % 3) * 0.2,
    y: 0.05 + Math.floor(index / 3) * 0.22,
    width,
    height,
    scale: 1,
    rotation: 0,
    opacity: 1,
    visible: true,
    settings: type === 'speed' ? { unit: 'km/h' } : {},
  };
}
const initialScene: WidgetScene = {
  widgets: [newWidget('speed', 0), newWidget('rpm', 1), newWidget('heartRate', 2)],
};
const emptyProject = (): ProjectFile => ({
  version: 1,
  sync: { offset: 0, timeScale: 1 },
  scene: initialScene,
  mapSettings: { providerId: 'none' },
  exportSettings: { quality: 'high' },
});

function formatTime(seconds: number): string {
  const safe = Number.isFinite(seconds) ? Math.max(0, seconds) : 0;
  const minutes = Math.floor(safe / 60),
    remainder = safe % 60;
  return `${String(minutes).padStart(2, '0')}:${remainder.toFixed(3).padStart(6, '0')}`;
}

export function App(): React.JSX.Element {
  const video = useRef<HTMLVideoElement>(null);
  const [media, setMedia] = useState<MediaInfo>();
  const [session, setSession] = useState<TelemetrySession>();
  const [videoPath, setVideoPath] = useState<string>();
  const [vboPath, setVboPath] = useState<string>();
  const [scene, setScene] = useState<WidgetScene>(initialScene);
  const [sync, setSync] = useState<SyncTransform>({ offset: 0, timeScale: 1 });
  const [time, setTime] = useState(0);
  const [duration, setDuration] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [selectedId, setSelectedId] = useState<string>();
  const [projectPath, setProjectPath] = useState<string>();
  const [environment, setEnvironment] = useState<EnvironmentInfo>();
  const [syncCandidate, setSyncCandidate] = useState<SyncResult>();
  const [syncing, setSyncing] = useState(false);
  const [message, setMessage] = useState('Open a video and VBO to begin.');
  const telemetryTime = videoToTelemetryTime(time, sync);
  const selected = scene.widgets.find(({ id }) => id === selectedId);
  const telemetrySummary = useMemo(
    () => ({
      speed: session ? valueAt(session, 'speed', telemetryTime) : undefined,
      rpm: session ? valueAt(session, 'rpm', telemetryTime) : undefined,
      heartRate: session ? valueAt(session, 'heartRate', telemetryTime) : undefined,
    }),
    [session, telemetryTime],
  );

  useEffect(() => {
    void window.flappedEar
      .inspectEnvironment()
      .then(setEnvironment)
      .catch((error) => setMessage(String(error)));
  }, []);
  useEffect(() => {
    let frame = 0;
    const update = () => {
      if (video.current) setTime(video.current.currentTime);
      frame = requestAnimationFrame(update);
    };
    if (playing) frame = requestAnimationFrame(update);
    return () => cancelAnimationFrame(frame);
  }, [playing]);

  const updateWidget = (id: string, changes: Partial<WidgetInstance>) =>
    setScene((current) => ({
      widgets: current.widgets.map((widget) =>
        widget.id === id ? { ...widget, ...changes } : widget,
      ),
    }));
  const project = (): ProjectFile => ({ ...emptyProject(), videoPath, vboPath, sync, scene });

  async function openVideo(): Promise<void> {
    try {
      const result = await window.flappedEar.openVideo();
      if (!result) return;
      setMedia(result);
      setVideoPath(result.path);
      setDuration(result.duration);
      setMessage(
        result.hasGoProTelemetry
          ? 'Video opened. GoPro telemetry metadata detected.'
          : 'Video opened. No GoPro telemetry found; manual sync is available.',
      );
    } catch (error) {
      setMessage(`Video error: ${String(error)}`);
    }
  }
  async function openVbo(): Promise<void> {
    try {
      const result = await window.flappedEar.openVbo();
      if (!result) return;
      const hydrated = hydrateSession(result.session);
      setSession(hydrated);
      setVboPath(result.path);
      if (!media) setDuration(hydrated.duration);
      setMessage(
        `VBO opened: ${hydrated.sampleCount.toLocaleString()} samples, ${hydrated.channels.size} numeric channels${hydrated.warnings.length ? `, ${hydrated.warnings.length} warning(s)` : ''}.`,
      );
    } catch (error) {
      setMessage(`VBO error: ${String(error)}`);
    }
  }
  async function autoSync(): Promise<void> {
    setSyncing(true);
    setSyncCandidate(undefined);
    setMessage('Extracting GoPro telemetry and correlating GPS speed…');
    try {
      const result = await window.flappedEar.autoSync();
      setSyncCandidate(result);
      setMessage(
        `Auto sync found offset ${result.offset >= 0 ? '+' : ''}${result.offset.toFixed(3)}s with ${(result.confidence * 100).toFixed(0)}% confidence.`,
      );
    } catch (error) {
      setMessage(`Auto sync error: ${String(error)}`);
    } finally {
      setSyncing(false);
    }
  }
  async function openProject(): Promise<void> {
    try {
      const result = await window.flappedEar.openProject();
      if (!result) return;
      setProjectPath(result.path);
      setVideoPath(result.project.videoPath);
      setVboPath(result.project.vboPath);
      setSync(result.project.sync);
      setScene(result.project.scene);
      setMessage('Project opened. Re-open source media to restore previews.');
    } catch (error) {
      setMessage(`Project error: ${String(error)}`);
    }
  }
  async function saveProject(saveAs = false): Promise<void> {
    try {
      const path = await window.flappedEar.saveProject(project(), saveAs ? undefined : projectPath);
      if (path) {
        setProjectPath(path);
        setMessage('Project saved.');
      }
    } catch (error) {
      setMessage(`Save error: ${String(error)}`);
    }
  }
  function seek(next: number): void {
    const value = Math.max(0, Math.min(duration, next));
    setTime(value);
    if (video.current) video.current.currentTime = value;
  }
  function togglePlayback(): void {
    if (!video.current) return;
    if (video.current.paused) void video.current.play();
    else video.current.pause();
  }

  return (
    <main className="app-shell">
      <header>
        <div className="brand">
          <span className="mark">FE</span>
          <div>
            <strong>FlappedEar Telemetry</strong>
            <small>{projectPath ?? 'Untitled project'}</small>
          </div>
        </div>
        <nav>
          <button
            onClick={() => {
              setMedia(undefined);
              setSession(undefined);
              setProjectPath(undefined);
              setSync({ offset: 0, timeScale: 1 });
              setScene(initialScene);
            }}
          >
            New
          </button>
          <button onClick={() => void openProject()}>Open Project</button>
          <button onClick={() => void saveProject()}>Save</button>
          <button onClick={() => void saveProject(true)}>Save As</button>
          <button
            className="export"
            disabled
            title="Frame-streaming HEVC export is the next implementation milestone"
          >
            Export HEVC
          </button>
        </nav>
      </header>
      <section className="workspace">
        <aside className="library">
          <h2>Widgets</h2>
          <p>Add telemetry overlays</p>
          {(Object.keys(WIDGET_LABELS) as WidgetType[]).map((type) => (
            <button
              key={type}
              onClick={() =>
                setScene((current) => ({
                  widgets: [...current.widgets, newWidget(type, current.widgets.length)],
                }))
              }
            >
              <span>+</span>
              {WIDGET_LABELS[type]}
            </button>
          ))}
          <h2>Sources</h2>
          <button onClick={() => void openVideo()}>Open Video</button>
          <button onClick={() => void openVbo()}>Open VBO</button>
        </aside>
        <section className="stage-column">
          <div className="stage">
            <div className="video-frame">
              {media ? (
                <video
                  ref={video}
                  src={media.mediaUrl}
                  onLoadedMetadata={(event) => setDuration(event.currentTarget.duration)}
                  onPlay={() => setPlaying(true)}
                  onPause={() => setPlaying(false)}
                  onSeeked={(event) => setTime(event.currentTarget.currentTime)}
                  onError={(event) => {
                    const mediaError = event.currentTarget.error;
                    setPlaying(false);
                    setMessage(
                      `Video playback error${mediaError ? ` (${mediaError.code})` : ''}: ${mediaError?.message || 'unknown media error'}`,
                    );
                  }}
                />
              ) : (
                <div className="empty-stage">
                  <strong>VIDEO PREVIEW</strong>
                  <span>Open an MP4 or MOV file</span>
                </div>
              )}
              <OverlayCanvas
                scene={scene}
                session={session}
                telemetryTime={telemetryTime}
                selectedId={selectedId}
                onSelect={setSelectedId}
                onMove={(id, x, y) => updateWidget(id, { x, y })}
              />
            </div>
          </div>
          <div className="transport">
            <button onClick={togglePlayback} disabled={!media}>
              {playing ? '❚❚' : '▶'}
            </button>
            <time>{formatTime(time)}</time>
            <input
              aria-label="Timeline"
              type="range"
              min="0"
              max={duration || 1}
              step="0.001"
              value={Math.min(time, duration || 1)}
              onChange={(event) => seek(Number(event.target.value))}
            />
            <time>{formatTime(duration)}</time>
          </div>
          <div className="timeline-info">
            <div>
              <b>VIDEO</b>
              <span>
                {media
                  ? `${media.video.width}×${media.video.height} · ${media.video.codec} · ${(media.video.averageFrameRate ?? 0).toFixed(3)} fps`
                  : 'Not loaded'}
              </span>
            </div>
            <div>
              <b>VBO</b>
              <span>
                {session
                  ? `${session.sampleCount.toLocaleString()} samples · ${session.channels.size} channels · ${formatTime(session.duration)}`
                  : 'Not loaded'}
              </span>
            </div>
            <strong>
              offset {sync.offset >= 0 ? '+' : ''}
              {sync.offset.toFixed(3)}s
            </strong>
          </div>
        </section>
        <aside className="inspector">
          <h2>Inspector</h2>
          {selected ? (
            <>
              <label>
                Widget
                <input value={WIDGET_LABELS[selected.type]} disabled />
              </label>
              <label>
                X
                <input
                  type="number"
                  min="0"
                  max="1"
                  step=".01"
                  value={selected.x}
                  onChange={(event) => updateWidget(selected.id, { x: Number(event.target.value) })}
                />
              </label>
              <label>
                Y
                <input
                  type="number"
                  min="0"
                  max="1"
                  step=".01"
                  value={selected.y}
                  onChange={(event) => updateWidget(selected.id, { y: Number(event.target.value) })}
                />
              </label>
              <label>
                Scale
                <input
                  type="range"
                  min=".25"
                  max="3"
                  step=".05"
                  value={selected.scale}
                  onChange={(event) =>
                    updateWidget(selected.id, { scale: Number(event.target.value) })
                  }
                />
                <output>{selected.scale.toFixed(2)}×</output>
              </label>
              <label>
                Opacity
                <input
                  type="range"
                  min="0"
                  max="1"
                  step=".05"
                  value={selected.opacity}
                  onChange={(event) =>
                    updateWidget(selected.id, { opacity: Number(event.target.value) })
                  }
                />
              </label>
              {selected.type === 'speed' && (
                <label>
                  Units
                  <select
                    value={String(selected.settings.unit)}
                    onChange={(event) =>
                      updateWidget(selected.id, {
                        settings: { ...selected.settings, unit: event.target.value },
                      })
                    }
                  >
                    <option value="km/h">km/h</option>
                    <option value="mph">mph</option>
                  </select>
                </label>
              )}
              <label className="check">
                <input
                  type="checkbox"
                  checked={selected.visible}
                  onChange={(event) => updateWidget(selected.id, { visible: event.target.checked })}
                />
                Visible
              </label>
              <button
                className="danger"
                onClick={() => {
                  setScene((current) => ({
                    widgets: current.widgets.filter((widget) => widget.id !== selected.id),
                  }));
                  setSelectedId(undefined);
                }}
              >
                Delete widget
              </button>
            </>
          ) : (
            <p>Select a widget in the preview.</p>
          )}
          <h2>Synchronization</h2>
          <label>
            Telemetry offset (s)
            <input
              type="number"
              step=".001"
              value={sync.offset}
              onChange={(event) =>
                setSync((current) => ({ ...current, offset: Number(event.target.value) }))
              }
            />
          </label>
          <input
            aria-label="Fine tune telemetry offset"
            type="range"
            min="-5"
            max="5"
            step=".01"
            value={Math.max(-5, Math.min(5, sync.offset))}
            onChange={(event) =>
              setSync((current) => ({ ...current, offset: Number(event.target.value) }))
            }
          />
          <div className="button-row">
            <button onClick={() => setSync({ offset: 0, timeScale: 1 })}>Reset</button>
            <button
              disabled={!media?.hasGoProTelemetry || !session || syncing}
              onClick={() => void autoSync()}
            >
              {syncing ? 'Syncing…' : 'Auto Sync'}
            </button>
          </div>
          {syncCandidate && (
            <div className="sync-result">
              <strong>{syncCandidate.strategy}</strong>
              <span>
                Offset {syncCandidate.offset >= 0 ? '+' : ''}
                {syncCandidate.offset.toFixed(3)}s
              </span>
              <span>Confidence {(syncCandidate.confidence * 100).toFixed(0)}%</span>
              <small>Correlation {syncCandidate.diagnostics.correlation.toFixed(3)}</small>
              <button
                onClick={() =>
                  setSync({
                    offset: syncCandidate.offset,
                    timeScale: syncCandidate.timeScale,
                  })
                }
              >
                Apply
              </button>
            </div>
          )}
          <h2>Live telemetry</h2>
          <dl>
            <dt>Speed</dt>
            <dd>
              {telemetrySummary.speed === undefined
                ? '—'
                : `${telemetrySummary.speed.toFixed(1)} km/h`}
            </dd>
            <dt>RPM</dt>
            <dd>{telemetrySummary.rpm === undefined ? '—' : Math.round(telemetrySummary.rpm)}</dd>
            <dt>Heart rate</dt>
            <dd>
              {telemetrySummary.heartRate === undefined
                ? '—'
                : `${Math.round(telemetrySummary.heartRate)} BPM`}
            </dd>
          </dl>
        </aside>
      </section>
      <footer>
        <span>{message}</span>
        <span>
          {environment
            ? `HEVC: ${environment.encoders.map((item) => item.label).join(', ') || 'unavailable'}`
            : 'Inspecting FFmpeg…'}
        </span>
      </footer>
    </main>
  );
}
