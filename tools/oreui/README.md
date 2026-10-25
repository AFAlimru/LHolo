# LHolo OreUI 工具

- build_menu.py：校验游戏脚本 SHA256，构建并生成 LHolo 页面复制件。
- runtime_adapter.js：同一份游戏 React/Facet、输入、滚动和音效的版本适配。
- package_menu.ps1：把当前 DLL 和页面组合成本地测试包。
- deploy_menu.ps1：退出游戏后一次更新 DLL 和页面，逐文件校验 SHA256，不操作游戏进程。
- resource_pack.json：内置资源包的固定身份；版本由原生构建清单注入，UUID 不随构建改变。
- package_resource_pack.py：将已生成页面与 DLL 组合成独立的本机资源包测试 ZIP，校验版本、UUID、协议、ZIP 和哈希；不修改正式发布目录。
- deploy_resource_pack.ps1：部署该测试包，直接覆盖并清理旧游戏目录的 LHolo v7 页面，避免测试假成功；保留配置和原版页面。
- restore_ui.ps1：仅用于旧风格试验的 v1 回滚点，不与当前 v4 DLL 混用。

当前资源标记为 lholo-menu-v7-settings。升级必须一起更新 DLL、HTML、JS。
不覆盖原版 gameplay/index，不把生成的游戏脚本提交或公开分发。
详细步骤见 ../../docs/OREUI_MIGRATION.md。

内置资源包实验（仍依赖本机 bundle，不公开分发）：先运行 `build_menu.py <hbui目录>` 和 `xmake -y`，再运行 `python tools/oreui/package_resource_pack.py`。退出游戏后执行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/oreui/deploy_resource_pack.ps1`，由用户启动并在聊天发送 `LHolo`。包内路径为 `hbui/`；2026-10-25 用户已在目标版本实测菜单打开成功。

原版 Hybrid HTML 入口会跳过未受信任包，当前通过 `src/oreui/OreUiMenuResources.*` 限定 URL 读取 LHolo 自有页面资源；未修改全局信任规则。目录挂载、页面读取和菜单启动已由用户实测通过；正式运行时资源生成和发行 CI 仍未完成。

实测成功后已移除资源探测、加载过程日志、HTML 诊断浮层、专用 JS report 全局函数和 ContentLog 转发钩子。保留版本标记校验、正常错误日志和 `lholo.menu.ready` 初始化通信。
