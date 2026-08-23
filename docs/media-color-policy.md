# Media and color policy

Source raster, bit depth, and color characteristics are data, not presets. FlappedEar does not impose a product-level 4K ceiling. A native export is accepted only when the active QRhi renderer, selected HEVC encoder/profile, and resource preflight support the exact request.

`MediaProbe` retains coded and display raster, exact and average rates, codec/profile, pixel format, optional bit depth and video bitrate, sample aspect ratio, optional rotation, and the raw FFmpeg color range, matrix, transfer, and primaries. Missing probe fields remain unset. Bit depth uses `bits_per_raw_sample` when valid and a conservative allowlist of known pixel formats otherwise; unknown formats do not become 8-bit by default.

The raw color fields are authoritative. A small policy classification distinguishes SDR, HLG HDR, PQ HDR, Log/extended, and unknown sources. 10-bit is not synonymous with HDR, and BT.2020 alone is not treated as a complete HDR policy.

The validated default export policy preserves 8-bit SDR as HEVC Main and 10-bit SDR as HEVC Main10. Stage A remains lossless FFV1/BGRA; Stage B explicitly composites a 10-bit source in a 10-bit filter format and validates the final pixel format, bit depth, codec profile, and known color tags. The QML overlay remains authored in its existing SDR/sRGB-like space.

HLG, PQ, and Log/extended material requires a validated color-managed compositor. It is detected and rejected during export preflight today. Unsupported HDR/Log material must never be silently converted to 8-bit SDR, silently tone-mapped, or described as supported merely because metadata can be copied.

Rotation and sample aspect ratio are retained and shown through the media model, but complete display-transform preservation remains pending. Native-resolution architecture is deterministic-tested through 8K representation; production 8K support remains a runtime renderer/encoder decision and still needs real hardware/media validation.

Recommended bitrate is a continuous pixel-rate curve anchored at 12.5 Mbps for 1080p30: `12.5 Mbps × (pixelRate / 1080p30)^0.66`, with a 1.15 multiplier for depths above 8-bit and quality multipliers of 0.70/1.00/1.30. Custom bitrate is defensively bounded to 0.5–500 Mbps. This supports future rasters without an arbitrary 4K plateau while retaining the established 1080p and 4K ballpark.
