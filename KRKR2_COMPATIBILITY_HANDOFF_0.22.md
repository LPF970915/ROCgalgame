# KRKR2 0.22 现状总结与兼容性修复交接

更新时间：2026-07-29 00:45（Asia/Shanghai）  
目标设备：GKD350H Ultra，`root@192.168.31.13`  
仓库：`D:\Works\ROCgalgame`  
当前分支：`main`

## 1. 本次交付结论

`0.22` 已完成构建、本地校验和真机原子部署。

- 功能基线：`0.20`。
- `0.21` 不作为前端二进制或功能基线，只吸收仓库当前新增 UI 资产。
- `0.20`、`0.21` 既有 ZIP 未覆盖、未删除、未改写。
- `0.22` 包含合并后的 KRKR2 原生 Wayland / SDL2 / EGL / OpenGL ES 2 GPU 核心、完整前端选择与输入桥接，以及当前 54 项 UI 资产。
- ONS、krkrsdl2、KRKR2 和 KRKR2 私有 GL 均复用已验证产物，没有重新全量编译核心。
- 真机现已显示 `version.txt=0.22`，部署后所有二进制哈希与本地一致。
- 真机的 `games`、`covers`、`game_covers`、`saves`、`cache`、`logs` 均由部署脚本保留。

本次收尾没有继续做新的单游戏兼容补丁，也没有在部署后进行肉眼可见的完整游戏交互验收。后续窗口应从第 8 节继续。

## 2. 版本和包体锁定值

发版目录：`GKD350HUltra/Downloads`

| 版本 | 大小（字节） | SHA-256 |
| --- | ---: | --- |
| 0.20 | 58,648,840 | `114c694c8e51b8b39cc674ab76849cfc4af054e9b77225094a05e9698b174e88` |
| 0.21 | 58,891,462 | `2f53aa1d2347c725f43a4344b624b34f30843e9c43fe04b0f2c0fc94859b1795` |
| 0.22 | 58,914,105 | `cdc2a904d3a34935f49c9fc79f9e5f4aba249692be55078ee654b770e7a94c54` |

`0.22` 文件：

```text
GKD350HUltra/Downloads/ROCgalgame ver0.22 for GKD350H Ultra.zip
GKD350HUltra/Downloads/ROCgalgame ver0.22 for GKD350H Ultra.zip.sha256
```

禁止使用 `-Force` 覆盖现有正式版本。后续有修复时顺延为 `0.23` 或更高版本。

## 3. 设备部署状态

安装目录：

```text
/storage/roms/ports/ROCgalgame
```

部署结果：

```text
version=0.22
frontend=847b7a4b169e40652d33b0b614f6af0cf9d2318e596bd3787059bb918ab42fea
ons=574eaca206f4331409adc629db94de15cb98aa8a1c7230c230f093f5fed2079e
krkr=6aee1f22494ec653b322cbde9ea0ac88d3ab949650ef706b9ebbdcd4146535e7
krkr2=df1a9980777dc1f8b295c989220e7daf8a3aede4739cb4e746414fe1b4889ccc
krkr2_gl=0e1d74952d5edcfd023c214a19f280a5248a256cbc179fbee2285b50bc3ec918
```

自动生成的部署前备份：

```text
/storage/roms/ports/ROCgalgame-backups/release-0.22-20260729-004211
```

部署前后一级数据项数量一致：

```text
games=24
covers=0
game_covers=16
saves=1
cache=2
logs=1
```

此前为诊断“向妈妈撒娇吧！”临时写入的 5 行 `game.ini` 已逐行确认后删除。其他游戏已有的 `game.ini` 未改动。

## 4. 0.22 中的通用兼容修复

主要实现位于：

```text
src/game_scanner.cpp
tests/core_launch_test.cpp
```

修复目标是让前端正确选择 KRKR2 和真实入口，不依赖游戏标题或固定文件名：

