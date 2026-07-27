# KRKR2 渲染与输入重构执行交接

更新日期：2026-07-26  
配套方案：`KRKR2_PERFORMANCE_AND_POINTER_OPTIMIZATION_PLAN.md`  
目标设备：GKD350H Ultra / `root@192.168.31.13`  
目标：彻底解决 KRKR2 画面严重卡顿、摇杆指针不动、按键响应迟钝或失效，同时保留 ROCgalgame 已有统一输入配置。

## 1. 开始重构前必须接受的结论

2026-07-26 10:22 的 NEKOPARA Vol.2 实机日志已经把问题从猜测缩小到确定范围：

```text
display backend=x11/xwayland
window=1600x1440 framebuffer=1600x1440
GL_VENDOR=Mesa
GL_RENDERER=softpipe
GL_VERSION=3.3 (Compatibility Profile) Mesa 24.3.4

frontend input bridge ready: /tmp/rocgalgame-krkr2-input-89281.fifo

fps=1.4 frame_ms_p50=689.14 p95=851.54 max=871.21
fifo_axis=48  fifo_axis_discarded=40
fps=1.4 frame_ms_p50=685.89 p95=851.54 max=871.21
fifo_axis=208 fifo_axis_discarded=200
```

因此：

1. FIFO 已创建、KRKR2 已打开，并且轴消息确实持续到达；不能再把主要问题归因于“FIFO 没连上”。
2. 当前 KRKR2 是 `softpipe` 软件渲染，不是硬件 GPU 渲染。
3. 1600x1440 framebuffer 每帧处理 230 万像素，软件路径下实测只有约 1.4 FPS。
4. 当前 KRKR2 指针代码把 `delta > 0.25s` 当作暂停，并将 `frameDelta` 设为 0。实测每帧约 0.69s，所以每一帧都被判定为暂停，移动量必然恒为 0。
5. FIFO 最新状态协议已经避免历史位移回放，但它无法让 1.4 FPS 的画面变流畅，也无法绕过错误的 250ms 判定。
6. ABXY、十字键和轴消息都在 Cocos `TVPMainScene::update()` 内消费。只要主线程一帧阻塞 700ms，所有游戏内输入都会同步迟钝；输入读取和退出控制不能继续完全依赖游戏帧循环。

这次重构必须同时修复渲染路径和输入生命周期。只调 `mouse_speed`、增加 FIFO 频率或继续叠加游戏专用补丁都不会解决根因。

## 2. 当前已部署基线

当前设备程序：

```text
frontend SHA-256:
2f3fbc37b126b4999f9394863287880b8ed1cff31fa01248db101dc3443bce89

krkr2 SHA-256:
4b2e07a04da471687f896ef7601ca4f05674938065844d32ca8fdeca13fb02a0

krkr2 BuildID:
910e9dc77904ba1f68aaa59c286044e6e0bcc5e4
```

该基线已经包含：

- `A <x> <y> <sequence>` 最新轴状态协议。
- `B/K` 离散事件协议。
- FIFO 断开和轴超时归零。
- ROCgalgame 模式禁用 Cocos 原生控制器重复输入。
- 线性 1080 px/s 指针参数。
- ROCgalgame 模式禁止日志同步调用 `TVPDrawSceneOnce()`。
- Release 可见的 GL、帧时间和 FIFO 统计。
- Xwayland 软件/硬件、SHM 和 framebuffer 环境开关。

不要把“已实现”误写成“已通过”。最新状态协议和诊断已工作，但实机流畅度及指针验收失败。

## 3. 必须保留的产品行为

