# KrKr2 Next 第三运行时评估与交接

状态：建议进入“可选实验运行时”验证，不进入默认自动选择

日期：2026-07-29

## 1. 结论

建议把 AURKNIX 携带的 `KrKr2 Next` 接入为 KRKR 家族下的第三个**可选、实验性原生运行时**，但目前没有依据把它设为默认，也不应替换现有 `krkrsdl2` 或当前已修改的 `krkr2`。

它值得接入的理由：

- 它是已在 RK3566 Linux 系统中分发的 AArch64 SDL2/EGL/GLES 可执行程序，移植风险低于从零开始移植另一个引擎。
- 二进制显示出 XP3 filter、PSB、Motion/Emote、KTX/BC7、FFmpeg、OpenAL、OpenCV、WebP、Opus 及多种归档格式等兼容路径，可能覆盖当前两个运行时未遇到或未完整实现的游戏组合。
- ROCgalgame 已经有同一 `core=krkr` 下选择多个运行时的架构，无需再造一套前端核心协议。

不能把它直接设为默认的理由：

- 没有源码、对应 commit、可靠兼容列表和可复现构建；目前只有发布二进制。
- 二进制字符串只能证明代码或符号存在，不能证明相关游戏能够进入标题、存档并持续游玩。
- 它没有真实 `PackinOne` 实现，因此不能解决当前实机样本中剩余的两个失败游戏。
- 它仍不能在 ARM64 上执行任意 Win32 DLL；内建同名插件映射只能覆盖它明确实现的插件。
- 它依赖较新的系统库，已观察到 `GLIBC_2.38` 需求。ROCgalgame 目标系统能否直接运行必须先验证，不能靠随包携带 glibc 草率解决。
- 发布和再分发前需要确认二进制来源、许可证及依赖许可证。

因此，当前判断不是“必须接入才能解决已知问题”，而是“值得用低侵入方式接入并做 A/B 测试，以发现未知游戏上的独占兼容收益”。只有在它至少对真实游戏产生可复现的独占通过结果后，才有必要进入自动选择。

## 2. 分析对象与证据

### 2.1 AURKNIX 系统

系统镜像：

```text
D:\Works\Tyranor\20260728\update\AURKNIX-RK3566.aarch64-20260728\target\SYSTEM
```

它是 LZO SquashFS。镜像中相关文件：

```text
/usr/config/krkr2/krkr2_sdl2
/usr/bin/start_krkr2.sh
/usr/config/krkr2/krkr2.gptk
KRKR2 中英文手册
```

`krkr2_sdl2` 大小为 11,487,720 字节，SHA-256：

```text
125309019a8b2fe861d41e6c23e8f0d0450bd6536f54f3093c491a7eac511a4c
```

二进制自称 `KrKr2 Next`，构建路径字符串包含：

```text
/home/ubuntu/KrKr2-Next
```

AURKNIX 本身没有从源码构建它。其资源来自公开 release 资产：

```text
https://github.com/AveyondFly/console_mod_res/releases/download/v0.9/krkr2_sdl2.tar.gz
```

该仓库没有提供 KrKr2 Next 源码、commit、兼容游戏表或足以判断行为的变更记录。AURKNIX 的启动脚本和 `.kr2`/`gptokeyb` 配置证明“它能被系统启动”，不证明“它覆盖所有 KRKR2 游戏”。

### 2.2 当前 ROCgalgame 实现

当前前端已有：

- `KrkrRuntime::{Auto,Sdl2,Krkr2,Wine}`。
- `game.ini` 的 `runtime=` 解析。
- XP3 文件签名检测及基于入口类型的初步选择。
- 每游戏保存目录、日志、SDL 生命周期恢复、Wayland/Xwayland、虚拟鼠标和手柄桥接。

关键代码：

```text
D:\Works\ROCgalgame\src\game_library.h
D:\Works\ROCgalgame\src\game_scanner.cpp
D:\Works\ROCgalgame\src\krkr_core_adapter.cpp
D:\Works\ROCgalgame\tests\core_launch_test.cpp
```

现有策略文档：

```text
D:\Works\ROCgalgame\GKD350HUltra\KRKR_RUNTIME_STRATEGY.md
D:\Works\ROCgalgame\GKD350HUltra\KRKR_CORE_SELECTION_ADR.md
```

当前修改版 `krkr2` 的 2026-07-28 实机库扫描结果为：10 个样本中 8 个完成启动，2 个因明确缺失 `PackinOne` 而受控退出，0 个原生崩溃。结果见：

