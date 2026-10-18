@echo off
setlocal
if "%~1"=="" (
  echo Usage: tools\run_runtime_zone_sweep_pilot.bat cerebro\map-zone-scan\YYYYMMDD-HHMMSS\route_priority.json
  exit /b 2
)
python tools\runtime_zone_sweeper.py ^
  --serial 192.168.3.155:43349 ^
  --route "%~1" ^
  --max-points 12 ^
  --settle 4 ^
  --screenshot-on-red
endlocal
