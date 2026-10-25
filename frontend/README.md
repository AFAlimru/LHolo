# LHolo OreUI 前端

使用当前自有菜单样式。聊天命令打开新菜单，快捷键仍打开旧菜单。
浏览、加载、关闭投影与恢复上次投影、材料清单弹窗、创建结构选区与导出、结构变换、渲染设置、HUD、快捷键、语言与实验性功能已接入。界面缩放按 OreUI 方案保留本地占位，不修改旧 ImGui 缩放。

## 浏览器实时预览

在项目根目录运行：

```powershell
npm.cmd --prefix frontend ci
npm.cmd --prefix frontend run preview
```

打开 http://127.0.0.1:4173/。修改 src/ 内的页面、控件或 CSS 后自动构建并刷新，无需启动游戏或编译 DLL。开发服务器只监听本机 127.0.0.1，终端 Ctrl+C 停止；端口占用时可先设置 `$env:LHOLO_PREVIEW_PORT = '4174'`。

顶部工具栏可切换“两点选区 / 恢复记录 / 未进入世界 / 重置”，并调整字号。调整浏览器窗口或使用开发者工具的设备尺寸功能可检查不同宽度。浏览选择只显示浏览器提供的文件名；模拟加载、恢复和导出不会读取世界、解析结构或生成结构文件。

preview/ 仅提供开发环境适配和模拟 engine，正式页面、应用 hooks、控件与菜单 CSS 均复用 src/。浏览器预览使用 Mojang @react-facet/dom-fiber 渲染器创建自己的开发 root，不依赖游戏字体或原版游戏脚本。正式游戏构建仍复用游戏运行时，构建检查禁止 preview/ 和额外 React/Facet 运行时混入。

浏览器适合检查排版、文案、输入和禁用状态；Gameface 的布局细节、游戏内焦点/手柄、原生文件选择和 C++ 生命周期仍需实机验证。

```powershell
npm.cmd --prefix frontend run preview:build  # 预览类型检查与独立构建
npm.cmd --prefix frontend test               # 正式游戏入口的隔离测试
```

## 开发

```powershell
npm.cmd --prefix frontend ci
npm.cmd --prefix frontend test
```

src/App.tsx 只装配页面、useStructureFile、useProjection、useMaterialsDraft、useCapture、useTransform、useRenderSettings、useSettings、useHudSettings、useHotkeySettings、useInterfaceSettings 和 useExperimentalSettings。
application/useStructureFile 管理文件草稿快照与请求状态，由装配入口注入桥接接口。
LHoloMenu 负责布局与导航，ProjectionPage 仅接收文件/投影状态及动作。
application/useProjection 管理原生投影快照和待应答状态，不推测业务结果。
application/useCapture 管理坐标文本草稿、整数校验、revision 同步与请求 id 应答；不读写世界。创建页使用 LHolo 自绘输入框/勾选框，有效坐标与实体开关自动同步选区，不增加应用按钮；导出使用现代另存为对话框。
platform/bridge 封装 engine、验证 JSON，并返回订阅清理函数。
components/LHoloControls 与 components/LHoloSelect 集中绘制自有按钮、输入、勾选框和下拉框，样式以旧 ImGui 为基准；platform/native 仅封装无外观交互原语和滚动，版本私有符号只存在于 tools/oreui 的适配器。

文件状态订阅覆盖整个菜单生命周期，切换页签不丢失路径回传。
取消、错误时保留原路径；等待选择时禁用按钮。选择文件不会自动加载投影。
不通过前端路径参数启动对话框，C++ 用自身草稿作为再次浏览的初始路径。
正式游戏入口不重复打包 React/Facet，不额外 createRoot；独立浏览器预览的开发 root 仅在 preview/ 中创建。

构建只验证类型、打包和隔离行为；用户自行在游戏内验收。
协议、资源版本和 DLL 一并部署步骤见 [迁移说明](../docs/OREUI_MIGRATION.md)。

