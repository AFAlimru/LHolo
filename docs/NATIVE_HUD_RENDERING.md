# 原生 HUD

目标环境：Minecraft Bedrock Windows `1.26.51.01`、LeviLamina `26.51.0/client`。普通投影 HUD 与材料显示 HUD 使用原版渲染入口和游戏字体，菜单内的 HUD 设置继续由 OreUI 提供。2026-10-26 用户确认普通/材料 HUD、图标、间距及多语言留白测试通过；下方清单保留作为后续版本回归范围。

## 接口依据

使用目标版本 SDK 的原生接口声明：

- [ScreenView](https://github.com/LiteLDev/LeviLamina/blob/v26.51.0/src-client/mc/client/gui/screens/ScreenView.h)：`render(UIRenderContext&)` 与 `getScreenName()`。
- [UIRenderContext](https://github.com/LiteLDev/LeviLamina/blob/v26.51.0/src-client/mc/client/gui/controls/UIRenderContext.h)：文字测量、`drawText`、`flushText`、`fillRectangle` 和绘制范围查询。
- [ClientInstance](https://github.com/LiteLDev/LeviLamina/blob/v26.51.0/src-client/mc/client/game/ClientInstance.h)、[FontHandle](https://github.com/LiteLDev/LeviLamina/blob/v26.51.0/src-client/mc/client/gui/FontHandle.h)：获取当前游戏字体；`Font::getWrapHeight` 提供行高基础。
- [TextMeasureData](https://github.com/LiteLDev/LeviLamina/blob/v26.51.0/src-client/mc/client/gui/TextMeasureData.h) 与 `CaretMeasureData`：文本缩放、阴影、对齐及光标参数。

这些是目标版本生成的接口声明，版本更新时须重新核对 ABI 和实机行为。

## 分层

实现集中在 `src/ui/hud/`，命名空间为 `lholo::ui::hud`：

| 文件 | 职责 |
| --- | --- |
| HudContentSource.cpp | 读取会话、设置、投影进度、准心名称、辅助放置模式及材料一致快照；处理菜单和维度暂停可见性 |
| HudContent.h / .cpp | 纯展示模型与本地化文本整理、材料缺口排序；不调用游戏绘制接口 |
| HudLayout.h | 纯布局规则：四角、同角堆叠和可见行数限制 |
| HudText.h | 在 UTF-8 字符边界二分截断，降低长文字的原生测宽次数 |
| NativeHudRenderer.h / .cpp | Hook 启停、原生测宽、UTF-8 安全省略、矩形与文字绘制、收集本次可见图标位置 |
| NativeHudIcons.h / .cpp | tick 线程准备图标物品、按结构代际缓存、原生物品绘制与状态恢复 |

AppKernel 只编排启停。业务统计仍在 projection / MaterialTracker；渲染线程不查世界、不遍历结构、不扫描背包、不写配置。材料需求和可用数量继续由 `StructureUiState::materialHudSnapshot()` 在同一把锁内复制，避免向量错配。

`ui/MaterialFormat` 为菜单和 HUD 共用的材料名称/组数格式及图标身份规则，独立于 ImGui 和原生渲染 SDK。普通/材料 HUD 的旧 ImGui 绘制函数及叠放缓存已移除；隐藏的旧菜单和动作提示条继续沿用现有实现。正式实现不包含验证文字、计时或屏幕候选日志。

## 显示规则

普通 HUD 沿用文件名、X/Y 层或材料筛选范围、整图/可见范围进度、朝向错误、类型错误、多余方块、准心名称和辅助放置模式。零错误项不显示；各项开关继续生效，材料编号在展示层转为一基。

材料 HUD 独立于普通 HUD 开关。沿用当前可见范围的材料缺口，扣除对应库存，只列出仍缺少的材料；按缺口降序显示，最多 14 项及剩余类型数。数量按每种物品的实际堆叠上限格式化。统计未就绪和材料齐全分别显示原有本地化提示。

四角编码保持原配置：0 左上、1 左下、2 右上、3 右下。同角时普通 HUD 靠角，材料 HUD 向屏幕内侧堆叠；普通 HUD 行间及两 HUD 之间保留旧 ImGui 风格的间隔；缺失材料 HUD 的行间距单独采用原版式 2 个 GUI 单位，内边距、图标大小和图标到文字的距离不变。窗口不足时限制可见行并在末行显示省略标记；长文件名、材料名等按实际字体测宽，在 UTF-8 字符边界省略，不换行覆盖下一行。

有已加载结构且主客户端为 Playing、有本地玩家时才显示；LHolo 菜单覆盖或投影因维度切换暂停时隐藏。退出/重进世界和返回投影所在维度后重新读取现有快照，不缓存跨帧世界对象或布局。

## 旧 ImGui 布局风格与原生绘制边界

外观按用户提供的旧 ImGui HUD 图恢复：整块深灰半透明背景、轻微圆角、较宽的内边距和行间距。布局比例直接来自 `ui/FluentTheme.cpp` 与旧 HUD 的 WindowPadding/ItemSpacing：逻辑字体 18、水平内边距 12、垂直内边距 8、行间距 8、圆角 4×0.7；这些比例按当前游戏字体行高转换为原生 GUI 单位，不引入 ImGui 主题或缩放状态。背景 RGB 为 0.075，alpha 继续保留用户选择的 **0.6**。普通文字与缺失材料行均使用默认白色；错误计数及统计中/齐全提示保留状态颜色。

UIRenderContext 没有圆角矩形接口，圆角通过不重叠的水平矩形带近似，避免半透明叠加加深，也不新增纹理、页面或渲染后端。字体继续使用游戏字体（含阴影），不会与截图中的旧 ImGui 字形完全相同。字体缩放为 1，跟随游戏 GUI 坐标空间，不读取旧 ImGui uiScale。


Hook 先执行原版 `ScreenView::render`，只在 `hud_screen` 的渲染调用中追加绘制。字体来自本次调用的 FontHandle，文字追加后立即 `flushText(0, nullopt)`，不额外推进混淆文字时间。不创建页面/JSON 控件，不进入场景栈，不修改输入分发、GUI 缩放或裁剪状态；物品绘制期间通过配对的矩阵栈 push/pop 和 ScopedShaderColor 恢复共享矩阵与颜色状态；字体、ScreenView、渲染上下文均不跨调用保留。

日志前缀 `[native-hud]`：安装时记录启用状态；C++ 绘制异常记录一次并停止本次启用周期的追加绘制。首次提交等验证期日志已清理。

## 缺失材料图标

每条缺失材料行携带独立的可选 `iconId`；标题、统计中/齐全提示及剩余类型数不携带图标。`ui/MaterialFormat::materialIconId` 同时供 OreUI 材料清单与 HUD 使用：优先采用真实物品 `itemId`，没有时采用 `typeName`，保留完整命名空间；图标身份不改变库存计数、缺口或放置规则。HUD 不引入 OreUI 页面，不请求 `geo-item://`，不复制贴图包。

显示用 ItemStack 通过原版 `reinit(id, 1, 0)` 在主本地玩家的 tickWorld 回调中准备。渲染线程只提交可见图标 ID 请求并读取准备好的只读对象，不访问物品注册表；缓存按结构代际失效，额外 epoch 拒绝清理前尚未完成的结果。缓存最多保留约 64 个身份，超过时整体更新；HUD 无图标、无世界、暂停显示或卸载时清理，不持有跨帧 ClientInstance、LocalPlayer 或渲染上下文。

图标绘制依据目标 SDK 的 [ItemRenderer::renderGuiItemNew](https://github.com/LiteLDev/LeviLamina/blob/v26.51.0/src-client/mc/client/renderer/actor/ItemRenderer.h)、[BaseActorRenderContext](https://github.com/LiteLDev/LeviLamina/blob/v26.51.0/src-client/mc/client/renderer/BaseActorRenderContext.h) 与 [MinecraftUIRenderContext](https://github.com/LiteLDev/LeviLamina/blob/v26.51.0/src-client/mc/client/renderer/screen/MinecraftUIRenderContext.h)。HUD 的同步文字 flush 建立只在本次调用有效的请求范围；匹配的具体 MinecraftUIRenderContext::flushText Hook 先提交文字，再使用该上下文的 ScreenContext 绘制图标，避免将通用 UIRenderContext 强制向下转换。其他文字刷新只调用原版。

图标高度随现有字体行高，名称列增加图标宽度和间隔，仍参与面板测宽及长文本省略；不增加行高。窄窗口放不下图标时优先保留文字。首次等待 tick 准备、未知物品或没有原生库存图标的方块（包括可能无法解析的水/岩浆身份）显示灰色占位，不以水桶替换材料身份。图标 Hook 不可用或 C++ 绘制异常时保留文字 HUD。

日志前缀 `[native-hud.icons]`：只保留 Hook 不可用、图标准备/绘制异常及上下文回调缺失的有界诊断；首次提交和未知身份等验证期日志已清理。用户已确认当前图标显示通过，特殊材质及后续游戏版本仍需实机回归。

## 收尾审计

- 原 ImGui 普通/材料 HUD 函数、声明和跨帧叠放缓存已移除；隐藏旧菜单及动作提示按项目要求保留。
- 业务层没有增加对 UI/OreUI 的依赖；菜单和原生 HUD 通过独立 MaterialFormat 共享展示规则，不通过 MenuWidgets 间接依赖 ImGui。
- 渲染不增加世界/结构/背包扫描。材料展示只选择前 14 项（partial_sort，相同缺口以原索引稳定排序），不排序不显示的尾部。
- 超长文字在 UTF-8 边界二分查找，测宽次数从逐字删除降为对数级；一万字符的测试将原生测宽调用限制在 18 次以内。
- 图标准备在处理队列前检查已发布缓存，避免 tick 处理期间渲染重复入队造成二次构造；epoch 检查保留，用于拒绝清理前的异步完成结果。锁、匹配回调作用域及矩阵/颜色恢复均为具体线程和渲染状态要求，不添加未使用的通用防御框架。

## 验证

`xmake` 构建完整 `bin/LHolo`，不增加 HUD 资源包文件。`LHoloLogicTests` 中的 `NativeHudTests` 覆盖字段文本、X/Y/材料范围、进度区别、错误颜色、开关独立性、材料缺口与堆叠格式、14 项限制、图标/文本排序对应、显示身份回退、命名空间与无图标状态行、四角及同角堆叠、窗口不足和无效尺寸。

部署后由用户自行启动游戏，加载结构并检查：

- 缺失材料的方块、工具等图标与清单一致，缺口排序变化后对应正确；切层、关 HUD、重载和退世界后没有旧图标残留；水/岩浆及未知 ID 的占位不改变文字与数量。
- 普通 HUD 各信息、单项开关、四角和语言切换；材料 HUD 独立开关、当前层变化、背包数量变化、扫描中和齐全提示。
- 两 HUD 选择同角时无重叠；小窗口、F11、游戏 GUI 缩放和长名称显示。
- 快捷栏滚轮/数字键、移动、左右键，以及聊天、背包、暂停和 LHolo 菜单。
- 维度往返、关闭/重载结构、退回主界面后重进世界；暂停维度隐藏，返回后自动恢复。
