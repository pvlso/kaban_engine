@echo off

set CommonCompilerFlags= -DENGINE_INTERNAL=1 -DENGINE_SLOW=0 -DENGINE_WIN32=1 -Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -FC -Z7 -wd4201 -wd4100 -wd4189 -wd4505
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib opengl32.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

:: cl %CommonCompilerFlags% ..\..\SpellweaverSaga\code\spellweaver.cpp -Fmspellweaver.map -LD /link -incremental:no -opt:ref -PDB:spellweaver_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender -EXPORT:DEBUGGameFrameEnd

cl %CommonCompilerFlags% ..\code\win32_engine.cpp -Fmwin32_engine.map /link %CommonLinkerFlags%
popd
