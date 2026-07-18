@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "PROJECT_WIN=%%~fI"
set "BASH_EXE="

if not "%MSYS2_ROOT%"=="" if exist "%MSYS2_ROOT%\usr\bin\bash.exe" set "BASH_EXE=%MSYS2_ROOT%\usr\bin\bash.exe"
if "%BASH_EXE%"=="" if exist "D:\Program Files\MSYS2\usr\bin\bash.exe" set "BASH_EXE=D:\Program Files\MSYS2\usr\bin\bash.exe"
if "%BASH_EXE%"=="" if exist "C:\msys64\usr\bin\bash.exe" set "BASH_EXE=C:\msys64\usr\bin\bash.exe"
if "%BASH_EXE%"=="" if exist "D:\msys64\usr\bin\bash.exe" set "BASH_EXE=D:\msys64\usr\bin\bash.exe"
if "%BASH_EXE%"=="" if exist "%LOCALAPPDATA%\Programs\msys64\usr\bin\bash.exe" set "BASH_EXE=%LOCALAPPDATA%\Programs\msys64\usr\bin\bash.exe"

if "%BASH_EXE%"=="" (
  echo [ERROR] MSYS2 bash not found. Install MSYS2 or set MSYS2_ROOT.
  pause
  exit /b 1
)

echo [INFO] Controls:
echo        Arrow keys = D-pad
echo        A or Z     = A / click / hold to drag
echo        B or X     = B / cancel
echo        I/J/K/L    = left stick / free virtual mouse movement
echo        Controller = physical controls are also accepted
echo.

set "MSYSTEM=UCRT64"
set "CHERE_INVOKING=1"
"%BASH_EXE%" -lc "taskkill //F //IM rocgalgame_sdl.exe //IM onsyuri.exe //IM tvpwin64.exe >/dev/null 2>&1 || true"

"%BASH_EXE%" -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; PROJECT_UNIX=$(cygpath -u \"$PROJECT_WIN\"); PROJECT_NATIVE=$(cygpath -w \"$PROJECT_UNIX\"); cd \"$PROJECT_UNIX\"; export ONS_ROOT=/d/Works/Tyranor/OnscripterYuri; make -j4 TARGET=Windows/build/rocgalgame_sdl.exe OBJDIR=Windows/build/obj; bash Windows/build_onsyuri_windows.sh; echo '[INFO] Starting ROCgalgame Windows preview...'; ROCGALGAME_ROOT=\"$PROJECT_NATIVE\" ROCGALGAME_WINDOWED=1 ROCGALGAME_WINDOW_W=1000 ROCGALGAME_WINDOW_H=900 ./Windows/build/rocgalgame_sdl.exe"

set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  echo.
  echo [ERROR] Preview failed, exit code %RC%.
  pause
  exit /b %RC%
)

echo [INFO] Preview exited normally.
pause
exit /b 0
