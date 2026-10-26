# 无 JSON UI 场景验证（实验分支）

目标：验证 Minecraft Bedrock Windows 1.26.51.01 / LeviLamina 26.51.0 client 的 `SceneFactory::createUIScene` 是否能使用内存中的 UI 定义打开独立 HUD 编辑场景。此实验不验证完整无资源包菜单，也不进入正式发行。

`experiment/jsonless-hud-scene` 分支将编辑器场景名改为 `lholo.hud_editor_probe`。该名称不在 `resources/ui/_ui_defs.json` 或任何打包 JSON 中。`UIDefRepository::findDef` Hook 仅对 `lholo` / `hud_editor_probe` 返回与已实测场景一致的内存定义；其余查询调用原函数。绘制、输入与保存逻辑仍使用原有编辑器。编译和链接已通过，内存定义与原 JSON 内容一致；**尚未验证游戏内创建、输入或关闭行为**。

独立测试目录为 `build/hud-scene-probe/LHolo/`，只包含 DLL、原生清单、许可证和 OreUI 菜单所需的资源包 HTML/JS；不包含两个 HUD JSON 文件。它仍使用资源包提供菜单，验证范围仅为 HUD 编辑场景不经 JSON 注册。

请在游戏退出后使用单独测试安装，或先移开原 `mods/LHolo` 目录，再把实验目录中的 `LHolo` 复制到 `mods/`。不要直接覆盖旧目录，否则旧 JSON 文件会残留，使测试无效。进入世界后打开 LHolo → HUD 信息显示 → HUD 布局编辑，依次检查场景打开、游标、面板拖动、Esc/取消、完成保存以及返回 HUD 设置页。查看 `logs/latest.log` 中的 `[native-hud.probe]` 行。

判读：出现 `SceneFactory requested` 说明定义查询进入 Hook；出现 `Created unregistered scene` 说明 `createUIScene` 返回场景。只有游戏内完成打开、交互和退出，才能认定这条无 JSON 场景链路可用。若未进入 Hook 或场景创建失败，保留日志，不将编译成功当作运行成功。验证后切回 `main` 并使用正式包；实验 DLL 不发布。
