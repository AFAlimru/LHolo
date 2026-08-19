# Java → Bedrock 方块映射生成器

LHolo 运行时不依赖 Java。此工具在开发阶段直接调用 Chunker 的 Java 与
Bedrock 方块 resolver，枚举 Chunker `cli/data/java` 中每一个合法方块状态，
并生成目标为 Bedrock 1.26.20 的压缩 C++ 映射表。

当前生成来源：Chunker commit
`f642f8f849bef6adfba67012f49cd8e36abf125d`。

要求：JDK、Git，以及可正常执行 `gradlew.bat :cli:shadowJar` 的 Chunker
源码仓库。生成过程最多为 JVM 分配 4 GiB 内存。

在 PowerShell 中更新映射：

```powershell
.\tools\java_to_bedrock\generate.ps1 -ChunkerRoot E:\Downloads\chat\Chunker
```

生成结果只会写入：

```text
src/structure/java_to_bedrock/GeneratedChunkerMappings.inc
```

更新 Chunker 后重新运行脚本，然后检查生成器末尾列出的 unsupported 状态。
它们表示该 Java 状态无法由 Chunker 输出到固定目标 Bedrock 1.26.20，不应在
LHolo 内自行猜测替代方块。

维护约束：

- 不直接编辑 `GeneratedChunkerMappings.inc`，也不在 `StructureLoader` 中添加
  单独方块补丁；所有 Java→Bedrock 转换集中在本目录和运行时映射模块。
- 更新目标游戏版本时，同时更新生成器的 Bedrock resolver 版本、
  `block_states.json` 校验来源和开发文档中的版本说明。
- 生成完成后执行 Release 构建，并在游戏中手动验证正/负 Size、四向楼梯、门、
  活塞、观察者、半砖和含水方块。静态生成成功不能替代实机验证。
- Chunker 许可归属保存在源码根目录 `THIRD_PARTY_NOTICES.md`，该文件不进入
  正式模组包。