1. 输入来源仍是项目现有 `InputManager` 和 `native_keymap.ini`。
2. 不为 KRKR2 新建另一套实体按键配置、死区或摇杆映射。
3. ONS 和 krkrsdl2 已验证的按键语义不能改变。
4. KRKR2 适配层只消费统一后的逻辑动作：A、B、X、Y、方向、Start、Select 和标准化轴状态。
5. 当前 `gkd350h-ultra` 物理映射及用户校准覆盖必须继续生效。
6. 保留 Start+Select 独立强制退出；退出不能等待 KRKR2 下一帧。
7. 保留 `krkrsdl2` 默认核心，不覆盖或改名。
8. 保留 Xwayland 兼容回退，原生 Wayland 未验收前不能删除。
9. 部署不得替换或删除 `games`、`covers`、`game_covers`、`saves`、`cache`、`native_config.ini`、`native_keymap.ini`。
10. 所有通用修复必须对最小 TJS 和多个游戏成立，不做 NEKOPARA 专用分支。

## 4. 推荐目标架构

### 4.1 前端输入会话

把核心运行期间的输入处理从 UI 生命周期中明确分离为 `CoreInputSession`：

```text
evdev / InputManager
  -> 固定周期 InputPump（建议 100～200 Hz）
  -> 最新轴状态快照
  -> 有序按钮/按键边沿队列
  -> 独立 Start+Select 监督器
  -> IPC transport
```

要求：

- `InputPump` 生命周期从启动核心前开始，到子进程退出后结束。
- SDL 窗口和渲染器关闭不能关闭核心会话使用的 evdev fd。
- 明确记录打开了哪些 `/dev/input/event*`、设备名和轴范围。
- Start+Select 由父进程直接向核心进程组发 SIGTERM/SIGKILL，不经过 KRKR2/TJS/Cocos。
- 不要同时从 SDL controller、直接 evdev 和 Cocos controller 三路重复产生同一个动作。

当前 `Shutdown(app)` 只明确关闭 `AppInputDevices` 的 SDL controller/joystick，`InputManager` 自己的 evdev fd 理论上仍在；但该生命周期必须通过 fd 和事件计数证明，不能继续依赖隐含所有权。

### 4.2 KRKR2 输入接收

把 FIFO 读取和协议解析从 `TVPMainScene::update()` 移到独立 transport 对象或输入线程：

```text
IPC reader thread
  -> 原子 latestAxis{x,y,sequence,timestamp}
  -> 有界 SPSC 离散事件队列

Cocos main thread
  -> 每帧读取一次 latestAxis
  -> 按顺序派发 B/K
  -> 每帧最多派发一次鼠标位置更新
```

线程规则：

- IPC 线程绝不能直接调用 Cocos、TVPWindow、TJS 或修改场景节点。
- 轴状态使用原子快照或短临界区，只保留最新值。
- B/K 必须保留 down/up 顺序，不能像轴一样合并。
- 离散队列必须有溢出策略；溢出时用最新按键状态做一次状态对账，防止按键永久卡住。
- 断线、失焦和核心退出时显式发布全归零/全释放状态。
- transport 读取不能依赖渲染帧是否到来。

### 4.3 指针时间模型

删除当前逻辑：

```cpp
delta <= 0.25f ? delta : 0.0f
```

持续低 FPS 和系统挂起不是同一件事。正确处理方式：

1. 使用 `steady_clock` 记录指针积分时间，不依赖 FIFO 轮询间隔。
2. 只在明确的断线、失焦、设备挂起/恢复事件时丢弃时间并清零。
3. 普通低 FPS 下仍按真实经过时间计算目标坐标。
4. 可把输入线程持续积分为“目标绝对坐标”，主线程每帧只把光标更新到最新目标；这样不会积压历史移动事件。
5. 低 FPS 时视觉仍只能每帧更新一次，因此该方案只保证正确距离和无历史回放，流畅度必须由渲染修复保证。
6. 保留圆形径向限速、单次 Y 翻转和窗口边界钳制。
7. 死区只在统一 `InputManager` 计算一次；KRKR2 不得再加 0.183 死区。

需要新增的时间模型测试：16.7ms、33ms、100ms、700ms 连续帧，以及显式 suspend/resume。700ms 连续帧必须移动，显式恢复的第一帧不得大跳。

## 5. 渲染重构顺序

### 5.1 先完成 Xwayland A/B，不要直接盲改 GLES

当前默认仍是：

