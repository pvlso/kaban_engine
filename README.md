# Kaban Engine

**A stubborn attempt to build a game engine from scratch.**
Educational, experimental, and not intended for commercial use.

---

### Overview

Kaban Engine is a personal game engine project written in C and C++, originally inspired by Casey Muratori’s *Handmade Hero*. It started as a learning exercise and gradually evolved into its own independent codebase.

The engine is currently **in active development**, incomplete, and not intended for production use. Its purpose is to explore engine architecture, asset pipelines, tools, rendering, and navigation systems by building them from scratch.

If you want to learn more about the engine development check out my [website](https://www.pvlso.com)

---

### Current State

Kaban Engine builds and runs on **Windows and Linux (X11)**. The platform layer uses **GLFW** for windows and input, **OpenGL** for rendering and **miniaudio** for sound, with a thin per-OS layer underneath. It contains multiple core systems, some functional and some still in their earliest stage:

**Implemented components include:**

* Editor framework with multiple modes
* Asset import/edit/build system
* Basic rendering pipeline
* Prototype navigation mesh generation & agent movement
* Internal tools for game data management
* Hot reloading of the engine code
* Projects: games live in `projects/`, the editor compiles them, runs them and hot reloads them on save
* Separate debug/profiler window (on a second monitor when one is available)

**Not yet ready / incomplete:**

* Games only run inside the editor, there is no standalone export yet
* Entity system (very early)
* Animation system (mostly conceptual)
* Several editor tools depend on unfinished systems
* No external documentation yet
* No guarantees of stability or compatibility

This repository is published to **show progress openly** and provide a reference for anyone interested in building similar systems from scratch.

---

### Who Is This For?

* Anyone learning how game engines are built from the ground up
* Developers who want to explore experimental or low-level engine code
* Students looking for study material beyond “use Unity/Unreal”
* Anyone curious about raw engine architecture and tooling

It is **not** intended to be used as a base for commercial games.

---

### Building the Engine

All third-party libraries (GLFW, GLEW, Nuklear, miniaudio, stb_image) are included in the repository as source and built by the build scripts. Output goes to `build/`.

**Windows**

Requires Visual Studio or the VS Build Tools with the *Desktop development with C++* workload (for the Windows SDK). The script finds and sets up the MSVC environment itself, so a normal terminal works.

```
build.bat                debug build (clang-cl if installed, otherwise cl)
build.bat release        optimized build
build.bat clang | msvc   force a compiler
build.bat clean          remove the build directory
```

Run `build\kaban.exe`.

**Linux**

Requires clang (or gcc) and the X11 development headers (X11, Xrandr, Xinerama, Xcursor, Xi). If something is missing, the script prints the install command for your package manager.

```
./build.sh               debug build (clang if installed, otherwise gcc)
./build.sh release       optimized build
./build.sh clean         remove the build directory
```

Run `build/kaban`.

**Projects**

A game is a folder `projects/<name>/` with `<name>.cpp` in it. The game includes only `code/engine_api.h` and exports `GameUpdateAndRender`. The title screen lists the projects, and **New Project** writes a small template to start from.

**Run** builds the selected project, loads it, and rebuilds and hot reloads it whenever one of its source files changes. Build errors and the build log show up in the debug window's Project panel, along with Stop and Restart. A project can also be built by hand:

```
build.bat project <name> [debug|release] [clang|msvc]
./build.sh project <name> [debug|release]
```

The output goes to `projects/<name>/build/`.

**Notes**

* The engine looks for `data/` and `projects/` next to the `build/` directory and creates them if they're missing.
* Every project has its own assets in `projects/<name>/data/` and its own `editor_metadata.json`. The Assets Mode and a running game use the selected project's data. The engine's own `data/` is for the editor only. Game assets are not part of the repository.
* Rebuilding while the editor is running hot reloads the engine code.
* Editor keys: `Alt+E` toggles the UI, `Alt+M` mutes audio, `Alt+Enter` toggles fullscreen, `Alt+F4` quits.
* Expect missing features, breakage, or incomplete subsystems.

---

### License (Summary)

Kaban Engine is distributed under a **non-commercial educational license.**
You may read, study, fork, and modify the source code.
You may share derivative works publicly, but **commercial use is prohibited**.

Some parts of this engine contain code influenced by **Handmade Hero**, which is also shared for educational purposes only. Third-party libraries remain under their own licenses.

For complete terms, see:
→ **[`LICENSE.md`](./LICENSE.md)**

---

### Contributions

At this stage, contributions are not actively requested. However:

* Forks for educational purposes are welcome
* Issue reports are fine if they are technical and specific
* Pull requests may be considered once the systems are more stable

---

### Final Notes

This engine is not a product. It is a work in progress with the goal of understanding systems by creating them. Publishing the repository is not a claim of readiness, but a commitment to transparent development.

Progress will be slow, experimental, and sometimes messy—but it will continue.
