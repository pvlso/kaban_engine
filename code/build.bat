@echo off

set CommonCompilerFlags= -DENGINE_INTERNAL=1 -DENGINE_SLOW=0 -DENGINE_WIN32=1 -Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -FC -Z7 -wd4201 -wd4100 -wd4189 -wd4505
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib opengl32.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build


REM 64-bit build
del *.pdb > NUL 2> NUL
REM Optimization switches /O2
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% ..\code\engine.cpp -Fmengine.map -LD /link -incremental:no -opt:ref -PDB:engine_%random%.pdb -EXPORT:EditorGetSoundSamples -EXPORT:EngineUpdateAndRender -EXPORT:DEBUGEditorFrameEnd
del lock.tmp
cl %CommonCompilerFlags% ..\code\win32_engine.cpp -Fmwin32_engine.map /link %CommonLinkerFlags%

popd
