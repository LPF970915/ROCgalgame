# ROCgalgame Refactor Progress

更新日期：2026-07-19

## 参考基线

- ROCgalgame 分支：`main`
- ROCgalgame 提交：`af76de10207af4aa83b0643cee3d0870b19ec440`
- ROCreader 分支：`main`
- ROCreader 提交：`2c1fdd8c2fe3d263f673d1683269fcba3f1c4d19`
- ROCreader 工作区例外：`src/app_loop.cpp` 有未提交的 Start+Select 长按退出修改，SHA-256 为 `F286032723A1C128D3DEA694EDE0BCBEBF84239A683A1DAA8651D21A49F50CFD`。公共模块迁移以提交版本为准，该差异不静默带入。
- KRKR 参考提交：`ecfd1979d202088540f606098603d159d267aca4`

## 受保护现场

- `ROCgalgame.sh` 在开始重构前已有一行未提交修改：设置 `ROCREADER_SYSTEM_VOLUME_LEVELS=20`。不得回退。
- `REFACTOR_HANDOFF.md` 在开始重构前为未跟踪交接文档。
- 不清理或重建 `build/gkd350h/krkrsdl2`、`build/gkd350h/obj`、`GKD350HUltra/sysroot_device` 或 `GKD350HUltra/dist_lowglibc`。
- 不修改、部署或替换 KRKR 核心。
- 不写入 `games/`、`covers/`、`game_covers/`、`saves/` 或 `cores/`。

## 本地构建产物基线

| 产物 | SHA-256 |
| --- | --- |
| `build/gkd350h/rocgalgame_sdl` | `175F52332AA1B45759322A56C173C3E16C7CB89BD91C4E541CCA583737CDCAF1` |
| `build/gkd350h/onsyuri/onsyuri` | `8C69889F8C93CDE3D14C9462D8997050D4FBCE2E36D75BA990203DC89F46FCB5` |
| `build/gkd350h/krkrsdl2/krkrsdl2` | `57EE663331BEA3FF894798DE5492EE13819BAF33664D47FBD42C487E0FC3F294` |

`build/gkd350h/build_checkpoint.json` 中记录的前端哈希为 `E63AAE603903E148C4147DC2BBC51FA9A3923842AED24ECF21F14FA27269452F`，早于当前前端产物，不能作为重构起始哈希。

## 自动化基线

- MSYS2 UCRT64：`make test` 通过。
- Ubuntu WSL2：`make OBJDIR=build/wsl-test-obj TEST_TARGET=build/core_launch_test_wsl test` 通过。
- 结果：`core launch tests passed`。
- 默认 WSL `make test` 会读取 MSYS2 生成的 `build/obj/*.d` 并报 `multiple target patterns`；跨工具链验证必须使用独立 `OBJDIR`。

## Windows 截图基线

截图保存在被 Git 忽略的 `build/refactor_baseline/windows/`，尺寸为 1600x1440。

| 截图 | SHA-256 |
| --- | --- |
| `shelf.bmp` | `5641CC0A060DDAD4564833508DDDF611521161750C1B6E633E43DB69D0538AE6` |
| `menu.bmp` | `9313D9ADAF9F17B7D65BE0C02393111BE352D346D97D2EFD5A1D892C33E2F304` |
| `system_settings.bmp` | `DFEF1D07AF9F912C83E85E3604D4A44C7E6E592FD19496E6E82866E6501C2084` |
| `game_settings.bmp` | `3C9B34B364F6536E1E27FACDCF4693CE793FF2EB5E89CBDFDB55B0BEB9F69EAC` |
| `volume_overlay.bmp` | `37D1B26753D64DB409F6E27C1F083DEC64F23C1C8003CADA71CC3F5E3F4FE62B` |

## 阶段状态

