/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE
/***********************************************************************************************************/

package main

import (
	"errors"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sync"
	"time"
)

type iotype interface {
	Read(b []byte) (int, error)
	Write(b []byte) (int, error)
}

const (
	LZ_BUF_SIZE  uint16 = 259
	LZ_CAPACITY  uint8  = 24
	LZ_MIN_MATCH uint16 = 3
	LZ_EOF       uint16 = 0x100
	IO_BUF_SIZE  int    = 0x10000
	VOC_SIZE     int    = 0x10000
)

type vocpntr struct {
	in   uint16
	out  uint16
	skip bool
}

type commonwork struct {
	ibuf    []uint8
	obuf    []uint8
	vocbuf  [VOC_SIZE]uint8
	icbuf   int
	rpos    int
	wpos    int
	vocroot uint16
	length  uint16
	offset  uint16
	flags   uint8
	ifile   iotype
	ofile   iotype
	eoff    bool
	Err     int
	stat    chan [2]int
}

type Compressor struct {
	commonwork
	cbuffer [LZ_CAPACITY + 1]uint8
	vocarea [VOC_SIZE]uint16
	hashes  [VOC_SIZE]uint16
	vocindx []vocpntr
	cpos    uint8
	voclast uint16
}

type Decompressor struct {
	commonwork
	cbuffer  [LZ_MIN_MATCH + 1]uint8
	rle_flag bool
}

func (p *commonwork) initialize(ifile, ofile iotype, stat chan [2]int) {
	if p.ibuf == nil || len(p.ibuf) != IO_BUF_SIZE {
		p.ibuf = make([]uint8, IO_BUF_SIZE)
	}
	if p.obuf == nil || len(p.obuf) != IO_BUF_SIZE {
		p.obuf = make([]uint8, IO_BUF_SIZE)
	}
	p.icbuf, p.wpos, p.rpos, p.length, p.vocroot, p.Err = 0, 0, 0, 0, 0, 0
	p.ifile, p.ofile, p.eoff, p.stat = ifile, ofile, false, stat
}

func (p *Compressor) Initialize(ifile, ofile iotype, stat chan [2]int) {
	p.initialize(ifile, ofile, stat)
	p.flags, p.cpos, p.voclast = 8, 1, 0xfffc
	if p.vocindx == nil || len(p.vocindx) != VOC_SIZE {
		p.vocindx = make([]vocpntr, VOC_SIZE)
	}
	for i := range VOC_SIZE {
		p.vocbuf[i], p.hashes[i], p.vocindx[i].skip, p.vocarea[i] = 0xff, 0, true, uint16(i)+1
	}
	p.vocindx[0].in, p.vocindx[0].out, p.vocindx[0].skip = 0, 0xfffc, false
	p.vocarea[0xfffc], p.vocarea[0xfffd], p.vocarea[0xfffe], p.vocarea[0xffff] = 0xfffc, 0xfffd, 0xfffe, 0xffff
}

func (p *Decompressor) Initialize(ifile, ofile iotype, stat chan [2]int) {
	p.initialize(ifile, ofile, stat)
	p.flags = 0
	for i := range VOC_SIZE {
		p.vocbuf[i] = 0xff
	}
	p.rle_flag = false
}

func NewCompressor(ifile, ofile iotype, stat chan [2]int) *Compressor {
	var pack Compressor
	pack.Initialize(ifile, ofile, stat)
	return &pack
}

func NewDecompressor(ifile, ofile iotype, stat chan [2]int) *Decompressor {
	var pack Decompressor
	pack.Initialize(ifile, ofile, stat)
	return &pack
}

func (p *commonwork) Wbuf(c uint8) {
	if p.wpos == IO_BUF_SIZE {
		p.wpos = 0
		if _, err := p.ofile.Write(p.obuf); err != nil {
			p.Err = 8
			return
		}
		if p.stat != nil {
			p.stat <- [2]int{0, IO_BUF_SIZE}
		}
	}
	p.obuf[p.wpos] = c
	p.wpos++
}

func (p *commonwork) Rbuf() (c uint8) {
	var err error
	if p.rpos == p.icbuf {
		p.rpos = 0
		if p.icbuf, err = p.ifile.Read(p.ibuf); err != nil || p.icbuf == 0 {
			return
		}
		if p.stat != nil {
			p.stat <- [2]int{p.icbuf, 0}
		}
	}
	c = p.ibuf[p.rpos]
	p.rpos++
	return
}

func (p *Compressor) Finalize() {
	p.eoff = true
	for p.length != 0 {
		if p.PutC(0); p.Err != 0 {
			return
		}
	}
	if _, err := p.ofile.Write(p.obuf[:p.wpos]); err != nil {
		p.Err = 8
		return
	}
	if p.stat != nil {
		p.stat <- [2]int{0, int(p.wpos)}
	}
}

