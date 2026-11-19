#!/usr/bin/env bash
# Writes the VS Code debug settings of a project without opening the editor. On a fresh
# clone it first fetches the submodules and builds the editor (linux-clang-debug preset).
#
#   ./GenerateIdeSettings.sh <project folder or .chproject> [options]
#
# Options: -NoVSCode, -NoFollowCMakeTools, -Debugger=gdb|lldb
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
  echo "Usage: $0 <project folder or .chproject> [options]"
  echo "Options: -NoVSCode, -NoFollowCMakeTools, -Debugger=gdb|lldb"
  exit 1
fi
PROJECT="$(realpath "$1")"
shift

# 1. Submodules: a line starting with "-" is one that was never checked out.
if [ -e "$ROOT/.git" ] && git -C "$ROOT" submodule status --recursive | grep -q '^-'; then
  echo "Fetching the submodules..."
  git -C "$ROOT" submodule update --init --recursive
fi

# 2. Any editor already built; otherwise build one.
EDITOR=""
for folder in build/linux-clang-debug/lib build/linux-clang-release/lib; do
  if [ -x "$ROOT/$folder/chEditor" ]; then
    EDITOR="$ROOT/$folder/chEditor"
    break
  fi
done

if [ -z "$EDITOR" ]; then
  if ! command -v cmake > /dev/null; then
    echo "CMake was not found on the PATH. Install CMake 3.25 or newer."
    exit 1
  fi
  echo "No editor build found; building chEditor with the linux-clang-debug preset..."
  (cd "$ROOT" && cmake --preset linux-clang-debug &&
    cmake --build --preset linux-clang-debug --target chEditor)
  EDITOR="$ROOT/build/linux-clang-debug/lib/chEditor"
fi

# 3. The editor writes the files and exits.
echo "Using $EDITOR"
"$EDITOR" -project="$PROJECT" -GenerateIdeDebugSettings "$@"