```text
D:\Works\ROCgalgame\KRKR2_GPU_REFACTOR_RESULT.md
```

## 3. 三个原生运行时对比

| 维度 | `krkrsdl2` | 当前 `krkr2` | `krkr2-next` |
| --- | --- | --- | --- |
| 引擎基础 | Kirikiri Z 的 SDL2 移植 | KrKr2 Emulator，Cocos2d-x 主机 | KrKr2 Next，直接 SDL2/EGL/GLES（由 ELF 推断） |
| 当前二进制大小 | 约 9.9 MB | 约 61.1 MB | 约 11.5 MB |
| 源码/维护性 | 有源码，可构建 | 有源码，ROCgalgame 已有大量补丁 | 无对应源码和 commit，只能黑盒使用 |
| 已知定位 | 轻量、目录项目和 KRKRZ 路径 | 当前主要原生兼容核心 | 候选补充核心 |
| 商业游戏保证 | 上游明确说不支持未修改商业游戏 | 上游列表很小，但本项目已实测扩展到 8/10 | 没有公开兼容列表 |
| 插件策略 | 依赖已移植功能/插件 | 多个 DLL 名映射为内建模块 | 有内部插件映射（由 ELF 推断） |
| XP3 filter | 当前实现需按游戏验证 | 有 `xp3filter` 源码实现 | 会加载 `xp3filter.tjs` |
| PSB/Motion/Emote | 本项目已有相关兼容工作 | PSB、MotionPlayer、EmotePlayer 已内建并持续补齐 | 存在对应实现，但 `setEmotePSBDecryptFunc` 明确未实现 |
| 图像/GPU | SDL2 渲染路径 | Cocos2d-x/OpenGL/GLES，已有设备补丁 | KTX、BC7、FBO fallback、`krkrgles` 字符串 |
| 音视频 | 已移植 FFmpeg 等路径 | FFmpeg、OpenAL/OpenCV 等依赖 | FFmpeg、OpenAL、OpenCV、WebP、Opus |
| 归档 | 以 KRKR 资源路径为主 | 7zip/libarchive/unrar 等依赖 | ZIP/TAR/7z/RAR 风格支持（由 ELF 推断） |
| `PackinOne` | 无真实实现 | 无真实实现 | 无真实实现 |
| 任意 Windows DLL | ARM64 上不能执行 | ARM64 上不能执行 | ARM64 上不能执行 |
| 设备集成 | 已完整接入 | 已接入保存、输入、显示和日志 | AURKNIX 可启动；尚未接 ROCgalgame 契约 |
| 主要风险 | 老商业游戏覆盖有限 | 体积/依赖大，兼容补丁维护成本高 | 黑盒、来源和许可证、GLIBC 2.38、真实覆盖未知 |

注意：单个可执行文件大小不能代表总包体。`krkr2-next` 的动态依赖闭包尚未统计，不能据 11.5 MB 判断它一定比当前 `krkr2` 更轻。

## 4. 对现有游戏库的实际意义

### 4.1 已知失败不会因此解决

当前两个失败样本的日志都显示真实 `PackinOne.dll` 行为缺失。KrKr2 Next 同样没有 `PackinOne` 实现，所以接入第三运行时不会直接把当前结果从 8/10 提升到 10/10。

游戏目录中出现 `PackinOne.dll` 也不等于一定失败。当前库有 5 个游戏携带该文件，但只有实际脚本调用并依赖其行为的游戏才会阻塞；例如 NEKOPARA Vol.2 仍能通过当前 `krkr2` 的启动和存档测试。因此静态扫描只能把它标成风险，不能直接宣布“不支持”。

### 4.2 可能产生收益的路径

KrKr2 Next 最值得测试的候选是：

- 含 `xp3filter.tjs`，且现有运行时在解密、挂载或脚本过滤阶段失败的游戏。
- 使用 KTX、BC7 或 krkrgles 风格资源，现有渲染路径不识别的游戏。
- 使用 PSB、Motion/Emote、LayerEx 组合，并且现有实现只覆盖了部分 API 的游戏。
- 使用其内建插件表中已有、而当前两个运行时没有实现的插件 API 的游戏。
- 对 Cocos2d-x 主机有兼容问题、但能在直接 SDL2/EGL/GLES 主机正常运行的游戏。

