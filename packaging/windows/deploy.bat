@echo off
rem Makes a folder that can be copied to the shop computer and run there.
rem
rem 1. Build in Release mode (Qt Creator: choose "Release", then Build).
rem 2. Open the "Qt 6.x (MinGW or MSVC)" command prompt from the Start menu,
rem    so that windeployqt is on the PATH.
rem 3. Run:  deploy.bat C:\path\to\build\src\app\ShopInventory.exe
rem
rem Result: packaging\windows\ShopInventory\ with the .exe and all Qt DLLs.

if "%~1"=="" (
    echo Usage: deploy.bat path\to\ShopInventory.exe
    exit /b 1
)

set OUT=%~dp0ShopInventory
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%"

copy "%~1" "%OUT%\" >nul || exit /b 1

rem Copies the Qt DLLs, the "platforms" plugin and the SQLite driver.
windeployqt --release --no-translations "%OUT%\ShopInventory.exe" || exit /b 1

echo.
echo Done: %OUT%
echo Copy this folder to the shop computer and create a desktop shortcut to ShopInventory.exe.