- [x] Stage 0：本地源码、产物、自动化测试和 Windows 截图基线。
- [x] Stage 0：GKD 真机配置/数据备份、截图、行为记录和设备端哈希。
- [x] Stage 1：构建图和应用基础层。
- [x] Stage 2：Stores、运行路径和公共 UI 基础设施。
- [x] Stage 3：输入 profile 对齐。
- [x] Stage 4：系统控制和电源生命周期。
- [x] Stage 5：菜单场景和公共面板。
- [x] Stage 6：Game Settings 模块。
- [x] Stage 7：书架场景和游戏库服务。
- [x] Stage 8：模块化游戏核心启动。
- [x] Stage 9：删除旧路径和单体逻辑。
- [x] Stage 10：多机型扩展准备。
- [x] Stage 11：清理、文档和 Windows 发布候选；完整 GKD 真机功能矩阵按用户要求延期为手工测试。

## GKD 真机基线与备份

- 设备：`root@192.168.31.13`，RK3576S / aarch64。
- 备份：`/storage/games-external/ROCgalgame_refactor_backups/20260718_054914`。
- 备份包含旧前端、launcher、ONS/KRKR cores、config、keymap、covers、game_covers 和 saves。
- `games/` 为 26.3 GB，只读记录了 1691 个文件的清单和目录大小；用户明确决定不复制游戏，重构部署流程不写入该目录。
- 曾启动的完整 `games/` 复制已于 2026-07-18 通过 SSH 终止，并确认没有残留 `cp`、`rsync` 或 `tar` 复制进程。可能已创建的部分备份目录保持原样，未经用户明确同意不清理。
- 旧前端真机截图保存在备份目录的 `screenshots/`，本地副本位于被忽略的 `build/refactor_baseline/gkd/`。

## Stage 1 结果

- Makefile 已按 app、platform、shelf、menu、UI、status 和 game domain 分组。
- 新增 `app_bootstrap`、`app_composition`、`app_context`、`app_environment`、`app_services`、`app_shell`、`app_layout`、`screen_profile` 和 `scene_manager`。
- 删除旧 `platform.*`；SDL、window 和 renderer 的创建/销毁只存在于 `app_bootstrap.cpp`。
- controller/joystick 句柄生命周期进入 `app_services`。
- 当前菜单、书架、核心启动参数和核心生命周期行为未重写。
- Ubuntu WSL2 全量前端构建通过。
- MSYS2 UCRT64 Windows 前端构建通过。
- GKD350H Ultra 前端增量交叉构建、ELF 依赖校验和 Stage package audit 通过。
- `core launch tests passed` 和 `app foundation tests passed` 在 Ubuntu WSL2 与 MSYS2 UCRT64 均通过。
- Windows 和 GKD 各五张回归截图在排除动态顶部状态栏后，与 Stage 0 基线逐张像素哈希完全一致。
- Stage 1 GKD 前端 SHA-256：`FA9649B4F6F44C67604D5DFEE8FAFC1543F956164DDBA9D6C4368DBE85F1A60C`。
- 真机只原子替换 `rocgalgame_sdl`；launcher、ONS、KRKR、config、keymap 和用户数据未改变。

Stage 1 之后按计划先完成配置格式兼容测试和保留未知键的 `ConfigStore`，再迁移公共 UI 基础设施。

## Stage 2 结果

- 新增 `ConfigStore`，由其拥有配置文件文档、dirty 状态和最后变更时间；主循环在 500 ms 安静期后刷新，退出或启动核心前强制刷新。
- 保留现有 `native_config.ini` 键；旧键 `auto_sleep_enabled`、`selected_avatar` 和旧 aspect 值 `fit-width` 会迁移到当前格式。
- 保存时保留未知键、注释和空行，合并重复的已知键，不再由按键重复每帧截断重写整个配置文件。
- 新增配置 round trip、旧键迁移、数值归一化、未知键保留和延迟 flush 测试。
- 新增 `ui_assets_loader`、`ui_text_cache`、`texture_registry`、`status_bar_runtime` 和 `volume_overlay`；文本纹理由有界 LRU cache 统一拥有并在字体和 renderer 关闭前释放。
- `audio_runtime` 对齐 ROCreader 的 Mixer 优先、SDL queue 回退结构；Windows 构建启用 Mixer，GKD sysroot 未提供 Mixer pkg-config 时保持 SDL queue 且不新增设备依赖。
- Ubuntu WSL2 与 MSYS2 UCRT64 均通过 `core launch tests passed`、`app foundation tests passed` 和 `config store tests passed`，完整前端构建通过。
- GKD350H Ultra 前端增量交叉构建、ELF 依赖检查和 Stage package audit 通过；Stage 目录的 `games`、`covers`、`saves`、`cache` 均为 0 个文件。
- Windows 与 GKD 各五张截图在排除动态状态栏后逐张像素一致；音量 overlay 左上 `240x80` 区域差异像素为 0。
- Stage 2 GKD 前端 SHA-256：`2E25BF46F5938B2A04CC9ED6898F50BBCCF1EFD8C5CC0B2F2ADC41510625A0FA`。
- 真机部署前端备份：`/storage/games-external/ROCgalgame_refactor_backups/20260718_054914/stage2_before_20260718_184538`。
- 真机只原子替换 `rocgalgame_sdl`；launcher、ONS、KRKR、config、keymap 和用户数据哈希未改变，完整进程表确认没有残留游戏复制任务。

