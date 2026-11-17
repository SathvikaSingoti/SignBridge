@echo off
rem Starts the SignBridge app on this laptop and opens it in Chrome or Edge.
cd /d "%~dp0app"
echo SignBridge is starting at http://localhost:8766
echo Keep this window open while you use it. Close it to stop.
start "" http://localhost:8766
where python >nul 2>nul
if %errorlevel%==0 (python -m http.server 8766) else (py -m http.server 8766)
if errorlevel 1 (
  echo.
  echo Python was not found. Install it from python.org, tick "Add python.exe to PATH", then run this file again.
  pause
)
