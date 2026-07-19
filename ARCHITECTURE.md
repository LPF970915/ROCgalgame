# ROCgalgame Architecture Notes

本文档记录 Stage 6–11 重构完成后的稳定模块边界。公共前端结构以 ROCreader
提交 `2c1fdd8c2fe3d263f673d1683269fcba3f1c4d19` 为事实来源；游戏领域差异集中在
模型、回调、adapter 和少量应用组装层。

## App Layer

- `main.cpp` 只调用 `RunApp()`。
- `app_bootstrap.*` 独占 SDL、window、renderer 和相关子系统的创建与销毁。
- `app_composition.*`、`app_context.*`、`app_services.*` 和 `app_stores.*` 组装稳定依赖。
- `app_environment.*` 将 desktop、system volume、brightness、screen power、evdev、
  external core process 和 packaging verification 表达为 capability。
- `app_loop.*` 负责生命周期和场景路由，不拥有具体菜单面板实现、核心参数或平台命令。
- `settings_composition.*` 是公共菜单与 ROCgalgame 专用设置之间的组合边界。

## Platform Layer

- `screen_profile.*` 和 `input_manager.*` 保留 Desktop、H700、Trimui Brick、
  GKD350H Ultra 和 RGDS profile。
- Windows 强制使用 desktop input profile，不访问 ALSA、evdev、sysfs 或 screen-power 命令。
- `system_controls.*`、`lid_power_control.*` 和 `power_lifecycle.*` 拥有设备系统控制生命周期。
- 只有 GKD350H Ultra 当前标记为 `packaging_verified`；其他 profile 可编译不代表已经支持对应设备。

## Menu Layer

- `menu_scene.*` 负责公共菜单场景绘制和输入入口。
- `settings_runtime.*` 和 `settings_panel_router.*` 负责菜单状态、焦点、动画和 panel 路由。
- System Controls、Key Guide、Key Calibration、Contributors、Contact、Update 和 Exit
  保持 ROCreader 同名公共模块边界。
- `game_settings_runtime.*` 拥有类型化游戏设置、持久化 callbacks 和显示值转换；
  `game_settings_snapshot.h` 提供不依赖 SDL 的不可变启动快照。
- `game_settings_panel.*` 只负责行布局和绘制，不直接修改 `AppConfig`。
- Update 在没有 manifest URL 时只显示安全的未配置状态，不自动访问网络。

## Shelf Layer

- `game_scanner.*` 扫描游戏并读取可选 `game.ini`。
- `game_library_service.*` 拥有游戏列表、分类过滤、收藏、历史和持久化。
- `cover_resolver.*` 只解析封面文件路径；`cover_service.*` 只解析运行时纹理。
- `cover_cache_runtime.*` 拥有有界封面纹理缓存及其 renderer 生命周期。
- `shelf_runtime.*` 拥有分类、焦点、分页、动画和标题滚动状态。
- `shelf_scene.*` 负责书架输入/绘制组合，不扫描文件或启动核心。

## Game Domain

- `GameEntry`、`GameOverrides` 和 `CoreKind` 是游戏库模型。
- `IGameCoreAdapter` 将一个 `GameEntry` 和不可变的有效设置映射为 `CoreLaunchSpec`。
- `ons_core_adapter.*` 是 ONS 参数和环境变量的唯一所有者。
- `krkr_core_adapter.*` 是 KRKR 参数和环境变量的唯一所有者。
- `game_core_registry.*` 按 `CoreKind` 注册 adapter；新增第三核心不修改菜单、书架或进程层。
- `ICoreProcessRunner` 是可测试进程边界，`CoreProcessRunner` 是 Linux/Windows 实现。
- `game_launch_service.*` 只选择 adapter、构建 spec、执行进程并返回类型化结果。
- `app_loop.*` 在启动前销毁前端 SDL 生命周期，核心退出后重建前端并恢复分类、页码和焦点。

## Launch Contract

核心启动只有一条内部管线：

```text
Shelf -> GameLaunchService -> IGameCoreAdapter -> CoreLaunchSpec
      -> CoreProcessRunner -> LaunchResult -> frontend restore
```

`ROCgalgame.sh` 只负责准备运行环境并启动前端。旧的 `cache/launch_request.ini` 和
退出码 `42` 外部启动协议已经删除。

## Intentional ROCreader Differences

- ROCreader 将 `BookItem` 交给进程内 reader module；ROCgalgame 将 `GameEntry` 交给外部核心 adapter。
- ROCgalgame 保留 ONS、KRKR、收藏、历史四个分类，而不是图书格式和在线书架分类。
- ROCgalgame 的应用专用设置是 aspect、filter、virtual mouse、mouse speed 和 mouse acceleration。
- 前端在外部核心运行期间必须完全释放 SDL 显示/音频/输入资源，ROCreader 的进程内阅读器不需要该重建循环。
- KRKR 仍是实验核心；架构接入完成不等于兼容性与稳定性达到 ONS 水平。
- ROCgalgame 不包含 ROCreader 的 reader、online source、download 和 reading progress 模块。

## Protected Data And Builds

普通前端重构、测试和清理不得写入或删除：

```text
games/
covers/
game_covers/
saves/
cores/
build/gkd350h/krkrsdl2
build/gkd350h/obj
GKD350HUltra/sysroot_device
GKD350HUltra/dist_lowglibc
```

`ROCgalgame.sh` 必须继续保留 `ROCREADER_SYSTEM_VOLUME_LEVELS=20`。未经明确许可不部署，
不重建或替换 KRKR，也不把本机游戏、封面、存档或核心加入发布产物。

## Extension Rules

- 新增核心：实现一个 adapter 并注册；不要修改 shelf、menu 或 process runner。
- 新增设备：提供 screen、input、storage、system-control 和 power capabilities；不要修改游戏领域。
- 新增公共面板：提供 state、callbacks、draw 和 router 注册；不要在 `app_loop.cpp` 直接绘制。
- 新增游戏设置：只修改 Game Settings 和 launch-spec mapping；不要复制 System Settings。
- capability 和真机验证负责人齐备以前，不声明新的设备支持。
