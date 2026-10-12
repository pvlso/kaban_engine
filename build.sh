#!/usr/bin/env bash
#
# Kaban Engine - Linux build
#
#   ./build.sh            debug build
#   ./build.sh release    optimized build
#   ./build.sh clean      remove the build directory
#   ./build.sh project <name> [debug|release]
#                         build projects/<name>/<name>.cpp into projects/<name>/build/<name>.so,
#                         the editor runs this one
#
# Compiler: clang is preferred, gcc is used as fallback.
# Override with CC=... CXX=... ./build.sh
#

set -e

Project=""
if [ "$1" == "project" ]; then
    if [ -z "$2" ]; then
        echo "Usage: ./build.sh project <name> [debug|release]"
        exit 1
    fi
    Project="$2"
    shift 2
fi

Mode=${1:-debug}
Root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
Code="$Root/code"
Build="$Root/build"

if [ "$Mode" == "clean" ]; then
    rm -rf "$Build"
    echo "Cleaned."
    exit 0
fi

if [ "$Mode" != "debug" ] && [ "$Mode" != "release" ]; then
    echo "Usage: ./build.sh [debug|release|clean]"
    exit 1
fi

if [ "$(uname -s)" != "Linux" ]; then
    echo "ERROR: build.sh only supports Linux for now (got $(uname -s)). On Windows use build.bat."
    exit 1
fi

#
# NOTE(pvlso): Package manager hint, used in the error messages below
#
if command -v dnf > /dev/null; then
    InstallHint="sudo dnf install clang libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel"
elif command -v apt-get > /dev/null; then
    InstallHint="sudo apt install clang libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev"
elif command -v pacman > /dev/null; then
    InstallHint="sudo pacman -S clang libx11 libxrandr libxinerama libxcursor libxi"
elif command -v zypper > /dev/null; then
    InstallHint="sudo zypper install clang libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel"
else
    InstallHint="install clang and the X11 development headers (X11, Xrandr, Xinerama, Xcursor, Xi)"
fi

#
# NOTE(pvlso): Compiler detection
#
if [ -z "$CC" ] || [ -z "$CXX" ]; then
    if command -v clang > /dev/null && command -v clang++ > /dev/null; then
        CC=clang
        CXX=clang++
    elif command -v gcc > /dev/null && command -v g++ > /dev/null; then
        CC=gcc
        CXX=g++
    else
        echo "ERROR: No C/C++ compiler found (looked for clang/clang++ and gcc/g++)."
        echo "       $InstallHint"
        exit 1
    fi
fi

#
# NOTE(pvlso): System headers GLFW needs. These can't be vendored, they come from the distro.
# Projects don't use GLFW, so they skip the check.
#
MissingHeaders=""
if [ -n "$Project" ]; then
    Headers=""
else
    Headers="X11/Xlib.h X11/extensions/Xrandr.h X11/extensions/Xinerama.h X11/Xcursor/Xcursor.h X11/extensions/XInput2.h"
fi
for Header in $Headers; do
    if ! echo "#include <$Header>" | $CC -x c -fsyntax-only - > /dev/null 2>&1; then
        MissingHeaders="$MissingHeaders $Header"
    fi
done
if [ -n "$MissingHeaders" ]; then
    echo "ERROR: Missing X11 development headers:$MissingHeaders"
    echo "       $InstallHint"
    exit 1
fi

echo "Building in ${Mode^^} mode with $CXX ($($CXX --version | head -n 1))"

#
# NOTE(pvlso): Flags
#
Warnings="-Wall -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-missing-braces -Wno-switch -Wno-unused-value -Wno-sign-compare -Wno-missing-field-initializers -Wno-format -Wno-enum-compare"
if [[ "$CXX" == *clang* ]]; then
    Warnings="$Warnings -Wno-writable-strings -Wno-null-dereference"
else
    Warnings="$Warnings -Wno-write-strings -Wno-class-memaccess -Wno-unused-result -Wno-strict-aliasing -Wno-deprecated-enum-enum-conversion"
fi

CommonFlags="-mavx2 -ffast-math -fno-exceptions -fno-rtti -fPIC -I$Code"
if [ "$Mode" == "debug" ]; then
    CommonFlags="$CommonFlags -O0 -g -DEDITOR_INTERNAL=1 -DEDITOR_SLOW=1"
