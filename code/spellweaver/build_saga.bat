@echo off

set CommonCompilerFlags= -DSPELLWEAVER_INTERNAL=0 -DSPELLWEAVER_SLOW=0 -DSPELLWEAVER_WIN32=1 -DSPELLWEAVER_LAPTOP=0 -O2 -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4127 -wd4456 -FC -Z7
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib opengl32.lib

REM TODO - can we just build both with one exe?

IF NOT EXIST ..\..\build\build_saga mkdir ..\..\build\build_saga
pushd ..\..\build\build_saga

REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

cl %CommonCompilerFlags% -D_CRT_SECURE_NO_WARNINGS ..\..\SpellweaverSaga\code\simpler_preprocessor.cpp /link %CommonLinkerFlags%
pushd ..\..\SpellweaverSaga\code
..\..\build\build_saga\simpler_preprocessor.exe > spellweaver_generated.h
popd

REM 64-bit build
del *.pdb > NUL 2> NUL
REM Optimization switches /O2
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% ..\..\SpellweaverSaga\code\spellweaver.cpp -Fmspellweaver.map -LD /link -incremental:no -opt:ref -PDB:spellweaver_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender -EXPORT:DEBUGGameFrameEnd
del lock.tmp
cl %CommonCompilerFlags% ..\..\SpellweaverSaga\code\win32_spellweaver.cpp -Fmwin32_spellweaver.map /link %CommonLinkerFlags%
