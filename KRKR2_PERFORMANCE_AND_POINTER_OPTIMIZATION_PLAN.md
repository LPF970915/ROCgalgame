# KRKR2 渲染与摇杆指针流畅度改进方案

更新日期：2026-07-26  
适用项目：ROCgalgame / GKD350H Ultra  
目标核心：KRKR2（以 `krkrsdl2` 的真机流畅度和指针手感为参照）

## 1. 问题结论

目前 KRKR2 的图层延迟和摇杆指针停顿不是两个独立问题，而是同一条低帧率渲染链路在不同位置的表现：

1. KRKR2 运行在 `Cocos2d-x + GLFW/X11 + 私有 Xwayland` 上，当前还显式强制软件 OpenGL，并关闭 Xwayland glamor 加速。
2. KRKR2 的 Xwayland framebuffer 使用完整的 1600×1440，像素量是 960×640 的 3.75 倍，软件合成和上传成本被进一步放大。
3. KRKR2 控制台日志会同步触发场景绘制；现有节流补丁仍允许 important 日志强制绘制，游戏加载期容易不断阻塞正常帧循环。
4. 摇杆当前在 ROCgalgame 端每约 12 ms 计算一次位移增量，再通过 FIFO 发送；KRKR2 只能在 Cocos 帧循环中消费这些增量。渲染掉帧后，FIFO 中的增量先积压、再被同一帧批量执行，因此光标表现为停顿和跳跃。
5. 当前默认指针曲线为 `720 px/s + 1.6 指数`，而 `krkrsdl2` 基本是线性响应。半程摇杆在 KRKR2 中只有约 166 px/s，`krkrsdl2` 约为 540 px/s，手感会明显更慢、更黏。

因此，正确方向不是继续提高 FIFO 发送频率，也不是单独增大 `mouse_speed`，而是同时处理渲染后端、同步日志绘制和指针采样模型。

## 2. 已确认的源码与日志证据

### 2.1 KRKR2 被强制走软件 Xwayland

`src/core_process_runner.cpp` 当前启动命令包含：

```text
LIBGL_ALWAYS_SOFTWARE=1
Xwayland :2
-glamor off
-shm
```

其中 `LIBGL_ALWAYS_SOFTWARE=1` 明确禁止硬件 GL，`-glamor off` 关闭 Xwayland 的 GL 加速路径。KRKR2 渲染完成后还需要经过 Xwayland 和 Sway 合成，链路比 SDL2 原生 Wayland 更长。

`src/krkr_core_adapter.cpp` 还会设置：

```text
DISPLAY=:2
GDK_BACKEND=x11
SDL_VIDEODRIVER=x11
```

这说明当前 KRKR2 是固定的 X11 兼容运行方式，不是原生 Wayland。

### 2.2 framebuffer 分辨率过高

`src/krkr_core_adapter.cpp` 将前端屏幕尺寸直接传给私有 Xwayland。GKD350H Ultra 当前为 1600×1440：

```text
1600 × 1440 = 2,304,000 pixels
960 × 640   =   614,400 pixels
比例        = 3.75
```

在软件 OpenGL 和软件合成条件下，完整屏幕 framebuffer 会显著增加清屏、纹理上传、混合和最终合成的成本。

### 2.3 控制台日志同步绘制

`GKD350HUltra/patches/krkr2-rocgalgame-console-throttle.patch` 所修改的 `TVPConsoleLog()` 会调用：

```cpp
TVPDrawSceneOnce(100);
```

现有补丁只节流普通日志，`important` 日志仍可立即触发绘制。游戏启动和图层创建阶段日志密集时，这会让脚本执行、解码和场景绘制互相阻塞。

真机日志中，脚本自身报告的操作耗时常为 1～15 ms，但相邻日志的真实时间间隔达到约 160～340 ms，与同步绘制或低帧率阻塞更吻合。

### 2.4 当前指针协议会在掉帧时积压

