CC=clang
CFLAGS=-O3 -Wall -Wextra -mllvm -align-all-nofallthru-blocks=6 -DNDEBUG -static -s
CFLAGS+=-march=silvermont -mtune=k8  -msse4.1 -msse4.2 -mpopcnt 
CFLAGS+=
olithink: olithink.c cerebrum.c



#   avx512     

#  -mpopcnt -msse4.1 -msse4.2 -mbmi -mfma -mavx2 -mbmi2 -mavx -mavx512cd -mavx512vl   
#  -mavx512f -mavx512bw -mavx512dq -march=x86-64-v4 -mtune=silvermont


#   bmi2

#   -mpopcnt -msse4.1 -msse4.2 -mbmi -mfma -mavx2 -mbmi2 -mavx -march=x86-64-v3 -mtune=silvermont 


#   avx2 zen2

#   -mpopcnt -march=znver2 -mtune=znver2 -msse4.1 -mbmi -mfma -mavx2 -mavx -mbmi


#   sse4

#   -march=silvermont -mtune=k8  -msse4.1 -msse4.2 -mpopcnt


#   sse3

#   -msse3 -mssse3 -march=k8 -mtune=core2

