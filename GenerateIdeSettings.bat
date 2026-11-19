@echo off
rem Writes the VS Code and Visual Studio debug settings of a project without opening the
rem editor. On a fresh clone it first fetches the submodules and builds the editor
rem (windows-msvc preset, Debug).
rem
rem   GenerateIdeSettings.bat <project folder or .chproject> [options]
rem
rem Options: -NoVSCode, -NoFollowCMakeTools, -NoVSSolution, -NoVSFolder
setlocal

set "ROOT=%~dp0"
if "%~1"=="" goto usage
for %%I in ("%~1") do set "PROJECT=%%~fI"

set "OPTIONS="
:collectOptions
shift
if "%~1"=="" goto optionsDone
set "OPTIONS=%OPTIONS% %1"
goto collectOptions
:optionsDone

rem 1. Submodules: a line starting with "-" is one that was never checked out.
if not exist "%ROOT%.git" goto submodulesDone
set "MISSING_SUBMODULES="
for /f "delims=" %%L in ('git -C "%ROOT%." submodule status --recursive 2^>nul ^| findstr /b /c:"-"') do set "MISSING_SUBMODULES=1"
if not defined MISSING_SUBMODULES goto submodulesDone
echo Fetching the submodules...
git -C "%ROOT%." submodule update --init --recursive
if errorlevel 1 goto failed
:submodulesDone

rem 2. Any editor already built; otherwise build one.
set "EDITOR="
for %%E in ("build\vs\lib\Debug" "build\vs\lib\Release" "build\windows-clang-debug\lib" "build\windows-clang-release\lib") do (
  if not defined EDITOR if exist "%ROOT%%%~E\chEditor.exe" set "EDITOR=%ROOT%%%~E\chEditor.exe"
)
if defined EDITOR goto run

where cmake >nul 2>&1
if errorlevel 1 (
  echo CMake was not found on the PATH. Install CMake 3.25 or newer.
  goto failed
)
if not defined VULKAN_SDK (
  echo VULKAN_SDK is not set. Install the Vulkan SDK and open a new terminal.
  goto failed
)
echo No editor build found; building chEditor with the windows-msvc preset ^(Debug^)...
pushd "%ROOT%"
cmake --preset windows-msvc
if errorlevel 1 (popd & goto failed)
cmake --build --preset windows-msvc-debug --target chEditor
if errorlevel 1 (popd & goto failed)
popd
set "EDITOR=%ROOT%build\vs\lib\Debug\chEditor.exe"

rem 3. The editor writes the files and exits.
:run
echo Using %EDITOR%
"%EDITOR%" -project="%PROJECT%" -GenerateIdeDebugSettings -log%OPTIONS%
if errorlevel 1 goto failed
exit /b 0

:usage
echo Usage: %~nx0 ^<project folder or .chproject^> [options]
echo Options: -NoVSCode, -NoFollowCMakeTools, -NoVSSolution, -NoVSFolder
exit /b 1

:failed
echo Failed.
exit /b 1