当前流程：

```text
InputManager
  -> CoreProcessRunner 每约 12 ms 调用 Poll()
  -> CoreInputBridge 按 dt 计算 dx/dy
  -> FIFO 写入 M <dx> <dy>
  -> KRKR2 在每个 Cocos 帧中读取全部消息
  -> 对每个 M 消息调用 moveVirtualMouse()
```

当 KRKR2 只有 5～15 FPS 时，生产者仍在持续写入 `M` 消息。下一帧到来时，多个历史增量会集中执行。提高 FIFO 频率只会制造更多历史增量，不能改善实时性。

### 2.5 当前曲线与 krkrsdl2 不一致

当前 KRKR2 默认值：

```text
mouse_speed = 720 px/s
mouse_acceleration = 1.6
```

当前速度公式：

```text
velocity = mouse_speed × abs(axis)^mouse_acceleration
```

半程摇杆：

```text
720 × 0.5^1.6 ≈ 238 px/s
```

若再考虑约 0.183 的死区重映射，实际有效速度约为 166 px/s。`krkrsdl2` 的行为接近线性，满轴约 1080 px/s，半轴约 540 px/s，无 1.6 指数压速。

### 2.6 XP3 读盘不是当前主因

NEKOPARA Vol.2 的真机日志显示 XP3 索引建立很快，没有与画面长时间停顿对应的大量 I/O 等待。后续仍可记录资源解码时间，但现阶段不应把主要精力放在 XP3 缓存或读盘优化上。

## 3. 目标架构

### 3.1 短期可落地架构

```text
SDL2 InputManager
  -> FIFO 只发送最新轴状态 A <x> <y> <sequence>
  -> KRKR2 每个 Cocos 帧丢弃旧轴状态，只保留 sequence 最大者
  -> 对最新轴状态做一次 deadzone、速度计算和 dt 积分
  -> 每帧最多调用一次 moveVirtualMouse()/onMouseMove()

KRKR2 Cocos2d-x/OpenGL
  -> 禁止日志同步绘制
  -> 优先启用 Xwayland glamor/硬件 GL
  -> 使用合理的逻辑 framebuffer
  -> Sway 负责最终缩放和呈现
```

### 3.2 最终推荐架构

```text
SDL2 InputManager
  -> 最新轴状态 FIFO
  -> KRKR2 帧同步指针积分

KRKR2 Cocos2d-x
  -> GLFW Wayland
  -> EGL + GLES2
  -> Sway/DRM/KMS
```

最终架构不再启动私有 Xwayland，不设置 `DISPLAY=:2`，避免 X11 兼容层和软件合成带来的额外开销。Xwayland 路径必须保留为兼容回退，不能在原生 Wayland 尚未完成真机验证前直接删除。

## 4. 分阶段实施方案

### 阶段 0：先建立可量化诊断

在修改渲染路径前，为 KRKR2 增加一次性启动信息和低频性能统计：

```text
GL_VENDOR
GL_RENDERER
GL_VERSION
窗口逻辑尺寸
framebuffer 实际尺寸
显示后端：X11/Xwayland 或 Wayland
EGL/GLX 类型
平均 FPS
帧时间 P50/P95/P99
最长帧时间
FIFO 每帧读取消息数
被丢弃的旧轴状态数
```

统计建议每 5 秒输出一次，不能每帧写日志。帧时间使用固定容量的内存环形缓冲区，汇总时再排序计算分位数。

判断硬件加速不能只看是否成功创建 GL context，必须检查 `GL_RENDERER`。出现以下内容视为仍在软件渲染：

```text
llvmpipe
softpipe
Software Rasterizer
```

预期改动位置：

- KRKR2 Cocos 启动或 GLView 初始化代码
- `GKD350HUltra/patches/` 中新增对应诊断补丁
- 真机启动脚本或日志采集脚本

### 阶段 1：移除日志触发的同步绘制

