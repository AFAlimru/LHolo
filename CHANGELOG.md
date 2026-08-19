# Changelog

## [26.20.5] - 2026-08-20

### Added

- 新增客户端“创建结构”页，支持两点选区、红色整体线框、实体开关和原版 `.mcstructure` 导出。
- HUD 新增总体进度，建造进度改为显示当前分层可见范围的进度。

### Changed

- 完善 `.litematic` 的 Java 至 Bedrock 方块映射和状态转换，映射生成工具仅保留在开发流程中。
- 调整设置导航顺序和“创建结构”页文案。

### Fixed

- 修复关闭投影后网格资源未完整释放、网格重建后未重新预检查，以及空结构反复尝试渲染的问题。
- 投影中隐藏活塞臂碰撞方块，避免错误渲染。
- 修复树叶投影缺少原版群系着色而显示为白色的问题。

## [26.20.4] - 2026-08-19

### Changed

- `.litematic` 的 Java 方块名称和状态改为使用由 Chunker 生成、集中维护的 Bedrock 1.26.20 完整映射表，并按源文件 `MinecraftDataVersion` 选择记录。
- 支持将 Java 含水状态拆分到基岩结构的第二液体层。

### Fixed

- 菜单打开及关闭输入过渡期间，在客户端阻止本地玩家开始或持续破坏方块；本地存档和远程服务器均有效，不影响其他玩家或正常移动。
- 修复退出世界后重新进入并加载或恢复 `.litematic` 投影时，复用已经失效的方块映射缓存导致客户端崩溃。
- 未进入服务器或单人存档时加载 `.litematic`，现在会提示尚未进入世界，不再导致客户端崩溃。
- 修复 Litematic 区域 `Size` 为负时错误倒序读取方块数组，导致整体结构被镜像或旋转、方向方块与 Java 源文件不一致的问题。

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
