@echo off
setlocal
cd /d "%~dp0"
if not exist "arcanum1.dat" (
  echo Extract this ZIP into your Arcanum game folder, next to Arcanum.exe and arcanum1.dat.
  pause
  exit /b 1
)
if not exist "modern-ui\Book.ttf" (
  echo The modern-ui folder is missing. Extract the complete ZIP first.
  pause
  exit /b 1
)
for %%A in (%*) do if "%%~A"=="-window" set "ARCANUM_IGNORE_DISPLAY_SETTINGS=1"
if "%~1"=="" (
  start "" /wait "%~dp0arcanum-modern.exe" -geometry=1280x720
) else (
  start "" /wait "%~dp0arcanum-modern.exe" %*
)
if errorlevel 1 (
  echo Arcanum Renaissance stopped with an error. Please report the error and the version you installed.
  pause
  exit /b 1
)
