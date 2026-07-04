@echo off
title Bambu Cloud Relay - Panda Touch
cd /d "%~dp0"

:loop
python bambu_weight_relay.py
echo.
echo Script stopped (crashed or was closed) - restarting in 10 seconds. Close this window to stop for good.
timeout /t 10
goto loop
