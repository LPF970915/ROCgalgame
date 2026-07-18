# KRKR Minimal TJS Smoke Test

This is a plugin-free engine baseline. It exercises project-directory entry,
window creation, a 1280x720 primary layer, text drawing and click dispatch.

To expose it in ROCgalgame without duplicating files, create a directory link
under `games/krkr` or temporarily copy this directory there. The expected
result is a dark 1280x720 screen with English and Chinese text; clicking exits
to the same frontend shelf position. The core should be launched with
`fonts/ui_font_02.ttf`, whose internal face is registered by the SDL core.

This project is intentionally not a commercial-game compatibility test and
does not cover KAG, plugins, audio, save/load or video.
