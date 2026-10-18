@echo off
setlocal
python tools\map_audit.py ^
  --input "..\cerebro\device-data\maps-all" ^
  --input "..\cerebro\device-data\texdb" ^
  --input "..\cerebro\tmp-col-extract" ^
  --input "_diagnostics\samp_data_pack\SAMP" ^
  --output "cerebro\map-audit-workspace" ^
  --html-limit 12000
endlocal
