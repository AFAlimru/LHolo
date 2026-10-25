# 共用展示组件

跨页面实际复用时再提取组件；单页专属组件与页面放在一起。
这里只处理展示和交互绑定，通过命名后的 platform 控件访问原版 UI，
不访问 engine、C++ 或原版私有符号。

LHoloControls.tsx 是可视控件入口：LHoloButton / LHoloTextField / LHoloCheckbox。外观以旧 ImGui 为准，自行定义 DOM 子树与 CSS，平台只提供 NativeInteractive 无外观焦点/激活原语。业务页面不访问原版私有符号。

LHoloSelect 是自绘下拉控件，只负责展示选项和编辑回调，不包含结构业务逻辑；关闭/换页清理外部点击和 Escape 监听。结构变换通过 application/useTransform 接收原生快照和提交字段编辑，控件不访问通信桥。
