# qbp  
  
Simple LZSS+RLE+RC32 compression algorithm realization.  
  
--- 
Usage : ./bin/qbp \<OPTIONS\> \<SOURCE\> \<DESTINATION\>  
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

**Test results**:  

| | |
----- | ----- |  
_Source file_ | [enwik9](https://cs.fit.edu/~mmahoney/compression/textdata.html) |  
_Source size (bytes)_ | 1000000000 |  
_Source hash (SHA1)_ | 2996e86fb978f93cca8f566cc56998923e7fe581 |  

Packer | Options | Resulting size (bytes) | Packing time (sec) | Unpacking time (sec) |
----- | ----- | ----- | ----- | ----- |    
zpaq (v7.15) | -m5 -t1 | 168590743 | 2842,39 | 2892,09 |  
paq8o9 | -1 | 193022025 | 7613,65 | 7556,39 |  
rk | -mx | 208096232 | 1657,54 | 1649,54 |  
xz | -9 -e | 211776220 | 871,57 | 13,38 |  
rar (rar5) | a -s -k -m5 -mt1 -md1g | 232248528 | 909,93 | 6,5 |  
bzip2 | -9 | 253977891 | 93,77 | 40,03 |  
ha | a2 | 285739328 | 284,51 | 290,46 |  
lha | a -o7 | 312912783 | 130,25 | 11,53 |  
gzip | -9 | 322591995 | 54,33 | 7,85 |  
pkzip | -add | 323644812 | 36,38 | 8,59 |  
makecab | | 324125809 | 58,86 | 5,81 |  
arj32 | a -m1 | 328553984 | 138,06 | 24,28 |  
lzop | -9 -F | 366349786 | 109,70 | 4,56 |  
lz4 | --best | 372443347 | 65,90 | 11,91 |  
**qbp** `(built with "gcc -O3")` | **c** | **373955706** | **45,62** | **35,64** |  
**unqbp** `(built with "gcc -O3")` |  | **373955706** |  | **27,63** |  
lzari <br/> [(reference realization by Haruhiko Okumura)](https://web.archive.org/web/19990209183635/http://oak.oakland.edu/pub/simtelnet/msdos/arcutils/lz_comp2.zip) | e | 388521578 | 182,69 | 77,72 |  
**lz16** `(built with "gcc -O3")` | **c** | **407218528** | **32,68** | **4,52** |  
**unlz16** `(built with "gcc -O3")` |  | **407218528** |  | **3,56** |  
lzss <br/> [(reference realization by Haruhiko Okumura)](https://oku.edu.mie-u.ac.jp/~okumura/compression/lzss.c) | e | 455245327 | 136,39 | 36,33 |  
lz4 | -1 | 509454838 | 10,07 | 8,97 |  
