# 🎮 GXTStudio - GTA 文本编辑器

> **由 GTAmod 中文组精心打造，献给每一位热爱 GTA 模组制作的你**

[![Version](https://img.shields.io/badge/version-3.0.0-blue.svg)](./VersionInfo.cpp)
[![Qt](https://img.shields.io/badge/Qt-6.0+-green.svg)](https://www.qt.io/)
[![License](https://img.shields.io/badge/license-MIT-yellow.svg)](LICENSE)

---

## 📖 项目简介

**GXTStudio** 是一款专为 GTA 系列游戏（侠盗猎车手）打造的现代化文本文件（GXT）编辑器。无论你是想汉化游戏、制作模组，还是单纯想探索游戏文本的奥秘，这款工具都能成为你的得力助手！

我们深知模组制作者的痛点——老旧的工具、卡顿的界面、繁琐的操作。因此，我们使用 **C++17** 和 **Qt6** 框架完全重写了这款编辑器，为你带来**原生级的流畅体验**。

---

## ✨ 核心特性

### 🚀 现代化架构，性能卓越

- **完全重写**：告别老旧代码，采用现代化 C++ Qt6.0 框架
- **原生性能**：C++ 底层实现，操作响应如丝般顺滑
- **多线程优化**：文件解析、保存、翻译等操作均在独立线程完成，界面永不卡顿

### 📑 多标签页编辑

- 同时打开多个 GXT 文件，像使用现代 IDE 一样轻松切换
- 每个标签页独立管理，互不影响
- 支持拖拽调整标签顺序，符合你的使用习惯

### 🔄 智能翻译系统

- **AI 驱动**：集成智能翻译引擎，支持批量翻译
- **自定义提示词**：可根据需求调整翻译风格和术语
- **进度可视化**：实时显示翻译进度，支持中途取消
- **智能频率控制**：自动调整请求频率，避免触发 API 限制

### 🛡️ 只读模式

- 一键开启只读模式，防止意外修改重要文件
- 适合查看参考文件或学习他人作品时使用
- 状态自动保存，下次启动保持设置

### 🔍 强大的查找替换

- **批量替换**：支持在当前表、所有表或选定区域内替换
- **正则表达式**：高级用户可以使用正则进行复杂匹配
- **大小写敏感**：按需开启，精准定位
- **循环搜索**：自动从头/尾继续搜索，不遗漏任何匹配

### 💾 自动保存

- 担心意外崩溃丢失进度？开启自动保存即可高枕无忧！
- 可配置保存间隔，后台静默执行不影响工作
- 快捷键 `Ctrl+Shift+A` 快速切换开关状态

### 🎨 文本渲染预览

- **实时预览**：选中文本即可看到游戏中的实际显示效果
- **颜色标识符可视化**：自动解析 GTA 的颜色标记（如 `~r~` 红色、`~g~` 绿色）
- **游戏字体仿真**：使用原版游戏字体，预览效果更真实
- **支持 GTA3/VC/SA/4 等多种版本**

### 📤 批量导出

- 支持将多个标签页的内容批量导出
- 导出格式灵活，方便后续处理
- 选定相应标签页即可一键导出

### 🔧 字符表生成

- 一键生成字符表（.dat 文件）
- 方便制作自定义字体模组
- 支持按 Unicode 排序，格式规范

### 🔑 哈希转换

- 打开圣安地列斯（SA）或 GTA4 的 GXT 文件时
- 自动将哈希值转换为可读的键名
- 告别看不懂的十六进制代码！

### 🎭 键名校验

- 针对 GTA3 和罪恶都市（VC）的键名长度限制
- 超出限制时自动提示并阻止保存
- 帮助你制作符合游戏规范的模组

### 🖼️ 个性化背景

- 支持设置自定义背景图片
- 可调节透明度、模糊度、亮度
- 让编辑器也能彰显你的个性

### 📋 广泛的格式支持

| 格式 | 说明 | 支持操作 |
|------|------|----------|
| GXT | GTA 传统文本格式 | 完整读写 |
| GXT2 | GTA4 及后续版本 | 完整读写 |
| DAT | 字符表文件 | 完整读写 |
| WHM | 压缩文档格式 | 解析与导出 |

---

## 🚀 快速开始

### 系统要求

- **操作系统**：Windows 7/8/10/11（64位）
- **运行库**：Visual C++ Redistributable for Visual Studio 2015-2022
- **磁盘空间**：约 50MB（不含字体文件）

### 安装步骤

1. **下载程序**
   - 从发布页面下载最新版本的 `GXTStudio.zip`
   - 解压到任意目录（建议路径不含中文和特殊字符）

2. **运行程序**
   - 双击 `GXTStudio.exe` 即可启动
   - 首次启动会自动加载字体资源

3. **开始使用**
   - 点击欢迎页的「打开文件」按钮
   - 或直接将 GXT 文件拖拽到窗口中
   - 享受流畅的编辑体验吧！

### 快捷键速查

| 快捷键 | 功能 |
|--------|------|
| `Ctrl + O` | 打开文件 |
| `Ctrl + S` | 保存文件 |
| `Ctrl + Shift + S` | 另存为 |
| `Ctrl + F` | 查找 |
| `Ctrl + H` | 替换 |
| `Ctrl + Shift + A` | 切换自动保存 |
| `F1` | 关于/帮助 |

---

## 🏗️ 项目架构

### 技术栈

- **编程语言**：C++17
- **GUI 框架**：Qt 6.0+
- **构建系统**：CMake 3.16+
- **压缩支持**：zlib（可选，用于 WHM 文件）

### 项目文件结构

```
GXTStudio/
├── � main.cpp                     # 程序入口
├── 📄 GXTStudio.{h,cpp,ui}         # 主窗口
│
├── 📁 核心解析模块/
│   ├── GXTParser.{h,cpp}           # GXT 文件解析器（支持 GXT/GXT2/DAT/WHM）
│   ├── GXTEditor.{h,cpp}           # 编辑器核心逻辑
│   ├── GXTTableModel.{h,cpp}       # 数据模型（Qt Model/View）
│   └── CharTableParser.{h,cpp}     # 字符表解析器
│
├── 📁 界面组件/
│   ├── WelcomeWidget.{h,cpp}       # 欢迎页面
│   ├── TextRenderWidget.{h,cpp}    # 文本渲染预览（游戏字体仿真）
│   ├── CharTableWidget.{h,cpp}     # 字符表编辑器
│   ├── ReplaceDialog.{h,cpp}       # 查找替换对话框
│   ├── TranslateConfigDialog.{h,cpp}# 翻译配置对话框
│   ├── BackgroundConfigDialog.{h,cpp}# 背景设置对话框
│   ├── MultiThreadProgressDialog.{h,cpp}# 多线程进度对话框
│   └── AboutDialog.{h,cpp}         # 关于对话框
│
├── 📁 智能翻译/
│   ├── SmartTranslator.{h,cpp}     # AI 翻译引擎（支持批量翻译）
│   └── CodeTableConverter.{h,cpp}  # 码表转换器
│
├── 📁 后台工作线程/
│   ├── ParseWorker.{h,cpp}         # 异步文件解析
│   ├── SaveWorker.{h,cpp}          # 异步文件保存
│   ├── ReplaceWorker.{h,cpp}       # 异步批量替换
│   └── CharTableExportWorker.{h,cpp}# 异步字符表导出
│
├── 📁 工具与配置/
│   ├── FontAwesome.h               # Font Awesome 图标封装
│   ├── AppConfig.h                 # 应用程序配置管理
│   ├── VersionInfo.{h,cpp}         # 版本信息
│   ├── ItemPool.{h,cpp}            # 对象池（性能优化）
│   └── DebugConfigDialog.h         # 调试配置
│
├── 📁 资源文件/
│   ├── font/                       # 字体文件（Font Awesome、游戏字体）
│   ├── icon/                       # 程序图标
│   └── keylist/                    # 键名列表（SA/IVT 键名映射）
│
└── 📄 CMakeLists.txt               # CMake 构建配置
```

### 性能优化亮点

- **内存映射文件读取**：大文件加载速度提升 300%+
- **LRU 缓存机制**：智能缓存最近使用的文件
- **对象池技术**：减少频繁创建销毁对象的开销
- **延迟加载**：非必要组件延迟初始化，启动更快
- **双缓冲绘制**：消除界面闪烁，视觉更流畅

---

## 🛠️ 编译指南

### 环境准备

1. **安装 Visual Studio 2022**
   - 工作负载：「使用 C++ 的桌面开发」
   - 组件：Windows SDK、CMake 工具

2. **安装 Qt 6**
   - 推荐版本：Qt 6.5 LTS 或更高
   - 组件：MSVC 2019/2022 64-bit、Qt 5 Compatibility Module

3. **安装 CMake**
   - 版本要求：3.16 或更高
   - 确保添加到系统 PATH

### 编译步骤

```bash
# 1. 克隆仓库
git clone https://github.com/your-repo/GXTStudio.git
cd GXTStudio

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置 CMake
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.5.0/msvc2019_64"

# 4. 编译
cmake --build . --config Release

# 5. 运行
./Release/GXTStudio.exe
```

### 可选：启用 zlib 支持

如需处理压缩的 WHM 文件，请：

1. 下载 zlib 源码并编译 x64 版本
2. 放置到 `third_party/zlib` 目录
3. CMake 会自动检测并链接

---

## 📚 使用技巧

### 💡 高效编辑技巧

1. **快速定位条目**
   - 使用左侧搜索框实时过滤
   - 支持模糊匹配，输入部分字符即可

2. **批量修改键名前缀**
   - 使用替换功能的正则模式
   - 例如：查找 `^OLD_`，替换为 `NEW_`

3. **翻译工作流程**
   - 先使用智能翻译批量处理
   - 再人工校对关键文本
   - 利用文本渲染预览最终效果

### 🎨 文本颜色标记

GTA 游戏支持丰富的文本颜色标记：

| 标记 | 颜色 | 说明 |
|------|------|------|
| `~r~` | 🔴 红色 | 危险、警告 |
| `~g~` | 🟢 绿色 | 成功、金钱 |
| `~b~` | 🔵 蓝色 | 信息提示 |
| `~y~` | 🟡 黄色 | 任务目标 |
| `~p~` | 🟣 紫色 | 特殊标记 |
| `~w~` | ⚪ 白色 | 默认颜色 |
| `~s~` | ⏹️ 停止 | 结束颜色标记 |

在文本渲染预览中，你可以实时看到这些颜色的效果！

---

## 🤝 参与贡献

GXTStudio 是一个开源项目，我们欢迎任何形式的贡献：

- 🐛 **提交 Bug**：发现问题请在 Issues 中反馈
- 💡 **功能建议**：有新想法？告诉我们！
- 🔧 **代码贡献**：提交 Pull Request 改进代码
- 🌍 **翻译文档**：帮助完善多语言文档
- 📢 **推广宣传**：分享给更多模组制作者

### 贡献流程

1. Fork 本仓库
2. 创建你的特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开 Pull Request

---

## 📜 开源协议

本项目采用 **MIT 协议** 开源，你可以自由使用、修改和分发。

详见 [LICENSE](LICENSE) 文件。

---

## 🙏 致谢

### 特别感谢

- **GTAmod 中文组** 全体成员的支持与测试
- **Font Awesome** 提供精美的图标字体
- **Qt Company** 提供优秀的跨平台框架
- 所有为 GTA 模组社区贡献过的开发者们

### 第三方资源

| 资源 | 来源 | 用途 |
|------|------|------|
| Font Awesome 7 | Font Awesome, Inc. | 界面图标 |
| ViceCitySans | Rockstar Games | 字体预览 |
| zlib | zlib.net | 压缩支持 |

---

## 📞 联系我们

- **GitHub Issues**: [提交问题](https://github.com/your-repo/GXTStudio/issues)
- **GTAmod 中文组**: [访问论坛](https://gtamod.com)
- **QQ 群**: 搜索 "GTA 模组制作"

---

## 🗺️ 路线图

### 近期计划（v3.x）

- [ ] 支持更多 GTA 版本（如 GTA5）
- [ ] 插件系统，支持自定义扩展
- [ ] 协作编辑功能
- [ ] 云同步配置

### 远期愿景（v4.0+）

- [ ] 跨平台支持（Linux、macOS）
- [ ] 集成脚本编辑器
- [ ] 完整的模组项目管理
- [ ] AI 辅助翻译优化

---

## ⭐ 支持我们

如果这个项目帮助到了你，请给我们一个 Star ⭐！

你的支持是我们持续改进的动力！

---

<div align="center">

**Made with ❤️ by GTAmod 中文组**

*让模组制作更简单，让游戏世界更精彩*

</div>
