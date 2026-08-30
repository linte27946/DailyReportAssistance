# DailyReport — 开发活动日报助手

DailyReport 是一个以 Linux 桌面为主要运行环境、同时保留 Windows 支持的 Qt 6 应用。它在本机采集开发活动，将事件过滤、分类并保存到 SQLite，再调用 OpenAI、Anthropic、DeepSeek 或本地 Ollama 生成 Markdown 日报和周报。

## 当前能力

数据链路已经打通：

```text
文件/进程/窗口/Git/构建/空闲监控
                ↓
       过滤 → 分类 → 时间线
                ↓
          SQLite 持久化
                ↓
       模板 + LLM → 日报/周报
```

| 功能 | Linux | Windows | 说明 |
|---|---:|---:|---|
| 递归文件变更 | ✅ | ✅ | 轮询快照，过滤构建目录与临时产物 |
| 开发相关进程 | ✅ | ✅ | Linux 读取 `/proc`，Windows 使用进程快照 |
| Git commit | ✅ | ✅ | 监控配置的 Git 仓库 |
| 构建进程 | ✅ | ✅ | 由进程事件识别 CMake、Ninja、Make、Cargo 等 |
| 前台窗口 | ⚠️ | ✅ | Linux X11 需要 `xdotool`；Wayland 暂不支持 |
| 用户空闲 | ⚠️ | ✅ | Linux X11 需要 `xprintidle`；Wayland 暂不支持 |
| 编辑器文件/项目上下文 | ⚠️ | ✅ | 支持 VS Code、Cursor、JetBrains、Visual Studio 等；Linux 依赖 X11 窗口信息 |
| PDF / Office 资料名称 | ⚠️ | ✅ | 支持 PDF、Word、PPT、Excel、LibreOffice/WPS 等，不读取正文 |
| 浏览器页面 | ⚠️ | ✅ | Windows 记录页面标题和清洗后的活动页 URL；Linux X11 当前记录页面标题；可选择单独标记娱乐/摸鱼页面 |
| OpenAI / Anthropic / DeepSeek / Ollama | ✅ | ✅ | 支持流式响应和请求超时 |
| 无 API 总结包 | ✅ | ✅ | 可复制提示词或导出 Markdown，交给任意 AI 网站总结 |
| 自动日报/周报 | ✅ | ✅ | 启动后会补生成错过时间但尚不存在的报告 |
| 历史报告管理 | ✅ | ✅ | 支持筛选、打开并单独删除指定报告，删除前会二次确认 |
| 企业微信实际参会记录 | ✅ | ✅ | 通过官方 `wecom-cli` 只读同步；仅用低优先级规则填补空闲片段 |

不支持的可选监控器不会阻止其他监控器启动。

## Linux 构建

需要 CMake 3.21+、C++20 编译器、Qt 6（Core、Widgets、Network、Sql、Concurrent、Test）、SQLite Qt 驱动、spdlog 和 nlohmann-json。`xdotool`、`xprintidle` 是 X11 下的可选运行时依赖。

以使用 Ninja 的单配置构建为例：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/DailyReport
```

安装到自定义前缀：

```bash
cmake --install build --prefix "$HOME/.local"
```

数据库迁移会安装到 `share/dailyreport/migrations`。直接从构建目录运行时，程序也会自动查找源码树中的迁移文件。

## Windows 构建

仓库包含 `vcpkg.json`，可使用 MSVC 和 vcpkg 工具链配置：

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release --target deploy
ctest --test-dir build -C Release --output-on-failure
& .\build\Release\DailyReport.exe
```

也可以直接双击仓库根目录的 `run.bat`。它会先编译最新代码、复制 Qt DLL 和插件，再启动 `build\Release\DailyReport.exe`。运行时不能只复制单个 EXE，需保留 `build\Release` 中的 DLL 与插件目录。

### Windows 安装程序

