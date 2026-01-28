; px -> 0..255
divf p0 px 640.0
mulf p0 p0 255.0
casti p0 p0 

; py -> 0..255
divf p1 py 480.0
mulf p1 p1 255.0
casti p1 p1 


mov pb 128
mov pr p0
mov pg p1

exit
