# 当前移植状态与后续开发边界

更新时间：2026-08-03。

当前仓库已经完成 Stage 6–10 的前端架构重构：公共菜单、Game Settings、书架服务、核心 adapter/registry/process runner 和 capability 层已经拆分，旧 `launch_request.ini`/退出码 `42` 协议已删除。Stage 11 的文档、自动化和 Windows 模拟校验已经完成；候选前端与 launcher 已部署到 GKD350H Ultra，并通过书架、菜单、系统设置和游戏设置的轻量截图冒烟。完整 GKD 真机功能矩阵按用户要求留给重构后的手工测试。

## 已固化能力

- 前端：ONS/KRKR 独立导航栏、平铺 `games/` 自动识别、封面与存档隔离、核心退出后返回原书架。
- 架构：公共前端模块边界对齐 ROCreader；游戏差异集中在 `GameEntry`、Game Settings callbacks、core adapters 和应用组合层。
- 启动：只有 `GameLaunchService -> IGameCoreAdapter -> CoreProcessRunner` 一条内部管线，前端负责核心退出后的 SDL 重建和书架状态恢复。
- 平台：Windows 使用 desktop capability；GKD350H Ultra 和 H700 34xxSP 当前声明 packaging verified，H700 真机输入/显示矩阵仍待回归。
- ONS：手柄导航、同款圆形虚拟光标、A 确认/B 取消、拖动、画面比例与 `fill-height`/`fill-width` 文本安全区处理。
- KRKR：ARM64 交叉构建、XP3/项目目录启动、基础插件兼容、`System.addFont`、中文字体、WebP、AAC/FFmpeg 音频、FFmpeg 视频、存档目录、手柄与虚拟光标基础。
- 默认中文字体：`fonts/ui_font_02.ttf`。
- 安全部署：Stage 11 只原子替换 launcher 与前端，ONS/KRKR、配置、键位和用户数据保持原哈希/目录状态；回滚备份与部署前哈希清单已保存。游戏、封面和存档不属于构建产物。

## KRKR 已知问题

- 《千恋万花》标题主界面的动态渐变/移动背景仍可能纯黑；普通游戏画面能够渲染。
- 快速连续确认曾出现退出码 11；事件去重修复已经进入当前源码，但仍需下一轮真机回归。
- `motionplayer` 的合成/持续 present 路径仍是后续渲染排查重点。
- KRKR 当前定位是“可启动、可进入游戏、音视频能力已接通，但兼容性与稳定性仍在验证”，不能标记为与 ONS 同等稳定。

## 当前开发优先级

1. 以 GKD350H Ultra 真机复现、最小修改、单线程增量构建、原子部署和真机回归为一轮，持续修复 KRKR2 核心能力。
2. 保持 GKD350H Ultra 与 H700 34xxSP 的复用式打包可用，发布前验证核心哈希、glibc 基线和包内目录。
3. 只补充明确需要的小功能；前端结构不再做大规模重构。

除非用户明确要求，不运行 `build_glibc234_all.*`、`build_release_docker.ps1`、`-Mode Full`、`-Clean` 或会重新配置/编译大规模 vcpkg 依赖图的命令。

## 必须保留的本地缓存

以下目录被 Git 忽略，但不应作为普通清理对象删除：

```text
build/gkd350h-glibc234/vcpkg      KRKR2 目标依赖与包缓存
build/gkd350h-glibc234/krkr2      KRKR2 CMake、对象和链接缓存
build/gkd350h-glibc234/krkrsdl2   KRKRSDL2 CMake 对象与预编译头缓存
build/gkd350h-glibc234/frontend   前端 ARM64 对象缓存
build/gkd350h-glibc234/sysroot    glibc 2.34 设备 sysroot 与目标依赖
GKD350HUltra/tools                 固定版本 CMake 与 vcpkg 源码/下载缓存
GKD350HUltra/dist_glibc234        当前 GKD staging 和已验证核心
H700/dist_lowglibc                H700 34xxSP stage 输出
```

游戏、封面、存档目录同样不得由构建/清理脚本覆盖：

```text
games/
covers/
game_covers/
saves/
```

## 推荐工作流

仅修改图片、字体、声音或配置时，不编译、不压缩：

```powershell
.\GKD350HUltra\build_package.ps1 -Mode Fast -Output Stage
```

修改前端 C++ 或 ONS 时，只增量构建前端与 ONS，继续复用 KRKR：

```powershell
.\GKD350HUltra\build_package.ps1 -Mode Incremental -BuildTargets Frontend,ONS -Output Stage
```

用户明确批准编译后，KRKR2 小范围源码修改默认复用现有 CMake 树并保持单线程低优先级：

```powershell
.\GKD350HUltra\build_krkr2.ps1 -Mode FastBuild -Jobs 1 -SafeCpuSet 0 -ConfirmHeavyBuild
```

构建成功后只原子部署 KRKR2，不替换前端、ONS、KRKRSDL2、配置或用户数据：

```powershell
.\GKD350HUltra\deploy_krkr2.ps1
```

H700 34xxSP 包装（复用已编好的低 glibc 核心）：

```powershell
.\H700\build_package.ps1 -Output Stage -Version 0.01
```

只有工具链/ABI 变化、缓存损坏或重大目录重构，并且用户主动要求时，才使用 `-Mode Full`。日常不要传 `-Clean`，也不要删除上述缓存目录。

需要发行压缩包时显式传 `-Output Zip`、`Tar` 或 `Both`。压缩包会保留空的游戏/封面/存档目录结构，但不会打入本机私人内容。

每次成功运行 PowerShell 构建或打包脚本后，会刷新被 Git 忽略的 `build/gkd350h-glibc234/build_checkpoint.json`，记录源码状态、缓存可用性和当前三个可执行产物哈希。