```text
ROCGALGAME_KRKR_XWAYLAND_RENDERER=software
ROCGALGAME_KRKR_XWAYLAND_SHM=1
1600x1440
LIBGL_ALWAYS_SOFTWARE=1
-glamor off
```

按以下顺序真机验证，每次都记录 GL renderer 和帧时间：

| 组 | renderer | glamor | framebuffer | 目的 |
|---|---|---|---|---|
| A | software | off | 1600x1440 | 已知基线，约 1.4 FPS |
| B | hardware | on | 1600x1440 | 判断目标 GPU/GLX 能否工作 |
| C | hardware | on | 游戏逻辑尺寸 | 判断像素量和缩放成本 |
| D | software | off | 游戏逻辑尺寸 | 硬件失败时判断降分辨率收益 |

注意事项：

- “窗口能打开”不代表硬件加速成功，必须检查 `GL_RENDERER`。
- `softpipe`、`llvmpipe`、`Software Rasterizer` 一律判定为软件路径。
- 开发阶段硬件路径失败时要保留原始 Xwayland stderr、GLX/EGL 错误和退出码，不要立即静默回退把证据抹掉。
- 检查 `/dev/dri` 权限、实际 GPU 驱动、GLVND vendor JSON、DRI driver 和 `LD_LIBRARY_PATH`，避免把 Ubuntu sysroot 库、设备系统库和宿主库混用。
- `libGL.so` 能加载不等于 DRI 驱动能创建硬件 context。
- `-shm` 必须实测开/关，不凭名称推断性能。
- framebuffer 应尽量接近游戏逻辑尺寸，并由 Sway 做最终缩放；必须保持游戏宽高比并正确 letterbox，不能为降像素再次引入拉伸和偏移。
- 不应永远硬编码 960x640。先记录游戏窗口/设计分辨率，再选择 960x640、1280x720 或实际逻辑尺寸。

Xwayland 硬件路径达到可接受帧率后，再决定原生 Wayland 是否仍有必要。

### 5.2 原生 Wayland/EGL/GLES2 单独做原型

不要在现有兼容分支里一次性替换全部窗口系统。建立可独立构建的后端：

```text
ROCGALGAME_KRKR_DISPLAY_BACKEND=xwayland
ROCGALGAME_KRKR_DISPLAY_BACKEND=wayland
```

Wayland 原型要求：

- GLFW 构建包含 Wayland backend，版本能力必须实际核对。
- 使用目标设备的 `wayland-client`、`wayland-egl`、`wayland-cursor`、`xkbcommon`、EGL 和 GLESv2 ABI。
- Cocos Linux 桌面 GL 代码逐项审查 shader 版本、纹理格式、扩展函数和 GLEW 依赖。
- 验证 swap interval、vsync、窗口 configure、缩放、旋转、焦点和输入法。
- Wayland 模式不启动私有 Xwayland，不设置 `DISPLAY=:2`、`GDK_BACKEND=x11` 或 `SDL_VIDEODRIVER=x11`。
- 任一原型失败不得破坏 Xwayland 可启动回退。

## 6. 必须补齐的诊断

当前核心只证明了消费端收到多少轴消息。重构期间需要同时记录五段计数，建议每秒或每五秒汇总一次：

```text
frontend_raw_abs_events
frontend_raw_key_events
frontend_axis_state_changes
frontend_axis_heartbeats
frontend_ipc_writes / write_failures / reconnects
core_ipc_bytes / parsed_axis / parsed_buttons / parsed_keys
core_discrete_queue_depth / overflows
core_pointer_dispatches
core_frame_count / P50 / P95 / P99 / max
```

一次实机操作必须能回答：

1. 物理摇杆是否产生 EV_ABS？
2. `InputManager` 输出的标准化 x/y 是多少？
3. IPC 是否写入并被核心读取？
4. 最新轴状态是否改变了目标坐标？
5. 主线程是否派发了光标更新？
6. 派发后游戏窗口是否接收了 mouse move/down/up？

日志不得每个事件写磁盘，否则诊断本身会制造卡顿。使用内存计数器和低频汇总。

