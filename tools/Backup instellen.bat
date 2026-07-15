@echo off
REM Dubbelklik dit bestand om de dagelijkse FilaTrack back-up in te stellen.
REM Er opent een venster waarin je kiest WAAR de back-ups moeten komen.
title FilaTrack - back-up instellen
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0backup_pull.ps1" -Install
echo.
pause
