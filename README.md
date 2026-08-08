# DailyReport — 日报助手

Windows 桌面应用，自动追踪开发者日常活动（文件编辑、进程、窗口焦点、浏览器 URL、Git 提交、构建事件），通过 LLM 生成结构化的日报/周报。

**开发环境**: Linux (Ubuntu 24.04) + 交叉编译 → **目标平台**: Windows 10/11

---

## 构建与运行

### 安装交叉编译工具链

```bash
# 安装 mingw-w64 交叉编译器和依赖
sudo apt install mingw-w64 cmake g++ make

# 安装 Qt6 (通过 vcpkg)
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
./vcpkg install qtbase:x64-mingw-static spdlog:x64-mingw-static nlohmann-json:x64-mingw-static
```

### 编译

```bash
cd DailyReportAssistance

# 配置 + 编译
make reconf      # 初始配置（只需一次）
make build       # 编译 (Release)
make debug       # 编译 (Debug)

# 部署 Qt 插件和 DLL
make deploy
```

### 常用命令

| 命令 | 说明 |
|------|------|
| `make build` | 交叉编译 (Release) |
| `make debug` | 交叉编译 (Debug) |
| `make clean` | 清理编译产物，保留 CMake 缓存 |
| `make distclean` | 删除整个 build 目录 |
| `make reconf` | 重新生成交叉编译配置 |
| `make deploy` | 编译 + 部署 Qt DLL 到输出目录 |
| `make format` | clang-format 格式化所有源码 |
| `make targets` | 显示所有可用命令 |

### 在 Windows 上运行

将 `build/DailyReport.exe` 和 `build/` 下部署的 DLL 目录复制到 Windows 机器：

```bash
# 或直接在 Linux 上用 wine 测试
wine build/DailyReport.exe
```

首次启动会弹出 SetupWizard 配置向导，需要至少配置一个 LLM 后端（OpenAI / Anthropic / Ollama）才能生成报表。

---

## 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                        UI Layer                              │
│  ┌──────────┐ ┌──────────────┐ ┌────────────┐ ┌──────────┐ │
│  │SystemTray│ │ MainWindow   │ │ReportViewer│ │SetupWizard│ │
│  │ 系统托盘  │ │ 主窗口(多标签) │ │ 报表查看器  │ │ 首次配置   │ │
│  └──────────┘ └──────────────┘ └────────────┘ └──────────┘ │
├─────────────────────────────────────────────────────────────┤
│                       Report Layer                           │
│  ┌───────────────┐ ┌───────────────┐ ┌──────────────────┐  │
│  │ReportGenerator│ │TemplateEngine │ │ReportScheduler   │  │
│  │ 报表编排       │ │ 模板引擎       │ │ 定时调度(日/周)    │  │
│  └───────────────┘ └───────────────┘ └──────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                        LLM Layer                             │
│  ┌──────────┐ ┌──────────────┐ ┌─────────────┐ ┌─────────┐ │
│  │LlmClient │ │OpenAiBackend │ │AnthropicBck │ │OllamaBck│ │
│  │ 路由门面  │ │ GPT-4o       │ │ Claude       │ │ 本地LLM  │ │
│  └──────────┘ └──────────────┘ └─────────────┘ └─────────┘ │
├─────────────────────────────────────────────────────────────┤
│                      Pipeline Layer                          │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐ ┌────────────┐ │
│  │Collector │→│ Filter   │→│ Classifier   │→│ Assembler  │ │
│  │ 缓冲批量  │ │ 去噪过滤  │ │ 分类加标签    │ │ 时间线组装  │ │
│  └──────────┘ └──────────┘ └──────────────┘ └────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                      Monitor Layer                           │
│  ┌────────┐┌────────┐┌──────────┐┌────────┐┌──────────┐   │
│  │FileSys ││Process ││WindowFocus││Input   ││BrowserUrl│   │
│  │Monitor ││Monitor ││Monitor    ││Monitor ││Monitor   │   │
│  └────────┘└────────┘└──────────┘└────────┘└──────────┘   │
│  ┌──────────┐ ┌──────────────┐                              │
│  │GitMonitor│ │BuildMonitor  │  ← 每个 Monitor 独立线程    │
│  └──────────┘ └──────────────┘                              │
├─────────────────────────────────────────────────────────────┤
│                      Storage Layer                           │
│  ┌────────┐ ┌──────────┐ ┌─────────────────┐ ┌───────────┐ │
│  │Database│ │EventRepo │ │SettingsRepository│ │ReportRepo │ │
│  │ SQLite │ │ 事件CRUD  │ │ 键值设置          │ │ 报表CRUD  │ │
│  └────────┘ └──────────┘ └─────────────────┘ └───────────┘ │
├─────────────────────────────────────────────────────────────┤
│                       Core Layer                             │
│  RawEvent · ActivityEvent · ActivitySummary · Timeline      │
│  Result<T> · EventType · EventCategory                      │
├─────────────────────────────────────────────────────────────┤
│                       Util Layer                             │
│  Log · JsonUtils · WinUtils · CryptoUtils                   │
└─────────────────────────────────────────────────────────────┘
```

**依赖方向**: `ui → app → report → llm → pipeline → monitor → storage → core → util`

---

## 模块详解

### monitor — 活动监控层

7 个 Monitor 各运行在独立 `QThread` 上，全部基于 Win32 API：

| 监控器 | 检测机制 | 事件类型 |
|--------|----------|----------|
| `FileSystemMonitor` | `ReadDirectoryChangesW` + IOCP | FileCreated/Modified/Deleted/Renamed |
| `ProcessMonitor` | `CreateToolhelp32Snapshot` + WMI 轮询 (5s) | ProcessStarted/Ended |
| `WindowFocusMonitor` | `SetWinEventHook(EVENT_OBJECT_FOCUS)` | WindowFocusChanged |
| `InputActivityMonitor` | `GetLastInputInfo()` 轮询 (5s) | UserActive/Idle |
| `BrowserUrlMonitor` | UI Automation API | UrlVisited |
| `GitMonitor` | `git log` 轮询 (60s, QProcess) | GitCommit/Push/Pull/BranchSwitch |
| `BuildMonitor` | 进程匹配 (30+ 构建工具) | BuildStarted/Completed |

### llm — LLM 集成层

| 后端 | 默认模型 | 协议 |
|------|----------|------|
| `OpenAiBackend` | GPT-4o | OpenAI Chat Completions API |
| `AnthropicBackend` | Claude Sonnet 4 | Anthropic Messages API |
| `OllamaBackend` | Llama3 | Ollama 本地 API |

---

## 技术栈

| 类别 | 技术 | 用途 |
|------|------|------|
| 语言 | C++20 | `std::variant`, `std::optional`, structured bindings |
| GUI | Qt 6 (Widgets) | 主窗口、系统托盘、设置对话框 |
| 数据库 | SQLite (WAL) via Qt6::Sql | 事件存储、设置、报表持久化 |
| 日志 | spdlog | 控制台 + 滚动文件 |
| JSON | nlohmann-json | LLM API 请求/响应解析 |
| 构建 | CMake 3.21+ | 跨模块构建 |
| 包管理 | vcpkg | Qt6, spdlog, nlohmann-json |
| 交叉编译 | mingw-w64 | Linux → Windows x64 |
| 平台 API | Win32 (DPAPI, WMI, UI Automation, ETW) | 活动监控、加密、单实例 |
| LLM | OpenAI / Anthropic / Ollama HTTP API | 报表生成 |
