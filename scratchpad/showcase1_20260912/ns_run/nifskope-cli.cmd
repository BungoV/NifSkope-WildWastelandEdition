@echo off
setlocal
rem NifSkope headless batch mode.
rem
rem NifSkope.exe is linked -subsystem,windows, so neither cmd nor PowerShell
rem waits for it and a plain redirect returns before any output exists.
rem "start /b /wait" shares this console and waits, so stdout, stderr and the
rem exit code all behave like a normal console tool.
rem
rem   nifskope-cli info model.nif
rem   nifskope-cli get model.nif -b 73 -f "Num Vertices"
rem   nifskope-cli spells vertex
start "" /b /wait "%~dp0NifSkope.exe" -no-gui %*
exit /b %errorlevel%