游戏前端按 ES2019 构建，不依赖 BigInt。尺寸使用精确整数跨度，体积用十进制乘法，隔离测试显式移除 BigInt 并复现依次记录两点。

创建页坐标使用紧凑横排，XYZ 与取玩家位置按钮同行。输入使用自有 HTML input（显式 DOM input 事件），不使用原版 Form.TextField、BaseTextField 或 Switch 可视组件；字体和尺寸使用 LHolo 旧 ImGui 风格。

迁移遵循旧 ImGui 的按钮与操作集合，不自行增加“应用”“确认”等步骤。创建页只有原有取位置、清除与导出等操作；输入自动同步，较早应答不覆盖新编辑。预览及游戏内均隐藏滚动条但保留滚动功能。

## 结构变换

application/useTransform 以原生快照为准，前端只保留未完成文本和待应答编辑。旋转、镜像、XYZ 偏移即时同步，连续编辑合并，旧应答不会覆盖新输入。快捷键和旧菜单更新回显；冲突或投影会话变化时回到原生状态。重开菜单读取真实值，不归零；不新增应用或重置按钮。

## 渲染设置

application/useRenderSettings 订阅原生参数与三个分层轴的真实最大索引。整体边框、轴/范围/当前层、填充/描边百分比、两个穿透开关即时同步；恢复默认纠错样式以一个字段批次提交 15%/100%。材料编号只在页面做一基转换，原生保持零基。

只发送有改动的字段及对应原值，连续编辑合并，旧应答不覆盖新输入；加载 revision 或外部分层轴变化会丢弃失效的层输入，冲突回显原生值。重开读取原生状态，不用前端默认值覆盖配置。切换层/材料输入语义时重新建立输入控件。未收到原生快照前禁用控件；不增加应用按钮。

## HUD、快捷键、界面与实验性设置

App 只装配 useSettings 与四个具体页面 hook。useSettings 串行提交字段编辑/命令、合并连续编辑并处理应答，页面 hook 只持有输入文本、弹窗和缩放占位。engine 与 JSON 协议集中在 platform/settingsBridge；页面不访问 engine，不维护独立的配置副本。

- HUD 原生快照驱动开关、位置、八项信息和材料 HUD。HUD 总开关关闭时禁用其条目，材料 HUD 独立控制。单字段更新保留其它设置，尤其不覆盖旧 ImGui 的 uiScale。
- 快捷键槽位、文案键、当前按键名称由原生下发；绑定动作启动原生捕获，不在正式前端注册 keydown/mousedown。复用旧输入规则：纯修饰键不绑定、F11 留给游戏、Esc 取消、Backspace/Delete 清除、中键和侧键可绑定、成功捕获清除重复绑定。恢复默认与全部恢复调用原生默认值，全部恢复还恢复 Alt+滚轮开关。离开页签或关闭菜单取消捕获，吞掉所认领按键的重复按下和释放，不让它继续触发页面。
- 语言列表和名称从原生动态注册表取得，以稳定 locale code 提交，原生语言变化主动刷新菜单字典，无需重开。缩放仅保留本地假控件和占位说明，协议没有 uiScale 字段，不调整 OreUI 或 ImGui 缩放。
- 实验性知情说明沿用旧 UI 六段文字和按钮，取消不发送启用请求；确认由原生记录授权并开启指定模式，前端不先行授予授权。三个模式由原生互斥切换，未授权的模式启用会被后端拒绝。授权/半径/冷却持久化，模式是世界会话状态，单纯切模式不请求保存；退出世界关闭模式。模式勾选按原生应答回显。

请求 id、会话 context、字段原值和快捷键 revision 防止重复、旧应答和外部变更覆盖。未收到快照前禁用真实设置操作；页面切换保留输入，重新打开从原生恢复。preview/settings.ts 的模拟状态/默认值/DOM 捕获仅用于浏览器，不得进入正式构建。前端测试验证跨页、连续编辑、冲突、动态语言和缩放无后端写入；原生与实机要求见迁移说明。

