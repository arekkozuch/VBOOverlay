export interface MapProvider {
  id: string;
  name: string;
  createStyle(): string | Record<string, unknown> | null;
}
export class NoMapProvider implements MapProvider {
  id = 'none';
  name = 'No map';
  createStyle(): null {
    return null;
  }
}
export class RasterXyzProvider implements MapProvider {
  constructor(
    public id: string,
    public name: string,
    private readonly tileUrl: string,
    private readonly attribution: string,
  ) {}
  createStyle(): Record<string, unknown> {
    return {
      version: 8,
      sources: {
        raster: {
          type: 'raster',
          tiles: [this.tileUrl],
          tileSize: 256,
          attribution: this.attribution,
        },
      },
      layers: [{ id: 'raster', type: 'raster', source: 'raster' }],
    };
  }
}
export class StyleUrlProvider implements MapProvider {
  constructor(
    public id: string,
    public name: string,
    private readonly styleUrl: string,
  ) {}
  createStyle(): string {
    return this.styleUrl;
  }
}

// Public OSM tiles are appropriate for interactive light use, never bulk export.
export const MAP_PROVIDERS: MapProvider[] = [
  new NoMapProvider(),
  new RasterXyzProvider(
    'osm',
    'OpenStreetMap',
    'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
    '© OpenStreetMap contributors',
  ),
];
