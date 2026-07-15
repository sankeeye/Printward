@echo off
REM Dubbelklik dit bestand om NU meteen een back-up van de tablet op te halen.
REM Zonder ingestelde map komt hij in Documenten\PandaTouch_backups terecht.
title PandaTouch - back-up nu maken
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0backup_pull.ps1"
echo.
pause