func (p *Decompressor) Finalize() {
	if _, err := p.ofile.Write(p.obuf[:p.wpos]); err != nil {
		p.Err = 8
	}
	if p.stat != nil {
		p.stat <- [2]int{0, int(p.wpos)}
	}
}

func (p *Compressor) PutC(b uint8) {
	var offset, symbol, rle, rle_shift, length, i, k uint16
putc_loop:
	for {
		if p.length != LZ_BUF_SIZE && !p.eoff {
			if p.vocarea[p.vocroot] == p.vocroot {
				p.vocindx[p.hashes[p.vocroot]].skip = true
			} else {
				p.vocindx[p.hashes[p.vocroot]].in = p.vocarea[p.vocroot]
			}
			p.vocarea[p.vocroot], p.vocbuf[p.vocroot] = p.vocroot, b
			hash := p.hashes[p.voclast] ^ uint16(p.vocbuf[p.voclast]) ^ uint16(b)
			hash = (hash << 4) | (hash >> 12)
			p.voclast++
			p.vocroot++
			p.length++
			p.hashes[p.voclast] = hash
			if p.vocindx[hash].skip {
				p.vocindx[hash].in = p.voclast
			} else {
				p.vocarea[p.vocindx[hash].out] = p.voclast
			}
			p.vocindx[hash].skip, p.vocindx[hash].out = false, p.voclast
			break
		}
		p.cbuffer[0] <<= 1
		length = LZ_MIN_MATCH
		if p.length != 0 {
			symbol = p.vocroot - p.length
			rle_shift = symbol + LZ_BUF_SIZE
			for rle = symbol + 1; rle != p.vocroot && p.vocbuf[symbol] == p.vocbuf[rle]; rle++ {
			}
			if rle -= symbol; p.length > LZ_MIN_MATCH && rle != p.length {
				for cnode := p.vocindx[p.hashes[symbol]].in; cnode != symbol; cnode = p.vocarea[cnode] {
					if p.vocbuf[uint16(symbol+length)] == p.vocbuf[uint16(cnode+length)] {
						for i, k = symbol, cnode; i != p.vocroot && p.vocbuf[i] == p.vocbuf[k]; i, k = i+1, k+1 {
						}
						if i -= symbol; i >= length {
							if k = cnode - rle_shift; k <= 0xfefe {
								if length, offset = i, k; i >= p.length {
									break
								}
							}
						}
					}
				}
			}
			if rle > length {
				length, offset = rle, uint16(p.vocbuf[symbol])
			} else {
				offset = ^offset
			}
			if rle = uint16(length - LZ_MIN_MATCH); rle != 0 {
				p.cbuffer[p.cpos] = uint8(rle - 1)
				p.cpos++
				p.cbuffer[p.cpos] = uint8(offset)
				p.cpos++
				p.cbuffer[p.cpos] = uint8(offset >> 8)
				p.length -= length
			} else {
				p.cbuffer[p.cpos] = p.vocbuf[symbol]
				p.cbuffer[0] |= 1
				p.length--
			}
		} else {
			p.cpos++
			p.cbuffer[p.cpos] = 0
			p.cpos++
			p.cbuffer[p.cpos] = 1
			length = 0
		}
		p.cpos++
		p.flags--
		if p.flags != 0 && length != 0 {
			continue
		}
		p.cbuffer[0] <<= p.flags
		for i := range p.cpos {
			if p.Wbuf(p.cbuffer[i]); p.Err != 0 {
				break putc_loop
			}
		}
		if p.flags, p.cpos = 8, 1; length == 0 {
			break
		}
	}
}

func (p *Decompressor) GetC() (b uint8) {
getc_loop:
	for {
		if p.length != 0 {
			if !p.rle_flag {
				p.cbuffer[1] = p.vocbuf[p.offset]
				p.offset++
			}
			b, p.vocbuf[p.vocroot] = p.cbuffer[1], p.cbuffer[1]
			p.vocroot++
			p.length--
			break
		}
		if p.flags != 0 {
			p.length, p.rle_flag = 1, true
			if p.cbuffer[0]&0x80 != 0 {
				if p.cbuffer[1] = p.Rbuf(); p.Err != 0 {
					break
				}
			} else {
				for c := uint8(1); c < 4; c++ {
					if p.cbuffer[c] = p.Rbuf(); p.Err != 0 {
						break getc_loop
					}
				}
				p.length = LZ_MIN_MATCH + 1 + uint16(p.cbuffer[1])
				p.offset = uint16(p.cbuffer[2]) | uint16(p.cbuffer[3])<<8
				switch {
				case p.offset > LZ_EOF:
					p.offset, p.rle_flag = ^p.offset+p.vocroot+LZ_BUF_SIZE, false
				case p.offset == LZ_EOF:
					p.eoff = true
					break getc_loop
				default:
					p.cbuffer[1] = uint8(p.offset)
				}
			}
			p.cbuffer[0] <<= 1
			p.flags--
			continue
		}
		p.flags = 8
		if p.cbuffer[0] = p.Rbuf(); p.Err != 0 {
			break
		}
	}
	return
}

