addf p2 p2 0.02
roty m0 p2
rotx m1 0.3232
mulm m2 m0 m1
trans m1 0 0 5
mulm m0 m1 m2
mvp m0
exit