# DailyReport — 日报助手

Windows 桌面应用，自动追踪开发者日常活动（文件编辑、进程、窗口焦点、浏览器 URL、Git 提交、构建事件），通过 LLM 生成结构化的日报/周报。

## 目录

- [架构概览](#架构概览)
- [模块详解](#模块详解)
  - [core — 核心数据结构](#core--核心数据结构)
  - [util — 工具库](#util--工具库)
  - [storage — 持久化层](#storage--持久化层)
  - [monitor — 活动监控层](#monitor--活动监控层)
  - [pipeline — 事件处理管道](#pipeline--事件处理管道)
  - [llm — LLM 集成层](#llm--llm-集成层)
  - [report — 报表生成层](#report--报表生成层)
  - [app — 应用框架](#app--应用框架)
  - [ui — 用户界面](#ui--用户界面)
- [数据流](#数据流)
- [构建与运行](#构建与运行)
- [技术栈](#技术栈)

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

### core — 核心数据结构

**类型**: INTERFACE 库（header-only）  
**依赖**: `util`

在整个项目中流通的基础数据类型，所有模块都依赖此层。

| 类型 | 说明 |
|------|------|
| `RawEvent` | 监控器捕获的原始事件 — 时间戳、类型、进程名、窗口标题、文件路径、URL、JSON 元数据 |
| `ActivityEvent` | 经管道分类后的事件 — 增加分类标签、持续时间、会话 ID、文件扩展名 |
| `ActivitySummary` | 单日统计摘要 — 各分类耗时占比、文件编辑次数、Git commit 数、构建成功/失败数、最常用文件/应用 Top N |
| `Timeline` | 按时间排序的 ActivityEvent 集合，支持按日期/分类过滤和 ActivitySummary 聚合 |
| `EventCategory` | 活动大类枚举: `Coding`, `Debugging`, `Building`, `Testing`, `Documentation`, `Communication`, `VersionControl`, `Browsing`, `Idle`, `Other` |
| `EventType` | 细粒度事件类型: `FileCreated/Modified/Deleted/Renamed`, `ProcessStarted/Ended`, `WindowFocusChanged`, `UserActive/Idle`, `UrlVisited`, `GitCommit/Push/Pull/BranchSwitch/Merge`, `BuildStarted/Completed`, `SessionStarted/Ended` |
| `Result<T>` | Rust 风格 Result 类型 — `ok()` / `err()` 含错误信息，支持 `.map()` 链式变换。另有 `Result<void>` 特化 |

---

### util — 工具库

**类型**: INTERFACE 库（header-only）  
**依赖**: Qt6::Core, spdlog, nlohmann_json

| 组件 | 说明 |
|------|------|
| `Log` | spdlog 初始化 — 控制台彩色输出 + 滚动文件（10MB×3），日志写入 `%LOCALAPPDATA%/logs/dailyreport.log` |
| `JsonUtils` | QJsonDocument ↔ QString / QByteArray 便捷转换 |
| `WinUtils` | Windows 辅助: 开机自启注册表操作、管理员权限检测 |
| `CryptoUtils` | Windows DPAPI 加解密 — 用于安全存储 API Key 等敏感配置 |

---

### storage — 持久化层

**类型**: INTERFACE 库（header-only）  
**依赖**: `core`, Qt6::Core, Qt6::Sql

基于 SQLite (WAL 模式)，线程独立连接，版本化迁移。

| 组件 | 说明 |
|------|------|
| `Database` | 单例 SQLite 管理器 — 线程独立连接（`localData` 模式）、WAL 日志、自动运行迁移 |
| `DatabaseMigrator` | 版本化迁移引擎 — 从 `migrations/` 目录读取 `.sql` 文件，按版本号顺序执行，`schema_version` 记录当前版本 |
| `EventRepository` | `activity_events` 表 CRUD — 批量插入、按日期/类型/分类查询、时间线构建 |
| `SettingsRepository` | `app_settings` 键值表读写 — LLM 配置、报表时间、监控路径等 |
| `ReportRepository` | `reports` 表 CRUD — 保存/加载生成的日报周报、历史查询 |

**数据库表结构**（migration `001_initial_schema.sql`）:
- `activity_events` — 事件主表（timestamp, type, category, description, application, file_path, url, duration_secs, session_id, metadata）
- `app_settings` — 设置键值（key, value）
- `reports` — 已生成报表（report_type, date, content, llm_backend, llm_model, generation_time_secs）
- `report_templates` — 自定义模板
- `monitored_paths` — 监控目录列表

---

### monitor — 活动监控层

**类型**: STATIC 库  
**依赖**: `core`, Qt6::Core, Win32 API

每个 Monitor 运行在独立 `QThread` 上，通过信号（queued connection）向管道线程发送事件。

#### 监控基类

```cpp
class IMonitor : public QObject {
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual QString name() const = 0;
signals:
    void rawEventCaptured(const RawEvent &event);
    void monitorError(const QString &name, const QString &error);
};
```

#### 7 个监控器

| 监控器 | 事件类型 | 检测机制 |
|--------|----------|----------|
| `FileSystemMonitor` | FileCreated/Modified/Deleted/Renamed | `ReadDirectoryChangesW` + IOCP，监控配置目录中的文件变更 |
| `ProcessMonitor` | ProcessStarted/Ended | WMI 轮询 (`Win32_ProcessStartTrace` / `Win32_ProcessStopTrace`)，每 5s |
| `WindowFocusMonitor` | WindowFocusChanged | `SetWinEventHook(EVENT_OBJECT_FOCUS)`，捕获活动窗口标题和进程名 |
| `InputActivityMonitor` | UserActive/Idle | `GetLastInputInfo()` 轮询，超过阈值（默认 300s）判定为 Idle |
| `BrowserUrlMonitor` | UrlVisited | UI Automation API 读取浏览器地址栏（Chrome/Edge/Firefox），支持文档站点识别 |
| `GitMonitor` | GitCommit/Push/Pull/BranchSwitch/Merge | `git log` 轮询（默认 60s），解析哈希/作者/时间/消息/文件列表 |
| `BuildMonitor` | BuildStarted/Completed | 进程匹配 — 识别 MSVC/GCC/Clang/Rust/Cargo/Go/dotnet/npm 等 30+ 构建工具，跟踪进程生命周期 |

#### MonitorEngine

管理所有 Monitor 的生命周期 — `registerMonitor()` 注册（接管 `unique_ptr`），`startAll()` / `stopAll()` 统一启停，将各 Monitor 的 `rawEventCaptured` 信号**中继转发**到管道。

---

### pipeline — 事件处理管道

**类型**: STATIC 库  
**依赖**: `core`, `util`, Qt6::Core

四阶段处理链，运行在独立工作线程 (`PipelineWorker`) 上。接收原始事件，产出结构化时间线。

```
MonitorEngine → rawEventCaptured
                      ↓
             ┌────────────────┐
             │ EventCollector │  缓冲原始事件，定时（或满批次）flush
             └───────┬────────┘
                     ↓  QList<RawEvent>
             ┌────────────────┐
             │  EventFilter   │  去噪: 忽略临时文件(.tmp)、系统目录(node_modules/.git/build)、
             └───────┬────────┘  忽略自身进程、去重重复事件
                     ↓
             ┌──────────────────┐
             │ActivityClassifier│  规则引擎分类: 进程名通配 → EventType + EventCategory
             └───────┬──────────┘  规则可 JSON 配置，支持扩展名/URL 模式/事件类型匹配
                     ↓  QList<ActivityEvent>
             ┌──────────────────┐
             │TimelineAssembler │  排序、合并间隔<2min 的同类型事件、计算持续时间
             └──────────────────┘  输出完整 Timeline + ActivitySummary
                     ↓
              timelineUpdated / summaryUpdated
```

| 组件 | 说明 |
|------|------|
| `EventPipeline` | 管道门面 — `onRawEvent()` 槽接收事件，`start()`/`stop()` 控制工作线程 |
| `EventCollector` | 事件缓冲器 — 缓冲至 N 条或定时（默认 5s），批量提交下游 |
| `EventFilter` | 过滤器 — 忽略路径/进程/扩展名黑名单，去重相同事件 |
| `ActivityClassifier` | 规则分类器 — 基于可配置规则集（进程名→活动类型），支持通配符匹配 |
| `TimelineAssembler` | 时间线组装 — 排序、合并相邻事件、计算 ActivitySummary |
| `PipelineWorker` | 工作线程 — 将整个管道移出主线程，通过信号与 UI 通信 |

---

### llm — LLM 集成层

**类型**: STATIC 库  
**依赖**: `core`, `util`, Qt6::Core, Qt6::Network

策略模式的多后端架构，通过 `LlmClient` 门面统一调用。

#### 后端接口

```cpp
class ILlmBackend : public QObject {
    virtual QString name() const = 0;                    // "OpenAI" / "Anthropic" / "Ollama"
    virtual void configure(const LlmConfig &config) = 0; // 配置端点/Key/模型/参数
    virtual QFuture<bool> isAvailable() = 0;             // 健康检查
    virtual QFuture<QString> generate(systemPrompt, userPrompt) = 0; // 异步生成
signals:
    void streamingToken(const QString &token);            // 流式 token
    void generationComplete(const QString &fullText);     // 生成完成
    void generationError(const QString &error);           // 生成失败
};
```

#### 后端实现

| 后端 | 默认模型 | 协议 |
|------|----------|------|
| `OpenAiBackend` | GPT-4o | OpenAI Chat Completions API (`/v1/chat/completions`) |
| `AnthropicBackend` | Claude Sonnet 4 | Anthropic Messages API (`/v1/messages`) |
| `OllamaBackend` | Llama3 | Ollama 本地 API (`/api/generate`) |

#### 其他组件

| 组件 | 说明 |
|------|------|
| `LlmClient` | 后端门面 — `registerBackend()` 注册后端，`setActiveBackend()` 切换，信号转发（含流式 token），可用性检查 |
| `LlmConfig` | 后端配置 — endpoint, apiKey, model, temperature, maxTokens, timeout，提供各后端预设值 |
| `PromptTemplate` | 提示词模板 — `{{placeholder}}` 变量替换，支持 Markdown 格式模板 |

#### 安全

API Key 通过 `CryptoUtils` 使用 Windows DPAPI 加密存储，以当前用户身份绑定。

---

### report — 报表生成层

**类型**: STATIC 库  
**依赖**: `core`, `storage`, `llm`, `util`, Qt6::Core

| 组件 | 说明 |
|------|------|
| `ReportGenerator` | 报表编排 — `generateDailyReport()` / `generateWeeklyReport()`，从数据库加载 Timeline → 用 TemplateEngine 构建 prompt → 调用 LlmClient → 保存 ReportRecord → 发出 `generationCompleted(result)` |
| `TemplateEngine` | 模板管理 — 从数据库加载/保存模板，`render()` 执行 `{{placeholder}}` 替换，`buildReportContext()` 将 ActivitySummary+Timeline 转为模板变量 map |
| `MarkdownRenderer` | MD → HTML — 支持标题、粗斜体、inline code、代码块(```)、链接、图片、表格、水平线，用于 ReportViewer 的富文本预览 |
| `ReportScheduler` | 定时调度 — `QTimer` 每日/每周定时自动触发 `ReportGenerator`。可配置日报时间（如 17:00）和周报日（如周五） |

#### 报表模板变量

模板使用 `{{variable}}` 语法，内置变量包括：

| 变量 | 来源 |
|------|------|
| `{{date}}` | 报表日期 |
| `{{activeHours}}` | `ActivitySummary::activeHours()` |
| `{{totalActiveSecs}}` | 活跃总秒数 |
| `{{idleTime}}` | 空闲总秒数 |
| `{{fileEditCount}}` | 文件编辑次数 |
| `{{gitCommitCount}}` | Git 提交次数 |
| `{{buildCount}}` / `{{buildFailureCount}}` | 构建次数/失败次数 |
| `{{topFiles}}` | 最常编辑文件（逗号分隔） |
| `{{topApps}}` | 最常用应用（逗号分隔） |
| `{{categoryBreakdown}}` | 各分类耗时明细 |
| `{{timelineMarkdown}}` | 时间线 Markdown 表格 |

---

### app — 应用框架

**类型**: INTERFACE 库（header-only）  
**依赖**: `storage`, Qt6::Core, Qt6::Widgets

| 组件 | 说明 |
|------|------|
| `Application` | QApplication 子类 — 生成 Session UUID，`initialize()` 初始化数据库+迁移，`dataDirectory()` 返回 `%LOCALAPPDATA%/DailyReport/DailyReport` |
| `SingleInstance` | 单实例保证 — Windows Named Mutex (`Global\DailyReport_SingleInstance`)，检测到已有实例时 `notifyExistingInstance()` 将已有窗口提到前台 |
| `ServiceLocator` | 泛型 DI 容器 — `registerInstance<T>(name)` / `resolve<T>(name)`，支持工厂函数 |

---

### ui — 用户界面

**类型**: STATIC 库  
**依赖**: `app`, `report`, `storage`, `util`, Qt6::Core, Qt6::Widgets, Qt6::Network

| 组件 | 说明 |
|------|------|
| `MainWindow` | 主窗口 — `QStackedWidget` 多标签切换: 报表查看、历史列表、时间线、设置。关闭时 `closeEvent → closeToTray` 最小化到托盘 |
| `SystemTray` | 系统托盘 — 右键菜单: 今日/本周报表、手动生成日报/周报、设置、退出。通知气泡提示报表生成状态 |
| `ReportViewer` | 报表预览 — 用 `MarkdownRenderer::toHtml()` 渲染 Markdown 为富文本，支持日期选择 |
| `ReportHistoryWidget` | 历史列表 — 展示已生成的日报/周报，按日期倒序，点击查看详情 |
| `TimelineWidget` | 时间线视图 — 可视化当日活动时间轴，分类颜色编码 |
| `SettingsDialog` | 设置对话框 — LLM 后端配置（类型选择/API Key/端点/模型）、报表时间设置、监控路径管理 |
| `SetupWizard` | 首次运行向导 — 引导用户配置 LLM 后端和基础设置 |

---

## 数据流

```
用户活动 (键盘/鼠标/编辑器/Git/浏览器/编译器)
        │
        ▼
  ┌───────────┐
  │ 7 Monitors │  每个独立线程，捕获系统级原始事件
  └─────┬─────┘
        │ rawEventCaptured (Qt signal, QueuedConnection)
        ▼
  ┌───────────────┐
  │ MonitorEngine  │  中继转发到 Pipeline
  └───────┬───────┘
          │ onRawEvent
          ▼
  ┌──────────────────────────────────────────┐
  │           EventPipeline (独立线程)         │
  │  Collect → Filter → Classify → Assemble  │
  └──────────────────┬───────────────────────┘
          │ timelineUpdated / summaryUpdated
          ▼
  ┌──────────────┐
  │   Storage     │  ActivityEvent 写入 SQLite
  └──────┬───────┘
         │ 按需查询 (定时 / 用户手动)
         ▼
  ┌────────────────────────────────┐
  │       ReportGenerator          │
  │  Timeline → TemplateEngine     │
  │  → Prompt → LlmClient → 报表   │
  └────────────┬───────────────────┘
               │ ReportResult
               ▼
  ┌──────────────────────┐
  │  SystemTray / Viewer  │  通知 + 展示
  └──────────────────────┘
```

---

## 构建与运行

### 环境要求

- **Windows 10/11** (x64) — 使用了 Win32 DPAPI、UI Automation、WMI、ETW
- **Visual Studio 2022** (MSVC 19.x, C++20)
- **CMake ≥ 3.21**
- **vcpkg** (包管理器)

### 安装依赖

```bash
# 1. 克隆 vcpkg（如果还没有）
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg && .\bootstrap-vcpkg.bat

# 2. 安装依赖（Qt6 编译约 30-60 分钟）
cd C:\vcpkg
.\vcpkg install spdlog:x64-windows nlohmann-json:x64-windows --classic
.\vcpkg install qtbase:x64-windows --classic
```

### 构建

```bash
cd DailyReportAssistance
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### 运行

```bash
# 直接运行
.\build\Release\DailyReport.exe

# 首次运行后，应用以系统托盘方式驻留
# 右键托盘图标 → Generate Daily Report 或 Settings
```

> **提示**: 首次启动会弹出 SetupWizard 配置向导，需要至少配置一个 LLM 后端（OpenAI / Anthropic / Ollama）才能生成报表。

### 运行测试

```bash
ctest --test-dir build --build-config Release
```

---

## 技术栈

| 类别 | 技术 | 用途 |
|------|------|------|
| 语言 | C++20 | `std::variant`, `std::optional`, structured bindings, `contains()` |
| GUI | Qt 6.11 (Widgets) | 主窗口、系统托盘、设置对话框 |
| 数据库 | SQLite (WAL) via Qt6::Sql | 事件存储、设置、报表持久化 |
| 日志 | spdlog 1.17 | 控制台 + 滚动文件 |
| JSON | nlohmann-json 3.12 | LLM API 请求/响应解析 |
| 构建 | CMake 3.21 + vcpkg | 跨模块构建 + 依赖管理 |
| 平台 | Win32 API | `ReadDirectoryChangesW`, WMI, UI Automation, `SetWinEventHook`, `GetLastInputInfo`, ETW, DPAPI |
| LLM | OpenAI / Anthropic / Ollama HTTP API | 报表生成 |