在 ROCgalgame 运行模式下，`TVPConsoleLog()` 只记录日志，不允许调用 `TVPDrawSceneOnce()`，包括 `important` 日志。

推荐逻辑：

```cpp
if (!rocgalgameRuntime) {
    TVPDrawSceneOnce(100);
}
```

如果上游 KRKR2 桌面模式仍依赖控制台即时刷新，则仅保留非 ROCgalgame 分支的旧行为。不要把 important 日志转成更短的节流间隔，因为加载期仍可能形成同步绘制风暴。

需要更新：

- `GKD350HUltra/patches/krkr2-rocgalgame-console-throttle.patch`
- 外部 KRKR2 工作树中的对应源码
- 补丁说明和构建校验

### 阶段 2：把 FIFO 位移增量改成最新轴状态

#### 2.1 新协议

保留按钮和按键协议：

```text
B L 1
B L 0
B R 1
B R 0
K <virtual-key> <0|1>
```

废弃持续发送的相对位移协议：

```text
M <dx> <dy>
```

新增轴状态协议：

```text
A <x> <y> <sequence>
```

约束：

- `x`、`y` 使用标准化浮点数，范围 `[-1.0, 1.0]`。
- `sequence` 是单调递增的无符号整数，用于识别最新状态。
- 轴变化时发送；回到中心时必须发送一次 `A 0 0 <sequence>`。
- 可增加 100～250 ms 的低频心跳，防止异常丢失中心状态，但不能恢复高频位移增量。
- 单条消息必须远小于 `PIPE_BUF`，保持单写入原子性。

#### 2.2 生产端改造

修改 `src/core_input_bridge.cpp`：

1. 不再保存 `residual_x`、`residual_y`，不再在 ROCgalgame 进程中计算 `dx/dy`。
2. 每次轮询读取最新摇杆轴。
3. 轴变化超过很小的量化阈值，或需要发送中心状态/心跳时，写入一条 `A` 消息。
4. `B` 和 `K` 仍按状态边沿发送，不能被轴消息合并或丢弃。
5. FIFO 写失败后维持现有的重连或 XTest 回退行为。

#### 2.3 KRKR2 消费端改造

修改 `GKD350HUltra/patches/krkr2-frontend-input-bridge.patch` 对应的 KRKR2 代码：

1. 每帧开始时非阻塞读空 FIFO。
2. `B` 和 `K` 按读取顺序立即处理。
3. 对同一帧读到的多个 `A`，只保存 sequence 最大的一条。
4. 更新持久化的 `latestAxisX/latestAxisY`，旧状态直接丢弃。
5. 每个 Cocos 帧只根据最新轴状态积分一次，并最多派发一次鼠标移动事件。
6. FIFO 生效时禁用 Cocos 原生控制器轴移动，避免同一摇杆被处理两次。
7. FIFO 断开、窗口失焦或输入超时后把轴归零，避免光标自行漂移。

按钮/按键是离散事件，必须保持顺序；轴是连续状态，只需要最新值。这两类输入不能使用同一种队列语义。

### 阶段 3：使用帧同步、线性的圆形指针算法

推荐初始参数：

```text
InputManager hardware deadzone ≈ 0.183
mouse_speed = 1080 px/s
mouse_acceleration = 1.0
```

`InputManager` 已经根据设备轴范围和 `flat` 完成死区与 `[-1, 1]`
重映射，KRKR2 不得再次应用 `0.183` 死区。KRKR2 只对收到的最终轴状态做
径向限速，保证圆形运动和对角线总速度一致：

```cpp
float magnitude = sqrt(x * x + y * y);
if (magnitude <= 0.0f) {
    velocityX = 0;
    velocityY = 0;
} else {
    float scaled = clamp(magnitude, 0.0f, 1.0f);
    float directionX = x / magnitude;
    float directionY = y / magnitude;
    velocityX = directionX * scaled * mouseSpeed;
    velocityY = directionY * scaled * mouseSpeed;
}
```

