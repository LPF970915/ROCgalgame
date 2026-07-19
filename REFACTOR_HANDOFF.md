# ROCgalgame 前端架构重构交接文档

状态：可交接实施

编写日期：2026-07-18

参考项目：`D:\Works\ROCreader`

目标项目：`D:\Works\ROCgalgame`

首要目标设备：GKD350H Ultra / ROCKNIX / 1600x1440 / aarch64

## 1. 文档目的

本文档定义 ROCgalgame 前端的完整重构目标、模块边界、实施顺序、保护项和验收指标，供另一个开发窗口直接接手执行。

本次目标不是让 ROCgalgame 在视觉上“模仿”ROCreader，而是让二者拥有同一套前端框架、运行时行为、UI 组合方式、输入模型、系统控制生命周期和平台适配边界。

两个应用正确的产品分工应当是：

- ROCreader 管理图书库，并根据文件格式将内容交给 PDF、EPUB、TXT、ZIP/图片等阅读模块。
- ROCgalgame 管理游戏库，并根据游戏类型将内容交给外部 ONS 或 KRKR 核心进程。
- 在“打开具体内容”以前，二者的前端框架原则上应保持同构；确有产品差异时必须明确记录原因。

音量、亮度、休眠、按键校准、公共菜单、状态栏和公共输入不应继续作为 ROCgalgame 独立重写的功能。此次重构要从架构上消除两套实现继续漂移的可能。

## 2. 参考基线和事实来源

以 ROCreader 当前稳定实现作为公共前端的行为基准，不使用旧截图、历史片段或此前 ROCgalgame 的临时实现作为标准。

首要参考文档：

- `D:\Works\ROCreader\ARCHITECTURE.md`
- `D:\Works\ROCreader\REFACTOR_BASELINE.md`

首要公共模块参考：

- `D:\Works\ROCreader\src\app_bootstrap.*`
- `D:\Works\ROCreader\src\app_composition.*`
- `D:\Works\ROCreader\src\app_context.*`
- `D:\Works\ROCreader\src\app_shell.*`
- `D:\Works\ROCreader\src\app_services.*`
- `D:\Works\ROCreader\src\app_runtime.*`
- `D:\Works\ROCreader\src\scene_manager.*`
- `D:\Works\ROCreader\src\input_manager.*`
- `D:\Works\ROCreader\src\system_controls.*`
- `D:\Works\ROCreader\src\lid_power_control.*`
- `D:\Works\ROCreader\src\system_settings_runtime.*`
- `D:\Works\ROCreader\src\settings_runtime.*`
- `D:\Works\ROCreader\src\settings_panel_router.*`
- `D:\Works\ROCreader\src\menu_scene.*`
- `D:\Works\ROCreader\src\shelf_scene.*`
- `D:\Works\ROCreader\src\volume_overlay.*`
- `D:\Works\ROCreader\src\status_bar_runtime.*`

ROCgalgame 领域逻辑参考：

- `CURRENT_PORT_STATUS.md`
- `KRKR_GKD350HUltra_PORTING_PLAN.md`
- `README.md`
- `src/game_library.*`
- `src/core_launcher.*`
- `D:\Works\Tyranor\OnscripterYuri` 中的 ONS 修改
- `D:\Works\Tyranor\krkrsdl2` 中的 KRKR 修改

如果 ROCreader 公共模块和 ROCgalgame 自行实现的同类功能存在冲突，应保留 ROCreader 的公共行为，只适配 ROCgalgame 的领域数据、文案和回调。

实施前必须记录实际采用的 ROCreader 分支和提交号，避免重构过程中参考基线发生变化。

## 3. 当前协作和工作区约束

交接时的已知状态：

- `ROCgalgame.sh` 存在其他任务留下的未提交修改，不得回退。
- KRKR 核心正由另一个窗口修改。未经该任务明确交接，不得覆盖、重建或部署 KRKR 核心。
- 不得为了获得“干净构建”而删除被忽略的本地构建缓存。
- 前端架构修改和 ONS/KRKR 引擎行为修改必须分开提交。
- 每个重构阶段应保持可构建、可验证、可回退。

必须保护的本地目录：

```text
build/gkd350h/krkrsdl2
build/gkd350h/obj
GKD350HUltra/sysroot_device
GKD350HUltra/dist_lowglibc
games/
covers/
game_covers/
saves/
cores/
```

必须保护的真机目录：

```text
/storage/roms/ports/ROCgalgame/cores/
/storage/roms/ports/ROCgalgame/games/
/storage/roms/ports/ROCgalgame/covers/
/storage/roms/ports/ROCgalgame/game_covers/
/storage/roms/ports/ROCgalgame/saves/
/storage/roms/ports/ROCgalgame/native_config.ini
/storage/roms/ports/ROCgalgame/native_keymap.ini
```

