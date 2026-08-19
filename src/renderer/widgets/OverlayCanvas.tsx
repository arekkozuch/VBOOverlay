import { useEffect, useMemo, useRef } from 'react';
import type { WidgetScene } from '../../shared/models';
import type { TelemetrySession } from '../../telemetry/core/session';
import { buildTrackGeometry } from '../../telemetry/geometry/track';
import { renderWidgetScene } from './renderScene';

export function OverlayCanvas({
  scene,
  session,
  telemetryTime,
  selectedId,
  onSelect,
  onMove,
}: {
  scene: WidgetScene;
  session?: TelemetrySession;
  telemetryTime: number;
  selectedId?: string;
  onSelect(id?: string): void;
  onMove(id: string, x: number, y: number): void;
}): React.JSX.Element {
  const canvas = useRef<HTMLCanvasElement>(null);
  const track = useMemo(() => (session ? buildTrackGeometry(session) : undefined), [session]);
  useEffect(() => {
    if (canvas.current) renderWidgetScene(canvas.current, scene, { session, telemetryTime, track });
  }, [scene, session, telemetryTime, track]);
  return (
    <div className="overlay-layer" onPointerDown={() => onSelect(undefined)}>
      <canvas ref={canvas} width={1280} height={720} />
      {scene.widgets
        .filter((widget) => widget.visible)
        .map((widget) => (
          <button
            key={widget.id}
            className={`widget-hitbox ${selectedId === widget.id ? 'selected' : ''}`}
            style={{
              left: `${widget.x * 100}%`,
              top: `${widget.y * 100}%`,
              width: `${widget.width * widget.scale * 100}%`,
              height: `${widget.height * widget.scale * 100}%`,
              transform: `rotate(${widget.rotation}deg)`,
            }}
            aria-label={`Select ${widget.type} widget`}
            onPointerDown={(event) => {
              event.stopPropagation();
              onSelect(widget.id);
              const target = event.currentTarget;
              target.setPointerCapture(event.pointerId);
              const bounds = target.parentElement!.getBoundingClientRect();
              const startX = event.clientX,
                startY = event.clientY,
                originalX = widget.x,
                originalY = widget.y;
              const move = (moveEvent: PointerEvent) =>
                onMove(
                  widget.id,
                  Math.max(
                    0,
                    Math.min(
                      1 - widget.width * widget.scale,
                      originalX + (moveEvent.clientX - startX) / bounds.width,
                    ),
                  ),
                  Math.max(
                    0,
                    Math.min(
                      1 - widget.height * widget.scale,
                      originalY + (moveEvent.clientY - startY) / bounds.height,
                    ),
                  ),
                );
              const up = () => {
                target.removeEventListener('pointermove', move);
                target.removeEventListener('pointerup', up);
              };
              target.addEventListener('pointermove', move);
              target.addEventListener('pointerup', up);
            }}
          />
        ))}
    </div>
  );
}
