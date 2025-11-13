/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
/***********************************************************************************************************/

package main

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
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

type packer struct {
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
}

func (p *packer) initialize(ifile, ofile iotype) {
	p.icbuf, p.wpos, p.rpos, p.buf_size, p.flags, p.cpos, p.cntxs[0] = 0, 0, 0, 0, 8, 1, 0
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

func (p *packer) wbuf(c uint8) bool {
	if p.wpos == 0x10000 {
		p.wpos = 0
		if _, err := p.ofile.Write(p.obuf[:]); err != nil {
			return true
		}
	}
	p.obuf[p.wpos] = c
	p.wpos++
	return false
}

func (p *packer) rbuf() uint8 {
	if p.rpos == p.icbuf {
		p.rpos = 0
		r, err := p.ifile.Read(p.ibuf[:])
		p.icbuf = uint32(r)
		if err != nil || r == 0 {
			return 0
		}
	}
	c := p.ibuf[p.rpos]
	p.rpos++
	return c
}

func (p *packer) rc32_rescale(f *[256]uint16, fc *uint16, c uint8) {
	p.rnge *= uint32((*f)[c])
	(*f)[c]++
	if *fc++; *fc == 0 {
		for i := range 256 {
			(*f)[i] = ((*f)[i] >> 1) | ((*f)[i] & 1)
			*fc += (*f)[i]
		}
	}
}

func (p *packer) rc32_getc(c *uint8, cntx uint8) bool {
	fc := &p.fcs[cntx]
	for p.hlp < p.low || p.low^(p.low+p.rnge) < 0x1000000 || p.rnge < uint32(*fc) {
		p.hlp <<= 8
		if *p._hlp = p.rbuf(); p.rpos == 0 {
			return true
		}
		p.low <<= 8
		p.rnge <<= 8
		if p.rnge > ^p.low {
			p.rnge = ^p.low
		}
	}
	p.rnge /= uint32(*fc)
	var i, j uint32 = (p.hlp - p.low) / p.rnge, 0
	if i >= uint32(*fc) {
		return true
	}
	f := &p.frequency[cntx]
	var s uint16 = 0
	for ; j < 256; j++ {
		s += (*f)[j]
		if s > uint16(i) {
			break
		}
	}
	p.low += uint32((s - (*f)[j])) * p.rnge
	*c = uint8(j)
	p.rc32_rescale(f, fc, uint8(j))
	return false
}

func (p *packer) rc32_putc(c uint8, cntx uint8) bool {
	fc := &p.fcs[cntx]
	for p.low^(p.low+p.rnge) < 0x1000000 || p.rnge < uint32(*fc) {
		p.hlp <<= 8
		if p.wbuf(*p._low) {
			return true
		}
		p.low <<= 8
		p.rnge <<= 8
		if p.rnge > ^p.low {
			p.rnge = ^p.low
		}
	}
	var s uint32 = 0
	f := &p.frequency[cntx]
	for _, v := range (*f)[:c] {
		s += uint32(v)
	}
	p.rnge /= uint32(*fc)
	p.low += s * p.rnge
	p.rc32_rescale(f, fc, c)
	return false
}

func (p *packer) putc(b uint8) bool {
	var offset, rle, length uint16
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
		return false
	}
	p.cbuffer[0] <<= 1
	length = LZ_MIN_MATCH
	if p.buf_size != 0 {
		symbol := uint16(p.vocroot - p.buf_size)
		rle_shift := (symbol + LZ_BUF_SIZE)
		rle = symbol + 1
		for rle != p.vocroot && p.vocbuf[symbol] == p.vocbuf[rle] {
			rle++
		}
		if rle -= symbol; p.buf_size > LZ_MIN_MATCH && rle != p.buf_size {
			for cnode := p.vocindx[p.hashes[symbol]].in; cnode != symbol; {
				if p.vocbuf[uint16(symbol+length)] == p.vocbuf[uint16(cnode+length)] {
					i := symbol
					k := cnode
					for i != p.vocroot && p.vocbuf[i] == p.vocbuf[k] {
						k++
						i++
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
	for i := 0; i < int(p.cpos); i++ {
		if p.rc32_putc(p.cbuffer[i], p.cntxs[i]) {
			return true
		}
	}
	p.flags, p.cpos = 8, 1
	if length == 0 {
		for j := 3; j >= 0; j-- {
			if p.wbuf(uint8(p.low >> (8 * j))) {
				return true
			}
		}
		if _, err := p.ofile.Write(p.obuf[:p.wpos]); err != nil {
			return true
		}
	} else {
		goto putc_start
	}
	return false
}

func (p *packer) getc(b *uint8) bool {
getc_start:
	if p.length != 0 {
		if !p.rle_flag {
			p.cbuffer[1] = p.vocbuf[p.offset]
			p.offset++
		}
		p.vocbuf[p.vocroot], *b = p.cbuffer[1], p.cbuffer[1]
		p.vocroot++
		p.length--
		return false
	}
	if p.flags != 0 {
		p.length, p.rle_flag = 1, true
		if p.cbuffer[0]&0x80 != 0 {
			if p.rc32_getc(&p.cbuffer[1], p.vocbuf[uint16(uint16((p.vocroot)-1))]) {
				return true
			}
		} else {
			for c := uint8(1); c < 4; c++ {
				if p.rc32_getc(&p.cbuffer[c], uint8(c)) {
					return true
				}
			}
			p.length, p.offset = LZ_MIN_MATCH+1+uint16(p.cbuffer[1]), uint16(p.cbuffer[2])
			p.offset |= uint16(p.cbuffer[3]) << 8
			switch {
			case p.offset > 0x0100:
				p.offset, p.rle_flag = ^p.offset+p.vocroot+LZ_BUF_SIZE, false
			case p.offset == 0x0100:
				p.eoff = true
				return false
			default:
				p.cbuffer[1] = uint8(p.offset)
			}
		}
		p.cbuffer[0] <<= 1
		p.flags--
		goto getc_start
	}
	if p.rc32_getc(&p.cbuffer[0], 0) {
		return true
	}
	p.flags = 8
	goto getc_start
}

func (p *packer) init_unpack() bool {
	p.length, p.flags = 0, 0
	for range 4 {
		p.hlp <<= 8
		*p._hlp = p.rbuf()
		if p.rpos == 0 {
			return true
		}
	}
	return false
}

func (p *packer) unpack() bool {
	if p.init_unpack() {
		return true
	}
	var c uint8
	for {
		if p.getc(&c) {
			return true
		}
		if p.eoff {
			break
		}
		if p.wbuf(c) {
			return true
		}
	}
	if _, err := p.ofile.Write(p.obuf[:p.wpos]); err != nil {
		return true
	}
	return false
}

func (p *packer) pack() bool {
	for {
		if !p.eoff {
			b := p.rbuf()
			if p.rpos == 0 {
				p.eoff = true
				break
			} else {
				if p.putc(b) {
					return true
				}
			}
		}
	}
	for p.buf_size != 0 {
		if p.putc(0) {
			return true
		}
	}
	return false
}

func main() {
	var (
		exitCode int = 0
		errMsg   string
	)
	n := filepath.Base(os.Args[0])
	defer func() {
		if exitCode != 0 {
			fmt.Println(errMsg)
		}
		os.Exit(exitCode)
	}()
	if len(os.Args) == 4 {
		goto normal
	}
usage:
	fmt.Print("qbp file compressor\n",
		"to   compress use: ", n, " c input output\n",
		"to decompress use: ", n, " d input output\n")
	exitCode = 1
	return
normal:
	info, err := os.Stat(os.Args[2])
	if errors.Is(err, os.ErrNotExist) {
		errMsg, exitCode = "Error: input file doesn't exist!", 2
		return
	}
	if info.IsDir() {
		errMsg, exitCode = "Error: input file is a directory!", 3
		return
	}
	if !info.Mode().IsRegular() {
		errMsg, exitCode = "Error: input file is not a regular file!", 4
		return
	}
	if _, err := os.Stat(os.Args[3]); !errors.Is(err, os.ErrNotExist) {
		errMsg, exitCode = "Error: output file is already exist!", 5
		return
	}
	var pack packer
	switch os.Args[1][0] {
	case 'c', 'd':
		ifile, err := os.OpenFile(os.Args[2], os.O_RDONLY, 0644)
		if err != nil {
			errMsg, exitCode = "Error: unable to open input file!", 6
			return
		}
		defer ifile.Close()
		ofile, err := os.OpenFile(os.Args[3], os.O_CREATE|os.O_WRONLY, 0644)
		if err != nil {
			errMsg, exitCode = "Error: unable to create output file!", 7
			return
		}
		defer ofile.Close()
		pack.initialize(ifile, ofile)
		if os.Args[1][0] == 'c' {
			if pack.pack() {
				errMsg, exitCode = "An unexpected error occurred while packing the file!", 8
			}
		} else {
			if pack.unpack() {
				errMsg, exitCode = "An unexpected error occurred while unpacking the file!", 9
			}
		}
	default:
		errMsg = "Error: wrong mode!"
		goto usage
	}
}
