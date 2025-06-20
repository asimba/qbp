# qbp  
  
Simple LZSS+RLE+RC32 compression algorithm realization.  
| Internal parameters | |
----- | ----- |  
_Sliding window size (bytes)_ | 65536 |  
_Window buffer size (bytes)_ | 259 |  
_Minimum match length_ | 4 |  
_Maximum match length_ | 259 |  
_Memory requirements (encoding algorithm, bytes)_ | ~724562‬ |  
_Memory requirements (decoding algorithm, bytes)_ | ~197140 |  
_Range coder type_ | byte-oriented, mixed order (0/1) |  
_Input buffer size (bytes)_ | 65536 |  
_Output buffer size (bytes)_ | 65536 |  
  
--- 
Usage :  
&emsp;[./qbp](https://github.com/asimba/qbp/raw/refs/heads/master/bin/linux-x86_64/qbp) \<OPTIONS\> \<SOURCE\> \<DESTINATION\>  
&emsp;[qbp.exe](https://github.com/asimba/qbp/raw/refs/heads/master/bin/windows-x86_64/qbp.exe) \<OPTIONS\> \<SOURCE\> \<DESTINATION\>  
**Examples:**  
```  
qbp c source_file packed_file  
qbp d packed_file unpacked_file  
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

**[Test results (direct link to qbp result)](#result)**:  

| | |
----- | ----- |  
_Source file_ | [enwik9](https://cs.fit.edu/~mmahoney/compression/textdata.html) |  
_Source size (bytes)_ | 1000000000 |  
_Source hash (SHA1)_ | 2996e86fb978f93cca8f566cc56998923e7fe581 |  
|_Testing platform_ | Intel Core i7 9700K / 64 GB RAM / Windows 10 Pro |  

| | Packer | Options | Resulting size (bytes) | Packing time (sec) | Unpacking time (sec) |
|----- | ----- | ----- | ----- | ----- | ----- |    
| | mcm (0.85) <br/> [(by Mathieu Chartier)](https://github.com/mathieuchartier/mcm) | -x11 | 139343403 | 942,72 | 938,02 |  
| | tangelo (1.0) <br/> [(by Matt Mahoney,Jan Ondrus)](https://encode.su/threads/1738-TANGELO-new-compressor-%28derived-from-PAQ8-FP8%29) | c | 156355536 | 7472,22 | 7549,93 |  
| | zcm (0.93) <br/> [(by Nania Francesco Antonio)](http://heartofcomp.altervista.org/Zcm.htm) | a -t1 -s -m8 | 159135549 | 295,03 | 297,55 |  
| | bcm (2.03) <br/> [(by Ilya Muravyov)](https://compressme.net/) | -b1000x- -t1 | 163646387 | 105,74 | 68,09 |  
| | zpaq (7.15) <br/> [(by Matt Mahoney)](https://mattmahoney.net/dc/zpaq.html) | a -m5 -t1 | 168590740 | 2472,30 | 2519,52 |  
| | lrzip (0.651) <br/> [(by Con Kolivas)](https://github.com/ckolivas/lrzip) | -z -p 1 -L 9 -U | 169357574 | 1967,30 | 1991,57 |  
| | rz (1.03.7) <br/> (by Christian Martelock) | a -d 1023M | 173041178 | 3070,63 | 14,31 |  
| | nanozip (0.09a) <br/> [(by Sami Runsas)](https://web.archive.org/web/20160304232219/http://nanozip.net/) | a -p1 -cO -m512m | 178225148 | 56,31 | 28,93 |  
| | freearc (0.51) <br/> [(by Bulat Ziganshin)](https://sourceforge.net/projects/freearc/) | a -mx | 185153701 | 219,45 | 187,81 |  
| | freearc (0.666) <br/> [(by Bulat Ziganshin)](https://web.archive.org/web/20161118235221/http://freearc.org/Download.aspx) | a -mt1 -mx | 189696374 | 210,74 | 182,99 |  
| | bsc (3.3.2) <br/> [(by Ilya Grebnov)](http://libbsc.com/) | e -b2047 -m8 -cf -e2 -H28 -M4 -t -T | 194367010 | 76,06 | 153,76 |  
| | lzip (1.23) <br/> [(Lzip home page)](http://www.nongnu.org/lzip/lzip.html) | -k -9 -s29 | 199138993 | 1134,42 | 20,09 |  
| | lzham (alpha 7 r1) <br/> [(by Rich Geldreich)](https://code.google.com/archive/p/lzham/downloads) | -m4 -d29 -t0 -x -o -e c | 199611734 | 4164,14 | 6,42 |  
| | ybs (0.03f) <br/> (by Vadim Yoockin) | -m16m | 202110510 | 314,00 | 138,69 |  
| | csarc (3.3) <br/> [(by Siyuan Fu)](https://github.com/fusiyuan2010/CSC) | a -m5 -d1024m -t1 | 203995004 | 491,60 | 16,07 |  
| | 7z (24.09) <br/> [(by Igor Pavlov)](https://7-zip.org/) | a -t7z -bd -ms=on -m0=LZMA2 -mx9 -myx=9 -mmt=1 -scsUTF-8 -ssc | 204719141 | 926,78 | 9,94 |  
| | nlzm (1.03) <br/> [(by Nauful)](https://github.com/nauful/NLZM) | -window:28 c | 206548024 | 1966,19 | 9,38 |  
| | uharc (0.6b) <br/> (by Uwe Herklotz) | a -d0 -md32768 -mm+ -mx | 208026696 | 493,92 | 383,33 |  
| | rar (7.11) <br/> [(by Alexander Roshal)](https://www.rarlab.com/) | a -s -k -m5 -mcx -mt1 -md4g | 208079690 | 2743,68 | 5,50 |  
| | rk (1.04.1a) <br/> (by Malcolm Taylor) | -mx | 208096232 | 1380,00 | 1372,69 |  
| | grzipii (0.2.4) <br/> [(by Ilya Grebnov)](http://web.archive.org/web/20070819094051/http://magicssoft.ru/?folder=projects&page=GRZipII) | e -b8m -m1 | 208993966 | 108,79 | 109,66 |  
| | xz (5.2.9) <br/> [(XZ Utils home page)](https://tukaani.org/xz/) | -9 -e | 211776220 | 871,57 | 13,38 |  
| | 7z (22.01) <br/> [(by Igor Pavlov)](https://7-zip.org/) | a -t7z -ms=on -m0=LZMA2 -mx9 -mmt=1 -scsUTF-8 -ssc | 213323101 | 690,12 | 8,50 |  
| | zstd (1.5.7) <br/> [(Zstandard GitHub home page)](https://github.com/facebook/zstd/) | --ultra -22 --no-progress --single-thread | 213726895 | 971,26 | 2,16 |  
| | boa (0.58b) <br/> (by Ian Sutton) | -a -s -m15 | 213845481 | 1758,10 | 1926,75 |  
| | flashzip (1.1.3) <br/> [(by Nania Francesco Antonio)](http://heartofcomp.altervista.org/Flashzip.htm) | a -t1 -s -mx3 -k7 -b32 | 213919392 | 152,78 | 35,27 |  
| | zstd (1.5.2) <br/> [(Zstandard GitHub home page)](https://github.com/facebook/zstd/) | --ultra -22 --no-progress --single-thread | 214910502 | 794,37 | 2,75 |  
| | dark (0.51) <br/> [(by Dmitry Malyshev)](http://darchiver.narod.ru/index.html) | p -b16mi0 | 218507990 | 97,00 | 98,09 |  
| | brotli (1.1.0) <br/> [(by Google)](https://github.com/google/brotli) | -Z -n | 223344816 | 2526,47 | 4,31 |  
| | sbc (0.969b) <br/> (by Sami J. Makinen) | c -m3 -b5 -os -hn | 224017402 | 291,50 | 104,75 |  
| | alzip (12.16.0.2) <br/> [(by ESTsoft)](https://www.altools.co.kr/Download/ALZip.aspx) | -a -s -m3 | 227906109 | 754,58 | 15,88 |  
| | rar (6.11) <br/> [(by Alexander Roshal)](https://www.rarlab.com/) | a -s -k -m5 -mt1 -md1g | 232248528 | 811,92 | 5,64 |  
| | szip (1.11b) <br/> [(by Michael Schindler)](http://www.compressconsult.com/szip/) | -b41 -o6 | 232346611 | 79,50 | 88,43 |  
| | balz (1.50) <br/> [(by Ilya Muravyov)](https://sourceforge.net/projects/balz/) | cx | 241575273 | 1059,91 | 33,53 |  
| | zzip (0.36c) <br/> [(by Damien Debin)](https://web.archive.org/web/20080509103426/http://debin.net/zzip/index.php) | a -a -mx -mm | 244746544 | 174,98 | 94,90 |  
| | squeez (6.0) <br/> [(by Sven Ritter)](https://www.speedproject.com/download/old/) | a /mx /fmt sqx | 246152882 | 597,93 | 8,89 |  
| | makecab/expand (5.0.1.1) <br/> [(by Microsoft Corporation)](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/makecab) | /D CompressionType=LZX /D CompressionMemory=21 | 250764075 | 539,68 | 4,90 |  
| | sr3a <br/> [(by Matt Mahoney, Nania Francesco Antonio)](http://mattmahoney.net/dc/sr3a.zip) | c | 253031977 | 55,95 | 67,09 |  
| | rzip (2.1) <br/> [(by Andrew Tridgell)](https://rzip.samba.org/) | -9 -k | 253920811 | 113,04 | 56,55 |  
| | bzip2 (1.0.8) <br/> [(bzip2 home page)](https://sourceware.org/bzip2/) | -9 | 253977891 | 81,44 | 40,03 |  
| | imp (1.12) <br/> [(by Technelysium Pty Ltd.)](https://technelysium.com.au/wp/imp-file-archiver/) | a -g -2 -mm | 256861382 | 77,49 | 21,71 |  
| | squid (0.03) <br/> [(by Ilya Muravyov)](https://compressme.net/) | cx | 269347430 | 65,66 | 34,37 |  
| | irolz <br/> [(by Andrew Polar)](http://ezcodesample.com/rolz/rolz_article.html) | e | 273786077 | 130,22 | 55,95 |  
| | crush (1.00) <br/> [(by Ilya Muravyov)](https://sourceforge.net/projects/crush/) | cx | 279491430 | 1812,58 | 7,96 |  
| | arhangel (1.40) <br/> (by George Lyapko) | a -mzf -mm -c31900 -1 -2 | 284775209 | 584,47 | 400,50 |  
| | lzpx (1.5b) <br/> [(by Ilya Muravyov)](https://compressme.net/) | e | 285234480 | 49,47 | 46,37 |  
| | ha (0.999c) <br/> (by Harri Hirvola) | a2 | 285739328 | 278,29 | 285,12 |  
| | jar (1.02) <br/> [(by ARJ Software)](http://www.arjsoftware.com/) | a -m4 | 288825883 | 179,01 | 60,11 |  
| | lizard (1.0.0) <br/> [(by Y.Collet, P.Skibinski)](https://github.com/inikep/lizard) | -49 -BD --no-frame-crc | 289266199 | 574,45 | 8,97 |  
| | 7z (22.01) <br/> [(by Igor Pavlov)](https://7-zip.org/) | a -tzip -m0=Deflate64 -mx9 | 298494477 | 861,46 | 6,26 |  
| | ace (1.2b) <br/> (by Marcel Lemke) | a -s -d512 -m5 | 298687158 | 173,79 | 8,24 |  
| | winzip (27.0) <br/> (using wzzip/wzunzip, limited to single thread) <br/> [(by Alludo)](https://www.winzip.com/) |  -a -ee | 298774317 | 456,93 | 8,33 |  
| | zhuff (0.99b) <br/> [(by Yann Collet)](https://web.archive.org/web/20230523125825/https://fastcompression.blogspot.com/p/zhuff.html) | -c2t1s | 308234163 | 34,38 | 1,54 |  
| | brieflz (1.3.0) <br/> [(by Joergen Ibsen)](https://github.com/jibsen/brieflz) | --optimal | 308781584 | 221,49 | 4,71 |  
| | zopfli (1.0.3) <br/> [(by Google)](https://github.com/google/zopfli/) |  | 309578152 | 2508,81 |  |  
| | hap/pah (3.00) <br/> (by Harald Feldmann) |  | 309615837 | 440,31 | 466,25 |  
| | rar (2.50) <br/> [(by Alexander Roshal)](https://www.rarlab.com/) | a -s -m5 | 309827109 | 196,01 | 38,05 |  
| | kzip <br/> [(by Ken Silverman)](http://advsys.net/ken/utils.htm) | /q /s0 | 310281906 | 2181,83 |  |  
| | 7z (22.01) <br/> [(by Igor Pavlov)](https://7-zip.org/) | a -tzip -m0=Deflate -mx9 | 310706257 | 790,08 | 6,54 |  
| | yzenc/yzdec (1.06.1) <br/> [(by BinaryTechnology)](https://web.archive.org/web/20040830010118/http://member.nifty.ne.jp/yamazaki/DeepFreezer/) |  | 312789435 | 48,74 | 23,86 |  
| | lha (1.14i) <br/> [(by Tsugio Okamoto, Koji Arai)](https://sourceforge.net/projects/gnuwin32/files/lha/1.14i/) | a -o7 | 312912781 | 113,52 | 10,46 |  
| :arrow_right: | <span id="result">$\textbf{qbp}$ <br/> `(built with "gcc -O3")` </span> <br/> [link](https://github.com/asimba/qbp/raw/refs/heads/master/bin/linux-x86_64/qbp) | $\textbf{c}$ | $\textbf{313013756}$ | $\textbf{43,89}$ | $\textbf{28,38}$ |  
| :arrow_right: | $\textbf{unqbp}$ <br/> `(built with "gcc -O3")` <br/> [link](https://github.com/asimba/qbp/raw/refs/heads/master/bin/linux-x86_64/unqbp) |  | $\textbf{313013756}$ |  | $\textbf{27,88}$ |  
| | uc2 (uc2 3 pro) <br/> (by Ad Infinitum Programs (AIP-NL)) | a -tt | 313795096 | 256,12 | 41,40 |  
| | thor (0.96a) <br/> [(Oscar Garcia)](https://web.archive.org/web/20080725180307/http://www.maximumcompression.com/thor_096.zip) | e4 | 314092324 | 26,36 | 9,73 |  
| | lzfse (1.0) <br/> [(LZFSE home page)](https://github.com/lzfse/lzfse/) | -encode | 319756993 | 25,34 | 9,67 |  
| | limit (1.2) <br/> (by J.Y. Lim) | a -mx | 320342601 | 138,10 | 38,25 |  
| | sqz (1.08.3) <br/> (by Jonas I. Hammarberg) | -q0 -m4 | 322400393 | 195,15 | 42,25 |  
| | gzip (1.12) <br/> [(GNU Gzip home page)](https://www.gnu.org/software/gzip/) | -9 | 322591995 | 49,25 | 8,73 |  
| | info-zip (3.0) <br/> [(using zip/unzip)](https://infozip.sourceforge.net/) | -9 | 322592221 | 46,73 | 6,61 |  
| | pkzip (2.50b) <br/> [(by PKWARE)](https://www.pkware.com/zip/products/pkzip) | /add | 323645288 | 32,94 | 7,04 |  
| | makecab/expand (5.0.1.1) <br/> [(by Microsoft Corporation)](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/makecab) |  | 324125809 | 57,91 | 5,81 |  
| | ain (2.32) <br/> (by Transas Marine (UK) Ltd.) | a /m1 | 324538744 | 90,10 | 28,51 |  
| | quark (1.06) <br/> (by Robert Kunz) | a | 325196497 | 177,25 | 97,30 |  
| | arj32 (3.20) <br/> [(by ARJ Software)](http://www.arjsoftware.com/) | a -m1 | 328536964 | 67,71 | 9,25 |  
| | ar <br/> (by Haruhiko Okumura) | a | 353022236 | 514,50 | 59,71 |  
| | zoo (2.10) <br/> [(source)](https://archive.debian.org/debian/pool/main/z/zoo/zoo_2.10.orig.tar.gz) | ah | 353022373 | 104,18 | 30,69 |  
| | ulz (1.02b) <br/> [(by Ilya Muravyov)](https://compressme.net/) | c9 | 360342071 | 45,76 | 1,18 |  
| | lzop (1.04) <br/> [(LZO home page)](http://www.lzop.org/) | -9 -F | 366349786 | 105,33 | 3,31 |  
| | hyper (2.6) <br/> (by Klaus Peter Nischke, Peter Sawatzki) | -a | 368163826 | 126,25 | 96,10 |  
| | lz4 (1.9.4) <br/> [(LZ4 home page)](https://github.com/lz4/lz4) | --best | 372443347 | 65,90 | 11,91 |  
| | lzss (0.02) <br/> [(by Ilya Muravyov)](http://compressme.net/lzss002.zip) | cx | 380192378 | 146,15 | 3,37 |  
| | lzari <br/> [(reference realization by Haruhiko Okumura)](https://web.archive.org/web/19990209183635/http://oak.oakland.edu/pub/simtelnet/msdos/arcutils/lz_comp2.zip) | e | 388521578 | 182,69 | 77,72 |  
| | mtari (0.02) <br/> [(by David Catt)](https://encode.su/threads/1837-FARI-Fast-Arithmetic-Compressor?p=35705&viewfull=1#post35705) | c 1 | 397232608 | 32,12 | 38,99 |  
| :arrow_right: | $\textbf{lz16}$ <br/> `(built with "gcc -O3")` <br/> [link](https://github.com/asimba/qbp/raw/refs/heads/master/bin/linux-x86_64/lz16) | $\textbf{c}$ | $\textbf{407180647}$ | $\textbf{31,50}$ | $\textbf{3,34}$ |  
| :arrow_right: | $\textbf{unlz16}$ <br/> `(built with "gcc -O3")` <br/> [link](https://github.com/asimba/qbp/raw/refs/heads/master/bin/linux-x86_64/unlz16) |  | $\textbf{407180647}$ |  | $\textbf{3,34}$ |  
| | qpress (1.1) <br/> [(by Lasse Reinhold)](https://web.archive.org/web/20220524060234/http://www.quicklz.com/index.php) | -L3T1i | 422626641 | 41,11 | 8,25 |  
| | ncompress (5.1) <br/> [(by Mike Frysinger)](https://github.com/vapier/ncompress) |  | 448136005 | 31,98 | 12,85 |  
| :arrow_right: | $\textbf{rc32}$ <br/> `(built with "gcc -O3")` <br/> [link](https://github.com/asimba/qbp/raw/refs/heads/master/bin/linux-x86_64/rc32) | $\textbf{c}$ | $\textbf{454498180}$ | $\textbf{26,85}$ | $\textbf{60,60}$ |  
| :arrow_right: | $\textbf{unrc32}$ <br/> `(built with "gcc -O3")` <br/> [link](https://github.com/asimba/qbp/raw/refs/heads/master/bin/linux-x86_64/unrc32) |  | $\textbf{454498180}$ |  | $\textbf{60,91}$ |  
| | larc (3.33) <br/> (by Haruhiko Okumura, Kazuhiko Miki) | a | 455245358 | 214,15 | 36,23 |  
| | lzss <br/> [(reference realization by Haruhiko Okumura)](https://oku.edu.mie-u.ac.jp/~okumura/compression/lzss.c) | e | 455245327 | 136,39 | 36,33 |  
| | shrinker <br/> [(by Siyuan Fu)](https://code.google.com/archive/p/data-shrinker/) | c | 459825318 | 4,76 | 1,74 |  
| | lzwc (0.7) <br/> [(by David Catt)](https://encode.su/threads/1661-LZWC-A-fast-tree-based-LZW-compressor?p=31955&viewfull=1#post31955) | c | 463892454 | 38,64 | 36,78 |  
| | context compressor <br/> [(by Maxim Smirnov)](http://ezcodesample.com/ralpha/Subbotin.txt) | c | 478467738 | 55,85 | 90,64 |  
| | carryless rangecoder <br/> [(reference algorithm by Dmitry Subbotin with the order-1 frequency model)](https://web.archive.org/web/20020420161153/http://www.softcomplete.com/algo/pack/rus-range.txt) |  | 480296513 | 28,12 |  |  
| | rangecoder (1.3) <br/> [(reference realization by Michael Schindler)](http://www.compressconsult.com/rangecoder/) | comp1/decomp1 | 489741259 | 44,36 | 53,35 |  
| | flzp (1.0) <br/> [(by Matt Mahoney)](http://www.mattmahoney.net/dc/flzp.zip) | c | 497535428 | 74,15 | 16,34 |  
| | snappy (1.1.1.8) <br/> [(by Robert Važan)](https://snappy.machinezoo.com/) <br/>[(algorithm by Google)](https://google.github.io/snappy/) | snzip/snunzip | 507879264 | 3,97 | 2,25 |  
| | lz4 (1.9.4) <br/> [(LZ4 home page)](https://github.com/lz4/lz4) | -1 | 509454838 | 10,07 | 8,97 |  
| | fpaqc <br/> [(by Matt Mahoney)](http://mattmahoney.net/dc/fpaqc.zip) | c | 620278358 | 112,53 | 89,40 |  
| | fpaqb (2) <br/> [(by Matt Mahoney)](http://mattmahoney.net/dc/fpaqb.zip) | c | 620278361 | 116,20 | 81,57 |  
| | q-coder <br/> [(realization by Dmitry Mastrukov)](http://compression.ru/download/sources/ar/mastrukov_arith.rar) | e | 635523992 | 109,20 | 118,21 |  
| | fpc <br/> [(by Konstantinos Agiannis)](https://github.com/kagiannis/FPC) | -b 0 | 635922910 | 9,21 | 2,12 |  
| | fpaq0 <br/> [(by Matt Mahoney)](http://mattmahoney.net/dc/fpaq0.cpp) | c | 641421110 | 95,09 | 122,44 |  
| | rangecoder (1.3) <br/> [(reference realization by Michael Schindler)](http://www.compressconsult.com/rangecoder/) | comp/decomp | 644426106 | 48,38 | 57,54 |  
| | tinylzp (0.1) <br/> [(by David Catt)](https://encode.su/threads/1619) | c | 694274932 | 12,44 | 8,73 |  
| | arc (5.21) <br/> [(source)](https://archive.debian.org/debian/pool/main/a/arc/arc_5.21q.orig.tar.gz) | a | 886038366 | 38,49 | 20,50 |  
