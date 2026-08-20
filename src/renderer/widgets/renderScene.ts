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
  const background = String(widget.settings.backgroundColor ?? '#080b10');
  const backgroundOpacity = Math.max(
    0,
    Math.min(1, Number(widget.settings.backgroundOpacity ?? 0.72)),
  );
  context.save();
  context.globalAlpha *= backgroundOpacity;
  context.fillStyle = background;
  context.fill();
  context.restore();
  context.strokeStyle = String(widget.settings.borderColor ?? '#ffffff38');
  context.lineWidth = Math.max(0, Number(widget.settings.borderWidth ?? 1));
  context.beginPath();
  context.roundRect(0, 0, width, height, 12);
  context.stroke();
}

function text(
  context: CanvasRenderingContext2D,
  value: string,
  x: number,
  y: number,
  size: number,
  align: CanvasTextAlign = 'left',
  color = '#f7fafc',
): void {
  context.font = `700 ${size}px Inter, system-ui, sans-serif`;
  context.textAlign = align;
  context.textBaseline = 'middle';
  context.fillStyle = color;
  context.fillText(value, x, y);
}

function bar(
  context: CanvasRenderingContext2D,
  label: string,
  value: number | undefined,
  y: number,
  width: number,
  color: string,
  textColor: string,
): void {
  text(context, label, 12, y, 11, 'left', textColor);
  const x = 82,
    valueWidth = 44,
    barWidth = width - x - valueWidth - 10;
  context.fillStyle = '#29313b';
  context.fillRect(x, y - 6, barWidth, 12);
  context.fillStyle = color;
  context.fillRect(x, y - 6, barWidth * Math.min(1, Math.max(0, (value ?? 0) / 100)), 12);
  text(
    context,
    value === undefined ? '--' : `${Math.round(value)}%`,
    width - 8,
    y,
    11,
    'right',
    textColor,
  );
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
    const textColor = String(widget.settings.textColor ?? '#f7fafc');
    const accentColor = String(widget.settings.accentColor ?? '#45d6ff');
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
        textColor,
      );
      text(context, mph ? 'mph' : 'km/h', width / 2, height * 0.78, 13, 'center', textColor);
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
        textColor,
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
      context.fillStyle = String(widget.settings.accentColor ?? '#ff4f68');
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
      const configuredAcceleratorLabel = widget.settings.acceleratorLabel;
      const acceleratorLabel =
        typeof configuredAcceleratorLabel === 'string' && configuredAcceleratorLabel
          ? configuredAcceleratorLabel
          : acceleratorSource.toLowerCase().includes('accelerator')
            ? 'ACCELERATOR'
            : 'THROTTLE';
      const configuredBrakeLabel = widget.settings.brakeLabel;
      const brakeLabel =
        typeof configuredBrakeLabel === 'string' && configuredBrakeLabel
          ? configuredBrakeLabel
          : 'BRAKE';
      bar(
        context,
        acceleratorLabel,
        session ? valueAt(session, acceleratorSource, input.telemetryTime) : undefined,
        height * 0.35,
        width,
        '#31d17c',
        textColor,
      );
      bar(
        context,
        brakeLabel,
        session
          ? valueAt(session, widgetChannel(widget, 'brakeSource', 'brake'), input.telemetryTime)
          : undefined,
        height * 0.68,
        width,
        '#f25f5c',
        textColor,
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
      context.lineWidth = 1;
      context.beginPath();
      context.moveTo(width * 0.15, height / 2);
      context.lineTo(width * 0.85, height / 2);
      context.moveTo(width / 2, height * 0.15);
      context.lineTo(width / 2, height * 0.85);
      context.stroke();
      context.beginPath();
      context.arc(width / 2, height / 2, Math.min(width, height) * 0.35, 0, Math.PI * 2);
      context.stroke();
      context.fillStyle = accentColor;
      context.beginPath();
      context.arc(
        width / 2 + Math.max(-1, Math.min(1, lateral)) * width * 0.25,
        height / 2 - Math.max(-1, Math.min(1, longitudinal)) * height * 0.25,
        8,
        0,
        Math.PI * 2,
      );
      context.fill();
      const combined = Math.sqrt(lateral * lateral + longitudinal * longitudinal);
      text(
        context,
        `|G| ${combined.toFixed(2)} g`,
        width / 2,
        height * 0.92,
        12,
        'center',
        textColor,
      );
    } else if (widget.type === 'track' && track?.points.length) {
      const padding = 14;
      context.strokeStyle = String(widget.settings.accentColor ?? '#57e6a0');
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
    } else if (widget.type === 'customValue') {
      const source = widgetChannel(widget, 'source', '');
      const rawValue = session ? valueAt(session, source, input.telemetryTime) : undefined;
      const multiplier = Number(widget.settings.multiplier ?? 1);
      const decimals = Math.max(0, Math.min(6, Number(widget.settings.decimals ?? 1)));
      const value = rawValue === undefined ? undefined : rawValue * multiplier;
      const label = String((widget.settings.label ?? source) || 'VALUE');
      const unit = String(widget.settings.unit ?? '');
      text(context, label, width / 2, height * 0.22, 11, 'center', textColor);
      text(
        context,
        value === undefined || !Number.isFinite(value) ? '--' : value.toFixed(decimals),
        width / 2,
        height * 0.58,
        Math.min(36, height * 0.36),
        'center',
        textColor,
      );
      if (unit) text(context, unit, width / 2, height * 0.84, 12, 'center', textColor);
    }
    const configuredTitle = widget.settings.title;
    if (typeof configuredTitle === 'string' && configuredTitle)
      text(context, configuredTitle, width / 2, 12, 10, 'center', textColor);
    context.restore();
  }
}
