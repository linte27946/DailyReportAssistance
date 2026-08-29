# DailyReport — 开发活动日报助手

DailyReport 是一个以 Linux 桌面为主要运行环境、同时保留 Windows 支持的 Qt 6 应用。它在本机采集开发活动，将事件过滤、分类并保存到 SQLite，再调用 OpenAI、Anthropic 或本地 Ollama 生成 Markdown 日报和周报。

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
| 浏览器 URL | ❌ | ✅ | Linux 需要后续增加浏览器扩展或桌面集成 |
| OpenAI / Anthropic / Ollama | ✅ | ✅ | 支持流式响应和请求超时 |
| 自动日报/周报 | ✅ | ✅ | 启动后会补生成错过时间但尚不存在的报告 |

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
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## 首次使用

首次启动会打开配置向导：

1. 添加需要监控的项目目录；
2. 选择 OpenAI、Anthropic 或 Ollama；
3. 配置模型、API 地址和报告语言；
4. 设置日报、周报生成时间。

设置保存在本机 SQLite。Windows API Key 使用 DPAPI 保护；Linux 当前仅进行本地编码存储，若机器是多人共享环境，请优先使用无密钥的本地 Ollama，或限制应用数据目录的访问权限。

修改 LLM、报告时间和语言后会立即生效。监控目录和监控器开关在重启应用后生效。

## 项目结构

```text
src/
├── app/       应用生命周期、单实例
├── core/      事件、分类、时间线和汇总模型
├── monitor/   文件、进程、窗口、输入、Git、构建监控
├── pipeline/  收集、过滤、分类和时间线组装
├── storage/   SQLite、迁移、事件/设置/报告仓库
├── llm/       OpenAI、Anthropic、Ollama 后端
├── report/    模板、生成器、调度器、Markdown 渲染
├── ui/        主窗口、托盘、设置、历史和时间线
└── util/      日志、JSON、凭据和平台工具
tests/         Qt Test 单元与集成测试
```

## 数据与隐私

- 活动数据只写入本机 SQLite，程序不会自行同步时间线。
- 生成报告时，模板中整理后的时间线会发送给所选 LLM；使用 Ollama 时可保持全程本地。
- 文件监控记录路径和变更类型，不读取或保存源文件正文。
- 默认排除 `.git`、`node_modules`、`build`、`target`、`dist` 等生成目录。

日志和数据库位置由 Qt 的 `QStandardPaths::AppLocalDataLocation` 决定。

## 已知限制

- Wayland 没有统一的全局前台窗口和空闲时间接口，目前这两项只支持 X11 辅助工具。
- Linux 浏览器 URL 采集尚未实现；浏览器进程和窗口标题仍可被其他监控器记录。
- 进程轮询无法可靠取得任意已结束进程的退出码，因此构建完成事件可能只有耗时而没有成功/失败状态。
- 文件重命名在便携轮询实现中会表现为“删除 + 新建”。

## 测试

测试覆盖事件分类、流水线持久化输出、SQLite 往返、模板渲染以及真实文件创建/修改/删除监控：

```bash
ctest --test-dir build --output-on-failure
```