当前库里的 `桃色恋恋` 和 `向妈妈撒娇吧！` 含 `xp3filter.tjs`，但已经通过当前 `krkr2` 启动。因此它们适合作为第三运行时的回归/A-B 样本，不是已证明的独占收益。

### 4.3 “能否覆盖所有游戏”的答案

不能。三个原生运行时都无法执行任意 Win32 插件，也都没有真实 `PackinOne`。即使补齐已知插件，商业游戏还可能依赖私有解密、DirectShow、特殊字体/视频、厂商修改版 TJS、外部 EXE 或 DRM。

合理目标应是：三个原生运行时覆盖尽可能大的可验证子集；无法原生覆盖的游戏由后续 Wine + x86 执行层承担。Wine 也不能被当成天然 100% 兼容，仍需验证 32 位运行、视频、输入和图形栈。

## 5. 运行时选择原则

选择优先级必须固定为：

1. 用户在 `game.ini` 中的显式选择。
2. 用户确认过的、与当前游戏指纹和运行时 Build ID 匹配的结果。
3. 项目维护的已验证兼容数据库。
4. 静态特征产生的候选排序。
5. 隔离环境中的有界探测结果。
6. 保守默认值。

静态特征只能回答“先试哪个”，不能回答“哪个一定可玩”。进程存活或退出码为 0 也不能单独作为兼容通过。

建议支持：

```ini
core=krkr
runtime=auto
```

```ini
runtime=krkrsdl2
```

```ini
runtime=krkr2
```

```ini
runtime=krkr2-next
```

未来可增加：

```ini
runtime=wine
```

## 6. 建议的选择状态机

```text
扫描游戏
  |
  +-- game.ini 显式指定？ -- 是 --> 校验运行时存在 --> 使用或报告明确错误
  |
  +-- 已有同一游戏指纹 + Runtime Build ID 的用户验证？ -- 是 --> 使用
  |
  +-- 已有失败/黑名单结果？ --> 从候选中排除对应构建
  |
  +-- 静态扫描生成候选顺序
  |
  +-- 尚未允许自动探测？ -- 是 --> 沿用当前 Auto 行为
  |
  +-- 使用临时保存目录依次做有界探测
  |
  +-- 产生“推荐”，由用户确认可玩后锁定
```

锁定后不得因为扫描规则、超时或偶发退出而静默换核心。尤其在正式保存文件已经产生后，自动换核心可能导致保存格式差异、配置覆盖或误判损坏。

第三运行时验证期间应使用独立保存目录，例如：

```text
cache/krkr-probes/<game-fingerprint>/krkr2-next/
```

正式切换运行时前应保留现有保存目录，并记录运行时锁；如以后允许用户手动迁移，先备份再操作。

## 7. 静态候选规则

初始规则应只调整排序，不直接写死最终运行时：

| 特征 | 建议候选顺序/动作 | 说明 |
| --- | --- | --- |
| 目录项目、`startup.tjs` | `krkrsdl2` 优先 | 保持当前轻量路径；失败再试当前 `krkr2` |
| 原始或改名 XP3 签名 | 当前 `krkr2` 优先 | 保持现有已验证行为 |
| `xp3filter.tjs` | 提高当前 `krkr2` 与 Next 的分数 | 两者都有相关路径，不可仅凭文件名选 Next |
| `.ktx`、BC7 资源或已知 krkrgles 特征 | Next 优先探测 | 这是 Next 较明确的差异化候选 |
| `psbfile.dll`、`motionplayer.dll`、`emoteplayer.dll` | 当前 `krkr2` 与 Next 都进入候选 | 两者均有实现，必须用实际场景区分 |
| `layerEx*.dll` | 按具体 DLL/API 建能力表 | 不能把所有 LayerEx 当成同一功能 |
| `PackinOne.dll` 仅存在 | 标风险，不直接拒绝 | 文件可能未被当前游戏路径调用 |
| 日志已确认调用 PackinOne 后阻塞 | 标记“原生运行时已知不支持” | 不再无意义轮询三个原生核心 |
| 未在内建表中的 Win32 DLL | 标风险并收集插件名 | 原生 ARM64 不能加载 PE DLL；后续考虑 Wine |

首轮接入必须保持当前 `Auto` 行为不变。只有 A/B 数据足够后才提交选择规则变化，这样能把“第三核心接入回归”和“自动分类误判”分开定位。

## 8. 游戏指纹与结果缓存

建议新增版本化 JSON 缓存，逻辑位置可设为：

```text
cache/krkr-runtime-results.json
```

建议记录：

