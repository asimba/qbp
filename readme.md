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
mcm (0.85) <br/> [(by Mathieu Chartier)](https://github.com/mathieuchartier/mcm) | -x11 | 139343403 | 942,72 | 938,02 |  
tangelo (1.0) <br/> [(by Matt Mahoney,Jan Ondrus)](https://encode.su/threads/1738-TANGELO-new-compressor-%28derived-from-PAQ8-FP8%29) | c | 156355536 | 7472,22 | 7549,93 |  
zcm (0.93) <br/> [(by Nania Francesco Antonio)](http://heartofcomp.altervista.org/index.htm) | a -t1 -s -m8 | 159135549 | 295,03 | 297,55 |  
bcm (2.03) <br/> [(by Ilya Muravyov)](https://compressme.net/) | -b1000x- -t1 | 163646387 | 105,74 | 68,09 |  
zpaq (7.15) <br/> [(by Matt Mahoney)](https://mattmahoney.net/dc/zpaq.html) | a -m5 -t1 | 168590740 | 2472,30 | 2519,52 |  
lrzip (0.651) <br/> [(by Con Kolivas)](https://github.com/ckolivas/lrzip) | -z -p 1 -L 9 -U | 169357574 | 1967,30 | 1991,57 |  
rz (1.03.7) <br/> (by Christian Martelock) | a -d 1023M | 173041178 | 3070,63 | 14,31 |  
nanozip (0.09a) <br/> [(by Sami Runsas)](https://web.archive.org/web/20160304232219/http://nanozip.net/) | a -p1 -cO -m512m | 178225148 | 56,31 | 28,93 |  
freearc (0.51) <br/> [(by Bulat Ziganshin)](https://sourceforge.net/projects/freearc/) | a -mx | 185153701 | 219,45 | 187,81 |  
freearc (0.666) <br/> [(by Bulat Ziganshin)](https://web.archive.org/web/20161118235221/http://freearc.org/Download.aspx) | a -mt1 -mx | 189696374 | 210,74 | 182,99 |  
lzip (1.23) <br/> [Lzip home page)](http://www.nongnu.org/lzip/lzip.html) | -k -9 -s29 | 199138993 | 1134,42 | 20,09 |  
ybs (0.03f) <br/> (by Vadim Yoockin) | -m16m | 202110510 | 314,00 | 138,69 |  
uharc (0.6b) | a -d0 -md32768 -mm+ -mx | 208026696 | 493,92 | 383,33 |  
rk (1.04.1a) | -mx | 208096232 | 1380,00 | 1372,69 |  
xz (5.2.9) <br/> [XZ Utils home page)](https://tukaani.org/xz/) | -9 -e | 211776220 | 871,57 | 13,38 |  
7z (22.01) <br/> [(by Igor Pavlov)](https://7-zip.org/) | a -t7z -ms=on -m0=LZMA2 -mx9 -mmt=1 -scsUTF-8 -ssc | 213323101 | 690,12 | 8,50 |  
boa (0.58b) <br/> (by Ian Sutton) | -a -s -m15 | 213845481 | 1758,10 | 1926,75 |  
zstd (1.5.2) <br/> [Zstandard GitHub home page)](https://github.com/facebook/zstd/) | --ultra -22 --no-progress --single-thread | 214910502 | 794,37 | 2,75 |  
brotli (1.0.9) [(by Google)](https://github.com/google/brotli) | -Z -n | 223345718 | 2287,31 | 3,36 |  
sbc (0.969b) | c -m3 -b5 -os -hn | 224017402 | 291,50 | 104,75 |  
rar (6.11) [(by Alexander Roshal)](https://www.rarlab.com/) | a -s -k -m5 -mt1 -md1g | 232248528 | 811,92 | 5,64 |  
szip (1.11b) <br/> [(by Michael Schindler)](http://www.compressconsult.com/szip/) | -b41 -o6 | 232346611 | 79,50 | 88,43 |  
balz (1.50) <br/> [(by Ilya Muravyov)](https://sourceforge.net/projects/balz/) | cx | 241575273 | 1059,91 | 33,53 |  
squeez (6.0) | a /mx /fmt sqx | 246152882 | 597,93 | 8,89 |  
sr3a <br/> [(by Matt Mahoney,Nania Francesco Antonio)](http://mattmahoney.net/dc/sr3a.zip) | c | 253031977 | 55,95 | 67,09 |  
rzip (2.1) <br/> [(by Andrew Tridgell)](https://rzip.samba.org/) | -9 -k | 253920811 | 113,04 | 56,55 |  
bzip2 (1.0.8) <br/> [(bzip2 home page)](https://sourceware.org/bzip2/) | -9 | 253977891 | 81,44 | 40,03 |  
imp (1.12) <br/> [(by Technelysium Pty Ltd.)](https://technelysium.com.au/wp/imp-file-archiver/) | a -g -2 -mm | 256861382 | 77,49 | 21,71 |  
squid (0.03) <br/> [(by Ilya Muravyov)](https://compressme.net/) | cx | 269347430 | 65,66 | 34,37 |  
crush (1.00) <br/> [(by Ilya Muravyov)](https://sourceforge.net/projects/crush/) | cx | 279491430 | 1812,58 | 7,96 |  
arhangel (1.40) <br/> (by George Lyapko) | a -mzf -mm -c31900 -1 -2 | 284775209 | 584,47 | 400,50 |  
lzpx (1.5b) <br/> [(by Ilya Muravyov)](https://compressme.net/) | e | 285234480 | 49,47 | 46,37 |  
ha (0.999c) <br/> (by Harri Hirvola) | a2 | 285739328 | 278,29 | 285,12 |  
jar (1.02) <br/> [(by ARJ Software)](http://www.arjsoftware.com/) | a -m4 | 288825883 | 179,01 | 60,11 |  
lizard (1.0.0) <br/> [(by Y.Collet & P.Skibinski)](https://github.com/inikep/lizard) | -49 -BD --no-frame-crc | 289266199 | 574,45 | 8,97 |  
7z (22.01) <br/> [(by Igor Pavlov)](https://7-zip.org/) | a -tzip -m0=Deflate64 -mx9 | 298494477 | 861,46 | 6,26 |  
ace (1.2b) | a -s -d512 -m5 | 298687158 | 173,79 | 8,24 |  
winzip (27.0) <br/> (using wzzip/wzunzip,limited to single thread) |  -a -ee | 298774317 | 456,93 | 8,33 |  
brieflz (1.3.0) <br/> [(by Joergen Ibsen)](https://github.com/jibsen/brieflz) | --optimal | 308781584 | 221,49 | 4,71 |  
zopfli (1.0.3) [(by Google)](https://github.com/google/zopfli/) |  | 309578152 | 2508,81 |  |  
hap/pah (3.00) <br/> (by Harald Feldmann) |  | 309615837 | 440,31 | 466,25 |  
rar (2.50) [(by Alexander Roshal)](https://www.rarlab.com/) | a -s -m5 | 309827109 | 196,01 | 38,05 |  
kzip <br/> [(by Ken Silverman)](http://advsys.net/ken/utils.htm) | /q /s0 | 310281906 | 2181,83 |  |  
7z (22.01) <br/> [(by Igor Pavlov)](https://7-zip.org/) | a -tzip -m0=Deflate -mx9 | 310706257 | 790,08 | 6,54 |  
yzenc/yzdec (1.06.1) <br/> [(by BinaryTechnology)](https://web.archive.org/web/20040830010118/http://member.nifty.ne.jp/yamazaki/DeepFreezer/) |  | 312789435 | 48,74 | 23,86 |  
lha (1.14i) | a -o7 | 312912781 | 113,52 | 10,46 |  
uc2 (uc2 3 pro) | a -tt | 313795096 | 256,12 | 41,40 |  
$\textcolor[RGB]{0,154,23}{\textbf{qbp}}$ `(built with "gcc -O3")` | $\textcolor[RGB]{0,154,23}{\textbf{c}}$ | $\textcolor[RGB]{0,154,23}{\textbf{317542161}}$ | $\textcolor[RGB]{0,154,23}{\textbf{45,56}}$ | $\textcolor[RGB]{0,154,23}{\textbf{29,46}}$ |  
$\textcolor[RGB]{0,154,23}{\textbf{unqbp}}$ `(built with "gcc -O3")` |  | $\textcolor[RGB]{0,154,23}{\textbf{317542161}}$ |  | $\textcolor[RGB]{0,154,23}{\textbf{30,20}}$ |  
lzfse (1.0) <br/> [(LZFSE home page)](https://github.com/lzfse/lzfse/) | -encode | 319756993 | 25,34 | 9,67 |  
limit (1.2) | a -mx | 320342601 | 138,10 | 38,25 |  
sqz (1.08.3) | -q0 -m4 | 322400393 | 195,15 | 42,25 |  
gzip (1.12) <br/> [(GNU Gzip home page)](https://www.gnu.org/software/gzip/) | -9 | 322591995 | 49,25 | 8,73 |  
info-zip (3.0) <br/> [(using zip/unzip)](https://infozip.sourceforge.net/) | -9 | 322592221 | 46,73 | 6,61 |  
pkzip (2.50b) | /add | 323645288 | 32,94 | 7,04 |  
makecab/expand (5.0.1.1) |  | 324125809 | 57,91 | 5,81 |  
ain (2.32) | a /m1 | 324538744 | 90,10 | 28,51 |  
quark (1.06) | a | 325196497 | 177,25 | 97,30 |  
arj32 (3.20) <br/> [(by ARJ Software)](http://www.arjsoftware.com/) | a -m1 | 328536964 | 67,71 | 9,25 |  
ar <br/> (by Haruhiko Okumura) | a | 353022236 | 514,50 | 59,71 |  
zoo (2.10) <br/> [(source)](https://archive.debian.org/debian/pool/main/z/zoo/zoo_2.10.orig.tar.gz) | ah | 353022373 | 104,18 | 30,69 |  
ulz (1.02b) <br/> [(by Ilya Muravyov)](https://compressme.net/) | c9 | 360342071 | 45,76 | 1,18 |  
lzop (1.04) <br/> [(LZO home page)](http://www.lzop.org/) | -9 -F | 366349786 | 105,33 | 3,31 |  
hyper (2.6) <br/> (by Klaus Peter Nischke,Peter Sawatzki) | -a | 368163826 | 126,25 | 96,10 |  
lz4 (1.9.4) <br/> [(LZ4 home page)](https://github.com/lz4/lz4) | --best | 372443347 | 65,90 | 11,91 |  
lzss (0.02) <br/> [(by Ilya Muravyov)](http://compressme.net/lzss002.zip) | cx | 380192378 | 146,15 | 3,37 |  
lzari <br/> [(reference realization by Haruhiko Okumura)](https://web.archive.org/web/19990209183635/http://oak.oakland.edu/pub/simtelnet/msdos/arcutils/lz_comp2.zip) | e | 388521578 | 182,69 | 77,72 |  
$\textcolor[RGB]{0,154,23}{\textbf{lz16}}$ `(built with "gcc -O3")` | $\textcolor[RGB]{0,154,23}{\textbf{c}}$ | $\textcolor[RGB]{0,154,23}{\textbf{407180647}}$ | $\textcolor[RGB]{0,154,23}{\textbf{31,50}}$ | $\textcolor[RGB]{0,154,23}{\textbf{3,86}}$ |  
$\textcolor[RGB]{0,154,23}{\textbf{unlz16}}$ `(built with "gcc -O3")` |  | $\textcolor[RGB]{0,154,23}{\textbf{407180647}}$ |  | $\textcolor[RGB]{0,154,23}{\textbf{3,86}}$ |  
qpress (1.1) <br/> [(by Lasse Reinhold)](https://web.archive.org/web/20220524060234/http://www.quicklz.com/index.php) | -L3T1i | 422626641 | 41,11 | 8,25 |  
ncompress (5.1) |  | 448136005 | 31,98 | 12,85 |  
larc (3.33) <br/> (by Haruhiko Okumura,Kazuhiko Miki) | a | 455245358 | 214,15 | 36,23 |  
lzss <br/> [(reference realization by Haruhiko Okumura)](https://oku.edu.mie-u.ac.jp/~okumura/compression/lzss.c) | e | 455245327 | 136,39 | 36,33 |  
shrinker <br/> [(Shinker Fast&Light Compression Demo by fusiyuan2010@gmail.com)](https://code.google.com/archive/p/data-shrinker/) | c | 459825318 | 4,76 | 1,74 |  
carryless rangecoder <br/> [(reference algorithm by Dmitry Subbotin with the order-1 frequency model)](https://web.archive.org/web/20020420161153/http://www.softcomplete.com/algo/pack/rus-range.txt) |  | 480296513 | 28,12 |  |  
rangecoder (1.3) <br/> [(reference realization by Michael Schindler)](http://www.compressconsult.com/rangecoder/) | comp1/decomp1 | 489741259 | 44,36 | 53,35 |  
flzp (1.0) <br/> [(by Matt Mahoney)](http://www.mattmahoney.net/dc/flzp.zip) | c | 497535428 | 74,15 | 16,34 |  
lz4 (1.9.4) <br/> [(github)](https://github.com/lz4/lz4) | -1 | 509454838 | 10,07 | 8,97 |  
fpaqc <br/> [(by Matt Mahoney)](http://mattmahoney.net/dc/fpaqc.zip) | c | 620278358 | 112,53 | 89,40 |  
fpaqb (2) <br/> [(by Matt Mahoney)](http://mattmahoney.net/dc/fpaqb.zip) | c | 620278361 | 116,20 | 81,57 |  
fpaq0 <br/> [(by Matt Mahoney)](http://mattmahoney.net/dc/fpaq0.cpp) | c | 641421110 | 95,09 | 122,44 |  
rangecoder (1.3) <br/> [(reference realization by Michael Schindler)](http://www.compressconsult.com/rangecoder/) | comp/decomp | 644426106 | 48,38 | 57,54 |  
arc (5.21i) | a | 886038366 | 38,49 | 20,50 |  
