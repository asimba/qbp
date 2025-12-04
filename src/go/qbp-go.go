/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
/***********************************************************************************************************/

package main

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"time"
	"unsafe"
)

type iotype interface {
	Read(b []byte) (int, error)
	Write(b []byte) (int, error)
}

const (
	LZ_BUF_SIZE  uint16 = 259
	LZ_CAPACITY  uint8  = 24
	LZ_MIN_MATCH uint16 = 3
)

type vocpntr struct {
	in   uint16
	out  uint16
	skip bool
}

type Packer struct {
	ibuf      [0x10000]uint8
	obuf      [0x10000]uint8
	icbuf     uint32
	wpos      uint32
	rpos      uint32
	cbuffer   [LZ_CAPACITY + 1]uint8
	cntxs     [LZ_CAPACITY + 2]uint8
	vocbuf    [0x10000]uint8
	vocarea   [0x10000]uint16
	hashes    [0x10000]uint16
	vocindx   [0x10000]vocpntr
	frequency [256][256]uint16
	fcs       [256]uint16
	flags     uint8
	cpos      uint8
	buf_size  uint16
	voclast   uint16
	vocroot   uint16
	length    uint16
	offset    uint16
	low       uint32
	hlp       uint32
	rnge      uint32
	_low      *uint8
	_hlp      *uint8
	ifile     iotype
	ofile     iotype
	eoff      bool
	rle_flag  bool
	Err       int
	stat      chan [2]int
}

func (p *Packer) Initialize(ifile, ofile iotype) {
	p.icbuf, p.wpos, p.rpos, p.buf_size, p.flags, p.cpos, p.cntxs[0], p.Err = 0, 0, 0, 0, 8, 1, 0, 0
	p.low, p.hlp, p.rnge, p.vocroot, p.voclast = 0, 0, 0xffffffff, 0, 0xfffc
	for i := range 256 {
		for j := range 256 {
			p.frequency[i][j] = 1
		}
		p.fcs[i] = 256
	}
	for i := range 0x10000 {
		p.vocbuf[i], p.hashes[i], p.vocindx[i].skip, p.vocarea[i] = 0xff, 0, true, uint16(i)+1
	}
	p.vocindx[0].in, p.vocindx[0].out, p.vocindx[0].skip = 0, 0xfffc, false
	p.vocarea[0xfffc], p.vocarea[0xfffd], p.vocarea[0xfffe], p.vocarea[0xffff] = 0xfffc, 0xfffd, 0xfffe, 0xffff
	p._hlp, p._low = (*uint8)(unsafe.Pointer(&p.hlp)), (*uint8)(unsafe.Pointer(uintptr(unsafe.Pointer(&p.low))+3))
	p.eoff, p.rle_flag = false, false
	p.ifile, p.ofile = ifile, ofile
}

func (p *Packer) Wbuf(c uint8) {
	if p.wpos == 0x10000 {
		p.wpos = 0
		if _, err := p.ofile.Write(p.obuf[:]); err != nil {
			p.Err = 8
			return
		}
		if p.stat != nil {
			p.stat <- [2]int{0, 0x10000}
		}
	}
	p.obuf[p.wpos] = c
	p.wpos++
}

func (p *Packer) Rbuf() (c uint8) {
	if p.rpos == p.icbuf {
		p.rpos = 0
		r, err := p.ifile.Read(p.ibuf[:])
		p.icbuf = uint32(r)
		if p.stat != nil {
			p.stat <- [2]int{r, 0}
		}
		if err != nil || r == 0 {
			return
		}
	}
	c = p.ibuf[p.rpos]
	p.rpos++
	return
}

func (p *Packer) rc32_rescale(f *[256]uint16, fc *uint16, c uint8) {
	p.rnge *= uint32((*f)[c])
	(*f)[c]++
	if *fc++; *fc == 0 {
		for i := range 256 {
			(*f)[i] = ((*f)[i] >> 1) | ((*f)[i] & 1)
			*fc += (*f)[i]
		}
	}
}

func (p *Packer) rc32_getc(c *uint8, cntx uint8) {
	fc, f, s := &p.fcs[cntx], &p.frequency[cntx], uint32(0)
	for p.hlp < p.low || p.low^(p.low+p.rnge) < 0x1000000 || p.rnge < uint32(*fc) {
		p.hlp <<= 8
		if *p._hlp = p.Rbuf(); p.rpos == 0 {
			p.Err = 9
			return
		}
		p.low <<= 8
		p.rnge <<= 8
		if p.rnge > ^p.low {
			p.rnge = ^p.low
		}
	}
	p.rnge /= uint32(*fc)
	i := uint32((p.hlp - p.low) / p.rnge)
	if i >= uint32(*fc) {
		p.Err = 10
		return
	}
	for j := range 256 {
		s += uint32((*f)[j])
		if s > i {
			s -= uint32((*f)[j])
			*c = uint8(j)
			break
		}
	}
	p.low += s * p.rnge
	p.rc32_rescale(f, fc, *c)
}

