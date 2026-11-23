@echo off
REM Windows-native PS Vita build (vitaGL 3D renderer). Requires C:\vitasdk with vitaGL installed (vdpm install vitaGL).
setlocal
cd /d "%~dp0"
if "%VITASDK%"=="" set VITASDK=C:\vitasdk
set PATH=%VITASDK%\bin;%PATH%
set OUT=build3d
if not exist %OUT% mkdir %OUT%
set CF=-Wall -Wno-unused-variable -Wno-unused-function -Wno-misleading-indentation -O3 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -ffast-math -fomit-frame-pointer -Isrc -D__vita__

for %%f in (main game player enemies moves font save_system audio_vita renderer_vita3d g3d maps_data) do (
  echo Compiling %%f.c
  arm-vita-eabi-gcc %CF% -c src/%%f.c -o %OUT%/%%f.o || goto :fail
)

pushd %OUT%
arm-vita-eabi-gcc -Wl,-q -Wl,--gc-sections main.o game.o player.o enemies.o moves.o font.o save_system.o audio_vita.o renderer_vita3d.o g3d.o maps_data.o ^
  -lvitaGL -lvitashark -lmathneon -lSceShaccCgExt -ltaihen_stub -lSceShaccCg_stub -lSceGxm_stub -lSceDisplay_stub ^
  -lSceCtrl_stub -lSceTouch_stub -lSceAudio_stub -lScePower_stub -lSceSysmodule_stub -lSceAppMgr_stub -lSceCommonDialog_stub ^
  -lSceKernelDmacMgr_stub -lSceLibKernel_stub -lstdc++ -lm -lz -o poke-survivor-3d.elf || (popd & goto :fail)
vita-elf-create poke-survivor-3d.elf poke-survivor-3d.velf || (popd & goto :fail)
psp2rela -src=poke-survivor-3d.velf -dst=poke-survivor-3d.velf 2>nul
vita-make-fself -s poke-survivor-3d.velf eboot.bin || (popd & goto :fail)
vita-mksfoex -d ATTRIBUTE=0x8000 -d ATTRIBUTE2=12 -s TITLE_ID=POKE30001 "poke-survivors vita" param.sfo || (popd & goto :fail)
popd

if not exist dist mkdir dist
del /q "dist\poke-survivor.vpk" 2>nul
del /q "dist\poke-survivors vita.vpk" 2>nul
vita-pack-vpk -s %OUT%/param.sfo -b %OUT%/eboot.bin ^
  -a sce_sys/icon0.png=sce_sys/icon0.png ^
  -a sce_sys/pic0.png=sce_sys/pic0.png ^
  -a sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml ^
  -a sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png ^
  -a sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png ^
  "dist/poke-survivors vita.vpk" || goto :fail
if exist "dist\data\PokemonSurvivors" rmdir /s /q "dist\data\PokemonSurvivors"
if not exist "dist\data\poke-survivors vita" mkdir "dist\data\poke-survivors vita"
robocopy assets "dist\data\poke-survivors vita" /MIR /NFL /NDL /NJH /NJS /NC /NS >nul
if %ERRORLEVEL% GEQ 8 goto :fail
echo [SUCCESS] dist\poke-survivors vita.vpk and dist\data\poke-survivors vita
exit /b 0
:fail
echo [FAILED]
exit /b 1