普通前端部署只能替换：

```text
/storage/roms/ports/ROCgalgame/rocgalgame_sdl
/storage/roms/ports/ROCgalgame.sh
```

只有明确修改 UI、字体或声音资源时才同步相应资源目录。真机替换必须经过备份、上传临时文件、SHA-256 校验和原子重命名。

## 4. 当前架构诊断

### 4.1 单体前端协调器

当前 `src/app.cpp` 同时承担：

- 全局应用状态；
- SDL 初始化和销毁；
- 输入路由和设备打开；
- 书架导航和绘制；
- 菜单路由和面板绘制；
- 音量和亮度交互；
- 自动休眠和显示器电源命令；
- 按键校准；
- 贡献者、联系、更新面板；
- 配置持久化；
- 游戏启动状态和前端恢复。

这使得公共行为无法与 ROCreader 保持同步，并导致设备修复、UI 修改和领域逻辑相互耦合。

### 4.2 输入实现被大幅简化

ROCgalgame 的 `src/input.*` 是一套缩减实现，缺少 ROCreader 已具备的完整能力：

- 输入和屏幕机型配置；
- Power、L2/R2、Start、Select 等完整按键；
- SDL controller、joystick、axis、hat 和 Linux evdev/ABS 的统一模型；
- 长按、重复、按下和释放的完整状态；
- 完整按键校准元数据；
- 输入设备刷新和唤醒后重连；
- 电源键抑制和唤醒去抖。

当前闲置计时还将 SDL 事件观察与 evdev 轮询分开。一个 evdev 输入可能已经改变应用状态，却没有可靠地进入屏幕电源状态机。

### 4.3 系统控制上层逻辑重复实现

`src/system_controls.*` 的底层代码与 ROCreader 接近，但 ROCgalgame 没有使用 ROCreader 的以下上层结构：

- `VolumeController`
- `AppUiState`
- 统一实体音量键入口
- System Settings 回调模型
- 音量结果到 overlay、配置和 SFX 的统一同步

已经观察到的直接后果：

- 前端提示音启动时被固定到最大音量；
- 菜单和实体音量键没有持续同步提示音音量；
- 输入源去重和系统音量写入没有走 ROCreader 的稳定路径；
- 真机连续实体音量输入曾触发 DRM 黑屏、音量归零或提示音消失。

亮度同样采用直接调用方式。即使尚未稳定复现故障，也必须按同类风险处理。

### 4.4 休眠和唤醒状态机不完整

ROCgalgame 目前主要依赖一个 `screen_off` 布尔值和直接 `swaymsg` 调用。

ROCreader 已经区分：

- 正常唤醒状态；
- 自动休眠状态；
- 手动熄屏状态；
- 不同机型的电源控制策略；
- GKD 电源键与 ROCKNIX 的竞态；
- 唤醒输入消费；
- 输入设备和帧时钟重同步。

因此，ROCgalgame 菜单中能修改休眠间隔并不等于休眠已经正确接入。

### 4.5 核心启动协议重复

当前同时保留两条核心启动路径：

1. 前端直接 `fork/exec` 并等待 ONS/KRKR。
2. shell 通过 `launch_request.ini` 和退出码 `42` 启动核心。

两条路径的参数和环境变量可能继续漂移。重构完成后必须只保留一条权威启动管线。

### 4.6 配置和 UI 资源所有权不足

ROCgalgame 在许多按键操作中直接写入完整配置文件；ROCreader 使用 store、dirty 状态和受控刷新点。

ROCgalgame 还缺少 ROCreader 完整的：

- 文本纹理缓存；
- 纹理注册表；
- 公共 overlay runtime；
- UI 资源加载器；
- 屏幕布局和机型配置组合。

## 5. 目标架构

目标结构应对齐 ROCreader 的层次和文件命名。公共模块尽量保持相同文件名，使后续差异对比能够直接反映有意的产品差异。

建议目标目录：

```text
src/
  main.cpp

  app_loop.*
  app_bootstrap.*
  app_composition.*
  app_context.*
  app_environment.h
  app_services.*
  app_runtime.*
  app_shell.*
  app_layout.*
  app_config_bridge.*
  app_stores.*
  scene_manager.*

  screen_profile.*
  input_manager.*
  system_controls.*
  lid_power_control.*
  system_status.*
  runtime_log.*

  menu_scene.*
  menu_runtime.*
  settings_runtime.*
  settings_panel_router.*
  system_settings_runtime.*
  system_controls_panel.*
  key_guide_panel.*
  key_calibration_runtime.*
  key_calibration_panel.*
  contributor_avatar_runtime.*
  avatar_panel.*
  contact_panel.*
  version_update_runtime.*
  update_panel.*
  exit_panel.*
  game_settings_runtime.*
  game_settings_panel.*

  shelf_scene.*
  shelf_runtime.*
  game_library_service.*
  game_scanner.*
  cover_service.*
  cover_cache_runtime.*

  game_core_adapter.*
  game_core_registry.*
  game_launch_service.*
  core_process_runner.*
  ons_core_adapter.*
  krkr_core_adapter.*

  ui_assets.*
  ui_assets_loader.*
  ui_text_cache.*
  texture_registry.*
  status_bar_runtime.*
  volume_overlay.*
  audio_runtime.*
```

