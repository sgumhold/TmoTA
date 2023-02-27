%~d1
cd %~p1
ffmpeg -i %1 -f image2 -q:v 1 image%%04d.jpg
dir *.jpg
pause