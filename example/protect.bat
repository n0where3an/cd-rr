@echo off

: computing of protection
..\win32\rrenc -wiu@ protect.lst -s .. -r rrd_head.mkv -o _image_\example-image 3600 example\_recvr_\protection.rrt example\_recvr_\protection.rrd
if errorlevel 1 goto error

: image creation
echo for .XCD image creation enter command:
echo.
echo ..\win32\m2cdm -paramfile _image_\example-image.um2
echo.
echo Image files _image_\example-image.toc, _image_\example-image.cue,
echo _image_\example-image.bin will be created; you can burn this image
echo onto CD-ROM or mount it under DAEMON Tools or similar program.
echo.
goto exit
:error
echo Something was wrong...
:exit