func (p *Decompressor) Proceed() {
	var c uint8
	for {
		if c = p.GetC(); p.Err != 0 {
			break
		}
		if p.eoff {
			p.Finalize()
			break
		}
		if p.Wbuf(c); p.Err != 0 {
			break
		}
	}
}

func (p *Compressor) Proceed() {
	for {
		if b := p.Rbuf(); p.rpos == 0 {
			p.Finalize()
			break
		} else if p.PutC(b); p.Err != 0 {
			break
		}
	}
}

func main() {
	var (
		exitCode     int = 0
		errMsg       string
		wg           sync.WaitGroup
		ifile, ofile *os.File
		info         os.FileInfo
		stat         chan [2]int
		err          error
	)
	n := filepath.Base(os.Args[0])
	help := func() {
		fmt.Print("\n",
			"--------------------------------------------------------------\n",
			"\t\t      lz16 file compressor\n",
			"--------------------------------------------------------------\n",
			"Usage:\n",
			"--------------------------------------------------------------\n\t",
			n, " <c|d>[s] <input_file|-> <output_file|->\n",
			"\t------------------------------------------------------\n",
			"\texample:\n\t\t", n, " c input_file_name output_file_name\n",
			"\t------------------------------------------------------\n",
			"\t'c' - compress mode\n",
			"\t'd' - decompress mode\n",
			"\t's' - suppress progress output\n",
			"\t'-' - use stdin/stdout for input/output\n",
			"--------------------------------------------------------------\n")
	}
	defer func() {
		switch exitCode {
		case 1:
			errMsg = "Error: wrong mode!"
		case 2:
			errMsg = "Error: input file doesn't exist!"
		case 3:
			errMsg = "Error: input file is a directory!"
		case 4:
			errMsg = "Error: input file is not a regular file!"
		case 5:
			errMsg = "Error: output file is already exist!"
		case 6:
			errMsg = "Error: unable to open input file!"
		case 7:
			errMsg = "Error: unable to create output file!"
		case 8:
			errMsg = "Error writing output file!"
		case 9:
			errMsg = "Error reading input file!"
		}
		if exitCode != 0 {
			if exitCode == 1 {
				help()
			} else {
				log.Println(errMsg)
			}
		}
		os.Exit(exitCode)
	}()
	defer func() {
		if stat != nil {
			close(stat)
			wg.Wait()
		}
	}()
	if len(os.Args) == 4 {
		goto normal
	} else if len(os.Args) != 1 {
		exitCode = 1
		return
	}
	help()
	return
normal:
	if os.Args[2] != "-" {
		info, err = os.Stat(os.Args[2])
		if errors.Is(err, os.ErrNotExist) {
			exitCode = 2
			return
		}
		if info.IsDir() {
			exitCode = 3
			return
		}
		if !info.Mode().IsRegular() {
			exitCode = 4
			return
		}
	} else {
		ifile = os.Stdin
	}
	switch os.Args[1][0] {
	case 'c', 'd':
		if ifile == nil {
			ifile, err = os.OpenFile(os.Args[2], os.O_RDONLY, 0644)
			if err != nil {
				exitCode = 6
				return
			}
			defer ifile.Close()
		}
		if os.Args[3] != "-" {
			ofile, err = os.OpenFile(os.Args[3], os.O_CREATE|os.O_WRONLY|os.O_EXCL, 0644)
			if err != nil {
				if os.IsExist(err) {
					exitCode = 5
				} else {
					exitCode = 7
				}
				return
			}
			defer func() {
				ofile.Sync()
				ofile.Close()
			}()
		} else {
			ofile = os.Stdout
		}
		if !(len(os.Args[1]) != 1 && os.Args[1][1] == 's') {
			stat = make(chan [2]int, 8)
			wg.Go(func() {
				var rsize, wsize int64 = 0, 0
				start := time.Now()
				for v := range stat {
					rsize += int64(v[0])
					wsize += int64(v[1])
					fmt.Fprintf(os.Stderr, "\rProcessing [%#07.02fs] : %#-12d->%#12d", time.Since(start).Seconds(), rsize, wsize)
				}
				fmt.Fprintf(os.Stderr, "\n")
			})
		}
		if os.Args[1][0] == 'c' {
			pack := NewCompressor(ifile, ofile, stat)
			pack.Proceed()
			exitCode = pack.Err
		} else {
			pack := NewDecompressor(ifile, ofile, stat)
			pack.Proceed()
			exitCode = pack.Err
		}
	default:
		exitCode = 1
		return
	}
}
