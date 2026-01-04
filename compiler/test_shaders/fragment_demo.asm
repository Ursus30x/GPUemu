; UV
divf  p0 px 640.0      ; uv.x = x / 640
divf  p1 py 480.0      ; uv.y = y / 480

mulf  p0 p0 2.0        ; uv.x = uv.x * 2
subf  p0 p0 1.0        ; uv.x = uv.x - 1
mulf  p1 p1 2.0        ; uv.y = uv.y * 2 
subf  p1 p1 1.0        ; uv.y = uv.y - 1 

mulf  p0 p0 1.3333333  ; uv.x *= iResolution.x / iResolution.y

atan  p2 p1 p0         ; p2 = atan(y, x)
lenf  p1 p1 p0         ; p1 = length(uv)

lduf  p0, 64           ; load iTime to p0
mulf  p3 p1 10.0       ; p3 = 10.0 * p1
mulf  p4 p0 2.0        ; p4 = iTime * 2
subf  p3 p3 p4         ; p3 = p3 - p4
sin   p3 p3            ; p3 = sin(p3)

mulf  p4 p2 10.0       ; p4 = 10.0 * p2
mulf  p5 p0 3.0        ; p5 = iTime * 3.0
addf  p5 p4 p5
cos   p5 p5
addf  p5 p3 p5

addf  p6 p5 p0         ; p6 = p5 + iTime
tan   p6 p6

mulf  p7 p0 0.5        ; p7 = iTime * 0.5
subf  p7 p5 p7
tan   p7 p7

mulf  p3 p0 0.3        ; p3 = iTime * 0.3
mulf  p5 p5 0.7        ; p5 = p5 * 0.7
addf  p8 p3 p5
tan   p8 p8

vec3  m0 p6 p7 p8
mulv3 m0 m0 0.5
addv3 m0 m0 0.5

negf  p1 p1            ; p1 = -r
mulf  p1 p1 3.0        ; p1 = -r * 3.0
exp   p2 p1            ; p2 = exp(p1)
mulv3 m0 m0 p2         ; m0 = m0 * p2

colv3 m0               ; output fragColor
exit