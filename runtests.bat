@ECHO OFF
SET DBGDIR=%LOCALAPPDATA%\DBG\UI\Slow.20190520.1\amd64
SET HELPERDIR=f:\repos\ana\v8\out\debug_x64
SET BINDIR=%~dp0x64

IF NOT EXIST "%DBGDIR%\dbgeng.dll" (
  ECHO File 'dbgeng.dll' not found in %DBGDIR%. Please update %0
  EXIT /B 1
)

IF NOT EXIST "%BINDIR%\v8dbg.dll" (
  ECHO Debugger extension 'v8dbg.dll' not found. Please build first.
  EXIT /B 1
)

ROBOCOPY "%BINDIR%" "%DBGDIR%" v8dbg.dll v8dbg.pdb v8dbg-test.exe v8dbg-test.pdb > NUL
ROBOCOPY "%HELPERDIR%" "%DBGDIR%" v8_debug_helper.dll libc++.dll > NUL

IF /I "%1" == "dbg" (
  windbgx "%DBGDIR%\v8dbg-test.exe"
) ELSE (
  "%DBGDIR%\v8dbg-test.exe"
)