```json
{
  "schema": 1,
  "games": {
    "sha256:GAME_FINGERPRINT": {
      "features": ["xp3", "xp3filter.tjs", "motionplayer.dll"],
      "runtimes": {
        "krkr2-next": {
          "build_id": "sha256:RUNTIME_BINARY_HASH",
          "result": "user-verified",
          "milestones": ["archive-mounted", "startup-ended", "visible-frame", "input-accepted"],
          "tested_at": "2026-07-29T00:00:00+08:00",
          "note": "title and first scene passed"
        }
      },
      "locked_runtime": "krkr2-next"
    }
  }
}
```

游戏指纹不应只用目录名。建议组合：

- 入口文件相对路径、大小、mtime 和抽样哈希。
- `startup.tjs`、`Config.tjs`、`data.xp3` 等关键标记。
- 根目录和 `plugin/` 下插件文件名清单。
- XP3/归档文件名、大小及头尾分块哈希。
- 影响选择的 `game.ini` 内容。
- 指纹算法/schema 版本。

不建议每次启动对几十 GB 游戏做全量哈希。先用元数据和关键文件分块哈希；元数据变化时再重算。运行时 Build ID 至少包含可执行文件 SHA-256 和打包 manifest 版本。游戏或运行时变化后，旧探测结果必须失效，但用户显式 `game.ini` 仍优先。

结果状态建议区分：

```text
unknown
probe-failed
probe-passed
user-rejected
user-verified
known-unsupported
```

`probe-passed` 不能自动升级成 `user-verified`。

## 9. 有界兼容探测

探测必须在临时保存目录中运行，并至少收集以下里程碑：

- 归档或项目成功挂载。
- Startup 脚本结束，且没有未处理脚本异常。
- 创建窗口并持续输出可见、非全黑/非静止错误画面。
- 音频初始化；需要视频的样本还要验证视频解码。
- 至少一次前端输入被游戏消费。
- 在设定时间内进程稳定，没有崩溃、OOM 或错误弹窗。
- 能按退出组合键返回前端，SDL/显示状态恢复。

建议首轮探测时限 20-30 秒，但时限只用于发现早期失败。游戏停在合法的加载、警告或标题等待状态时，单纯超时不能判失败。

自动证据的可信度排序：

```text
用户完成标题/首场景/存读档确认
> 已知游戏兼容数据库
> 结构化运行时里程碑 + 图像/输入证据
> 日志关键字
> 进程存活
> 退出码
```

KrKr2 Next 是黑盒，若其日志没有稳定的结构化事件，第一版应输出“建议核心 + 测试结果”，由用户确认后锁定，不要假装探测器能够判断完整可玩性。

## 10. 打包与前端接入建议

建议隔离第三运行时及其动态库：

```text
cores/krkr/krkr2-next/
  krkr2_sdl2
  manifest.json
  lib/
```

`manifest.json` 至少记录：

- 上游下载 URL。
- 原始压缩包和可执行文件 SHA-256。
- ELF Build ID、架构、最低 GLIBC/GLIBCXX 版本。
- `DT_NEEDED` 完整列表及每个随包库的来源/许可证。
- 已验证设备、系统版本和显示后端。

不要直接采用 AURKNIX 的 `.kr2` stub 和 `gptokeyb` 作为 ROCgalgame 主设计。现有扫描器、原生输入桥、每游戏保存、日志和设置模型更完整。可以参考它的启动环境和依赖装载顺序，但应转换成 `KrkrCoreAdapter` 的运行时 profile。

不要随包替换系统 glibc。若目标系统低于 `GLIBC_2.38`，优先顺序应是：

1. 寻找或要求可复现源码并针对目标 sysroot 重编译。
2. 获取面向旧 glibc 构建的可信发布物。
3. 在完整隔离的兼容 rootfs/loader 中验证。
4. 无法满足时放弃该二进制，而不是污染全局运行环境。

## 11. 下一窗口实施阶段

### 阶段 A：只增加显式运行时，不改变 Auto

1. 增加 `KrkrRuntime::Krkr2Next`。
2. 解析并序列化 `runtime=krkr2-next`；可接受别名 `krkr2_next`，但规范输出只用 `krkr2-next`。
3. 在 `KrkrCoreAdapter` 增加独立可执行路径和独立 `LD_LIBRARY_PATH`。
4. 缺包、缺动态库或 GLIBC 不满足时返回结构化错误。
5. 保持 `Auto` 和 XP3 现有选择完全不变。
6. 补充扫描和 launch spec 单元测试。
7. 单独打包二进制和精确依赖闭包，不修改系统库。