每帧积分。Cocos 光标位置使用浮点坐标，本实现直接保留亚像素累计，不需要另设
整数 residual：

```cpp
float dx = velocityX * frameDelta;
float dy = velocityY * frameDelta;
```

实现要求：

- 使用 Cocos 实际帧间隔，不使用 FIFO 轮询间隔作为移动时间。
- 正常帧的移动速率与 FPS 无关。
- `frameDelta <= 0.25` 秒时使用真实帧间隔，保证 5 FPS 下仍保持正确速度；超过约 0.25 秒视为暂停，丢弃该段时间并清空状态，防止恢复时大跳。
- 最终坐标在游戏窗口范围内钳制。
- `dy` 的正负转换只做一次，避免生产端和消费端重复翻转。
- 每帧只有 `dx != 0 || dy != 0` 时才派发鼠标移动。

这套算法保留原圆形虚拟光标的方向感，同时让半程和满程速率接近 `krkrsdl2`。如果需要完全复制 `krkrsdl2` 的逐分量行为，可在真机 A/B 后改为分轴线性死区，但不建议默认牺牲对角线一致性。

### 阶段 4：先修复 Xwayland 快速路径

在原生 Wayland 移植完成前，先对当前 Xwayland 做 A/B 测试：

1. 移除 `LIBGL_ALWAYS_SOFTWARE=1`。
2. 移除 `-glamor off`，允许 Xwayland 尝试 glamor。
3. 保留和移除 `-shm` 分别测试，以真机兼容性、日志和帧时间决定；不要仅凭参数名称判断。
4. 检查真机 `GL_RENDERER` 是否切换到实际 GPU 驱动。
5. 如果 1600×1440 仍过重，临时使用接近游戏逻辑尺寸的 framebuffer，由 Sway 做最终缩放。

推荐的测试矩阵：

| 方案 | 软件 GL | glamor | framebuffer | 用途 |
|---|---:|---:|---:|---|
| A | 开 | 关 | 1600×1440 | 当前基线 |
| B | 关 | 开 | 1600×1440 | 验证硬件 GL/glamor |
| C | 关 | 开 | 游戏逻辑尺寸 | 验证像素量影响 |
| D | 关 | 关 | 游戏逻辑尺寸 | glamor 不兼容时的回退对照 |

如果关闭软件 GL 后无法创建 context，必须自动或通过配置回退到当前 Xwayland 模式，不能让 KRKR2 完全不可启动。

### 阶段 5：迁移到原生 Wayland/GLES2

这是最终解决渲染链路问题的方向，但改动面大于输入协议和日志修复，应单独制作原型和提交。

#### 5.1 构建依赖

- 使用带 `wayland` feature 的 GLFW。
- 准备 `wayland-client`、`wayland-cursor`、`wayland-egl`。
- 准备 `xkbcommon`。
- 链接 `EGL` 和 `GLESv2`。
- sysroot 目前有部分目标库但缺少完整开发头文件，应从 vcpkg 的 `wayland`、`wayland-protocols`、`libxkbcommon` 补齐交叉编译依赖。

#### 5.2 GLFW 初始化

在支持 GLFW 3.4 平台选择 API 的版本上：

```cpp
glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
```

如果当前 KRKR2/Cocos 使用的 GLFW 版本较旧，则通过 Wayland-only GLFW 构建选择后端，不能直接假定上述常量存在。

#### 5.3 Cocos2d-x GL 兼容

- Linux 路径从桌面 `GL/glew.h` 切换到 GLES2 头文件。
- 检查桌面 OpenGL 专用枚举、纹理格式、shader 版本和扩展函数。
- 确认 context 创建后再初始化 Cocos GL 状态。
- 检查 swap interval，默认以 60 Hz vsync 为目标。
- 记录 EGL 配置和实际 framebuffer 尺寸。

