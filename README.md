# LHolo

适用于 Minecraft 基岩版 Windows 客户端的投影模组

`Alt + M` 或聊天栏输入 `lholo` 打开投影菜单

## Litematic 方块映射

Java Edition 方块名称与状态通过 Chunker 生成的完整映射表转换到 Bedrock
1.26.20。运行时代码、生成数据和更新工具统一放在：

```text
src/structure/java_to_bedrock/
tools/java_to_bedrock/
```

映射更新方法见 [tools/java_to_bedrock/README.md](tools/java_to_bedrock/README.md)，
第三方许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。该许可说明只保留在源码仓库，
不进入正式模组包；LHolo 运行时不依赖 Java 或 Chunker。
