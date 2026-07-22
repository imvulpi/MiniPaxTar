@echo off
python scripts\pre_commit.py
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%