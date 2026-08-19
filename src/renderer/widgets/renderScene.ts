import type { WidgetInstance, WidgetScene } from '../../shared/models';
import type { TelemetrySession } from '../../telemetry/core/session';
import { valueAt } from '../../telemetry/core/session';
import {
  buildTrackGeometry,
  currentTrackPoint,
  type TrackGeometry,
} from '../../telemetry/geometry/track';

export interface RenderContext {
  session?: TelemetrySession;
  telemetryTime: number;
  track?: TrackGeometry;
}

function panel(
  context: CanvasRenderingContext2D,
  widget: WidgetInstance,
  width: number,
  height: number,
): void {
  context.fillStyle = 'rgba(8, 11, 16, .72)';
  context.strokeStyle = 'rgba(255, 255, 255, .22)';
  context.lineWidth = 1;
  context.beginPath();
  context.roundRect(0, 0, width, height, 12);
  context.fill();
  context.stroke();
}

function text(
  context: CanvasRenderingContext2D,
  value: string,
  x: number,
  y: number,
  size: number,
  align: CanvasTextAlign = 'left',
): void {
  context.font = `700 ${size}px Inter, system-ui, sans-serif`;
  context.textAlign = align;
  context.textBaseline = 'middle';
  context.fillStyle = '#f7fafc';
  context.fillText(value, x, y);
}

function bar(
  context: CanvasRenderingContext2D,
  label: string,
  value: number | undefined,
  y: number,
  width: number,
): void {
  text(context, label, 12, y, 11);
  const x = 82,
    barWidth = width - x - 12;
  context.fillStyle = '#29313b';
  context.fillRect(x, y - 6, barWidth, 12);
  context.fillStyle = label === 'BRAKE' ? '#f25f5c' : '#31d17c';
  context.fillRect(x, y - 6, barWidth * Math.min(1, Math.max(0, (value ?? 0) / 100)), 12);
}

function widgetChannel(widget: WidgetInstance, setting: string, automatic: string): string {
  const configured = widget.settings[setting];
  return typeof configured === 'string' && configured ? configured : automatic;
}

export function renderWidgetScene(
  canvas: HTMLCanvasElement,
  scene: WidgetScene,
  input: RenderContext,
): void {
  const context = canvas.getContext('2d');
  if (!context) return;
  context.clearRect(0, 0, canvas.width, canvas.height);
  const track = input.track ?? (input.session ? buildTrackGeometry(input.session) : undefined);
  for (const widget of scene.widgets) {
    if (!widget.visible) continue;
    const x = widget.x * canvas.width,
      y = widget.y * canvas.height;
    const width = widget.width * canvas.width,
      height = widget.height * canvas.height;
    context.save();
    context.translate(x, y);
    context.rotate((widget.rotation * Math.PI) / 180);
    context.scale(widget.scale, widget.scale);
    context.globalAlpha = widget.opacity;
    panel(context, widget, width, height);
    const session = input.session;
    if (widget.type === 'speed') {
      let speed = session
        ? valueAt(session, widgetChannel(widget, 'source', 'speed'), input.telemetryTime)
        : undefined;
      const mph = widget.settings.unit === 'mph';
      if (speed !== undefined && mph) speed *= 0.621371;
      text(
        context,
        speed === undefined ? '--' : Math.round(speed).toString(),
        width / 2,
        height * 0.42,
        Math.min(52, height * 0.48),
        'center',
      );
      text(context, mph ? 'mph' : 'km/h', width / 2, height * 0.78, 13, 'center');
    } else if (widget.type === 'rpm') {
      const rpm = session
        ? valueAt(session, widgetChannel(widget, 'source', 'rpm'), input.telemetryTime, 'nearest')
        : undefined;
      text(
        context,
        `${rpm === undefined ? '--' : Math.round(rpm)} RPM`,
        width / 2,
        height / 2,
        Math.min(25, height * 0.35),
        'center',
      );
    } else if (widget.type === 'heartRate') {
      const heartRate = session
        ? valueAt(
            session,
            widgetChannel(widget, 'source', 'heartRate'),
            input.telemetryTime,
            'nearest',
          )
        : undefined;
      context.fillStyle = '#ff4f68';
      context.font = `700 ${Math.min(28, height * 0.4)}px sans-serif`;
      context.textAlign = 'center';
      context.textBaseline = 'middle';
      context.fillText(
        `♥ ${heartRate === undefined ? '--' : Math.round(heartRate)} BPM`,
        width / 2,
        height / 2,
      );
    } else if (widget.type === 'pedals') {
      const acceleratorSource = widgetChannel(widget, 'acceleratorSource', 'throttle');
      bar(
        context,
        acceleratorSource.toLowerCase().includes('accelerator') ? 'ACCELERATOR' : 'THROTTLE',
        session ? valueAt(session, acceleratorSource, input.telemetryTime) : undefined,
        height * 0.35,
        width,
      );
      bar(
        context,
        'BRAKE',
        session
          ? valueAt(session, widgetChannel(widget, 'brakeSource', 'brake'), input.telemetryTime)
          : undefined,
        height * 0.68,
        width,
      );
    } else if (widget.type === 'gForce') {
      const lateral = session
        ? (valueAt(
            session,
            widgetChannel(widget, 'lateralSource', 'lateralAcceleration'),
            input.telemetryTime,
          ) ?? 0)
        : 0;
      const longitudinal = session
        ? (valueAt(
            session,
            widgetChannel(widget, 'longitudinalSource', 'longitudinalAcceleration'),
            input.telemetryTime,
          ) ?? 0)
        : 0;
      context.strokeStyle = '#82909f';
      context.beginPath();
      context.arc(width / 2, height / 2, Math.min(width, height) * 0.35, 0, Math.PI * 2);
      context.stroke();
      context.fillStyle = '#45d6ff';
      context.beginPath();
      context.arc(
        width / 2 + Math.max(-1, Math.min(1, lateral)) * width * 0.25,
        height / 2 - Math.max(-1, Math.min(1, longitudinal)) * height * 0.25,
        8,
        0,
        Math.PI * 2,
      );
      context.fill();
    } else if (widget.type === 'track' && track?.points.length) {
      const padding = 14;
      context.strokeStyle = '#57e6a0';
      context.lineWidth = 3;
      context.beginPath();
      track.points.forEach((point, index) => {
        const px = padding + point.x * (width - padding * 2),
          py = padding + point.y * (height - padding * 2);
        if (index === 0) context.moveTo(px, py);
        else context.lineTo(px, py);
      });
      context.stroke();
      const point = session ? currentTrackPoint(session, input.telemetryTime, track) : undefined;
      if (point) {
        context.fillStyle = '#fff';
        context.beginPath();
        context.arc(
          padding + point.x * (width - padding * 2),
          padding + point.y * (height - padding * 2),
          6,
          0,
          Math.PI * 2,
        );
        context.fill();
      }
    }
    context.restore();
  }
}
