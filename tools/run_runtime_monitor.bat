@echo off
setlocal
set SERIAL=%~1
if "%SERIAL%"=="" (
  python tools\runtime_monitor.py
) else (
  python tools\runtime_monitor.py --serial "%SERIAL%"
)
endlocal
