# OreUI 发布与更新方案

状态：2026-10-26，内置资源包与 LHolo 限定 URL 读取适配已由用户实机验证。OreUI 页面与运行时为必需资源，正式包随 DLL 一起分发；当前适配目标为 Minecraft 1.26.51.01 / LeviLamina 26.51.0 client。

当前统一构建流程为默认 `xmake`：生成前端、编译原生模块、组装并校验 `bin/LHolo/`，不生成 ZIP。可直接复制完整目录到目标 `mods/`；可选 `tools/oreui/deploy.ps1` 也只从 bin 读取，不重新构建或解压。旧独立打包和 ZIP 测试分支已移除。

资源包挂载、限定 URL 读取与菜单启动已由用户在目标版本实测；跨版本升级、缓存刷新及完整安装升级矩阵仍需验收。

## 一键构建完整目录（本机开发）

首次配置本机的目标游戏资源目录；该路径只保存在被忽略的 xmake 本地配置中，不写入源码：

```powershell
xmake f -a x64 -m release -p windows --target_type=client --oreui_hbui="D:/games/LeviLauncher/MC/versions/1.26.51.01/data/gui/dist/hbui" -y
```

之后只需：

```powershell
xmake
```

也可设置环境变量 `LHOLO_OREUI_HBUI` 覆盖本地 `oreui_hbui` 配置。需要现有 C++ 工具链、Node/npm 和 Python；首次缺少前端依赖时自动运行锁定版本的 `npm ci`。

构建钩子先验证并生成目标版本前端、检查 JS 语法，再完成原生 LHolo 编译，最后组装完整 `bin/LHolo/`。该目录可整体复制到目标游戏 `mods/`；不生成 ZIP，不自动部署、不启动或停止游戏。默认 `xmake` 即生成完整目录；只有显式配置 `--oreui_native_only=y` 的 CI 构建才跳过前端，不能作为完整安装包。

`lholo.oreui.package` 构建规则依赖原生 modpacker，`tools/oreui/package.lua` 提供前置资源生成和后置目录组装钩子；`build_menu.py` 管理版本适配与前端生成；`assemble_package.py` 管理安装文件清单、版本/UUID/协议标记校验和目录哈希。校验报告在 `build/oreui/package-validation.json`，不进入模组目录。该流水线始终重新生成页面，不使用旧的 build 页面作为成功回退。UUID 固定，版本来自原生 manifest 并与 tooth 校验；不会删除模组目录或覆盖配置。

这是完整安装目录，也作为正式发行的唯一输入。构建机使用经过 SHA256 审核的目标游戏 bundle 生成必需页面，安装者无需安装 Node、Python 或再次生成资源。普通 CI 的 native-only 产物仅供原生编译检查，不作为正式安装包。

## 已确认的加载约定

LeviLamina 的 Mod 将资源目录定义为 `mods/<模组名>/resource_packs`。客户端初始化时将各模组资源目录注册为资源包来源，并将这些包加入客户端资源栈。因此 LHolo 使用随模组分发的资源包，不要求用户另外导入 `.mcpack` 或修改全局资源包配置。

