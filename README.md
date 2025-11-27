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

Kaban Engine builds and runs on **Windows (Win32 + OpenGL)**. It contains multiple core systems, some functional and some still in their earliest stage:

**Implemented components include:**

* Editor framework with multiple modes
* Asset import/edit/build system
* Basic rendering pipeline
* Prototype navigation mesh generation & agent movement
* Internal tools for game data management

**Not yet ready / incomplete:**

* Entity system (very early)
* Animation system (mostly conceptual)
* Several editor tools depend on unfinished systems
* No external documentation or build guidance yet
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

Most required third-party libraries are included in the repository, but the project **currently requires manual setup of include paths and build configuration**.

There are no guaranteed build instructions yet. If you choose to compile it:

* You will need a Windows toolchain (Visual Studio recommended)
* You may need to adjust include/library paths manually
* Expect missing features, breakage, or incomplete subsystems

The project will eventually include proper build instructions as it matures.

---

### License (Summary)

Kaban Engine is distributed under a **non-commercial educational license.**
You may read, study, fork, and modify the source code.
You may share derivative works publicly, but **commercial use is prohibited**.

Some parts of this engine contain code influenced by **Handmade Hero**, which is also shared for educational purposes only. Third-party libraries remain under their own licenses.

For complete terms, see:
→ **[`LICENSE.txt`](./LICENSE.txt)**

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
