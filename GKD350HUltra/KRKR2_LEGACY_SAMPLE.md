# KrKr2 Legacy Acceptance Sample

## Test Game

- Library folder: `games/【汉化】丧服萝莉紧缚奇谭 美少女性奴隶调教`
- Engine family: KiriKiri2
- Observed engine version: `2.32.2.426`
- Windows architecture: 32-bit
- Main archive: `data.xp3`
- Visible native plugin: `krmovie.dll`
- Additional archives: `mofuku_exp.xp3`, `mofuku_wordchs.xp3`

The game is the first acceptance target for the second native `krkr2` runtime.
It must not receive a permanent `runtime=krkr2` override until the AArch64
binary is installed, because that would make the current library entry
unlaunchable during the port.

When the core reaches the first launch gate, use this temporary `game.ini`:

```ini
core=krkr
runtime=krkr2
entry=.
```

The same staged configuration is tracked at
`krkr2_test_profiles/mofuku_legacy.game.ini`. It is deliberately outside the
ignored private game directory until the core exists.

## Acceptance Order

1. The core opens the project and writes a per-game log without exit code 1.
2. XP3 archives mount and startup scripts reach the title screen.
3. Chinese text and game fonts render correctly.
4. Controller input, virtual cursor, left/right click and the exit chord work.
5. BGM, voices and sound effects play without format substitution.
6. `krmovie` calls either use the native FFmpeg movie path or fail gracefully.
7. Saves are written only below `saves/krkr/<game>` and survive relaunch.
8. Exiting returns to the existing ROCgalgame process and restores the shelf.

Failures should be recorded by missing API, plugin module, codec or renderer
capability. Do not add a game-title-specific branch to either runtime.
