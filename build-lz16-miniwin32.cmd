@echo off
gcc -Os -s -static -m32 -mconsole -march=i686 -mtune=i686 -Wall ^
-mpreferred-stack-boundary=2 -fomit-frame-pointer -fno-unwind-tables -fno-asynchronous-unwind-tables ^
-fmerge-constants -fpack-struct=1 -falign-jumps=1 -falign-loops=1 -falign-functions=1 -nodefaultlibs ^
-ffast-math -fno-stack-protector -fno-exceptions -fno-ident -nostartfiles ^
-ffunction-sections -Wl,--entry=_start,--enable-stdcall-fixup,--no-insert-timestamp,--gc-sections ^
-o lz16-miniwin32.exe lz16-miniwin32.c -lkernel32 -lshell32
