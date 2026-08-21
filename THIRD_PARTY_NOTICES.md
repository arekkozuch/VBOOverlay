# Third-Party Notices

This file is an engineering inventory, not final legal advice. Distribution requires a complete
license review, particularly if future builds bundle FFmpeg/HEVC components or map data.

| Project/package | Purpose | License | Upstream |
| --- | --- | --- | --- |
| Qt 6 | Native UI, multimedia, application, and test framework | LGPL-3.0/GPL/commercial, depending on distribution terms | https://www.qt.io/licensing |

The current source tree does not include Electron, React, Node packages, MapLibre, FFmpeg, or an HEVC
encoder. Export discovers a user-installed FFmpeg at runtime and may use its `libx265` encoder when a
working hardware HEVC encoder is unavailable. The x265 project publishes GPL-2.0-or-later licensing;
this is not a legal conclusion about a particular FFmpeg build or redistribution. Before distributing
or bundling FFmpeg/x265, review the exact binary configuration, license obligations, attribution,
patent considerations, and redistribution terms and update this inventory.

No user GoPro, VBO, RCZ, project, map credential, or API key is redistributed by this repository.
