@echo off
title Volkswagen MIB2 Infotainment (Golf 7) - Authentic Firmware
echo Starting Volkswagen MIB2 Infotainment runtime on Windows...
cd /d "%~dp0"
node src/track-a/poc/start-hmi.js
pause