## 7. 输入映射注意事项

1. GKD 当前物理映射中 `BTN_SOUTH -> Button::B`、`BTN_EAST -> Button::A`，这是为了匹配设备标签和现有前端行为。
2. `KrkrCoreAdapter` 当前又固定设置 `ROCGALGAME_KRKR_SWAP_AB=1`。重构必须画出最终语义表并验证是否发生二次交换。
3. 不要按 Linux 按键编号直接在 KRKR2 再写一套 switch；核心只接收统一逻辑动作。
4. A/B 应产生虚拟鼠标左右键，X/Y 和方向键的功能应与 krkrsdl2/ONS 的既有配置一致。
5. 按钮测试必须同时验证 down 和 up；只验证“按下有日志”会漏掉卡键问题。
6. 游戏内 overlay、窗口尚未创建或虚拟鼠标关闭时，`postVirtualMouseButton()` 当前会直接丢弃事件。重构需定义启动期事件是缓存、忽略还是状态对账，不能无记录丢弃。

## 8. 测试门禁

不要每次直接用完整 NEKOPARA 猜结果。按以下门禁推进：

### 门禁 1：纯输入 transport

- 用伪输入源注入轴和按钮，不启动 KRKR2。
- 确认 InputManager 映射、A/B/X/Y、方向、Start+Select 和轴归一化。
- 模拟 FIFO reader 延迟 1 秒，轴只保留最新状态，B/K 顺序完整。

### 门禁 2：KRKR2 输入最小窗口

- 使用最小 TJS 窗口和固定背景。
- 人工让渲染帧间隔变为 16、33、100、700ms。
- 验证每秒移动距离误差、按钮 down/up、回中停止和退出监督。
- 该门禁不依赖 NEKOPARA 插件或大量图层。

### 门禁 3：渲染后端 A/B

- 每组运行至少 30 秒。
- 记录 GL renderer、framebuffer、FPS、P50/P95/P99/max、CPU 和内存。
- 截图确认比例、位置和 letterbox 正确。
- 硬件组失败必须保留完整启动日志。

### 门禁 4：真实游戏

- 先最小 TJS，再《千恋＊万花》，最后 NEKOPARA Vol.2 和“如月真绫的指导”。
- 分别验收标题、剧情、音频、输入、存档和退出。
- 插件兼容问题与平台性能问题分开登记，不能因为某游戏缺插件就回退整个渲染架构。

## 9. 验收标准

输入：

- 摇杆在 16/33/100/700ms 连续帧下都有有效移动。
- 满轴约 1080 px/s，半轴约 540 px/s，允许 ±10%。
- 不回放历史位移，回中后一个可用呈现周期内停止。
- 每个呈现帧最多一次光标更新。
- B/K down/up 顺序完整，无卡键。
- Start+Select 即使 KRKR2 主线程阻塞也能在约 2 秒内退出核心并返回前端。

渲染：

- 默认生产路径 `GL_RENDERER` 不得为 softpipe/llvmpipe。
- 60 FPS 场景目标 P95 <= 20ms；无法达到 60 FPS 时也不得长期停留在 1～2 FPS。
- 加载期不出现持续数百毫秒的逐层刷新。
- 窗口宽高比、位置、可见区域和光标坐标一致。

发布：

- AArch64 ELF、`/lib/ld-linux-aarch64.so.1`、`ldd` 无 `not found`。
- 不链接宿主 x86_64 或 Android ABI 库。
- 外部 KRKR2 源码改动全部同步成可重放补丁。
- 前端、KRKR2 和依赖校验通过后才部署。
- 部署前后核对游戏、封面、存档、缓存和用户配置 inode/哈希。

## 10. 构建与源码管理

