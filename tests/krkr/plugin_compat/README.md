# KRKR Plugin Compatibility Smoke Test

This headless TJS project verifies the built-in compatibility modules used by
the KRKR port, including the real `KAGParserEx` parser and `textrender` text
layout implementation, the real PSB parser, and the no-D3D Motion/E-mote
backend. It checks Chinese glyph layout and writes a small `saveStruct` probe
inside the isolated test data path. It also verifies that copying a 437x184
Motion source into a 1920x1080 layer through the SeparateLayer and D3D
adaptors does not shrink the target canvas. A successful run exits with code 0.

Run it through `Windows/test_krkr_windows.ps1`, which explicitly passes
`fonts/ui_font_02.ttf`. Do not rely on the host having `Noto Sans CJK JP`
installed; doing so turns the text-render check into an environment-dependent
false failure.

For the GKD350H Ultra ARM64 build, run
`GKD350HUltra/test_krkr_plugin_compat_qemu.ps1`. It also verifies that the
`save2` probe produced a non-empty file and that the full plugin sequence
reached its final completion marker.
