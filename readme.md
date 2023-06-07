# qbp  
  
Simple LZSS+RLE+RC32 compression algorithm realization.  
  
--- 
Usage : ./qbp \<OPTIONS\> \<SOURCE\> \<DESTINATION\>  
Usage : ./lz16 \<OPTIONS\> \<SOURCE\> \<DESTINATION\>  
**Examples:**  
```  
qbp c source_file packed_file  
qbp d packed_file unpacked_file  
lz16 c source_file packed_file  
lz16 d packed_file unpacked_file  
unqbp packed_file unpacked_file  
unlz16 packed_file unpacked_file  
```  

---  

**Options**:  

Option | Value |
----- | ----- |  
c | compress data |  
d | decompress data |  
  
---  
  
<p align="justify">This is free software. There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.</p>  
  
---  
  
Written by Alexey V. Simbarsky.  
  
---

**Test results**:  

| | |
----- | ----- |  
_Source file_ | [enwik9](https://cs.fit.edu/~mmahoney/compression/textdata.html) |  
_Source size (bytes)_ | 1000000000 |  
_Source hash (SHA1)_ | 2996e86fb978f93cca8f566cc56998923e7fe581 |  
|_Testing platform_ | Intel Core i7 9700K / 32 GB RAM / Windows 10 Pro |  

Packer | Options | Resulting size (bytes) | Packing time (sec) | Unpacking time (sec) |
----- | ----- | ----- | ----- | ----- |    
zpaq (7.15) | a -m5 -t1 | 168590740 | 2472,30 | 2519,52 |  
lrzip (0.651) | -z -p 1 -L 9 -U | 169357574 | 1967,30 | 1991,57 |  
rz (1.03.7) | a -d 1023M | 173041178 | 3070,63 | 14,31 |  
nanozip (0.09a) <br/> [(by Sami Runsas)](https://web.archive.org/web/20160304232219/http://nanozip.net/) | a -p1 -cO -m512m | 178225148 | 56,31 | 28,93 |  
freearc (0.51) | a -mx | 185153701 | 219,45 | 187,81 |  
lzip (1.23) | -k -9 -s29 | 199138993 | 1134,42 | 20,09 |  
uharc (0.6b) | a -d0 -md32768 -mm+ -mx | 208026696 | 493,92 | 383,33 |  
rk (1.04.1a) | -mx | 208096232 | 1380,00 | 1372,69 |  
xz (5.2.9) | -9 -e | 211776220 | 871,57 | 13,38 |  
7z (22.01) | a -t7z -ms=on -m0=LZMA2 -mx9 -mmt=1 -scsUTF-8 -ssc | 213323101 | 690,12 | 8,50 |  
zstd (1.5.2) | --ultra -22 --no-progress --single-thread | 214910502 | 794,37 | 2,75 |  
brotli (1.0.9) | -Z -n | 223345718 | 2287,31 | 3,36 |  
rar (6.11) | a -s -k -m5 -mt1 -md1g | 232248528 | 811,92 | 5,64 |  
szip (1.11b) <br/> [(by Michael Schindler)](http://www.compressconsult.com/szip/) | -b41 -o6 | 232346611 | 79,50 | 88,43 |  
squeez (6.0) | a /mx /fmt sqx | 246152882 | 597,93 | 8,89 |  
rzip (2.1) | -9 -k | 253920811 | 113,04 | 56,55 |  
bzip2 (1.0.8) | -9 | 253977891 | 81,44 | 40,03 |  
imp (1.12) | a -g -2 -mm | 256861382 | 77,49 | 21,71 |  
ha (0.999c) | a2 | 285739328 | 278,29 | 285,12 |  
7z (22.01) | a -tzip -m0=Deflate64 -mx9 | 298494477 | 861,46 | 6,26 |  
ace (1.2b) | a -s -d512 -m5 | 298687158 | 173,79 | 8,24 |  
winzip (27.0) <br/> (using wzzip/wzunzip,limited to single thread) |  -a -ee | 298774317 | 456,93 | 8,33 |  
zopfli (1.0.3) |  | 309578152 | 2508,81 |  |  
rar (2.50) | a -s -m5 | 309827109 | 196,01 | 38,05 |  
kzip <br/> [(by Ken Silverman)](http://advsys.net/ken/utils.htm) | /q /s0 | 310281906 | 2181,83 |  |  
7z (22.01) | a -tzip -m0=Deflate -mx9 | 310706257 | 790,08 | 6,54 |  
yzenc/yzdec (1.06.1) |  | 312789435 | 48,74 | 23,86 |  
lha (1.14i) | a -o7 | 312912781 | 113,52 | 10,46 |  
lzfse (1.0) | -encode | 319756993 | 25,34 | 9,67 |  
gzip (1.12) | -9 | 322591995 | 49,25 | 8,73 |  
pkzip (2.50b) | /add | 323645288 | 32,94 | 7,04 |  
makecab/expand (5.0.1.1) |  | 324125809 | 57,91 | 5,81 |  
$\textcolor[RGB]{0,154,23}{\textbf{qbp}}$ `(built with "gcc -O3")` | $\textcolor[RGB]{0,154,23}{\textbf{c}}$ | $\textcolor[RGB]{0,154,23}{\textbf{324501858}}$ | $\textcolor[RGB]{0,154,23}{\textbf{45,87}}$ | $\textcolor[RGB]{0,154,23}{\textbf{29,65}}$ |  
$\textcolor[RGB]{0,154,23}{\textbf{unqbp}}$ `(built with "gcc -O3")` |  | $\textcolor[RGB]{0,154,23}{\textbf{324501858}}$ |  | $\textcolor[RGB]{0,154,23}{\textbf{29,31}}$ |  
ain (2.32) | a /m1 | 324538744 | 90,10 | 28,51 |  
arj32 (3.10) | a -m1 | 328553984 | 129,71 | 23,21 |  
zoo (2.10) | ah | 353022373 | 104,18 | 30,69 |  
lzop (1.04) | -9 -F | 366349786 | 105,33 | 3,31 |  
lz4 (1.9.4) | --best | 372443347 | 65,90 | 11,91 |  
lzari <br/> [(reference realization by Haruhiko Okumura)](https://web.archive.org/web/19990209183635/http://oak.oakland.edu/pub/simtelnet/msdos/arcutils/lz_comp2.zip) | e | 388521578 | 182,69 | 77,72 |  
$\textcolor[RGB]{0,154,23}{\textbf{lz16}}$ `(built with "gcc -O3")` | $\textcolor[RGB]{0,154,23}{\textbf{c}}$ | $\textcolor[RGB]{0,154,23}{\textbf{407180647}}$ | $\textcolor[RGB]{0,154,23}{\textbf{31,50}}$ | $\textcolor[RGB]{0,154,23}{\textbf{3,86}}$ |  
$\textcolor[RGB]{0,154,23}{\textbf{unlz16}}$ `(built with "gcc -O3")` |  | $\textcolor[RGB]{0,154,23}{\textbf{407180647}}$ |  | $\textcolor[RGB]{0,154,23}{\textbf{3,86}}$ |  
ncompress (5.1) |  | 448136005 | 31,98 | 12,85 |  
lzss <br/> [(reference realization by Haruhiko Okumura)](https://oku.edu.mie-u.ac.jp/~okumura/compression/lzss.c) | e | 455245327 | 136,39 | 36,33 |  
carryless rangecoder <br/> [(reference algorithm by Dmitry Subbotin with the order-1 frequency model)](https://web.archive.org/web/20020420161153/http://www.softcomplete.com/algo/pack/rus-range.txt) |  | 480296513 | 28,12 |  |  
rangecoder (1.3) <br/> [(reference realization by Michael Schindler)](http://www.compressconsult.com/rangecoder/) | comp1/decomp1 | 489741259 | 44,36 | 53,35 |  
lz4 (1.9.4) | -1 | 509454838 | 10,07 | 8,97 |  
rangecoder (1.3) <br/> [(reference realization by Michael Schindler)](http://www.compressconsult.com/rangecoder/) | comp/decomp | 644426106 | 48,38 | 57,54 |  
arc (5.21i) | a | 886038366 | 38,49 | 20,50 |  
