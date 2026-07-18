# KRKR Plugin Compatibility Smoke Test

This headless TJS project verifies the built-in compatibility modules used by
the KRKR port, including the real `KAGParserEx` parser and `textrender` text
layout implementation, the real PSB parser, and the no-D3D Motion/E-mote
backend. It checks Chinese glyph layout without writing save data. A successful
run exits with code 0.

Run it through `Windows/test_krkr_windows.ps1`, which explicitly passes
`fonts/ui_font_02.ttf`. Do not rely on the host having `Noto Sans CJK JP`
installed; doing so turns the text-render check into an environment-dependent
false failure.
