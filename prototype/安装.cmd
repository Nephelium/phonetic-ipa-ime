@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Install-Windows.ps1"
set "IPA_INSTALL_RESULT=%ERRORLEVEL%"
if not "%IPA_INSTALL_RESULT%"=="0" echo Installation failed. See the error above and the install-logs folder.
echo.
pause
exit /b %IPA_INSTALL_RESULT%
