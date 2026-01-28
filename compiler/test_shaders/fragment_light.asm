; light_point (400, 200) r = 100

subf p0 px 400.0
subf p1 py 200.0

lenf p2 p0 p1

subf p2 p2 100.0 ;radius = 100

clampf p2 p2 0.0 1.0

lerpf p2 1.0 0.3 p2

castf pr pr
castf pg pg
castf pb pb

mulf pr pr p2
mulf pg pg p2
mulf pb pb p2

casti pr pr
casti pg pg
casti pb pb

exit
