# Changelog

## [26.20.1] - 2026-08-15

### Fixed

- Fix water and lava projections not rendering.

## [26.20.0] - 2026-08-15

### Added

- Structure projection for `.mcstructure` and `.litematic` files using vanilla block models.
- Correction overlays for missing blocks (blue), wrong block types (red), and wrong states (directions, yellow), with configurable fill/outline opacity.
- Build progress HUD with placed/total counts and separate type/state error counters.
- Rotation, mirroring, XYZ offsets, and Y/X layer slicing with four display ranges.
- Textured water and lava projections drawn from the vanilla terrain atlas, purely client-side.
- Alt+M or `lholo` chat command opens the injected Dear ImGui menu; chat command stays local.
- Projection state persistence (last file, anchor, transform) and hotkey configuration.

### Fixed

- Structure loading on remote servers now resolves through the client-side multiplayer level instead of the integrated-server level.
- The OS cursor display counter is snapshotted while the menu is open and restored after closing, preventing an invisible cursor.