## 控件尺寸与设置页排版

普通按钮、下拉触发器、选项及文本输入统一为 `2.2rem` 高、`1rem` 正文字号，加减按钮为同高方形。导航保留独立尺寸。设置页行距和分组间距由 `src/styles/menu.css` 统一维护，结构变换与渲染设置的下拉框使用一致最小宽度，不在业务 hooks 或页面事件中计算样式。保留原有按钮和即时同步行为；浏览器预览已检查 18/26 字号下渲染页的尺寸与横向边界，Gameface 效果仍由用户实测。

## 导航视觉与动效（预览开发）

导航使用单个绝对定位的选中底板，在页签间以 240ms 位移动画切换；各行 `2.8rem` 高、下间距 `0.3rem`，与 MenuNavigation 的 `3.1rem` 位移步长配套维护。图标在 MenuNavigation 内定义，LHoloButton 仅提供可选 icon 槽，不引入图标依赖或业务状态。配色按用户反馈恢复中性黑灰，选中态使用灰色底板和白色文字，勾选状态沿用原蓝色，悬停/按下与键盘焦点分别反馈；右侧内容只在页签切换时淡入。`prefers-reduced-motion` 关闭动效。

本轮改动集中在共享前端和浏览器预览；2026-10-25 已重新生成游戏页面、编译 DLL 并覆盖部署到本机 1.26.51.01 测试环境。Gameface 对新增 transition/keyframes 的表现仍需实机验收。原有页签、按钮和业务协议保持不变。

功能区使用共享 `.lholo-setting-row`：说明在左，控件右边缘对齐；下拉框保留统一宽度，数字框使用短宽度，步进按钮组合靠右，透明度说明不再放在输入框右侧。内容最大宽度为 `42rem`，页面标题、分组标题和辅助说明有独立字号/间距。窄窗口缩紧导航并允许设置说明、坐标组和快捷键操作自然换行；18/26 字号的渲染页与创建页已检查无横向溢出。本轮预览调整现已编译部署到本机测试环境，游戏内比例与动效待用户验收。

## 材料清单弹窗外观

沿用主菜单中性黑灰表面、标题字号与共享关闭按钮。汇总使用辅助文字，三列维持物品/标识符/数量；标识符降低字号与亮度，数量固定窄列并右对齐，表头和行分隔线减弱。弹窗宽度 `38rem`、最大 `86vw`，表格保留独立滚动与隐藏滚动条，紧凑行高可在默认预览字号显示全部 7 行示例。仅调整外观和层级，不增加筛选、导出等操作，不接材料后端。

下拉框尺寸由共享 LHoloSelect 的 `size` 明确区分：旋转/镜像使用 compact（6rem），长选项使用 regular（11rem）；不按当前选中文字长度改变宽度，避免切换时跳动。

## 原版物品图标测试通路

材料行通过 `LHoloItemIcon → platform/native.NativeItemIcon → tools/oreui/runtime_adapter.js` 复用原版物品缩略图组件。版本私有组件 `u_`、尺寸换算 `uc` 和 `geo-item` 协议只存在于目标游戏适配器；页面不拼 URL，不访问原生物品注册表。构建脚本继续校验原版 bundle SHA256，并校验图标组件签名。

`MaterialItemDraft.itemId` 是可选的图标物品身份，与展示的方块 `typeName` 分离；不自动把方块 ID 当成物品 ID，也不将水伪装成水桶。打开材料清单时通过 `lholo.materials.request` 请求原生 `MaterialTracker` / `StructureUiState` 统计，经 `lholo:materials` 下发真实需求快照（包含 `displayName`、可选 `nameKey`、`typeName`、`itemId`、`count`）；正式构建不包含预览示例数据。

浏览器 preview/runtime 只渲染中性 SVG 占位，不请求游戏协议或复制游戏贴图。隔离测试验证 6 个物品请求与水的占位；实际缩略图、材质包覆盖、未知 ID、退出世界和反复开关需游戏内验收。
