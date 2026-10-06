@echo off
REM
REM Kaban Engine - Windows build
REM
REM   build.bat                  debug build, clang-cl if installed, otherwise cl
REM   build.bat release          optimized build
REM   build.bat clang / msvc     force a compiler
REM   build.bat clean            remove the build directory
REM
REM Needs Visual Studio or the VS Build Tools ("Desktop development with C++"), even
REM with clang, because that's where the Windows SDK headers and libs come from.
REM Running from a normal terminal is fine, the MSVC environment is set up automatically.
REM

setlocal EnableDelayedExpansion

set Mode=debug
set Compiler=auto
for %%A in (%*) do (
    if /I "%%A"=="debug"   set Mode=debug
    if /I "%%A"=="release" set Mode=release
    if /I "%%A"=="clean"   set Mode=clean
    if /I "%%A"=="clang"   set Compiler=clang
    if /I "%%A"=="msvc"    set Compiler=msvc
)

set "Root=%~dp0"
set "Code=%Root%code"
set "Build=%Root%build"

if not "%Mode%"=="clean" goto :SetupMSVC
if exist "%Build%" rmdir /s /q "%Build%"
echo Cleaned.
exit /b 0

REM -----------------------------------------------------------------------------
REM NOTE(pvlso): MSVC environment, found through vswhere when not already set up
REM -----------------------------------------------------------------------------
:SetupMSVC
where cl >nul 2>nul
if not errorlevel 1 goto :ChooseCompiler

set "VSWhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWhere%" goto :NoMSVC

