DailyReport for Windows / Windows 使用说明
===========================================

中文
----
1. 支持 Windows 10/11 64 位系统。
2. 请先完整解压 ZIP，再双击 DailyReport.exe；不要只复制 EXE 文件。
3. 第一次启动会显示设置向导。软件默认只在本机保存数据，不会上传源码或文档正文。
4. 数据库和设置位于当前 Windows 用户的本地应用数据目录，通常是：
   %LOCALAPPDATA%\DailyReport\DailyReport
5. 卸载便携版：先从系统托盘退出 DailyReport，再删除整个软件目录。如果还要清除个人数据，可另外删除上面的本地应用数据目录。
6. AI 日报为可选功能。可以填写自己的 API Key、使用本机 Ollama，或者导出 AI 总结包后交给免费的 AI 网站。
7. 企业微信会议同步为可选功能，不包含在本压缩包内。需要另外安装 Node.js 18+，然后执行：
   npm install -g @wecom/cli
   wecom-cli auth init
   返回“设置 → 集成”检查授权状态。
8. Windows 可能会提示这是来自互联网的未知发布者程序，因为当前版本尚未使用商业代码签名证书。这不影响程序功能；请仅从可信来源获取压缩包，并核对发布者提供的 SHA256。

English
-------
1. Windows 10/11 x64 is supported.
2. Extract the entire ZIP before starting DailyReport.exe. Do not copy only the EXE.
3. The first launch opens the setup wizard. Data stays on this PC by default; source code and document contents are not uploaded.
4. The database and settings are stored in the current user's local application-data directory, normally:
   %LOCALAPPDATA%\DailyReport\DailyReport
5. To remove the portable app, exit it from the system tray and delete the whole application folder. Delete the local application-data folder separately only if you also want to erase personal data.
6. AI report generation is optional. Use your own API key, local Ollama, or export an AI package for a free AI website.
7. WeCom meeting sync is optional and is not bundled. Install Node.js 18+ and @wecom/cli separately, authorize it, then verify authorization under Settings → Integrations.
8. Windows may show an unknown-publisher warning because this build is not yet signed with a commercial code-signing certificate. Obtain it from a trusted source and compare its SHA256 checksum.

