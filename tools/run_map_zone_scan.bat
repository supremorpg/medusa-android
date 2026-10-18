@echo off
setlocal
python tools\map_zone_scanner.py ^
  --audit-dir "cerebro\map-audit-workspace" ^
  --runtime-base "cerebro\runtime-monitor" ^
  --map-input "..\cerebro\device-data\maps-all" ^
  --cell-size 250 ^
  --min-samples 2
endlocal
