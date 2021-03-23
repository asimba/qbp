tcc arc-sfx.c -o arc-sfx.exe -m32 -Wl,-subsystem=console
upx --best --ultra-brute arc-sfx.exe
