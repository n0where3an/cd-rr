@echo off
if not "%1" == "" goto recvr
echo Please run with single parameter - target-dir
goto exit

:recvr
..\win32\rrdec -s .. -@ ..\example\_recvr_\protection.rrt -t %1 -a -w
if errorlevel 1 goto error
echo Example files are restored into %1 successfully
goto exit
:error
echo Something was wrong -- copy is unusable
:exit