依据：[Mod.cpp](https://github.com/LiteLDev/LeviLamina/blob/main/src/ll/api/mod/Mod.cpp)、[客户端初始化与资源栈挂载](https://github.com/LiteLDev/LeviLamina/blob/main/src-client/ll/core/main_win.cpp)。本机 LeviLamina 源码也已对照。

完整本机安装目录（正式发行沿用此结构）：

```text
LHolo/
├─ LHolo.dll
├─ manifest.json                 # LeviLamina 原生模组清单
├─ LICENSE
└─ resource_packs/
   └─ LHoloUI/
      ├─ manifest.json           # Mojang 资源包清单
      └─ hbui/
         ├─ lholo-menu.html
         └─ lholo-menu.js
```

两个 manifest 的格式和作用不同，不得共用。`tooth.json` 仍描述发布资产、安装位置和 LeviLamina 依赖，`bin/LHolo/` 安装到 `mods/LHolo/`。

## Mojang 清单与版本规则

采用稳定版资源包清单 `format_version: 2`、模块 `type: resources`，不为菜单打包引入预览版 manifest 3。填写名称、描述、header UUID、module UUID、三段整数版本和经目标游戏版本核对的 `min_engine_version`。

- header UUID 和 module UUID 互不相同，首次确定后提交源码，升级时保持不变；不得每次构建随机生成。
- 发布新版时提高包版本，header 与资源模块版本保持一致。官方说明同 UUID 包导入时，高版本替换旧版本；这不等同于证明内置包更新和 Gameface 缓存已正确刷新。
- `min_engine_version` 表示最低引擎与兼容处理依据，不是可运行于全部未来版本的承诺。私有 OreUI 运行时适配仍须锁定并验证目标游戏版本。
- 不仅凭 `experimental_custom_ui` 的名称就认为它能接通当前菜单；是否需要该能力声明应由实际加载结果确认。

依据：[Mojang 资源包 manifest 规范](https://learn.microsoft.com/en-us/minecraft/creator/reference/content/addonsreference/packmanifest?view=minecraft-bedrock-stable)。

## 分层与构建职责

- `frontend/`：LHolo 页面、控件与应用界面状态，独立浏览器预览继续使用开发入口。
- `src/oreui/`：页面生命周期、通信、业务绑定及 UI 资源兼容检查；不承担下载、安装或升级职责。
- `tools/oreui/`：目标游戏版本的运行时适配、资源生成、清单校验和打包。生成内容进入构建目录，最终汇入 `bin/LHolo/`。
- xmake 与发布 CI：先完成受控的前端/运行时资源构建，再统一收集 DLL、清单及资源包。保持一个正式发布目录，不另建一套不同结构的发行物。
- lip/启动器：安装和更新完整模组包。运行中的 DLL 不自更新，也不在启动时修改游戏安装目录来补资源。

普通 CI 只做原生构建；正式发布在具备目标游戏资源的构建机执行 `xmake`，校验完整目录并仅上传 ZIP 到草稿后发布。发行 CI 下载已发布附件，复查六项文件清单、整包哈希、原生/资源包版本、固定 UUID 与 UI 协议；不会用 native-only 包覆盖完整发行附件，也不重写已确认的更新日志。

## 升级、保留与回滚

1. DLL、页面和桥接协议作为一个发行单元构建、安装及回滚。资源包 UUID 不变，正式发行版本递增；发布前校验原生 manifest、tooth、资源包清单与发行标签一致，避免手工维护的版本漂移。发行版本由 xmake 与 tooth 同步维护，资源包版本从原生 manifest 派生。
2. 正式发行计划对 LHolo 自有 JS/CSS 采用内容散列文件名作为缓存隔离方案；当前测试资源仍使用固定文件名，适配响应设置 `Cache-Control: no-store`。仍须实测新版替换后的 Gameface 缓存行为，不能仅靠资源包版本号宣称已解决。
3. 配置和用户数据不进入发行物，也不参与资源目录清理。已有 `tooth.json` 保留 `mods/LHolo/config/config.json`；若增加持久化文件，应按真实路径补充保留策略。lip 的 `preserve_files` 是卸载阶段保留规则，并不是通用的配置备份或合并机制。
4. 新版移除的自有资源必须有明确的旧文件清理策略。lip 管理安装和手动 ZIP 覆盖应分别验证；手动覆盖不能假定会删除旧文件。清理限于可确认归属 LHolo 的发行文件，不能清空整个模组目录。
5. 更新需要退出游戏后完成，不承诺热更新。回滚恢复匹配的 DLL 和完整 UI 资源；配置如有未来格式迁移，须另行设计备份与兼容，本轮不改变配置格式。
6. 从现有开发部署迁移时，仅处理已确认属于 LHolo 的游戏目录测试文件；不得删除或改写原版 `gameplay` bundle、其他模组资源或用户资源包设置。

依据：[lip 包清单与文件保留规则](https://lip.levimc.org/concepts/package_manifest.html)、[LeviLamina 发布教程](https://docs.levimc.org/LeviLamina/developer_guides/tutorials/create_your_first_mod/)。

## 当前技术边界与下一步

实测定位：资源栈读取 `hbui/lholo-menu.html` 返回 true、协议标记匹配，Hybrid 处理器对 `ui://./hbui/lholo-menu.html` 却返回 Failure。对照本机 1.26.51 客户端二进制：Hybrid `onResourceRequest`（RVA `0x42f21d0`）调用查找函数（`0x42f27e0`），其路径辅助函数（`0x42ef270`）遍历包前检查 `PackAccessStrategy::isTrusted()`（虚表偏移 `0x28`），未受信任的包被跳过。SDK `PackAccessStrategy.h` 的虚表顺序已对照。以上地址仅为本机分析记录，运行代码不硬编码这些地址；普通资源管理器能读取不等于 Hybrid HTML 入口允许读取。

适配集中在 `src/oreui/OreUiMenuResources.*`：只匹配 `ui://./hbui/lholo-menu.html` 和 `.js` 两个完整 URL，并且只在 LHoloUI 包清单存在时接管。经原生 `ResourcePackManager::load(UserPackage)` 读取包内固定路径，校验 v7 标记；通过原版 `IAsyncResourceResponse::GetSpace` 复制数据、设置内容类型，返回处理状态，由 broker 完成响应。缺失/不兼容返回 Failure，不回退到游戏目录掩盖问题；其他 URL 保持原处理，不修改全局信任策略或原版文件。接口沿用当前游戏版本私有 ABI，需要版本适配与实机验证，不属于 Mojang 稳定脚本 API。

资源包挂载已实测确认；原版 Hybrid 入口不能直接读取本次模组包，需要上述限定 URL 适配。加入适配后，用户已实测确认页面成功启动。

当前 `OreUiScreen.cpp` 检查模组包内页面协议标记。`build_menu.py` 以指定游戏版本的原版 bundle 为输入，通过严格 SHA256 和替换锚点校验后生成菜单资源。必需的 HTML/JS 与资源包清单整体进入安装包；生成文件不提交源码仓库，随正式版本作为二进制附件分发。此方案仍与目标版本的私有运行时绑定，不宣称自动兼容其他游戏版本。

## 正式发布流程

日常构建仍只有 `xmake`，不会自动压缩或上传。发布阶段使用单独的归档工具：

```powershell
xmake
python tools/oreui/release_package.py --output build/release/LHolo-client-windows-x64.zip --tag v26.51.6
```

该工具只收集组装报告中的六项必需文件，再校验包内版本、UUID 与协议；同名 `.zip.sha256` 仅用于构建机本地校验，不作为发行附件上传。ZIP 顶层为 `LHolo/`，与 tooth 的安装规则一致；不包含配置、PDB、构建报告、游戏安装路径或其他本机文件。

提交并推送发布源码后，将草稿的 target 更新到该提交；只上传上述 ZIP，最后发布草稿。CI 从 GitHub 附件元数据读取 SHA256，在临时目录生成本地校验输入，不依赖或发布额外校验附件。`.github/workflows/release.yml` 负责下载和校验发行附件，支持 workflow_dispatch 重新验证已有版本。构建和安装资源版本必须匹配，不允许将旧页面与新 DLL 混装。

仍须持续进行干净安装、同 UUID 升级、缓存刷新、配置保留与不兼容版本提示的实机回归。

用户补充的完整日志及 14:48 第二次日志均确认资源栈可读、Hybrid 请求失败；已据此增加限定 URL 的资源读取适配，用户随后确认菜单启动成功。

## 实测后的诊断清理

资源包读取成功后已移除 `inspectMenuResourcePack` 探测、资源读取成功/路由过程日志、`diagnostics.js` 注入和浮层、`__lholoOreUiReport` 全局函数及 `MenuContentLog` 转发钩子。前端异常使用标准 `console.error`，不再经自定义诊断通道转发。`OreUiMenuResources` 的固定 URL 读取适配、版本标记检查及正常失败日志继续保留；`lholo.menu.ready` 是初始化标签/业务快照的正常协议，不能随诊断删除。
