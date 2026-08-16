# Changelog

## [26.20.3] - 2026-08-16

### Added

- 范围放置：自动放置玩家周围半径内的投影缺块

## [26.20.2] - 2026-08-16

### Added

- 轻松放置：准心对准投影的缺块位置，自动放置对应方块
- 箱子、告示牌等方块在投影中显示为贴图占位外壳
- HUD 可显示准心指向的方块实体名称

### Changed

- 菜单界面调整（轻松放置开关、投影样式入口位置）
- 配置版本升级到 5，新增两项设置

### Fixed

- 修复打开/关闭菜单后光标消失的问题

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
