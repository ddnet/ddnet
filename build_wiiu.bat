@echo off
echo ==================================================
echo Building DDNet for Wii U...
echo ==================================================

:: Check if Docker is running
docker ps >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: Docker is not running!
    echo Please start Docker Desktop and try again.
    pause
    exit /b %ERRORLEVEL%
)

:: 1. Rebuild ELF
echo [1/3] Compiling fresh binary via devkitpro Docker...
docker run --rm -v "%cd%:/src" -w /src/build devkitpro/devkitppc:latest bash -c "make -j4"
if %ERRORLEVEL% neq 0 (
    echo ERROR: Compile failed!
    pause
    exit /b %ERRORLEVEL%
)

:: 2. Apply Patches
echo [2/3] Applying PowerPC instruction patches...
python wiiu_tools\patch_atomics_universal.py build\DDNet.elf
python wiiu_tools\patch_elf.py build\DDNet.elf

:: 3. Package
echo [3/3] Packaging RPX and WUHB...
docker run --rm -v "%cd%:/src" -w /src devkitpro/devkitppc:latest bash -c "/opt/devkitpro/tools/bin/elf2rpl build/DDNet.elf build/DDNet.rpx && /opt/devkitpro/tools/bin/wuhbtool build/DDNet.rpx build/DDNet.wuhb --content=build/wiiu_content --name='DDraceNetwork' --short-name='DDNet' --author='DDNet team' --icon=build/icon.png --tv-image=build/tv_image.png --drc-image=build/drc_image.png > /dev/null"
if %ERRORLEVEL% neq 0 (
    echo ERROR: Packaging failed!
    pause
    exit /b %ERRORLEVEL%
)

echo ==================================================
echo SUCCESS: build/DDNet.wuhb is ready!
echo ==================================================
pause