set "VSPath="
for /f "usebackq tokens=*" %%i in (`"%VSWhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPath=%%i"
if not defined VSPath goto :NoMSVC

echo Setting up MSVC environment from "%VSPath%"
call "%VSPath%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
where cl >nul 2>nul
if errorlevel 1 goto :NoMSVC

REM -----------------------------------------------------------------------------
REM NOTE(pvlso): Compiler, clang-cl preferred. It is a drop-in replacement for cl.
REM -----------------------------------------------------------------------------
:ChooseCompiler
set "CC=cl"
if /I "%Compiler%"=="msvc" goto :CompilerChosen

where clang-cl >nul 2>nul
if not errorlevel 1 (
    set "CC=clang-cl"
    goto :CompilerChosen
)
if exist "%VCINSTALLDIR%Tools\Llvm\x64\bin\clang-cl.exe" (
    set "CC=%VCINSTALLDIR%Tools\Llvm\x64\bin\clang-cl.exe"
    goto :CompilerChosen
)
if exist "%ProgramFiles%\LLVM\bin\clang-cl.exe" (
    set "CC=%ProgramFiles%\LLVM\bin\clang-cl.exe"
    goto :CompilerChosen
)
if /I "%Compiler%"=="clang" goto :NoClang
echo NOTE: clang-cl not found, using cl. Install LLVM or the VS "C++ Clang tools" component to use clang.

:CompilerChosen
echo Building in %Mode% mode with "%CC%"

REM -----------------------------------------------------------------------------
REM NOTE(pvlso): Flags
REM -----------------------------------------------------------------------------
set CommonFlags=-nologo -fp:fast -fp:except- -EHsc -GR- -EHa- -Oi -FC -I "%Code%"

if "%CC%"=="cl" (
    set Warnings=-WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4456 -wd4127 -wd4996 -wd4116 -wd4146
    set CommonFlags=!CommonFlags! -Gm- -Zo
) else (
    set Warnings=-W3 -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-missing-braces -Wno-switch -Wno-unused-value -Wno-sign-compare -Wno-missing-field-initializers -Wno-format -Wno-enum-compare -Wno-writable-strings -Wno-null-dereference -Wno-microsoft-cast -Wno-unused-command-line-argument -Wno-deprecated-declarations
    set CommonFlags=!CommonFlags! -mavx2
)

if "%Mode%"=="debug" (
    set CommonFlags=!CommonFlags! -DEDITOR_INTERNAL=1 -DEDITOR_SLOW=1 -Od -MTd -Z7
    set CRT=debug
) else (
    set CommonFlags=!CommonFlags! -DEDITOR_INTERNAL=0 -DEDITOR_SLOW=0 -O2 -MT
    set CRT=release
)

set LibFlags=-nologo -O2 -Oi -W0 -I "%Code%"
set LinkFlags=-incremental:no -opt:ref

if not exist "%Build%" mkdir "%Build%"
pushd "%Build%"

REM -----------------------------------------------------------------------------
REM NOTE(pvlso): Third party libraries, only rebuilt when missing (build.bat clean to force)
REM -----------------------------------------------------------------------------
if exist glew.dll goto :GLEWDone
echo Building GLEW
"%CC%" %LibFlags% -DGLEW_BUILD -MT "%Code%\glew\glew.c" -LD /link %LinkFlags% opengl32.lib
if errorlevel 1 goto :Failed
:GLEWDone

REM NOTE(pvlso): GLFW is static, so it must use the same CRT (-MT/-MTd) as the exe
if not exist "%Code%\glfw_engine.cpp" goto :GLFWDone
if exist glfw_%CRT%.lib goto :GLFWDone
echo Building GLFW
if not exist glfw_obj mkdir glfw_obj
set GLFWSources=context.c init.c input.c monitor.c platform.c vulkan.c window.c egl_context.c osmesa_context.c null_init.c null_monitor.c null_window.c null_joystick.c win32_module.c win32_time.c win32_thread.c win32_init.c win32_joystick.c win32_monitor.c win32_window.c wgl_context.c
set GLFWFiles=
for %%F in (%GLFWSources%) do set GLFWFiles=!GLFWFiles! "%Code%\glfw\src\%%F"
if "%CRT%"=="debug" (set GLFWCRT=-MTd) else (set GLFWCRT=-MT)
"%CC%" %LibFlags% %GLFWCRT% -D_GLFW_WIN32 -D_CRT_SECURE_NO_WARNINGS -c %GLFWFiles% -Foglfw_obj\
if errorlevel 1 goto :Failed
lib -nologo -OUT:glfw_%CRT%.lib glfw_obj\*.obj
if errorlevel 1 goto :Failed
rmdir /s /q glfw_obj
:GLFWDone

echo Building Nuklear
"%CC%" %CommonFlags% -W0 -DNUKLEAR_DLL_BUILD "%Code%\nuklear\nuklear_imp.c" -LD /link %LinkFlags% -PDB:nuklear.pdb /OUT:nuklear.dll /IMPLIB:nuklear.lib
if errorlevel 1 goto :Failed

REM -----------------------------------------------------------------------------
REM NOTE(pvlso): Engine, hot reloaded by the platform layer. lock.tmp tells the
REM platform layer not to load the dll while the pdb is still being written.
REM -----------------------------------------------------------------------------
echo Building Engine
del engine_*.pdb > NUL 2> NUL
echo WAITING FOR PDB > lock.tmp
"%CC%" %CommonFlags% %Warnings% "%Code%\engine.cpp" -LD nuklear.lib /link %LinkFlags% -PDB:engine_%random%.pdb -EXPORT:EngineUpdateAndRender -EXPORT:EngineGetSoundSamples -EXPORT:DEBUGEditorFrameEnd
set BuildError=%errorlevel%
del lock.tmp
if not "%BuildError%"=="0" goto :Failed

REM -----------------------------------------------------------------------------
REM NOTE(pvlso): Platform layers. win32_engine stays until glfw_engine replaces it.
REM -----------------------------------------------------------------------------
echo Building Win32 Platform
"%CC%" %CommonFlags% %Warnings% "%Code%\win32_engine.cpp" nuklear.lib /link %LinkFlags% user32.lib gdi32.lib winmm.lib opengl32.lib glew.lib
if errorlevel 1 goto :Failed

if not exist "%Code%\glfw_engine.cpp" goto :PlatformDone
echo Building GLFW Platform
REM NOTE(pvlso): glfw_engine.cpp uses a plain main on every OS. Debug keeps the console for
REM stderr output, release is a windows subsystem exe that still enters through main.
set GLFWSubsystem=-SUBSYSTEM:CONSOLE
if "%Mode%"=="release" set GLFWSubsystem=-SUBSYSTEM:WINDOWS -ENTRY:mainCRTStartup
"%CC%" %CommonFlags% %Warnings% -I "%Code%\glfw\include" "%Code%\glfw_engine.cpp" -Fekaban.exe nuklear.lib glfw_%CRT%.lib /link %LinkFlags% %GLFWSubsystem% user32.lib gdi32.lib shell32.lib winmm.lib opengl32.lib glew.lib
if errorlevel 1 goto :Failed
:PlatformDone

popd
echo Done.
exit /b 0

:Failed
popd
echo BUILD FAILED.
exit /b 1

:NoMSVC
echo ERROR: Could not find the MSVC toolchain.
echo        Install Visual Studio or the "Build Tools for Visual Studio" with the
echo        "Desktop development with C++" workload: https://visualstudio.microsoft.com/downloads/
exit /b 1

:NoClang
echo ERROR: clang was requested but clang-cl was not found.
echo        Install LLVM (https://github.com/llvm/llvm-project/releases) or the VS "C++ Clang tools" component.
exit /b 1