目标运行流程：

```text
main
  -> app bootstrap
  -> app context/composition
  -> app shell + scene manager
       -> shelf scene
       -> settings scene
  -> game launch service
       -> core registry
            -> ONS adapter
            -> KRKR adapter
       -> process runner
  -> 核心退出后恢复前端场景
```

`app_loop.cpp` 只负责组装依赖和路由场景结果，不允许继续吸收面板行为、平台命令、游戏扫描或核心专用参数。

## 6. 公共层和应用专用层边界

### 6.1 必须与 ROCreader 保持一致

以下内容属于公共前端，应从 ROCreader 迁移，不在 ROCgalgame 中重新设计：

- 进程、SDL 启动和销毁顺序；
- app context、shell、帧时序、事件泵和场景路由；
- 屏幕配置和输入配置选择；
- GKD350H Ultra、H700、Trimui Brick、RGDS 的平台抽象；
- 输入状态、重复、长按、校准和设备刷新；
- 菜单打开关闭、焦点、面板激活和动画；
- 系统设置布局和交互；
- 音量 overlay 的位置、时长、数值换算和提示音行为；
- 亮度控制行为；
- 休眠、熄屏和唤醒状态机；
- 时间、电量、贡献者头像状态 UI；
- 按键说明、按键校准、贡献者、联系、更新和退出面板；
- UI 资源、字体、文本缓存、纹理缓存、缩放和颜色；
- 配置 dirty 状态和受控持久化；
- 公共本地化框架。

### 6.2 必须保留为 ROCgalgame 专用

- ONS、KRKR、收藏、历史书架分类；
- 游戏检测和 `game.ini` 解析；
- 游戏封面和存档路径解析；
- ONS/KRKR 启动校验、参数和环境；
- 单游戏启动覆盖项；
- 画面比例、滤镜、虚拟鼠标、速度和加速度设置；
- 子进程日志和错误呈现；
- 启动核心前释放前端、核心退出后恢复前端。

### 6.3 概念对应关系

| ROCreader | ROCgalgame |
| --- | --- |
| `BookItem` | `GameEntry` 或公共 ShelfItem 视图 |
| `BookLibraryService` | `GameLibraryService` |
| `ReaderLaunchService` | `GameLaunchService` |
| `IReaderModule` 注册表 | `IGameCoreAdapter` 注册表 |
| PDF/EPUB/TXT/ZIP 模块 | ONS/KRKR adapter |
| TXT Settings runtime | Game Settings runtime |
| Reader Scene | 外部核心生命周期状态 |

ONS 和 KRKR 不应承担属于前端的 UI、系统设置和设备行为。前端也不应知道 adapter contract 以外的引擎内部细节。

## 7. 强制行为契约

### 7.1 输入契约

- 将 ROCreader `InputManager` 和 GKD profile 作为一个整体迁移。
- GKD 使用已经在 ROCreader 验证的 evdev 映射和 SDL 重复事件抑制。
- 音量键不得使用 `EVIOCGRAB`。
- 不得停止、替换或接管 ROCKNIX `input_sense`。
- 所有输入源必须汇聚到同一套 Button 状态机。
- 任何有效 SDL 或 evdev 用户输入都必须重置闲置计时。
- 校准记录 source、code、direction、device name 和 pressed 状态，并使用 ROCreader 兼容格式。
- 显示器唤醒和核心退出后必须能够重新打开输入设备。

### 7.2 音量契约

- 实体音量键和 System Settings 加减控件必须进入同一个 `VolumeController`。
- 每次成功调整只生成一次权威百分比结果。
- overlay、配置值、系统控制和 SFX 音量都由该结果派生。
- 必须先更新 SFX 音量，再播放 Change 提示音。
- 输入处理器不得调用 `/usr/bin/volume`、`pactl`、显示电源或其他外部 helper。
- 不得引入输入独占。
- 启动游戏并关闭前端后，不得残留前端音量处理器；ONS/KRKR 中实体音量键继续由 ROCKNIX 正常管理。

### 7.3 亮度契约

- 启动同步、菜单调整、持久化和 UI 显示必须统一经过 ROCreader 的 System Settings service/callback 模型。
- 菜单和场景不得直接写 sysfs、`/dev/disp` 或执行 shell 命令。
- 硬件写入失败时刷新真实状态并报告不可用，不能让 UI 假报成功。
- 调整亮度不得执行屏幕电源命令，也不得重建 SDL。

