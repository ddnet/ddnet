@echo off

if exist "%APPDATA%\DDNet\" (
	start explorer "%APPDATA%\DDNet\"
	exit /b
)

if exist "%APPDATA%\Teeworlds\" (
	start explorer "%APPDATA%\Teeworlds\"
	exit /b
)

echo No configuration directory was found.
pause
