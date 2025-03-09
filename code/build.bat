@echo off

set CommonCompilerFlags= -DEDITOR_INTERNAL=1 -DEDITOR_SLOW=1 -DEDITOR_WIN32=1 -Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -FC -Z7 -wd4201 -wd4100 -wd4189 -wd4505 -wd4456 -wd4127
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib opengl32.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build


set GLEWLinkerFlags= -incremental:no -opt:ref opengl32.lib
set GLEWCompilerFlagsD= -I ..\code -DGLEW_BUILD -Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -FC -Z7 -wd4456
set GLEWCompilerFlagsO= -I ..\code -DGLEW_BUILD -O2 -Oi -MT -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -WX -W4 -wd4456 

cl %GLEWCompilerFlagsD% ..\code\glew.c -Fmglew.map -LD /link %GLEWLinkerFlags% 

IF NOT EXIST ..\build\glfw mkdir ..\build\glfw
cmake -S ..\glfw-3.4 -B glfw
cmake --build glfw --config Debug

REM 64-bit build
del *.pdb > NUL 2> NUL
REM Optimization switches /O2
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% ..\code\engine.cpp -Fmengine.map -LD /link -incremental:no -opt:ref -PDB:engine_%random%.pdb -EXPORT:EditorGetSoundSamples -EXPORT:EngineUpdateAndRender -EXPORT:DEBUGEditorFrameEnd
del lock.tmp

cl %CommonCompilerFlags% ..\code\win32_engine.cpp -Fmwin32_engine.map /link %CommonLinkerFlags%
cl %CommonCompilerFlags% ..\code\win32_engine_glfw.cpp -Fmwin32_engine_glfw.map /link %CommonLinkerFlags%

popd
