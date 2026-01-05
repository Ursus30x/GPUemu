
movi p0 0
loop:
cmpi neq 0 10
!jmp end
addi p0 p0 1
jmp loop
end:
movi pg p0