下一阶段从 ROCreader `InputManager`、完整 input profile 和现有 `native_keymap.ini` 校准结果兼容测试开始；不在 Stage 3 顺带修改系统音量、亮度或休眠状态机。

## Stage 3 结果

- 以 ROCreader `InputManager` 为事实来源迁移 19 个逻辑按钮和 7 个输入 profile：Desktop、H700 默认、34xxSP、35xxH、Trimui Brick、GKD350H Ultra、RGDS；删除旧 `input.*` 单一路径。
- `AppState` 在配置加载与 screen profile 解析后统一解析 canonical input profile；状态栏和运行诊断使用解析结果，不再直接依赖原始配置字符串。
- 输入覆盖键支持 keyboard、SDL controller/joystick button、controller/joystick axis、joystick hat、Linux EV_KEY 和 EV_ABS；GKD profile 使用 evdev 并抑制重复的 SDL controller/joystick 输入源。
- 主循环接入 `BeginFrame` / SDL event / `EndFrame` 生命周期，Linux 输入在 `EndFrame` 轮询；保留 press、release、repeat、long press 和 power suppression 状态。
- 校准面板对齐 ROCreader 的 15 键顺序，包含 L2/R2/Start/Select；采样保留 source、code、direction、device name 和 pressed/released。新保存格式为 ROCreader `calibration_version=5` 的 `source.code.direction=Button`，同时继续读取旧 `Button=Source:Code[:Direction]` 格式。
- 校准文件写入进入输入模块，保留非校准注释和手工旧格式映射；新增写后再读 round trip、profile 解析、SDL button/axis/hat、旧格式、重复/长按和 GKD 重复源抑制测试。
- Ubuntu WSL2 与 MSYS2 UCRT64 均通过 `core launch tests passed`、`app foundation tests passed`、`config store tests passed` 和 `input manager tests passed`，完整前端构建通过。
- GKD350H Ultra 前端增量交叉构建、ELF 依赖检查和 Stage package audit 通过；Stage 的 `games`、`covers`、`saves`、`cache` 均为 0 个文件，ONS/KRKR 未重建。
- Windows 五张截图相对基线在 `y >= 80` 的主内容区域逐像素一致；GKD 五张截图相对真机基线在 `y >= 80` 逐像素一致。顶部差异来自动态电量/时间以及设备当前音量值。
- 真机 `/dev/uinput` 冒烟创建临时 `gkd_atom_joypad_smoke`，前端同时打开真实 `gkd_atom_joypad` 和临时设备，并记录 `LINUX_EV_KEY code=316 mapped=Menu`；测试脚本和日志随后从设备 `/tmp` 清理。
- Stage 3 GKD 前端 SHA-256：`92FC0A7982EBB158437051F0C9386CF2E61C1DC927AE9F456482D37ADA3C1EEC`。
- 真机前端回滚备份：`/storage/games-external/ROCgalgame_refactor_backups/20260718_054914/stage3_before_20260718_073742/rocgalgame_sdl`。
- 真机只原子替换 `rocgalgame_sdl`；launcher、ONS、KRKR、config 和 keymap 保持部署前哈希，完整进程表确认没有 ROCgalgame 或游戏复制残留进程。
- 本阶段未复制游戏，未写入项目或设备的 `games/`，未修改/重建/部署 KRKR，未回退 `ROCgalgame.sh` 的 `ROCREADER_SYSTEM_VOLUME_LEVELS=20`。