#### 5.4 运行时选择与回退

建议增加明确的后端配置：

```text
ROCGALGAME_KRKR_DISPLAY_BACKEND=wayland
ROCGALGAME_KRKR_DISPLAY_BACKEND=xwayland
```

Wayland 模式：

- 不调用 `EnsureKrkr2Xwayland()`。
- 不设置 `DISPLAY=:2`。
- 不强制 `GDK_BACKEND=x11` 或 `SDL_VIDEODRIVER=x11`。
- 传递正确的 `WAYLAND_DISPLAY` 和 `XDG_RUNTIME_DIR`。

Xwayland 模式保留当前逻辑，作为不兼容游戏或驱动的回退。

## 5. 需要修改的项目文件

前端仓库预计涉及：

- `src/core_input_bridge.cpp`
- `src/core_input_bridge.h`
- `src/core_process_runner.cpp`
- `src/krkr_core_adapter.cpp`
- `src/config.h`、`src/config.cpp` 或游戏设置相关文件
- `native_config.ini`
- `tests/input_manager_test.cpp`
- `tests/core_launch_test.cpp`
- `tests/gkd_krkr2_bridge_smoke.py`

KRKR2 补丁预计涉及：

- `GKD350HUltra/patches/krkr2-frontend-input-bridge.patch`
- `GKD350HUltra/patches/krkr2-rocgalgame-console-throttle.patch`
- 新增 GL/帧时间诊断补丁
- 新增 Wayland/GLES2 平台补丁
- `GKD350HUltra/patches/README.md`

构建系统预计涉及：

- `GKD350HUltra/build_krkr2.sh`
- `GKD350HUltra/build_krkr2.ps1`
- `GKD350HUltra/toolchain/aarch64-gkd-krkr2.cmake`
- `GKD350HUltra/vcpkg-ports/`
- `GKD350HUltra/vcpkg-triplets/`
- 发布包依赖校验和构建 checkpoint

外部 KRKR2 源码位于 `D:/Works/Tyranor/krkr2`。任何有效修改都必须同步生成到本仓库 `GKD350HUltra/patches/`，否则清理或重新构建后会丢失。

## 6. 测试方案

### 6.1 主机侧自动测试

至少覆盖：

1. `A` 协议序列化和解析。
2. 同一帧多个 `A` 只采用最大 sequence。
3. `B`、`K` 顺序不被轴合并破坏。
4. 发送中心状态后停止移动。
5. FIFO 断开后轴归零。
6. 线性死区边界：0、0.182、0.183、0.5、1.0。
7. 30/60/120 FPS 下，同一秒的总移动距离基本一致。
8. 250 ms 以上暂停后不会产生恢复大跳。
9. 对角线满轴速度不超过设定的 1080 px/s。
10. FIFO 启用时 Cocos 原生轴路径被禁用。

### 6.2 真机 A/B 游戏

使用两款已知样本：

- 《千恋＊万花》：重点观察启动、背景和人物图层加载。
- NEKOPARA Vol.2：重点观察摇杆连续画圆、半程微调和满程横移。

每个版本至少采集：

- 启动后 `GL_VENDOR/GL_RENDERER/GL_VERSION`。
- 进入主菜单所需时间。
- 连续 30 秒的 FPS 和帧时间 P50/P95/P99。
- 加载场景期间的最长帧。
- FIFO 每帧轴消息读取数和丢弃数。
- 满轴横向移动 1 秒的像素距离。
- 半轴横向移动 1 秒的像素距离。
- 连续画圆的视频或高帧率屏摄。

### 6.3 验收标准

渲染：

- `GL_RENDERER` 不得是 llvmpipe、softpipe 或 Software Rasterizer。
- 正常 60 FPS 场景帧时间 P95 目标不高于 20 ms。
- 图层显示不再出现可感知的数百毫秒逐层延迟。
- 日志密集期不再伴随同步画面刷新停顿。

