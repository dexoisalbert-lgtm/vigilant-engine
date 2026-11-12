@echo off
rem Gathers build errors and the latest game log into one text file and
rem copies it to the clipboard, ready to paste into the chat.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0CollectLogs.ps1"
pause