func (p *Packer) rc32_putc(c uint8, cntx uint8) {
	fc, f, s := &p.fcs[cntx], &p.frequency[cntx], uint32(0)
	for p.low^(p.low+p.rnge) < 0x1000000 || p.rnge < uint32(*fc) {
		if p.Wbuf(*p._low); p.Err != 0 {
			return
		}
		p.low <<= 8
		p.rnge <<= 8
		if p.rnge > ^p.low {
			p.rnge = ^p.low
		}
	}
	for i := range c {
		s += uint32((*f)[i])
	}
	p.rnge /= uint32(*fc)
	p.low += s * p.rnge
	p.rc32_rescale(f, fc, c)
}

func (p *Packer) PutC(b uint8) {
	var offset, symbol, rle, rle_shift, length uint16
putc_start:
	if p.buf_size != LZ_BUF_SIZE && !p.eoff {
		if p.vocarea[p.vocroot] == p.vocroot {
			p.vocindx[p.hashes[p.vocroot]].skip = true
		} else {
			p.vocindx[p.hashes[p.vocroot]].in = p.vocarea[p.vocroot]
		}
		p.vocarea[p.vocroot], p.vocbuf[p.vocroot] = p.vocroot, b
		var hs uint16 = p.hashes[p.voclast] ^ uint16(p.vocbuf[p.voclast]) ^ uint16(b)
		hs = (hs << 4) | (hs >> 12)
		p.voclast++
		p.vocroot++
		p.buf_size++
		p.hashes[p.voclast] = hs
		if p.vocindx[hs].skip {
			p.vocindx[hs].in = p.voclast
		} else {
			p.vocarea[p.vocindx[hs].out] = p.voclast
		}
		p.vocindx[hs].skip, p.vocindx[hs].out = false, p.voclast
		return
	}
	p.cbuffer[0] <<= 1
	length = LZ_MIN_MATCH
	if p.buf_size != 0 {
		symbol = p.vocroot - p.buf_size
		rle_shift, rle = symbol+LZ_BUF_SIZE, symbol+1
		for rle != p.vocroot && p.vocbuf[symbol] == p.vocbuf[rle] {
			rle++
		}
		if rle -= symbol; p.buf_size > LZ_MIN_MATCH && rle != p.buf_size {
			for cnode := p.vocindx[p.hashes[symbol]].in; cnode != symbol; {
				if p.vocbuf[uint16(symbol+length)] == p.vocbuf[uint16(cnode+length)] {
					i, k := symbol, cnode
					for ; i != p.vocroot && p.vocbuf[i] == p.vocbuf[k]; i++ {
						k++
					}
					if i -= symbol; i >= length {
						if k = cnode - rle_shift; k > 0xfefe {
							cnode = p.vocarea[cnode]
							continue
						}
						length, offset = i, k
						if i == p.buf_size {
							break
						}
					}
				}
				cnode = p.vocarea[cnode]
			}
		}
		if rle > length {
			length, offset = rle, uint16(p.vocbuf[symbol])
		} else {
			offset = ^offset
		}
		if rle = uint16(length - LZ_MIN_MATCH); rle != 0 {
			p.cbuffer[p.cpos], p.cntxs[p.cpos] = uint8(rle-1), 1
			p.cpos++
			p.cbuffer[p.cpos], p.cntxs[p.cpos] = uint8(offset), 2
			p.cpos++
			p.cbuffer[p.cpos], p.cntxs[p.cpos] = uint8(offset>>8), 3
			p.buf_size -= length
		} else {
			p.cbuffer[p.cpos], p.cntxs[p.cpos] = p.vocbuf[symbol], p.vocbuf[uint16(symbol-1)]
			p.cbuffer[0] |= 1
			p.buf_size--
		}
	} else {
		p.cntxs[p.cpos] = 1
		p.cpos++
		p.cbuffer[p.cpos], p.cntxs[p.cpos] = 0, 2
		p.cpos++
		p.cbuffer[p.cpos], p.cntxs[p.cpos] = 1, 3
		length = 0
	}
	p.cpos++
	p.flags--
	if p.flags != 0 && length != 0 {
		goto putc_start
	}
	p.cbuffer[0] <<= p.flags
	for i := range p.cpos {
		if p.rc32_putc(p.cbuffer[i], p.cntxs[i]); p.Err != 0 {
			return
		}
	}
	p.flags, p.cpos = 8, 1
	if length == 0 {
		for j := 3; j >= 0; j-- {
			if p.Wbuf(uint8(p.low >> (8 * j))); p.Err != 0 {
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
	} else {
		goto putc_start
	}
}

func (p *Packer) GetC(b *uint8) {
getc_start:
	if p.length != 0 {
		if !p.rle_flag {
			p.cbuffer[1] = p.vocbuf[p.offset]
			p.offset++
		}
		p.vocbuf[p.vocroot], *b = p.cbuffer[1], p.cbuffer[1]
		p.vocroot++
		p.length--
		return
	}
	if p.flags != 0 {
		p.length, p.rle_flag = 1, true
		if p.cbuffer[0]&0x80 != 0 {
			if p.rc32_getc(&p.cbuffer[1], p.vocbuf[uint16(uint16((p.vocroot)-1))]); p.Err != 0 {
				return
			}
		} else {
			for c := uint8(1); c < 4; c++ {
				if p.rc32_getc(&p.cbuffer[c], uint8(c)); p.Err != 0 {
					return
				}
			}
			p.length, p.offset = LZ_MIN_MATCH+1+uint16(p.cbuffer[1]), uint16(p.cbuffer[2])
			p.offset |= uint16(p.cbuffer[3]) << 8
			switch {
			case p.offset > 0x0100:
				p.offset, p.rle_flag = ^p.offset+p.vocroot+LZ_BUF_SIZE, false
			case p.offset == 0x0100:
				p.eoff = true
				return
			default:
				p.cbuffer[1] = uint8(p.offset)
			}
		}
		p.cbuffer[0] <<= 1
		p.flags--
		goto getc_start
	}
	if p.rc32_getc(&p.cbuffer[0], 0); p.Err != 0 {
		return
	}
	p.flags = 8
	goto getc_start
}

func (p *Packer) Init_Unpack() {
	p.length, p.flags = 0, 0
	for range 4 {
		p.hlp <<= 8
		if *p._hlp = p.Rbuf(); p.rpos == 0 {
			p.Err = 9
			break
		}
	}
}

func (p *Packer) Unpack() {
	if p.Init_Unpack(); p.Err != 0 {
		return
	}
	var c uint8
	for {
		if p.GetC(&c); p.Err != 0 {
			break
		}
		if p.eoff {
			if _, err := p.ofile.Write(p.obuf[:p.wpos]); err != nil {
				p.Err = 8
			}
			if p.stat != nil {
				p.stat <- [2]int{0, int(p.wpos)}
			}
			break
		}
		if p.Wbuf(c); p.Err != 0 {
			break
		}
	}
}

func (p *Packer) Pack() {
pack_loop:
	for {
		if b := p.Rbuf(); p.rpos == 0 {
			p.eoff = true
			for p.buf_size != 0 {
				if p.PutC(0); p.Err != 0 {
					break pack_loop
				}
			}
			break
		} else if p.PutC(b); p.Err != 0 {
			break
		}
	}
}

func main() {
	var (
		exitCode int = 0
		errMsg   string
		rsize    int64 = 0
		wsize    int64 = 0
		wg       sync.WaitGroup
	)
	n := filepath.Base(os.Args[0])
	help := func() {
		fmt.Print("qbp file compressor\nUsage:\n",
			"\tto   compress use: ", n, " c[s] <input_file> <output_file>\n",
			"\tto decompress use: ", n, " d[s] <input_file> <output_file>\n",
			"\twhere 's' - silent mode\n")
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
		case 10:
			errMsg = "Error: the input file is corrupt or of an unknown/unsupported type!"
		}
		if exitCode != 0 {
			fmt.Println(errMsg)
			if exitCode == 1 {
				help()
			}
		}
		os.Exit(exitCode)
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
	info, err := os.Stat(os.Args[2])
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
	if _, err := os.Stat(os.Args[3]); !errors.Is(err, os.ErrNotExist) {
		exitCode = 5
		return
	}
	var pack Packer
	switch os.Args[1][0] {
	case 'c', 'd':
		ifile, err := os.OpenFile(os.Args[2], os.O_RDONLY, 0644)
		if err != nil {
			exitCode = 6
			return
		}
		defer ifile.Close()
		ofile, err := os.OpenFile(os.Args[3], os.O_CREATE|os.O_WRONLY, 0644)
		if err != nil {
			exitCode = 7
			return
		}
		defer ofile.Close()
		pack.Initialize(ifile, ofile)
		progress := func() {
			defer wg.Done()
			start := time.Now()
			for v := range pack.stat {
				rsize += int64(v[0])
				wsize += int64(v[1])
				fmt.Printf("\rProcessing [%#07.02fs] : %#-12d->%#12d", time.Since(start).Seconds(), rsize, wsize)
			}
			fmt.Println()
		}
		if !(len(os.Args[1]) != 1 && os.Args[1][1] == 's') {
			pack.stat = make(chan [2]int)
			wg.Add(1)
			go progress()
		}
		if os.Args[1][0] == 'c' {
			pack.Pack()
		} else {
			pack.Unpack()
		}
		if pack.stat != nil {
			close(pack.stat)
			wg.Wait()
		}
		if pack.Err != 0 {
			exitCode = pack.Err
		}
	default:
		exitCode = 1
		return
	}
}
