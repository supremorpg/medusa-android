@echo off
setlocal
set INPUT=%~1
if "%INPUT%"=="" set INPUT=app\build\outputs\apk\debug\app-debug.apk
python tools\map_audit.py --input "%INPUT%" --output cerebro\map-audit
endlocal
