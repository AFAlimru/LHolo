# LHolo 开发约束

LHolo 是当前开发项目。同级 OreUIProbe 仅供参考，ore-ui 提供官方基础设施源码参考。
修改前阅读 DEVELOPMENT.md 的相关章节；OreUI 工作另读 docs/OREUI_MIGRATION.md。

- 保持高内聚、低耦合、职责清楚；按实际功能增加模块，不预建通用框架或空实现。
- OreUI 按功能逐步迁移。未迁移的 ImGui 菜单、HUD、快捷键与输入行为继续有效。
- 前端在 frontend/；原生页面生命周期、通信和业务绑定集中在 src/oreui/；
  游戏版本私有符号及生成工具集中在 tools/oreui/。
- 页面不访问 engine 或原版私有符号。业务状态使用现有 C++ 模块，前端只保留界面状态。
- 原版 React、Facet 和渲染器复用同一页面的运行时，不重复打包，不额外 createRoot。
- 新界面以 LHolo 旧 ImGui 的自有深色样式为准，可视控件集中在 frontend/src/components/LHoloControls.tsx；原版只提供无外观的焦点/激活原语与运行环境，不套用原版表单、输入框或开关外观。
- structure/projection/place/settings/i18n 不得依赖 OreUI 或其协议。
- 不复制 Probe 演示数据、测试协议或游戏脚本生成件作为正式实现。
- 每次架构或迁移状态变化同步更新文档。类型检查和隔离测试不等于游戏内验证。
- 不为此次 UI 迁移顺带修改投影算法、配置格式或模组版本号。

- 当前保留自有菜单样式；命令打开 OreUI，快捷键打开旧 UI。游戏启动和实机测试由用户自行进行，不自动操作或结束游戏。
- 浏览保持页面 → 应用状态 → 平台桥接 → 原生桥 → MenuBindings → 独立文件选择服务分层；选择路径不自动加载或写配置。
- 投影动作通过 structure/ProjectionActions 进入现有应用控制帧；前端以原生投影快照为准，不复制加载逻辑。

- 创建结构使用独立 CaptureBindings / CaptureProtocol；capture/CaptureActions 在应用控制帧执行，保存对话框异步，revision 防止旧结果跨上下文导出。

- 浏览器开发预览在 frontend/preview/，复用 src/ 页面和样式；只在预览中提供模拟 engine 和独立 dom-fiber root，正式游戏构建不得引入预览代码或模拟数据。

- UI 迁移逐项对照旧 ImGui 的控件和操作，不擅自增加按钮、确认或应用步骤；创建选区使用自动同步。菜单隐藏滚动条但保留滚动。

- 渲染设置经 app/RenderSettingsActions 协调 projection 参数与 StructureSession 分层，AppKernel 控制帧执行；低层不反向依赖 app/OreUI。分层范围来自原生快照，材料编号仅在 UI 做一基转换，字段批次须检查原值/context/原轴。

- HUD/快捷键/语言/实验性设置通过 app/SettingsActions 的字段/命令通路，不复制完整配置；正式前端不得硬编码原生语言表、快捷键默认值或自行捕获按键。input/HotkeyCapture 共享旧规则，模式是会话态。OreUI 缩放暂为本地占位，协议不接受 uiScale。
