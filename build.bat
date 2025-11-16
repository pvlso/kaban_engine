@echo off

set BuildDebug=true
set BuildGLEW=false

set CommonCompilerFlagsD= -DEDITOR_INTERNAL=1 -DEDITOR_SLOW=1 -DEDITOR_WIN32=1 -EHsc -Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -FC -Z7 -wd4201 -wd4100 -wd4189 -wd4505 -wd4456 -wd4127 -wd4996
set CommonCompilerFlagsO= -DEDITOR_INTERNAL=0 -DEDITOR_SLOW=0 -DEDITOR_WIN32=1 -EHsc -O2 -Oi -MT -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4456 -wd4127
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib opengl32.lib glew.lib

IF NOT EXIST build mkdir build
pushd build

set GLEWLinkerFlags= -incremental:no -opt:ref opengl32.lib
set GLEWCompilerFlagsD= -I ..\code -DGLEW_BUILD -Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -FC -Z7 -wd4456
set GLEWCompilerFlagsO= -I ..\code -DGLEW_BUILD -O2 -Oi -MT -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -WX -W4 -wd4456 

if %BuildDebug% == true (
   echo Building in DEBUG mode.

   if %BuildGLEW% == true (
      echo Building GLEW Debug
      cl %GLEWCompilerFlagsD% ..\code\glew.c -Fmglew.map -LD /link %GLEWLinkerFlags% 
   )

   echo Building Engine Debug
   del *.pdb > NUL 2> NUL
   echo WAITING FOR PDB > lock.tmp
   cl %CommonCompilerFlagsD% ..\code\engine.cpp -Fmengine.map -LD /link -incremental:no -opt:ref -PDB:engine_%random%.pdb -EXPORT:EngineUpdateAndRender -EXPORT:EngineGetSoundSamples -EXPORT:DEBUGEditorFrameEnd
   del lock.tmp

   cl %CommonCompilerFlagsD% ..\code\win32_engine.cpp -Fmwin32_engine.map /link %CommonLinkerFlags%

) else (
   echo Building in OPTIMIZED mode.

   if %BuildGLEW% == true (
      echo Building GLEW Release
      cl %GLEWCompilerFlagsO% ..\code\glew.c -Fmglew.map -LD /link %GLEWLinkerFlags% 
   )

   echo Building Engine Release
   cl %CommonCompilerFlagsO% ..\code\engine.cpp -Fmengine.map -LD /link -incremental:no -opt:ref -PDB:engine_%random%.pdb -EXPORT:EngineUpdateAndRender -EXPORT:DEBUGEditorFrameEnd

   cl %CommonCompilerFlagsO% ..\code\win32_engine.cpp -Fmwin32_engine.map /link %CommonLinkerFlags%
)

popd