else
    CommonFlags="$CommonFlags -O2 -DEDITOR_INTERNAL=0 -DEDITOR_SLOW=0"
fi

# NOTE(pvlso): Third party code is built with its own flags and without our warnings
LibFlags="-O2 -g -fPIC -w -I$Code"

#
# NOTE(pvlso): A game project, hot reloaded by the editor. It's written under a temp name
# and renamed, so the editor never sees a half written library.
#
if [ -n "$Project" ]; then
    ProjectDir="$Root/projects/$Project"
    if [ ! -f "$ProjectDir/$Project.cpp" ]; then
        echo "ERROR: $ProjectDir/$Project.cpp doesn't exist."
        exit 1
    fi
    if [ ! -f "$Build/libnuklear.so" ]; then
        echo "ERROR: $Build/libnuklear.so doesn't exist, build the engine first."
        exit 1
    fi

    mkdir -p "$ProjectDir/build"
    cd "$ProjectDir/build"

    echo "Building $Project"
    # NOTE(pvlso): Copies left by editor runs that ended with the game loaded. Deleting the
    # one in use is fine on Linux, the editor keeps its mapping.
    rm -f "${Project}"_temp_*.so
    $CXX $CommonFlags $Warnings -shared "$ProjectDir/$Project.cpp" -o "${Project}_build.so" \
         -L"$Build" -lnuklear -Wl,-rpath,"$Build"
    mv -f "${Project}_build.so" "$Project.so"

    echo "Done."
    exit 0
fi

mkdir -p "$Build"
cd "$Build"

#
# NOTE(pvlso): Third party libraries, only rebuilt when missing (./build.sh clean to force)
#
if [ ! -f libglfw.a ]; then
    echo "Building GLFW"
    mkdir -p glfw_obj
    GLFWSources="context.c init.c input.c monitor.c platform.c vulkan.c window.c egl_context.c osmesa_context.c
                 null_init.c null_monitor.c null_window.c null_joystick.c
                 posix_module.c posix_time.c posix_thread.c posix_poll.c linux_joystick.c
                 x11_init.c x11_monitor.c x11_window.c xkb_unicode.c glx_context.c"
    for Source in $GLFWSources; do
        $CC $LibFlags -D_GLFW_X11 -D_DEFAULT_SOURCE -c "$Code/glfw/src/$Source" -o "glfw_obj/${Source%.c}.o" &
    done
    wait
    ar rcs libglfw.a glfw_obj/*.o
    rm -rf glfw_obj
fi

if [ ! -f libglew.a ]; then
    echo "Building GLEW"
    $CC $LibFlags -DGLEW_STATIC -DGLEW_NO_GLU -c "$Code/glew/glew.c" -o glew.o
    ar rcs libglew.a glew.o
    rm -f glew.o
fi

# NOTE(pvlso): Like the others only built when missing, same as build.bat (a running editor has it loaded)
if [ ! -f libnuklear.so ]; then
    echo "Building Nuklear"
    $CC $LibFlags -DNUKLEAR_DLL_BUILD -shared "$Code/nuklear/nuklear_imp.c" -o libnuklear.so -lm
fi

#
# NOTE(pvlso): Engine, hot reloaded by the platform layer. lock.tmp tells the
# platform layer not to load the library while it's still being written.
#
echo "Building Engine"
echo "WAITING FOR SO" > lock.tmp
$CXX $CommonFlags $Warnings -shared "$Code/engine.cpp" -o engine_temp.so -L. -lnuklear -Wl,-rpath,'$ORIGIN'
mv -f engine_temp.so engine.so
rm -f lock.tmp

#
# NOTE(pvlso): Platform layer. Written under a temp name and renamed, so a running editor keeps
# its copy (the engine hot reloads, the new kaban is used on the next start).
#
if [ -f "$Code/glfw_engine.cpp" ]; then
    echo "Building Platform"
    $CXX $CommonFlags $Warnings -DGLEW_STATIC -DGLEW_NO_GLU -I"$Code/glfw/include" "$Code/glfw_engine.cpp" -o kaban_temp \
         -L. -lglfw -lglew -lnuklear -l:libGL.so.1 -ldl -lpthread -lm -Wl,-rpath,'$ORIGIN'
    mv -f kaban_temp kaban
else
    echo "NOTE: code/glfw_engine.cpp doesn't exist yet, skipping the platform executable."
fi

echo "Done."