构建机需要先安装 [Inno Setup 7](https://jrsoftware.org/isdl.php)。从系统托盘退出正在运行的 DailyReport，然后执行：

```powershell
.\run.bat package
```

该命令会编译 Release 版本、整理全部运行库，并在 `dist` 下生成：

- `DailyReport-Setup-1.0.0.exe`：可发送给用户的标准安装程序；
- `DailyReport-Setup-1.0.0-SHA256.txt`：安装程序的完整性校验值。

用户双击安装程序后可以选择安装范围和安装目录，可选创建桌面快捷方式；安装完成后会创建开始菜单和系统卸载入口。安装器包含 Qt/SQLite/OpenSSL、Visual C++ 运行库、数据库迁移文件、使用说明和第三方许可证，不包含源码、PDB、开发工具、个人数据库或 API Key。卸载程序默认保留用户数据库和设置，避免误删个人数据。企业微信会议属于可选集成，用户仍需自行安装 Node.js 18+ 和官方 `wecom-cli` 并扫码授权。

如仍需生成解压即用的便携 ZIP，可执行：

```powershell
.\run.bat portable
```

## 首次使用

首次启动会打开配置向导：

1. 添加需要监控的项目目录；
2. 选择 OpenAI、Anthropic、DeepSeek 或 Ollama；
3. 配置模型、API 地址和报告语言；
4. 设置日报、周报生成时间。

设置保存在本机 SQLite。Windows API Key 使用 DPAPI 保护；Linux 当前仅进行本地编码存储，若机器是多人共享环境，请优先使用无密钥的本地 Ollama，或限制应用数据目录的访问权限。

修改 LLM、报告时间和语言后会立即生效。文件、浏览器等本机监控目录和开关在重启应用后生效；企业微信会议同步开关在保存后立即生效。

界面当前支持 English 和简体中文。在“设置 → 计划与语言”中修改“界面与报告语言”并保存后，主窗口、设置页、报告页、历史记录、活动时间线和系统托盘菜单会立即切换；同一选项也决定 AI 生成报告所使用的语言。

Windows 下重新编译前，需要先从系统托盘退出正在运行的 DailyReport，否则链接器无法覆盖 `build\Release\DailyReport.exe`。`run.bat` 会检测这一情况并给出提示。

## 企业微信会议

DailyReport 通过企业微信官方命令行工具读取本人实际参会记录，不读取桌面客户端私有数据库。首次使用需安装 Node.js 18+，然后执行：

```bash
npm install -g @wecom/cli
wecom-cli auth init
```

完成授权后，在 **设置 → 集成 → 企业微信会议** 中启用功能，可以立即同步，也可以配置自动同步间隔。设置页会自动执行 `wecom-cli auth show` 并明确区分“未安装、未授权、授权成功（含 Bot ID）和检测失败”；扫码后也可以点击“重新检测授权”。只有检测到授权成功后，“立即同步会议”按钮才会启用。桌面企业微信已经登录不等于 CLI 已授权，两者需要分别完成登录/授权。

预约会议不会直接计入工作时间。同步结果必须包含本人实际入会和离会时间；并且在该参会区间内，空闲时间占有效监控时间的比例必须严格大于默认的 30%。达到条件后也只把空闲片段补记为会议，编码、文档、浏览器和其他已检测活动保持原分类。重复同步使用稳定外部 ID 去重。

## 没有 API Key 时生成日报

进入“报告中心”，点击“复制 AI 提示词”可将当天整理后的提示词复制到剪贴板；点击“导出 AI 总结包”会生成一个 Markdown 文件。把文字粘贴到、或把文件上传到支持长文本的 AI 对话网站即可。总结包包含编辑文件名、项目上下文、Git/构建活动、技术网页和资料名称，不包含源码或文档正文。详见 [`docs/manual-ai-workflow.md`](docs/manual-ai-workflow.md)。

## 项目结构

```text
src/
├── app/       应用生命周期、单实例
├── core/      事件、分类、时间线和汇总模型
├── monitor/   文件、进程、窗口、输入、Git、构建监控
├── pipeline/  收集、过滤、分类和时间线组装
├── storage/   SQLite、迁移、事件/设置/报告仓库
├── llm/       OpenAI、Anthropic、DeepSeek、Ollama 后端
├── report/    模板、生成器、调度器、Markdown 渲染
├── ui/        主窗口、托盘、设置、历史和时间线
└── util/      日志、JSON、凭据和平台工具
tests/         Qt Test 单元与集成测试
```

## 数据与隐私

- 活动数据只写入本机 SQLite，程序不会自行同步时间线。
- 生成报告时，模板中整理后的时间线会发送给所选 LLM；使用 Ollama 时可保持全程本地。
- 文件监控记录路径和变更类型，不读取或保存源文件正文。
- 编辑器和资料监控只解析前台窗口中的文件、项目和文档名称，不读取源码、PDF 或 Office 正文。
- 浏览器默认删除 URL 用户信息、查询参数和片段，只保留协议、域名与路径；可在设置中显式选择保留查询参数。
- “记录娱乐与摸鱼浏览”默认关闭。开启后，常见直播、游戏和视频网站会单独归类为“娱乐/摸鱼”，并与技术调研分开展示；仍只保存页面标题和经过隐私过滤的网址。该监控开关在重启应用后生效。
- 默认排除 `.git`、`node_modules`、`build`、`target`、`dist` 等生成目录。

日志和数据库位置由 Qt 的 `QStandardPaths::AppLocalDataLocation` 决定。

### 数据保留与自动清理

活动记录和历史报告分别保存在本机 SQLite 数据库中。可以打开
**设置 → 数据与隐私**，分别配置两类数据的保留时间，默认均为
**3 个月**。

- 程序启动时执行一次清理，持续运行时每 24 小时自动清理一次。
- 活动清理包括编辑器、浏览器、文档、Git、构建、进程和窗口事件。
- 报告清理会删除应用内过期的日报和周报历史。
- 历史报告页也可以手动选中并删除某一份报告；该操作只影响应用数据库，不会删除已经导出的文件。
- 已导出的 Markdown、HTML、TXT 和 AI 总结包位于数据库之外，不会被自动删除。
- 设置页提供带确认提示的“立即清理过期数据”，并显示上次清理结果。

## 已知限制

- Wayland 没有统一的全局前台窗口和空闲时间接口，目前这两项只支持 X11 辅助工具。
- Linux X11 当前可记录活动浏览器页面标题，但不能稳定取得完整 URL；Wayland 下需要后续浏览器扩展或桌面门户支持。
- 当前记录的是编辑器窗口上下文与文件系统变更，无法知道某一行代码的语义；若需要函数级、任务级精度，后续应提供 VS Code/JetBrains 扩展，并由用户明确授权。
- 当前不扫描浏览器的完整历史数据库，只记录程序运行期间的活动页面；完整历史导入应作为单独的显式授权功能。
- 进程轮询无法可靠取得任意已结束进程的退出码，因此构建完成事件可能只有耗时而没有成功/失败状态。
- 文件重命名在便携轮询实现中会表现为“删除 + 新建”。

## 测试

测试覆盖事件分类、流水线持久化输出、SQLite 往返、模板渲染以及真实文件创建/修改/删除监控：

```bash
ctest --test-dir build --output-on-failure
```