### 7.4 休眠和屏幕电源契约

- 迁移 `LidPowerController` 和 ROCreader 已验证的屏幕状态机。
- 自动休眠和手动熄屏必须是不同状态。
- GKD 开关屏使用幂等的 compositor 命令，并保持 ROCreader 的竞态处理。
- 唤醒输入必须被消费，不能同时激活当前 UI 项目。
- 唤醒后重置帧时钟和输入状态，并按 profile 要求重同步设备。
- 屏幕唤醒时，方向、面键、肩键和音量输入都必须重置自动休眠计时。

### 7.5 配置契约

- 引入与 ROCreader 一致的 `ConfigStore` dirty tracking 和延迟刷新。
- 保留现有 `native_config.ini` 键和迁移行为。
- 尽可能保留未知键；确需移除时必须在实施前列出。
- 保留 `native_keymap.ini` 和现有用户校准结果。
- 前端销毁前，将本次核心设置固化为不可变 `CoreLaunchSpec`。
- 按键重复期间不得每帧同步重写整个配置文件。

### 7.6 UI 契约

- 当前 screen profile 下，公共书架 chrome、菜单 chrome、字体、分割线、焦点、动画、overlay 和面板间距与 ROCreader 一致。
- 使用 ROCreader layout metrics，不得继续向面板复制固定坐标。
- Game Settings 占据 ROCreader 应用专用设置的位置，并遵循 System Settings 的面板规范。
- 空书架分类只呈现空状态，不显示旧的画面中下方瞬时弹窗。
- 文本和纹理必须通过共享缓存并具有明确所有者。

### 7.7 核心启动契约

- `IGameCoreAdapter` 负责校验游戏并构建 `CoreLaunchSpec`。
- `CoreProcessRunner` 独占 fork/exec、日志重定向、等待状态和平台进程细节。
- `GameLaunchService` 负责选择 adapter、保存前端状态、触发前端销毁、运行核心并返回类型化结果。
- ONS 和 KRKR 参数只能分别存在于对应 adapter。
- shell 只负责运行环境和启动前端。直接启动路径通过恢复测试后，删除旧退出码 `42` 协议。
- 启动核心前释放前端 window、renderer、audio device 和 controller/joystick handles。
- 核心退出后恢复分类、页码和焦点，不改变核心二进制和游戏数据。

## 8. ROCreader 模块迁移映射

| ROCreader 源模块 | ROCgalgame 目标 | 迁移规则 |
| --- | --- | --- |
| `app_bootstrap.*` | 同名 | 原样迁移，只适配应用标题和运行路径 |
| `app_composition.*` | 同名 | 原样迁移 |
| `app_context.*` | 同名 | 通过 capability 去除 reader 专用字段，不做临时分叉 |
| `app_shell.*` | 同名 | 保留事件泵和帧行为 |
| `app_services.*` | 同名 | 保留公共服务，替换游戏 store 和 launch service |
| `app_runtime.*` | 同名 | 保留音量和菜单公共运行时 |
| `app_layout.*` | 同名 | 迁移全部 profile 布局 |
| `scene_manager.*` | 同名 | Reader scene 替换为外部核心转换状态或等价生命周期 |
| `screen_profile.*` | 同名 | 迁移全部 profile，先验收 GKD |
| `input_manager.*` | 同名 | 作为整体迁移 |
| `system_controls.*` | 同名 | ROCreader 作为权威版本 |
| `lid_power_control.*` | 同名 | 作为整体迁移 |
| `system_settings_runtime.*` | 同名 | 保留回调和布局 |
| `settings_runtime.*` | 同名 | TXT 专用入口替换成 Game Settings |
| `settings_panel_router.*` | 同名 | 路由公共面板和 Game Settings |
| `menu_scene.*` | 同名 | 保留公共行为，注入 ROCgalgame 菜单列表 |
| `shelf_runtime.*` | 同名 | 复用布局导航，通过 callback 适配游戏项 |
| `shelf_scene.*` | 同名 | 桥接 `GameEntry` 服务 |
| `ui_assets*` | 同名 | 迁移 loader/cache 所有权，保留游戏封面资源 |
| `ui_text_cache.*` | 同名 | 原样迁移 |
| `texture_registry.*` | 同名 | 原样迁移 |
| `status_bar_runtime.*` | 同名 | 原样迁移 |
| `volume_overlay.*` | 同名 | 原样迁移 |
| `audio_runtime.*` | 同名 | 保留 ROCreader backend/fallback 行为 |
| `key_calibration_runtime.*` | 同名 | 原样迁移 |
| `key_guide_panel.*` | 同名 | 保留布局，只替换游戏操作文案 |
| `contributor_avatar_runtime.*` | 同名 | 原样迁移 |
| `contact_panel.*` | 同名 | 原样迁移 |
| `version_update_runtime.*` | 同名 | 迁移 UI/状态，保持 manifest 未配置 |
| `txt_settings_runtime.*` | `game_settings_runtime.*` | 使用等价 state/callback 分层 |
| `reader_launch_service.*` | `game_launch_service.*` | 保留依赖注入方式 |
| `reader_manager.*` | `game_core_registry.*` | 注册外部核心 adapter |