## Stage 4 结果

- 新增 `VolumeController` 和 `AppUiState`，实体音量键与 System Settings 菜单统一经过同一调整入口；成功写入后同步配置、音量 overlay、SFX 音量和 Change SFX。
- System Settings 使用拥有明确生命周期的 callbacks；修复 SFX callback 按引用捕获临时对象导致首次调音量崩溃的问题。
- GKD 系统音量固定使用系统 `/usr/bin/amixer`，子进程清除打包版 `LD_LIBRARY_PATH`，写入后回读 `Master`；实体键成功后延迟 250 ms 幂等重确认目标值，避免 ROCKNIX `input_sense` 的晚到写入覆盖前端结果。
- 亮度写入通过 System Settings runtime 统一持久化真实硬件结果，失败时刷新而不伪造成功状态。
- 新增 `LidPowerController` 和 `PowerLifecycleController`，区分自动休眠与手动熄屏；唤醒后刷新 SDL/evdev 输入设备、抑制 Power 释放残留并消费唤醒输入。
- `app.cpp` 已移除直接 `swaymsg`、音量和亮度编排；平台命令集中在 `lid_power_control.*` 和 `system_controls.*`。
- MSYS2 UCRT64 与 Ubuntu WSL2 最终前端完整构建和五组测试通过；WSL 使用隔离的 `build/wsl-stage4/` 目录、低优先级和最多 2 个编译任务。GKD350H Ultra 最终增量交叉构建通过。
- 最终 Stage package audit 确认 `games`、`covers`、`saves`、`cache` 均为 0 个文件；`git diff --check` 通过，未发现 `EVIOCGRAB` 或替换/停止 `input_sense` 的代码。
- 真机已验证菜单音量 `50 -> 55 -> 50` 和亮度 raw `138 -> 108 -> 138`。自动 Power 测试会触发 ROCKNIX 系统级电源处理并中断 SSH；按用户要求停止自动电源、休眠、游戏启动和长输入测试，完整真机功能矩阵留待重构完成后由用户执行。
- Stage 4 GKD 前端 SHA-256：`03972C45B260EB86EB38D70D9AE22A20286002399C23905FE9721D7726E36B02`。
- 真机前端回滚备份：`/storage/games-external/ROCgalgame_refactor_backups/20260718_054914/stage4_before_20260718_094140/rocgalgame_sdl`。
- 重启后只读复核确认最终前端、launcher、ONS、KRKR、config 和 keymap 哈希均与保护值一致；`input_sense` 正常运行，没有 ROCgalgame 或测试残留进程，设备 `/tmp` 没有 Stage 4 临时项。
- 本阶段未复制游戏，未写入项目或设备的 `games/`，未修改/重建/部署 KRKR，未回退 `ROCgalgame.sh` 的 `ROCREADER_SYSTEM_VOLUME_LEVELS=20`。

## Stage 5 结果

- 迁移 `animation.*`、`menu_scene.*`、`menu_runtime.*`、`settings_runtime.*` 和 `settings_panel_router.*`。
- 公共 System Controls、Key Guide、Key Calibration、Contributors、Contact、Version Update 和 Exit 面板使用 ROCreader 同名模块边界。
- ROCgalgame 专用面板通过 `settings_composition.*` 注入，`app_loop.cpp` 不直接调用具体 panel input/draw 函数。
- Version Update 在未配置 manifest URL 时安全显示，不发起网络请求。
- 菜单运行时聚焦测试通过；Windows 菜单、面板进入和返回状态成功渲染。

## Stage 6 结果

- 新增 `game_settings_runtime.*`、`game_settings_panel.*` 和无 SDL 依赖的 `game_settings_snapshot.h`。
- aspect、filter、virtual mouse、mouse speed 和 mouse acceleration 使用类型化 state 与 callbacks。
- 旧 `fit-width` 继续迁移为 `contain`，UI 只展示 `stretch`、`fill-height`、`contain` 和真实 filter 值。
- panel 只负责公共行布局和绘制；配置写入集中在 runtime callbacks。
- adapter 从不可变 `GameSettingsSnapshot` 解析全局默认值与 `game.ini` 覆盖。

## Stage 7 结果