### 阶段 B：设备冒烟与契约适配

1. 最小 TJS 目录项目。
2. 普通 `data.xp3`。
3. 改名 XP3/签名入口。
4. 保存路径隔离和存读档。
5. Wayland/Xwayland、全屏、比例、虚拟鼠标、ABXY/D-pad、退出组合键。
6. 音频、视频、中文/日文字体和输入法相关路径。
7. 进程退出后前端 SDL/显示恢复。

### 阶段 C：同库 A/B

对当前 10 个样本用相同临时保存、相同时间窗和相同输入脚本，分别运行当前 `krkr2` 与 Next。结果至少区分：

```text
未挂载
脚本错误
窗口但黑屏
到标题
输入有效
进入首场景
存档成功
读档成功
音频/视频/动画正常
崩溃/OOM
```

重点样本：

- 两个 `xp3filter.tjs` 游戏，用于判断 Next 是否只是能力重复。
- NEKOPARA Vol.2，用于 PSB/Emote/存档回归。
- 如月真绫，用于标题输入、语音和 Emote 动画。
- 千恋万花及另一个真实 PackinOne 阻塞游戏，用于确认 Next 同样失败且不会崩溃。
- 新增 KTX/BC7/krkrgles 特征游戏，才能真正验证 Next 的差异化价值。

### 阶段 D：兼容缓存和手动锁定

先实现指纹、结果缓存、UI/配置锁定及清除结果操作。此阶段仍不自动切换；静态规则只展示推荐顺序。

### 阶段 E：有限 Auto

只有满足第 12 节门槛后才允许 Auto 使用已验证缓存。对没有验证记录的游戏，默认继续沿用当前行为，或让用户选择“测试其他核心”。不要在首次普通启动时后台轮流运行三个核心。

## 12. 验收门槛

第三运行时进入正式可选列表前：

- 在目标设备上通过 ELF/GLIBC/动态依赖预检。
- 最小项目、XP3、保存、输入、退出和前端恢复全部通过。
- 不读取或修改真实游戏存档完成 A/B 扫描。
- 失败时有独立日志和结构化错误，不导致前端崩溃。
- 来源、再分发许可和依赖许可可接受。

第三运行时进入 Auto 候选前：

- 至少有 2 个真实游戏在现有两个原生运行时失败、而 Next 能进入首场景并完成存读档；只到标题不算独占通过。
- 当前已通过样本没有严重输入、显示、音视频或保存回归。
- 静态规则在一组未参与制定规则的游戏上验证过，误选可被结果缓存纠正。
- 自动选择永远尊重 `game.ini` 和用户锁定。
- 已产生正式保存后不静默换核心。

如果 A/B 结果只是和当前 `krkr2` 重复，则保留手动实验入口即可，不值得增加 Auto 复杂度。

## 13. 暂停或回滚条件

出现以下任一情况，应暂停正式集成或只保留开发者手动入口：

- 无法确认合法再分发条件或二进制来源。
- `GLIBC_2.38`/依赖闭包无法在目标系统安全满足。
- 没有任何可复现的独占兼容通过。
- 内存占用在约 1 GB 设备上频繁触发 OOM；AURKNIX 手册也提示大型游戏可能受内存限制。
- 无法接入 ROCgalgame 的保存隔离、输入或退出契约。
- 黑盒崩溃无法诊断，且发生率高于当前 `krkr2`。
- 自动规则误选导致保存风险或明显用户体验回退。

回滚应只需要移除 `cores/krkr/krkr2-next/` 并关闭该枚举的可选状态；现有 `krkrsdl2`、当前 `krkr2` 和 `Auto` 行为不得受影响。

## 14. 最终决策摘要

```text
是否接第三核心：是，但仅作为实验性运行时先接入。
是否能解决当前 PackinOne 失败：不能。
是否已证明比当前 krkr2 覆盖更广：没有。
是否值得测试：值得，尤其是 KTX/BC7、krkrgles 及不同插件组合。
是否立即进入 Auto：不进入。
选择依据：显式配置 > 用户验证缓存 > 兼容数据库 > 静态排序 > 隔离探测。
长期无法原生覆盖的路径：后续 Wine + x86 执行层。
```

下一窗口应从“阶段 A”开始，只完成第三运行时的显式选择、打包预检和测试，不同时修改 Auto 策略。