公共文件确需分叉时，必须在代码注释或架构文档中说明产品原因，不能静默复制后独立演化。

## 9. 分阶段实施计划

每一阶段完成后必须构建和验证，不能一次大改后统一标记完成。

### Stage 0：冻结现场和建立基线

实施内容：

- 记录 Git 状态和前端、launcher、ONS、KRKR 哈希。
- 截取 GKD 书架、菜单列表、各面板和 overlay 基线截图。
- 备份当前 config、keymap、游戏、封面和存档。
- 记录音量、亮度、休眠、ONS、KRKR、核心返回的真机基线。
- 根据本文建立可勾选的阶段清单。

验收：

- 基线记录包含源码状态、构建标识和真机二进制哈希。
- 未改变用户数据或核心二进制。
- 现有 `make test` 通过。

### Stage 1：构建图和应用基础层

实施内容：

- 像 ROCreader 一样将 Makefile 源码分为 app、platform/core、shelf、menu、UI、status 和 game domain。
- 迁移 `app_bootstrap`、`app_context`、`app_composition`、`app_shell`、`app_layout`、`screen_profile` 和 `scene_manager`。
- 将 SDL/window/renderer 生命周期移出单体 AppState。
- 本阶段不改变可见行为。

验收：

- `main.cpp` 只调用 `RunApp()`。
- SDL 初始化和销毁只有一个 owner。
- Windows preview 和 GKD 仍能进入书架、打开菜单。
- 未修改核心代码和核心启动参数。

### Stage 2：Stores、运行路径和公共 UI 基础设施

实施内容：

- 迁移运行路径解析和 store 初始化模式。
- 使用 dirty state 和受控 flush 替换按键处理中的直接配置写入。
- 迁移 UI asset loader、text cache、texture registry、status bar、volume overlay 和 ROCreader audio runtime。
- 为当前配置格式建立显式加载、保存和迁移测试。

验收：

- 旧配置加载后的有效值完全一致。
- 长按按键不会每个渲染帧都写配置。
- 文本和纹理 cache 在关闭时有明确释放路径。
- GKD 状态栏和音量 overlay 与 ROCreader 布局一致。

### Stage 3：输入 profile 对齐

实施内容：

- 用 ROCreader `input_manager.*` 架构替换 `src/input.*`。
- 迁移全部 screen/input profiles，首先启用和验收 GKD。
- 原样迁移 GKD evdev 映射和 SDL 重复事件抑制。
- 迁移校准序列化和设备刷新行为。
- 统一事件观察和 idle activity。

验收：

- GKD 的方向、摇杆、A/B/X/Y、肩键、Menu/Start/Select、音量和 Power 每次物理动作只产生一次逻辑动作。
- 支持重复的按键与 ROCreader 重复节奏一致。
- 校准结果重启后可加载。
- 不存在 `EVIOCGRAB`，`input_sense` 保持运行。

### Stage 4：系统控制和电源生命周期

实施内容：

- 迁移 `VolumeController`、`AppUiState`、统一音量处理和 SFX 同步。
- 迁移 System Settings callbacks 和亮度状态同步。
- 迁移 `LidPowerController` 和完整熄屏/唤醒状态机。
- 删除 scene/menu 中的直接电源命令和直接音量/亮度编排。

验收：

- 通过第 11 节全部系统控制测试。
- 实体键和菜单音量生成相同数值、overlay 和提示音。
- 亮度 UI 始终反映真实应用结果。
- 自动休眠和唤醒不会留下黑屏。

### Stage 5：菜单场景和公共面板

实施内容：

- 迁移 `MenuScene`、`settings_runtime`、panel router 和菜单动画状态。
- 迁移 System Controls、Key Guide、Key Calibration、Contributors、Contact、Version Update 和 Exit 面板。
- 应用专用菜单通过注入扩展，不硬编码进公共绘制逻辑。
- 保留本地化能力。

验收：

- GKD 公共菜单 chrome、间距和 ROCreader 截图一致。
- 面板进入/返回、焦点恢复和 Menu 键开关行为一致。
- Contributors 和 Contact 顶部没有额外空行。
- Update 在未配置状态下安全显示，不发起网络请求。

### Stage 6：Game Settings 模块