1. 读取 XP3 的 11 字节魔数识别档案，而不是只看扩展名。
2. 支持 `.xp3`、`.bin`、`.dat` 和其他扩展名的 XP3 档案。
3. 标准 `data.xp3` 或 `startup.tjs` 保持原有 krkrsdl2 默认路径。
4. 没有标准入口时，唯一的非补丁 XP3，或明确的 `data.*` XP3，自动选择 KRKR2。
5. `patch*.xp3` 不会被当作主入口。
6. 多个无法判断的 XP3 不猜测，保持旧行为。
7. 显式 `game.ini` 的 `entry`、`runtime` 始终优先。
8. 只有扩展名像档案、但魔数不正确的文件不会被误判。

本次修复直接解决了“直接运行 `krkr2 data.bin` 能通过，但真实前端错误使用 `krkrsdl2 + 游戏目录` 并退出 1”的通用入口选择问题。没有添加任何游戏标题、中文目录名或单一作品判断。

## 5. GPU、渲染和输入现状

当前 KRKR2 核心已验证的运行路径：

```text
SDL2 native Wayland
SDL_CreateWindow / SDL_GL_CreateContext / SDL_GL_SwapWindow
EGL / OpenGL ES 2
Mali-G52
vsync=1
Cocos 60 FPS pacing
```

输入链路：

```text
物理输入/uinput
  -> 前端 InputManager
  -> CoreInputBridge FIFO
  -> KRKR2
  -> 虚拟指针和按键事件
```

已有自动化证据：

- 稳态约 56.5-59.2 FPS。
- 满幅摇杆持续 2 秒产生 115 次指针派发，约 59 次/秒。
- A/B 按下与释放、X/Y、十字键事件均进入核心。
- Start+Select 退出请求通过，核心退出后前端恢复。
- 队列溢出和 IPC 写失败均为 0。
- KRKR2 已加载圆形虚拟光标纹理。

限制：远程自动截图仍不能可靠证明圆形光标最终像素是否显示。光标外观、手感、A/B 实际点击目标和双 S 退出仍建议在设备屏幕上做一次人工验收。

## 6. 本次构建和测试结果

合并提交：

```text
539f11a 合并KRKR2 GPU前端重构
```

合并后 8 组前端测试全部通过：

```text
core launch tests passed
app foundation tests passed
config store tests passed
input manager tests passed
core input protocol tests passed
system runtime tests passed
menu runtime tests passed
game domain runtime tests passed
```

发版验证通过：

- `version.txt=0.22`
- 当前 54 项 UI 已生成加密 `ui.pack`
- ZIP 中没有根目录明文 `ui/`
- ZIP 中没有 debug KRKR 核心
- `games/covers/saves/cache` 为空目录
- 核心和私有 GL 哈希匹配 `release_core_hashes.sha256`
- ZIP SHA-256 与 `.sha256` 文件一致
- `0.20/0.21` 哈希在出包后仍与锁定值一致

发版脚本采用 `Jobs=2`。本次 `build_release_docker.sh` 对前端执行了 clean frontend build，因此前端对象重新编译；ONS/KRKR 核心、sysroot、Docker 层和其他重型缓存均保留。禁止运行 `git clean -x`、`git clean -fdx` 或删除以下缓存目录。

```text
build/
GKD350HUltra/sysroot_device/
GKD350HUltra/tools/
GKD350HUltra/dist_lowglibc/
WSL/Docker build cache
vcpkg/CMake/ccache cache
```

## 7. 已知游戏兼容性边界

完整证据见 `KRKR2_GPU_REFACTOR_RESULT.md`。最近一次隔离 sweep 的结论是 10 个案例中 8 个完整启动通过、2 个受控兼容退出、0 个 native crash。

已通过核心启动的重点案例：

- `NEKOPARA Vol.0`
- `NEKOPARA Vol.2`
- `向妈妈撒娇吧！`
- `如月真绫的指导`
- `桃色恋恋 ～与姐妹相系的H关系～`
- 另外三个设备库案例，详见结果文档中的表格

已知受控阻塞：

- `千恋万花`
- `吹弹！丰满！波涛汹涌！异世界魔法学园！`

两者当前共同缺少真实 `PackinOne.dll` 行为。现有核心会记录插件失败并受控退出，不再出现 native crash。不要通过伪造插件加载成功或按游戏标题绕过脚本来掩盖问题；应实现可复用的 PackinOne 能力或明确的通用兼容层。

注意：上述 sweep 大多是私有 `/tmp` 游戏镜像和私有存档的核心级测试。后续报告必须分开记录：

