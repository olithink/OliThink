CC=clang
CFLAGS=-O3 -Wall -Wextra -mllvm -align-all-nofallthru-blocks=6 -march=native -DNDEBUG
olithink: olithink.c cerebrum.c