实施内容：

- 创建 `game_settings_runtime.*` 和 `game_settings_panel.*`，对应 ROCreader 的应用专用设置模块。
- 将 aspect、filter、virtual mouse、mouse speed、mouse acceleration 变成类型化 state 和 callbacks。
- 使用 System Settings 的行布局：左侧标签，右侧箭头/控件/值。
- 引擎内部值与显示文本分离。

验收：

- aspect 只显示真实支持的 `stretch`、`fill-height` 和明确保留的兼容值。
- 已移除的 fit-width 不再出现在 UI，旧配置仍可安全迁移。
- virtual mouse 使用公共 toggle 外观。
- 数值速度使用公共 minus/value/plus 交互。
- 所有值持久化并进入不可变核心启动 spec。

### Stage 7：书架场景和游戏库服务

实施内容：

- 迁移 ROCreader 的 shelf 导航、布局、动画边界。
- 拆分扫描、游戏库组装、封面解析、封面纹理 cache、shelf runtime 和 shelf scene。
- 将 `GameEntry` 适配到 shelf view contract。
- 保留 ONS、KRKR、收藏和历史分类。

验收：

- 焦点移动、翻页、分类切换、标题省略/滚动、封面缩放和动画与 ROCreader 一致。
- 空分类不出现画面中下方弹窗。
- 扫描、收藏和历史行为不回退。
- 大型游戏库不会每帧重建全部文本和封面纹理。

### Stage 8：模块化游戏核心启动

实施内容：

- 引入 `IGameCoreAdapter`、registry、launch service 和 process runner。
- 将 ONS 参数和环境构建移入 `ons_core_adapter`。
- 将 KRKR 参数和环境构建移入 `krkr_core_adapter`。
- 保留日志、存档、入口、字体、aspect、filter、鼠标和单游戏覆盖行为。
- 前端销毁和恢复保留在应用生命周期服务中。

验收：

- 单元测试分别验证 ONS 和 KRKR spec。
- 新增第三核心只需要新 adapter 和注册，不需要修改菜单、书架和进程代码。
- 核心退出恢复原分类、页码和焦点。
- 启动失败返回类型化、本地化错误和日志路径。

### Stage 9：删除旧路径和单体逻辑

实施内容：

- Stage 8 真机通过后删除 shell 退出码 `42` 启动协议。
- 删除旧 `launch_request.ini` 前端代码和无效配置字段。
- 从 `app.cpp` 删除已迁移的绘制、输入和系统控制函数。
- 剩余协调器改为 `app_loop.cpp`，只保留组装和路由。
- 记录全部有意保留的 ROCreader 差异。

验收：

- 只剩一条核心启动管线。
- `app_loop.cpp` 没有直接面板绘制和系统命令。
- adapter 以外不存在核心专用参数构建。
- 不再存在旧 `src/input.*` 或第二套音量/休眠实现。

### Stage 10：多机型扩展准备

实施内容：

- 保留 ROCreader 针对 H700、Trimui Brick、GKD350H Ultra 和 RGDS 的 screen/input/platform 抽象。
- 应用行为依赖 capability，不散布目标机型宏。
- 只有存在真机验证负责人时才新增对应打包入口。
- 编译成功不能作为设备支持声明。

验收：

- GKD 继续完整通过验收。
- Windows preview 使用 desktop profile，不依赖设备 sysfs/ALSA。
- 其他 profile 能编译，或通过单一、明确的 gate 禁用并记录原因。
- 新设备可提供 screen、input、storage、system control 和 power capabilities，而无需修改 shelf/menu/game domain。

### Stage 11：清理、文档和发布候选

实施内容：

- 为重构结果新增 `ARCHITECTURE.md`。
- 更新 README 和构建/打包源码分组。
- 执行自动化、Windows preview 和完整 GKD 验收。
- 记录最终模块对齐情况和有意差异。
- 创建真机原子备份，只部署获准的前端文件。

验收：

- 通过第 11、12 节全部标准。
- 不包含无关用户数据或核心修改。
- 记录最终哈希、日志、截图和测试结果。

## 10. 自动化测试要求

保留 `tests/core_launch_test.cpp`，并拆分或新增以下测试：

- 配置 round trip 和旧键迁移；
- screen/input profile 解析；
- 输入映射解析和重复输入源处理；
- repeat、long press 时序；
- 校准序列化；
- auto-sleep 间隔换算；
- 音量 percent/level 换算；
- Game Settings 状态转换；
- 书架过滤、分页、收藏和历史；
- ONS launch spec；
- KRKR launch spec；
- 缺失核心、无效游戏、正常退出、非零退出、信号退出；
- 如引入前端恢复状态序列化，覆盖其 round trip。

测试不能要求真实 ALSA、evdev、Sway、游戏核心或网络。系统和进程层应支持注入 callback/fake，从而可以在 Windows 或 host Linux 测试状态机。

