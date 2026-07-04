@echo off
title Encrypt Bambu password
cd /d "%~dp0"
python encrypt_password.py
echo.
echo Klaar - je kunt dit venster sluiten.
timeout /t 8