1. 外部源码位于 `D:/Works/Tyranor/krkr2`，项目补丁位于 `GKD350HUltra/patches/`；两者必须同步。
2. 当前工作树包含大量尚未提交的兼容改动，禁止 reset、checkout 或清理用户修改。
3. 输入 transport 应从巨大的 `MainScene.cpp` 拆到独立 `.cpp/.h`，减少每次修改触发的大翻译单元重编。
4. 日常使用 FastBuild/增量构建，不清理 vcpkg 和整个 build 目录。
5. 建议 2 jobs、CPU 0/1、nice 19、ionice idle、工作 300 秒/冷却 60 秒；机器稳定时再调整。
6. 一次只改变一个可测变量。渲染 A/B 不同时改输入曲线，输入测试不同时更换 framebuffer。
7. 完成功能和真机回归后再做一次干净全量构建，用来验证补丁可重放和消除脏缓存风险；全量构建不是日常修 bug 手段。
8. Release 构建会裁掉 `CCLOG`，关键诊断必须使用 Release 实际保留的 logger，并用 `strings` 校验产物。

## 11. 部署与回滚

程序更新采用临时文件、SHA-256 校验、备份和原子 rename。禁止用整目录覆盖。

当前回滚点：

```text
KRKR2 backup:
/storage/games-external/ROCgalgame_refactor_backups/
krkr2_layerex_20260726-101734/krkr2

frontend backup:
/storage/roms/ports/ROCgalgame/
rocgalgame_sdl.pre-update-20260726-101738
```

每次部署必须先确认 `rocgalgame_sdl`、`krkr2`、`krkrsdl2` 和 `onsyuri` 均未运行。若核心失去退出能力，优先通过 SSH 杀进程，不要求用户再次强制关机。

## 12. 禁止事项

- 不要继续用 `delta > 0.25s -> 0` 区分低 FPS 和暂停。
- 不要只提高 `mouse_speed`。
- 不要恢复高频 `M dx dy` 历史位移队列。
- 不要在两个输入层重复应用 deadzone、加速度或 Y 翻转。
- 不要让 IPC 线程直接调用 Cocos/TJS。
- 不要在 FIFO 已配置但未验证健康时永久屏蔽所有原生输入而不给出诊断。
- 不要仅凭 context 创建成功宣称硬件加速。
- 不要把 softpipe 下调小 framebuffer 当成最终硬件优化完成。
- 不要为了 NEKOPARA 写游戏名判断或专用插件旁路。
- 不要删除 Xwayland 回退。
- 不要覆盖用户游戏、封面、存档和输入配置。

## 13. 建议首轮执行清单

1. 保存当前设备日志 `NEKOPARA Vol.2_20260726_102224.log` 作为失败基线。
2. 为前端和核心加入五段输入计数，不改变行为。
3. 写低 FPS 时间模型单元测试，先复现 700ms 帧下移动恒为零。
4. 删除 250ms 普通帧归零逻辑，加入显式 suspend/disconnect 语义。
5. 将 FIFO reader 与 `MainScene::update()` 解耦，但保持 Cocos 操作在主线程。
6. 验证最小窗口中轴、ABXY、方向和独立退出。
7. 真机运行 Xwayland B 组：hardware + glamor + 1600x1440。
8. 若 B 组创建失败，收集 `/dev/dri`、DRI/GLVND 和 Xwayland 日志后修驱动链；不要静默回软件路径。
9. 若 B 组成功，再测逻辑 framebuffer 和宽高比。
10. Xwayland 硬件路径仍不达标时，启动独立 Wayland/EGL/GLES2 原型。

## 14. 完成定义

只有以下条件全部成立才能关闭重构任务：

- 实机日志证明默认路径不再使用 softpipe/llvmpipe。
- 画面不再长期处于 1～2 FPS。
- FIFO 输入五段链路都有可核对计数。
- 700ms 连续低帧率测试仍能移动，显式恢复不大跳。
- 实机摇杆、ABXY、十字键和 Start+Select 全部通过。
- 比例、位置和光标坐标一致。
- 最小 TJS、千恋万花、NEKOPARA Vol.2、如月真绫的指导完成回归。
- Xwayland 回退仍可启动和操作。
- 增量构建和最终干净构建均通过，补丁可从记录基线重放。
- 部署未改变任何用户游戏、封面、存档、缓存或输入配置。