最低自动化门槛：

```text
make test                         PASS
Windows frontend build           PASS
GKD incremental frontend build   PASS
runtime dependency validation    PASS
package user-payload audit       PASS
```

## 11. GKD 真机验收矩阵

设备：`root@192.168.31.13`

每个涉及设备行为的阶段完成后执行相关子集；最终候选执行全部项目。

### 11.1 启动和书架

- 冷启动直接进入书架，没有空白帧和重复 driver retry。
- 时间、电量、头像状态显示并正确对齐。
- ONS/KRKR/收藏/历史导航正常。
- 空分类没有画面中下方弹窗。
- 书架闲置运行至少 30 分钟，无输入重复递增、日志洪泛、音频设备反复打开或 renderer reset。

### 11.2 实体音量键

- Volume+ 单击：恰好改变一级、更新一次 overlay、播放一次 Change SFX。
- Volume- 单击：反向行为一致。
- 长按重复节奏与 ROCreader 一致，并正确限制在 0/100。
- 连续按 Volume+ 至少 30 次。
- 连续按 Volume- 至少 30 次。
- 全程无黑闪、持续黑屏、由前端引起的 DRM disable/enable、音量归零、提示音消失或从零重计。
- overlay 数值和可听提示音音量保持单调变化。

### 11.3 菜单音量

- 在 System Settings 中使用十字键和加减控件调整。
- 每个成功步骤更新面板数值和 ROCreader 约定位置的音量数字提示。
- Change SFX 在每一步都使用新的音量。
- 实体键和菜单交替调整时是一条连续数值，不出现两套 cache。
- 重启后恢复预期值。

### 11.4 游戏内音量隔离

- 分别启动一个 ONS 和一个 KRKR 游戏。
- 两个核心内实体音量键继续正常调节设备音量。
- 核心运行时不出现前端 overlay 和前端 SFX。
- 前端销毁后不残留 evdev grab。
- 核心退出后前端音量 UI 不跳变、不归零。

### 11.5 屏幕亮度

- 菜单中从最低调至最高，再调回最低。
- 每一步真实亮度只变化一次。
- 离开并重新进入面板后，UI 与实际应用级别一致。
- 重启后行为符合持久化策略。
- 无黑闪、显示器 power toggle、SDL 重建或音量副作用。

### 11.6 休眠和唤醒

- 至少验证最短间隔和一个分钟级间隔。
- SDL 和 evdev 的方向、面键、音量输入都会重置 idle time。
- 只在达到配置闲置时间后自动熄屏。
- 十字键唤醒后不移动焦点、不激活菜单项。
- Power 手动开关屏遵循 ROCreader GKD 时序，不与 ROCKNIX 竞态。
- 连续执行 20 次休眠/唤醒，无持续黑屏和输入丢失。
- 唤醒后音量、菜单、书架和核心启动继续正常。

### 11.7 公共菜单对齐

- Key Guide 标题、分割线、行和右侧说明与 ROCreader 一致。
- Key Calibration 完成真实捕获、保存、重载和重置。
- System Settings 行布局和焦点行为一致。
- Contributors 顶部没有额外空行。
- Contact 的位置和资源与 ROCreader 一致。
- Version Update 位于 Contact 下方，安全显示未配置状态。
- Exit、Menu 和 B 的行为与 ROCreader 一致。

### 11.8 Game Settings

- 整体布局与 System Settings 一致。
- aspect、filter、virtual mouse、speed、acceleration 都可通过手柄完整操作。
- ONS 收到选中的值。
- KRKR 收到选中的值，同时不覆盖另一窗口维护的 KRKR 二进制。
- `fill-height` 继续只裁切左右场景，并将 ONS 文本约束在安全可见范围。

### 11.9 核心生命周期

- 已知 ONS 游戏启动和退出连续执行 10 次。
- 已知 KRKR 游戏启动和退出连续执行 10 次，允许另行记录 KRKR 引擎自身已知问题。
- 核心运行期间没有前端黑色窗口覆盖核心。
- 每次退出都恢复分类、页码、焦点、状态栏、输入、音频和休眠计时。
- 缺失或无效核心返回书架并显示可操作错误，不循环启动。

## 12. 可量化完成定义

只有以下项目全部满足，才能宣布重构完成。

### 12.1 架构指标

- `main.cpp` 是纯 `RunApp()` 入口。
- SDL 生命周期只有一个 owner。
- screen/profile 选择只有一个 owner。
- 输入映射和状态只有一个 owner。
- 音量只有一个 controller 和一条结果到 UI/SFX 的同步路径。
- 亮度只有一条 service callback 路径。
- 屏幕电源只有一个 controller 和显式状态机。
- 菜单输入和绘制使用 scene/runtime/panel 模块。
- Game Settings 与公共 System Settings 隔离。
- ONS/KRKR 使用 adapter 和唯一 process runner。
- `app_loop.cpp` 不包含直接 ALSA/sysfs/Sway 命令、面板绘制、游戏扫描实现或核心专用参数。
- 所有公共模块与 ROCreader 的差异均有说明。