- 新增 `game_scanner.*`、`game_library_service.*`、`shelf_runtime.*`、`shelf_scene.*`、`cover_resolver.*`、`cover_cache_runtime.*` 和 `cover_service.*`。
- 扫描、分类、收藏、历史、封面路径解析、纹理 cache、书架状态和绘制职责分离。
- ONS、KRKR、收藏和历史分类保留；历史按最近启动顺序排列，空分类保持普通空书架。
- 封面路径扫描与运行时纹理加载分离；有界 cache 避免每帧重建全部封面纹理。
- 底部状态栏继续使用用户原始 `bottom_hint_bar.png`，本轮未替换或修改该 UI 资产。

## Stage 8 结果

- 新增 `IGameCoreAdapter`、`GameCoreRegistry`、`GameLaunchService`、`ICoreProcessRunner` 和平台进程实现。
- ONS 参数和环境只存在于 `ons_core_adapter.cpp`；KRKR 参数和环境只存在于 `krkr_core_adapter.cpp`。
- fork/exec/wait 和 Windows `CreateProcessW` 只存在于 `core_process_runner.cpp`。
- `ICoreProcessRunner` 支持 fake 注入；新增第三核心测试只注册新 adapter，不修改菜单、书架和进程代码。
- ONS/KRKR spec、单游戏覆盖和类型化/本地化 launch result 测试通过。

## Stage 9 结果

- 删除旧 `src/app.*`、`src/core_launcher.*`、`src/input.*` 和 shell `launch_request.ini`/退出码 `42` 协议。
- `app.cpp` 重命名为 `app_loop.cpp`；当前约 1016 行，保留应用生命周期、依赖组装和场景路由。
- `settings_composition.*` 拥有具体 panel wiring；`app_loop.cpp` 不包含平台系统命令或核心专用参数构建。
- `ROCgalgame.sh` 只准备运行环境并启动前端，继续保留 `ROCREADER_SYSTEM_VOLUME_LEVELS=20`。
- 原计划要求 Stage 8 真机通过后再删除旧协议；本轮按用户要求不做设备交互，改由 WSL/Windows 单管线审计和 Windows 生命周期模拟确认，完整真机启动/返回矩阵延期。

## Stage 10 结果

- `app_environment.*` 提供 desktop preview、system volume、brightness、screen power、evdev、external core process 和 packaging verified capabilities。
- Windows 强制解析为 Desktop profile，不访问真实 ALSA、evdev、sysfs 或 screen-power 控制。
- H700、Trimui Brick、GKD350H Ultra 和 RGDS profile 保留在共享 screen/input 层。
- 只有 GKD350H Ultra 标记为 packaging verified；其他 profile 可编译不等于声明设备支持。

## Stage 11 结果

- 新增 `ARCHITECTURE.md`，记录 app/platform/menu/shelf/game-domain 边界、核心 adapter contract、生命周期、capability 和 ROCreader 有意差异。
- 更新 README，删除旧 `launch_request.ini`/退出码 `42` 说明，修正控制与 frontend-only Windows 构建说明。
- 新增 `tests/game_domain_runtime_test.cpp`，覆盖 Game Settings 持久化、书架分类/收藏/历史/分页和第三核心注册。
- Ubuntu WSL2 低优先级聚焦测试通过：`core launch`、`app foundation`、`menu runtime`、`game domain runtime`；最终前端增量构建通过。
- MSYS2 UCRT64 frontend-only 构建通过，未调用 ONS/KRKR 构建脚本。最终 Windows 前端 SHA-256：`972F42FD903708A04129EE85DCB42B61AE2C6C0381619ABDE21E2B37234D3ED9`。
- Windows 自动截图均为 `1600x1440` 且非黑帧：

| 截图 | SHA-256 |
| --- | --- |
| `shelf.bmp` | `AA21711B5BBC6DA8A54B5D9E321A29A5C375FE1F54CAC9DEB44F2C2C2BA35A66` |
| `menu.bmp` | `4CEF5673A0C3030C36BAEDC2C969A5866EEF8FE18AE39C5D1BFCB35A8CB029BD` |
| `system_settings.bmp` | `B03A911AFB4F3052279609088E3756AC06167CC5AA9E667DA431E2FACF63BF63` |
| `game_settings.bmp` | `52F6B84AB69B21EE56A59DDF8E611645739D7513E0ED9B95EDCBA5E767D3E11E` |
| `volume_overlay.bmp` | `BF916BA26D355D32D5A1286FBD4DF885177569A51B5E0F9EC6CE2B6AE07BC9F5` |

