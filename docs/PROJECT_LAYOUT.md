# 项目布局、清理边界与开发流程

本文档记录 2026-08-03 的项目盘点结果。实际设备目标名称是
`GKD350H Ultra` 和 `H700 34xxSP`；仓库没有名为 `GKD30H` 的独立目标。

本次清理实际释放约 23.1 GiB：约 12.5 GiB 已解压下载/开发缓存、5.0 GiB
重复清理归档、4.7 GiB 已合并的原始设备 sysroot，以及约 0.9 GiB 旧 staging、
诊断转储和历史本地包。当前 16.4 GiB `build/`、1.4 GiB `GKD350HUltra/tools/`
和两个设备 staging 均按下述规则保留。

## 活动源码

- `src/`：SDL2 前端。核心启动只经过 adapter、registry、launch service 和
  process runner，不包含 KRKR2 引擎源码。
- `GKD350HUltra/`：AArch64/glibc 2.34 工具链、增量构建、补丁、真机探针、
  部署和 GKD 发布脚本。
- `H700/`：复用 `GKD350HUltra/dist_glibc234` 的 H700 包装层，不编译核心。
- `tests/`：宿主单元测试、设备烟测脚本和最小 KRKR/KRKR2 兼容性项目。
- `scripts/`：资产打包、设备输入工具以及测试语料辅助脚本。
- `ui/`、`fonts/`、`sounds/`、`shaders/`：运行时资产。

KRKR2、KRKRSDL2 和 ONS 的活动源码分别位于
`D:\Works\ROCgalgame-krkr2-port`、`D:\Works\Tyranor\krkrsdl2` 和
`D:\Works\Tyranor\OnscripterYuri`。FFmpeg 头文件来自
`D:\Works\ROCgalgame-ffmpeg-n6-headers`。四个外部工作树必须保持在各自 lock
文件记录的干净提交；清理 ROCgalgame 时不能触碰它们。

## 必须保留

以下忽略目录是昂贵且可复用的开发状态，不是普通缓存：

| 路径 | 用途 |
| --- | --- |
| `build/gkd350h-glibc234/vcpkg` | KRKR2 目标依赖、下载和包缓存 |
| `build/gkd350h-glibc234/sysroot` | H700/GKD 共用 glibc 2.34 sysroot |
| `build/gkd350h-glibc234/krkr2` | KRKR2 CMake、对象和链接缓存 |
| `build/gkd350h-glibc234/krkrsdl2` | KRKRSDL2 增量缓存 |
| `build/gkd350h-glibc234/frontend` | 前端 AArch64 增量对象 |
| `GKD350HUltra/tools` | 固定版本 CMake 与 vcpkg 源码/下载缓存 |
| `GKD350HUltra/dist_glibc234` | GKD 当前 staging 与发布输入 |
| `H700/dist_lowglibc` | H700 当前 staging |

`games/`、`game_covers/`、`saves/` 和运行时 `cache/` 是本机用户数据，
构建、打包和日常清理都不得覆盖。发布脚本只创建空目录，不会把这些内容打包。

`.local/` 只保留当前基线的真机验收证据和最新源码状态报告。源码恢复依赖私有
fork 的提交历史和 lock 文件，不保留脏工作树副本。

## 可清理内容

可以直接清理的内容包括：

- `GKD350HUltra/Downloads/.<package>.stage/` 临时打包目录；
- 已被新 staging 替代的 `GKD350HUltra/dist_lowglibc/`；
- 已合并进当前 sysroot、需要时可从真机重新同步的 `GKD350HUltra/sysroot_device/`；
- `GKD350HUltra/screenshots/`、过期的帧缓冲转储和交互探针输出；
- 已确认解压到 `games/` 的 `cache/05fx_downloads/` 下载副本；
- `_tmp_*`、Python `__pycache__`、旧测试可执行文件和重复清理归档；
- H700 `Downloads/` 中已经被更高版本替代的本地 ZIP。

GKD `Downloads/` 是本地应用更新通道，不进入 Git。只保留 lock 文件引用的基线
包、当前已验收包及其 `.sha256`；隐藏 staging、旧版本和失败包可直接删除。

## 打包矩阵

只同步资产并验证 GKD staging，不编译：

```powershell
.\GKD350HUltra\build_package.ps1 -Mode Fast -Output Stage
```

从同一份 glibc 2.34 runtime 生成 H700 staging，不编译：

```powershell
.\H700\build_package.ps1 -Output Stage -Version 0.04
```

需要 ZIP 时把 `Stage` 改为 `Zip`。GKD 包内核心哈希的唯一基线是
`GKD350HUltra/release_core_hashes.sha256`；ZIP 校验器也读取这份文件，更新
真机验证过的核心时必须同步更新该清单。

`build_release_docker.ps1` 会干净重编前端，`build_glibc234_all.*` 会触发完整
核心和依赖构建。除非用户主动要求，不得运行这些命令，也不得使用 `-Mode Full`
或 `-Clean`。

## KRKR2 实机循环

1. 在真机上用现有核心和最小测试项目稳定复现问题，保存必要日志即可。
2. 在锁定的 KRKR2 私有 fork 做单一能力修复，一个修复一个提交，不维护重复 patch。
3. 用户明确批准后，仅运行 `build_krkr2.ps1 -Mode FastBuild -Jobs 1`，复用现有
   CMake/vcpkg/sysroot，不重新配置依赖图。
4. 使用 `deploy_krkr2.ps1` 原子替换真机 KRKR2；脚本会校验 SHA-256 并保留回滚副本。
5. 先跑最小探针，再跑目标游戏；记录通过/失败矩阵。模拟器和 QEMU 只做补充诊断，
   不作为最终兼容性结论。
6. 真机确认后更新 `release_core_hashes.sha256`，再分别做 GKD/H700 复用式 Stage/ZIP
   验证。

## 测试脚本结论

`tests/*.cpp` 均由 `make test` 引用，不是废弃脚本。`tests/krkr/`、
`GKD350HUltra/probes/` 和 `test_krkr_*_qemu.ps1` 覆盖不同的核心能力，也应保留。
原根目录 `_tmp_gfx_effect` 已归位为 `tests/krkr/gfx_effect_compat`；原始 Windows
核心可执行文件不属于测试夹具或发布输入。