### 12.2 UI 指标

- GKD 公共书架、菜单和状态 UI 使用与 ROCreader 相同的 assets、fonts、layout metrics、colors、separators 和 focus geometry。
- 截图自动或人工叠加对比中，共享 chrome 没有无法解释的差异；应用专用文案和项目不计入。
- 1600x1440 和 Windows preview 尺寸下所有文字都在容器内。

### 12.3 稳定性指标

- 实体音量键两个方向各连续 30 次：黑帧 0 次。
- 休眠/唤醒 20 次：持续黑屏 0 次，输入丢失 0 次。
- ONS 启动/返回 10 次：前端恢复失败 0 次。
- KRKR 启动/返回 10 次：除已记录核心问题外，前端生命周期失败 0 次。
- 音量和亮度测试中，不出现由前端产生的 DRM reset。
- 获准部署范围以外的设备和用户数据变更为 0。

### 12.4 扩展性指标

- 新增第三核心只需一个 adapter 和 registry entry。
- 新增 screen/input profile 不需要修改 shelf、menu 和 game domain。
- 新增公共菜单面板通过 state、callbacks、draw 和 router 注册完成，不修改主循环。
- 新增游戏设置只修改 Game Settings 和 launch-spec mapping，不修改公共 System Settings。

### 12.5 验证记录

- 保存自动化测试输出。
- 记录 Windows preview 结果。
- 保存 GKD 日志和二进制哈希。
- 保存重构前后截图。
- 记录备份路径和部署文件 checksum。

## 13. 重构护栏

- 先迁移已经验证的行为，再重新设计接口。
- 每阶段保持可构建和可回退。
- 前端抽取期间不顺便修复无关 ONS/KRKR 引擎问题。
- platform 差异不能隐藏在 scene 或 panel 中。
- 不得再创建一个包含全部子系统的新全局 AppState。
- 不得将 ROCreader 公共模块换名后复制成 ROCgalgame 私有实现。
- 未经真机验证不得宣称支持新机型。
- 移动代码和移除旧配置兼容不能放在同一步。
- 结构化配置必须使用结构化解析，不能使用临时字符串替换。
- 未明确 manifest、发版包网址和发布策略前，Update UI 不得执行真实更新。

## 14. 建议提交顺序

建议每个已验证边界单独提交：

```text
refactor: add frontend bootstrap and app shell
refactor: port shared layout and profile runtime
refactor: adopt ROCreader stores and UI caches
refactor: port unified input manager
refactor: port system controls and power lifecycle
refactor: adopt shared menu scene and panels
refactor: isolate game settings runtime
refactor: split game shelf services and scene
refactor: introduce game core adapters
refactor: remove legacy launch request path
test: add frontend architecture and device smoke coverage
docs: record ROCgalgame architecture and parity status
```

真机验证前不要将全部阶段压成一个提交。设备回归必须可以 bisect 到具体架构边界。

## 15. 交接检查表

实施开始前：

- [ ] 阅读 ROCreader `ARCHITECTURE.md` 和 `REFACTOR_BASELINE.md`。
- [ ] 确认作为事实来源的 ROCreader 分支和提交。
- [ ] 记录 ROCgalgame 工作区，不回退 `ROCgalgame.sh` 修改。
- [ ] 确认另一窗口对 KRKR 核心的所有权和交接状态。
- [ ] 保存基线二进制、哈希、配置、日志和截图。
- [ ] 确认设备 SSH 和备份目录。

宣布完成前：

- [ ] 每阶段验收全部完成，或明确记录延期项及原因。
- [ ] 自动化测试和 Windows preview 通过。
- [ ] 完整 GKD 真机矩阵通过。
- [ ] ONS/KRKR 数据和二进制保持不变，除非对应 owner 明确更新。
- [ ] 新增最终 `ARCHITECTURE.md` 并更新 README 和构建文档。
- [ ] 记录所有与 ROCreader 的有意差异。

## 16. 最终产品规则

ROCreader 是公共前端体验的权威基准。ROCgalgame 可以在游戏启动器与阅读器存在本质差异的位置采用专用实现，但不得独立重写公共前端行为。

未来遇到菜单布局、输入、音量、亮度、休眠、校准、状态 UI、公共资源或平台适配问题时，应先判断它是否属于 ROCreader 对齐的公共前端层。默认只有游戏库、Game Settings 和核心执行逻辑应采用 ROCgalgame 专用方案。
