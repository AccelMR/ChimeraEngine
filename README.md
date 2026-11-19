# Chimera

Chimera is a game engine written in C++20. It started as a school project and kept growing.
Today it has an editor, a Lua scripting layer, 3D and 2D rendering, and a small runtime that
plays a finished project without the editor.

It runs on Windows and Linux and draws with Vulkan, or Direct3D 12 on Windows. It's a
personal project, so expect rough edges.

## Quick start

You need:

- CMake 3.25 or newer
- A C++20 compiler: Visual Studio 2022 on Windows, Clang on Linux (Clang on Windows works too)
- The [Vulkan SDK](https://vulkan.lunarg.com/), even if you only plan to use Direct3D 12,
  because its shader compiler (`dxc`) builds every shader

Get the code and its dependencies (they are git submodules):

```sh
git clone https://github.com/AccelMR/ChimeraEngine.git
cd ChimeraEngine
git submodule update --init --recursive
```

Build it with one of the CMake presets:

```sh
# Windows (Visual Studio 2022)
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug

# Linux (Clang + Ninja)
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
```

Run the tests with `ctest --preset linux-clang-debug` (or `windows-msvc-debug`).

You can also skip the command line and build from an IDE. Both read the same presets from
`CMakePresets.json`:

- **VS Code** with the CMake Tools extension: open the folder, pick a configure and build
  preset in the CMake tab, then build. Personal presets (more build jobs, another folder) go
  in a `CMakeUserPresets.json` next to it; git ignores it.
- **Visual Studio 2022** with its CMake tools installed: use File > Open > Folder on the
  repository and pick a preset from the toolbar.

Once a project is open in the editor, File > Generate IDE Debug Settings... writes debugger
settings for VS Code and Visual Studio that start the editor or the game on that project.
The same can be done without opening the editor; on a fresh clone the script also fetches
the submodules and builds the editor first:

```sh
GenerateIdeSettings.bat Examples\Example2D      # Windows
./GenerateIdeSettings.sh Examples/Example2D     # Linux, add -Debugger=lldb for LLDB
```

Open one of the example projects in the editor:

```sh
# Windows
build/vs/lib/Debug/chEditor.exe -project=Examples/Example2D

# Linux
build/linux-clang-debug/lib/chEditor -project=Examples/Example2D
```

Without `-project` the editor opens a project browser where you can pick or create one. To
play a project without the editor, run `chGame` the same way. Add
`-GraphicsAPI=chDX12` to use Direct3D 12 instead of Vulkan.

There are two examples:

- **Examples/BaseExample**: a small 3D scene with a player you can move, a follow camera,
  an animated box and a level that loads when you walk to it.
- **Examples/Example2D**: a little top-down shooter with sprites, bullets, particles, sound
  and a CRT effect. Its own README lists what each part shows.

## What it can do today

**Editor**
- Dockable panels: scene tree, inspector, content browser, output log, profiler
- Move, rotate and scale gizmos, a grid, a 2D view, and a camera you fly with the mouse
- Undo and redo for scene edits, prefabs, and copy and paste
- Press Play to test the game inside the editor; your edits come back when you stop
- Import models (FBX, glTF, OBJ...), images (PNG, JPG, TGA, HDR, WebP), fonts, sounds and
  shaders
- An animation graph editor with a live preview
- Package Project: makes a folder with the game and only the assets it uses

**Rendering**
- Vulkan and Direct3D 12, picked when the program starts
- Deferred and forward lighting with PBR materials
- Directional, point and spot lights, all with shadows
- Lighting from an HDR sky image, plus ambient occlusion
- Debug views (normals, depth, wireframe and more) and a built-in profiler

**2D**
- Sprites, sprite sheets and frame animation
- Text with any TTF or OTF font, sharp at any size
- Large numbers of bullets and particles: tens of thousands at a time stay under a
  millisecond on a desktop PC
- Floating damage numbers, post-process effects and per-sprite shaders written in HLSL

**Gameplay**
- Lua scripts on objects, reloaded while you edit them
- Script values you can tweak from the inspector
- Keyboard, mouse and gamepad, with actions players can rebind
- Raycasts, spawning and destroying objects, levels that load and unload
- Skeletal animation with blending, state machines and masked layers
- 3D and 2D sound with music streaming
- Engine plugins (projectiles, particles) that a project turns on or off

## What it can't do yet

- No physics engine. There are raycasts, but no rigid bodies or collisions between objects.
- No game UI system (menus, buttons, health bars). It's being worked on.
- No macOS, mobile or consoles.
- Game code is Lua only. You can't add C++ to a project without changing the engine.
- Assets load all at once on the main thread, so big levels cause a hitch.
- Textures aren't compressed, so packaged games use more memory and disk than they should.
- Linux gets less testing than Windows.
- No networking.
- Editing an asset (a material, a sprite sheet) can't be undone yet; only scene edits can.

## Repository layout

| Folder | What's inside |
|---|---|
| `chUtilities` | Math, strings, files, logging, config files |
| `chCore` | The engine: rendering, assets, scenes, input, scripting, audio |
| `chGraphicsAPIs` | The Vulkan and Direct3D 12 back ends, loaded as plugins |
| `chCodecs` | Importers for models, images, fonts, sounds and shaders (editor only) |
| `chPlugins` | Optional engine plugins: projectiles, particles |
| `chEditor` | The editor |
| `chGame` | The runtime that plays a project |
| `Examples` | Sample projects |

Third-party libraries (SDL3, Lua, ImGui, assimp, miniaudio, libwebp and others) live in
submodules and keep their own licenses.

## License

Chimera is released under the [MIT License](LICENSE). The fonts in `chEditor/Content/Fonts`
and `chCore/Content/Fonts` keep their own licenses (SIL Open Font License).
