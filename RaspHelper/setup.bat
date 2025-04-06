if "%1" NEQ "" goto :setup

set PING_COUNT=

:ping-check
timeout /t 1 /nobreak >nul
ping raspberrypi5.local
if %ERRORLEVEL% NEQ 0 goto :ping-check
set PING_COUNT=%PING_COUNT%1
if %PING_COUNT% NEQ 11111 goto :ping-check

:setup
ssh-keygen -R raspberrypi5.local
scp %~dp0setup.sh hibic@raspberrypi5.local:~/
ssh hibic@raspberrypi5.local mkdir ~/temp; mv -f -v ~/setup.sh ~/temp/; bash ./misc/setup.sh