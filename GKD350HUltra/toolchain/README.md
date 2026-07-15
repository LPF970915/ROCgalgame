# GKD350H Ultra toolchain

The first GKD350H Ultra build route uses the WSL Ubuntu cross toolchain:

- target triple: `aarch64-linux-gnu`
- compiler prefix: `aarch64-linux-gnu-`
- build env: `CROSS_TOOL_PREFIX=aarch64-linux-gnu`

This matches the target ROCKNIX aarch64/glibc userland and is compatible with
the RK3576S/RK356x Linux development flow. Keep downloaded or experimental
toolchain archives under this folder if a device-specific toolchain is tested
later.

Current local probe:

```text
aarch64-linux-gnu-g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
target: aarch64-linux-gnu
path: /usr/bin
```
