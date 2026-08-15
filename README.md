# LHolo

适用于 Minecraft 基岩版 Windows 客户端的投影模组

## 功能

- 加载 `.mcstructure` / `.litematic`，使用原版方块模型投影
- 纠错提示：未放置（蓝）、方块类型错误（红）、方向/状态错误（黄），透明度可调
- 建造进度 HUD：已放置/总数、类型错误数、状态错误数，支持四角定位与单项开关
- 旋转（0°/90°/180°/270°）、镜像（X/Z/X+Z）、X/Y/Z 偏移、Y 轴分层与 X 轴切片
- `Alt + M` 或聊天栏输入 `lholo` 打开菜单（聊天命令在客户端拦截，不会发送到服务器）
- 投影文件、锚点与变换参数持久化，支持一键恢复上次投影

## 许可证

本项目以 [GPL-3.0](./LICENSE) 许可发布。你可以自由使用、修改与再分发，但修改或衍生版本必须同样以 GPL-3.0 开源。

## 安装

```bash
lip install github.com/MarmieQi/LHolo
```

或从 [Releases](https://github.com/MarmieQi/LHolo/releases) 下载 `LHolo-client-windows-x64.zip`，将压缩包内 `LHolo/` 目录解压到客户端 `mods/` 下。

## 构建

需要 Visual Studio 2022 C++ 工具链与 [xmake](https://xmake.io)。

```bash
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -y
```

产物位于 `bin/LHolo/`（`LHolo.dll` + `manifest.json`）。