指针：

- 满轴目标约 1080 px/s，允许误差 ±10%。
- 半轴目标约 540 px/s，允许误差 ±10%。
- 30 FPS 和 60 FPS 下移动一秒的总距离差异不超过 5%。
- 每个渲染帧最多派发一次移动事件。
- 掉帧后不出现历史位移集中回放。
- 回中后一个渲染帧内停止移动。
- 连续画圆无明显阶梯、停顿或对角线加速。

如果设备或游戏无法稳定达到 60 FPS，可接受更低的平均 FPS，但指针速度仍必须按实际时间保持一致，且不能积压历史位移。

## 7. 推荐实施顺序

1. 增加 GL、framebuffer、FPS 和帧时间诊断。
2. 在 ROCgalgame 模式彻底禁止 `TVPConsoleLog()` 同步绘制。
3. 将 FIFO 从 `M dx dy` 改为 `A x y sequence`。
4. 在 KRKR2 每帧只消费最新轴状态并按时间积分一次。
5. 将默认参数改为 `deadzone=0.183`、`mouse_speed=1080`、`mouse_acceleration=1.0`。
6. FIFO 生效时禁用 Cocos 原生控制器轴移动。
7. 完成单元测试和桥接 smoke test。
8. 对 Xwayland 软件/硬件 GL、glamor 和 framebuffer 尺寸做真机 A/B。
9. 单独制作原生 Wayland/EGL/GLES2 构建原型。
10. 原生 Wayland 达标后设为默认，继续保留 Xwayland 回退。

前 6 项可以直接解决“指针随掉帧积压”和“日志触发额外绘制”，风险相对可控；原生 Wayland/GLES2 是让 KRKR2 渲染链路真正接近 `krkrsdl2` 的最终工作。

## 8. 不推荐的处理方式

- 不要只把 `mouse_speed` 从 720 调高。它不能解决掉帧积压，反而会放大恢复时的跳跃。
- 不要提高 FIFO 的 `M` 消息频率。生产速度越高，低 FPS 时积压越严重。
- 不要在前端和 KRKR2 两侧同时做 dt 积分。移动距离会被重复计算。
- 不要同时保留 FIFO 轴输入和 Cocos 原生轴输入。会造成双倍移动或不稳定曲线。
- 不要只看平均 FPS。P95/P99 和最长帧更能解释图层延迟与指针停顿。
- 不要在没有检查 `GL_RENDERER` 的情况下认定已经启用硬件加速。
- 不要在原生 Wayland 尚未完成真机验证前删除 Xwayland 回退。
- 不要只修改 `D:/Works/Tyranor/krkr2` 外部源码而不生成项目补丁。

## 9. 最终建议配置

完成轴状态协议和 KRKR2 帧积分后，建议默认值为：

```ini
virtual_mouse=1
mouse_speed=1080
mouse_accel=1.0
```

硬件死区仍由统一 `InputManager` 根据设备轴范围和 `flat` 计算，不新增 KRKR2
专用的 `mouse_deadzone` 配置。

在旧 `M dx dy` 协议仍存在时，不应先部署上述速度配置，否则会让低 FPS 下的批量跳跃更明显。

## 10. 完成定义

只有同时满足以下条件，才能认为本次问题已处理完成：

- KRKR2 真机使用硬件渲染，或原生 Wayland/GLES2 已启用。
- 控制台日志不再同步驱动画面刷新。
- 指针输入已经从历史位移队列改为最新轴状态。
- 指针速度按 KRKR2 实际帧时间积分，并与帧率基本无关。
- KRKR2 内每帧最多派发一次指针移动。
- 千恋＊万花图层加载流畅度和 NEKOPARA Vol.2 画圆手感通过真机验收。
- 所有外部 KRKR2 修改已同步为 `GKD350HUltra/patches/` 下的可重放补丁。
- Xwayland 兼容回退仍可构建、启动和操作。