1. 核心兼容：直接核心启动是否到达 `Startup script ended`，是否无崩溃。
2. 前端可玩：真实前端是否扫描到正确入口、选择正确 runtime、生成非空日志，并到达同一检查点。
3. 交互可玩：标题按钮、弹窗、保存、读取、指针、A/B 和退出组合是否实际有效。

只通过第一项不能报告为“完整可玩”。

## 8. 下一窗口建议执行顺序

1. 确认仓库状态和当前设备版本，不切回 `0.21`，不替换 `0.20` 包。
2. 使用部署后的 `0.22`，在“向妈妈撒娇吧！”没有临时 `game.ini` 的条件下，从真实前端启动。
3. 检查新日志非空，并确认前端自动选择 `krkr2 + data.bin`。
4. 确认 `Mali-G52`、1600x1440、`Startup script ended`、标题脚本等待点和 15 秒存活。
5. 按设备库逐个测试，先完整记录全部失败，再按共同错误栈/缺失能力归类。
6. 优先复测 NEKOPARA Vol.2 的标题点击、保存/读取弹窗、摇杆画圆、A/B 和 Start+Select。
7. 优先复测“如月真绫的指导”从标题进入正文并完成一次私有存档。
8. 对 PackinOne 阻塞单独建通用能力任务，不写标题、目录名、特定散列或单一脚本行号硬编码。
9. 每次测试结束显式结束 `rocgalgame_sdl`、`krkr2`、`krkrsdl2` 和测试注入进程，确认没有残留。
10. 有代码修复后顺延打 `0.23`，不要覆盖 `0.22`。

## 9. 可直接使用的命令

合并后的 8 组测试：

```powershell
wsl -d Ubuntu -- bash -lc "cd /mnt/d/Works/ROCgalgame && env -u XDG_RUNTIME_DIR -u WAYLAND_DISPLAY -u SWAYSOCK -u ROCGALGAME_KRKR_DISPLAY_BACKEND make TARGET=build/wsl-compat-test/rocgalgame_sdl TEST_TARGET=build/wsl-compat-test/core_launch_test FOUNDATION_TEST_TARGET=build/wsl-compat-test/app_foundation_test CONFIG_STORE_TEST_TARGET=build/wsl-compat-test/config_store_test INPUT_MANAGER_TEST_TARGET=build/wsl-compat-test/input_manager_test CORE_INPUT_PROTOCOL_TEST_TARGET=build/wsl-compat-test/core_input_protocol_test SYSTEM_RUNTIME_TEST_TARGET=build/wsl-compat-test/system_runtime_test MENU_RUNTIME_TEST_TARGET=build/wsl-compat-test/menu_runtime_test GAME_DOMAIN_TEST_TARGET=build/wsl-compat-test/game_domain_runtime_test test -j2"
```

下一版发版（示例 `0.23`，2 并发）：

```powershell
powershell -ExecutionPolicy Bypass -File .\GKD350HUltra\build_release_docker.ps1 -Version 0.23 -Jobs 2
```

部署下一版：

```powershell
powershell -ExecutionPolicy Bypass -File .\GKD350HUltra\deploy_release.ps1 -Version 0.23 -DeviceHost root@192.168.31.13
```

部署脚本会拒绝正在运行的前端/核心，校验上传包和每个二进制哈希，保留用户数据，原子替换运行目录，并在失败时回滚。脚本已修正 Windows CRLF 远程命令传输问题。

## 10. 必须继续遵守的约束

- WSL Ubuntu 和 SSH 操作可以使用管理员权限。
- 不使用 8 核持续满载；默认 `Jobs=2`，除非确认温度和电压安全后再逐步提高。
- 保留全部编译缓存和已有正式包。
- 不执行破坏性 Git 清理，不重置其他窗口或用户改动。
- 不做单一游戏标题、路径、文件名或散列硬编码修复。
- 优先从共同崩溃栈、插件能力、档案格式、TJS 边界和渲染生命周期修复兼容性。
- 真机测试使用隔离存档，除非明确需要验证正式存档兼容且已先备份。
- 远程看不到屏幕时，只能报告日志、进程、窗口树、输入派发和帧率事实，不能声称肉眼画面或操作手感已经确认。

