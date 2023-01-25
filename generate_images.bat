%~d1
cd %~p1
%~dp0ffmpeg.exe -i %1 -f image2 -q:v 1 image%%03d.jpg
dir *.jpg
pause