- 五张截图的底部状态栏区域与原 `ui/1600x1440/bottom_hint_bar.png` 逐像素一致，差异像素均为 0。
- `git diff --check` 通过；runtime 不存在旧 launch protocol；核心参数只存在于 adapters；进程 API 只存在于 process runner。
- Git 状态确认未修改受保护的游戏、封面、存档、核心和 GKD/KRKR 缓存目录。本轮未部署、未连接设备、未启动游戏核心。
- 完整 GKD 真机功能矩阵、前端原子备份和部署按用户要求延期，由用户在重构后自行真机测试；因此本轮候选不声明已完成设备级发布验收。

## Stage 11 真机部署与轻量冒烟

- 2026-07-19 本地执行 GKD 前端 clean/full 交叉构建与 ONS 增量构建；Stage package audit 通过。ONS 新产物 SHA-256 仍为 `8C69889F8C93CDE3D14C9462D8997050D4FBCE2E36D75BA990203DC89F46FCB5`，与设备现有文件逐字节一致；KRKR 未重建、未上传、未替换。
- 部署前复核 launcher、frontend、ONS、KRKR、config、keymap 哈希以及 games/saves/covers inode、mtime 和目录计数，均与保护基线一致。存档目录数按递归口径为 12，顶层为 `ons`、`krkr` 两个目录。
- 只上传并原子替换 `/storage/roms/ports/ROCgalgame.sh` 和 `/storage/roms/ports/ROCgalgame/rocgalgame_sdl`；没有复制游戏，没有写入 games/covers/saves，没有替换 ONS/KRKR、配置、键位或 UI 资源。
- 回滚备份：`/storage/games-external/ROCgalgame_refactor_backups/20260718_054914/stage11_before_20260718_202601`。备份包含旧 launcher、旧 frontend、ONS、config、keymap、完整 `SHA256SUMS.before` 和 `DATA_STATE.before`；KRKR 记录在哈希清单中但未复制。
- 部署后 launcher SHA-256：`9B67A117E70087BB82942B92E4D4F5D0B8858BEFC3B3E0B3EC3D9FD81E47A707`；frontend SHA-256：`51807D5EEEFF0DF1E29B6D39EBA96F53D74C2AA915510A5B1E838C27974ABB98`。
- 部署后 ONS、KRKR、config、keymap 哈希保持不变；games/saves/covers 的 inode、大小、mtime 与目录计数保持不变；无 frontend、ONS、KRKR、scp、cp、rsync 或 tar 残留进程。
- 设备端完成书架、菜单、系统设置、游戏设置四次单帧截图冒烟，每次使用 `ROCGALGAME_EXIT_AFTER_CAPTURE=1` 立即退出，未设置游戏自动启动。四张截图均为 `1600x1440`、非黑帧，封面、中文文本、布局和用户原始底部状态栏视觉正常。

| 设备截图 | SHA-256 |
| --- | --- |
| `stage11_shelf.bmp` | `ADC16E9C69F256AA460C9E5BEE7006CCA5F450619CB46E1C2F7112681A159080` |
| `stage11_menu.bmp` | `2B477120022512884F0F49924C5C2DBFD154FAB75A2EE0E991CE53721CAD1824` |
| `stage11_system_settings.bmp` | `39004359206CB3A338862D1782A3826DE3D32015883C9B01BADD4C37D7FCEEB3` |
| `stage11_game_settings.bmp` | `5672E3962AC1D1C9816D233B816108EB73A893C3FEDFE2E443720B5B8BFC81FC` |

- SSH 截图会话日志包含 ALSA pipewire 模块缺失和 `SDL audio init failed` 警告；该会话没有可用音频设备，但 UI 渲染与退出正常。音频、实体按键、休眠/唤醒和 ONS/KRKR 启动/返回仍留给用户从系统菜单启动后做真机手工验证。
