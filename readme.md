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
Option  | Value |
----- | ------ |  
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

Packer | Options | Resulting size (bytes) | Time (sec)|
----- | ----- | ----- | ----- |  
xz | -9 -e | 211776220 | 871,57 |  
rar5 | a -s -k -m5 -mt1 -md1g | 232248528 | 909,93 |  
bzip2 | -9 | 253977891 | 93,77 |  
gzip | -9 | 322591995 | 54,33 |  
makecab | | 324125809 | 58,86 |  
lzop | -9 -F | 366349786 | 109,70 |  
lz4 | --best | 372443347 | 65,90 |  
**qbp** | c | **375322671** | **86,43** |  
lz4 | -1 | 509454838 | 10,07 |  
