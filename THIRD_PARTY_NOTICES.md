# Third-Party Notices

This file is an engineering inventory, not final legal advice. Distribution—especially bundled FFmpeg/HEVC components and commercial map providers—requires review before release.

| Project/package                 | Purpose                                                 | License                                                  | Upstream                                                                                 |
| ------------------------------- | ------------------------------------------------------- | -------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| Electron                        | Cross-platform desktop runtime                          | MIT                                                      | https://github.com/electron/electron                                                     |
| React / React DOM               | Editor user interface                                   | MIT                                                      | https://github.com/facebook/react                                                        |
| Vite and `@vitejs/plugin-react` | Development and renderer build                          | MIT                                                      | https://github.com/vitejs/vite                                                           |
| TypeScript                      | Static type checking                                    | Apache-2.0                                               | https://github.com/microsoft/TypeScript                                                  |
| Vitest                          | Unit and integration tests                              | MIT                                                      | https://github.com/vitest-dev/vitest                                                     |
| ESLint and typescript-eslint    | Linting                                                 | MIT / BSD-2-Clause                                       | https://github.com/eslint/eslint, https://github.com/typescript-eslint/typescript-eslint |
| Prettier                        | Formatting                                              | MIT                                                      | https://github.com/prettier/prettier                                                     |
| MapLibre GL JS                  | Future interactive map rendering                        | BSD-3-Clause                                             | https://github.com/maplibre/maplibre-gl-js                                               |
| OpenStreetMap data/tiles        | Optional configurable map source                        | ODbL data; tile usage policy separately applies          | https://www.openstreetmap.org/copyright                                                  |
| FFmpeg                          | External video probing and future encoding              | Build-dependent LGPL/GPL and optional component terms    | https://ffmpeg.org                                                                       |
| x265 / `libx265`                | Software HEVC encoder when present in the user's FFmpeg | GPL-2.0-or-later; patent considerations may apply        | https://bitbucket.org/multicoreware/x265_git                                             |
| Platform HEVC encoders          | Optional hardware encoding discovered at runtime        | Platform/vendor terms and possible patent considerations | Apple, NVIDIA, Intel, AMD documentation                                                  |

FFmpeg is not bundled by this repository. The application discovers the user's executable at runtime. That does not remove the need to review licensing if a future installer bundles FFmpeg.

No GoPro GPMF parser dependency has been added. Before selecting one, inspect its current maintenance, license, native build requirements, and fixture redistribution rights; update this notice at that time.

Google Maps support is an architectural placeholder only. No Google SDK, tiles, or API key is included. Provider terms must be reviewed before implementation, and keys must stay outside Git.
