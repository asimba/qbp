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
	flags     uint8
	cbuffer   [LZ_CAPACITY + 1]uint8
	cntxs     [LZ_CAPACITY + 2]uint8
	vocbuf    [0x10000]uint8
	vocarea   [0x10000]uint16
	hashes    [0x10000]uint16
	vocindx   [0x10000]vocpntr
	frequency [256][256]uint16
	fcs       [256]uint16
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
	ifile     *os.File
	ofile     *os.File
}

func (p *packer) initialize() {
	p.cntxs[0] = 0
	p.flags = 0
	p.low = 0
	p.hlp = 0
	p.wpos = 0
	p.rpos = 0
	p.icbuf = 0
	p.buf_size = 0
	p.vocroot = 0
	p.voclast = 0xfffc
	p.rnge = 0xffffffff
	for i := 0; i < 256; i++ {
		for j := 0; j < 256; j++ {
			p.frequency[i][j] = 1
		}
		p.fcs[i] = 256
	}
	for i := 0; i < 0x10000; i++ {
		p.vocbuf[i] = 0xff
		p.hashes[i] = 0
		p.vocindx[i].skip = true
		p.vocarea[i] = uint16(i) + 1
	}
	p.vocindx[0].in = 0
	p.vocindx[0].out = 0xfffc
	p.vocindx[0].skip = false
	p.vocarea[0xfffc] = 0xfffc
	p.vocarea[0xfffd] = 0xfffd
	p.vocarea[0xfffe] = 0xfffe
	p.vocarea[0xffff] = 0xffff
	p._low = (*uint8)(unsafe.Pointer(uintptr(unsafe.Pointer(&p.low)) + 3))
	p._hlp = (*uint8)(unsafe.Pointer(&p.hlp))
}

func (p *packer) wbuf(c uint8) bool {
	if p.wpos == 0x10000 {
		p.wpos = 0
		_, err := p.ofile.Write(p.obuf[:])
		if err != nil {
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
	*fc++
	if *fc == 0 {
		for i := 0; i < 256; i++ {
			(*f)[i] = ((*f)[i] >> 1) | ((*f)[i] & 1)
			*fc += (*f)[i]
		}
	}
}

func (p *packer) rc32_shift() {
	p.low <<= 8
	p.rnge <<= 8
	if p.rnge > ^p.low {
		p.rnge = ^p.low
	}
}

func (p *packer) rc32_getc(c *uint8, cntx uint8) bool {
	fc := &p.fcs[cntx]
	for p.hlp < p.low || p.low^(p.low+p.rnge) < 0x1000000 || p.rnge < uint32(*fc) {
		p.hlp <<= 8
		*p._hlp = p.rbuf()
		if p.rpos == 0 {
			return true
		}
		p.rc32_shift()
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
		p.rc32_shift()
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

func (p *packer) pack() bool {
	var (
		i         uint16
		rle       uint16
		rle_shift uint16 = 0
		cnode     uint16
		cpos      uint8  = 1
		eoff      bool   = false
		eofs      bool   = false
		cntx      uint8  = 1
		_offset   *uint8 = (*uint8)(unsafe.Pointer(uintptr(unsafe.Pointer(&p.offset)) + 1))
	)
	p.flags = 8
	for true {
		if !eoff {
			if LZ_BUF_SIZE-p.buf_size > 0 {
				p.vocbuf[p.vocroot] = p.rbuf()
				if p.rpos == 0 {
					eoff = true
					continue
				} else {
					if p.vocarea[p.vocroot] == p.vocroot {
						p.vocindx[p.hashes[p.vocroot]].skip = true
					} else {
						p.vocindx[p.hashes[p.vocroot]].in = p.vocarea[p.vocroot]
					}
					p.vocarea[p.vocroot] = p.vocroot
					var hs uint16 = p.hashes[p.voclast] ^ uint16(p.vocbuf[p.voclast]) ^ uint16(p.vocbuf[p.vocroot])
					hs = (hs << 4) | (hs >> 12)
					p.voclast++
					p.hashes[p.voclast] = hs
					indx := &p.vocindx[hs]
					if indx.skip {
						indx.in = p.voclast
					} else {
						p.vocarea[indx.out] = p.voclast
					}
					indx.skip = false
					indx.out = p.voclast
					p.vocroot++
					p.buf_size++
					continue
				}
			}
		}
		for j := 1; j < 4; j++ {
			p.cntxs[cntx] = uint8(j)
			cntx++
		}
		p.cbuffer[0] <<= 1
		var symbol uint16 = p.vocroot - p.buf_size
		rle = symbol
		if p.buf_size != 0 {
			rle++
			for rle != p.vocroot && p.vocbuf[symbol] == p.vocbuf[rle] {
				rle++
			}
			rle -= symbol
			p.length = LZ_MIN_MATCH
			if p.buf_size > LZ_MIN_MATCH && rle != p.buf_size {
				cnode = p.vocindx[p.hashes[symbol]].in
				rle_shift = p.vocroot + LZ_BUF_SIZE - p.buf_size
				for cnode != symbol {
					if p.vocbuf[uint16(symbol+p.length)] == p.vocbuf[uint16(cnode+p.length)] {
						i = symbol
						k := cnode
						for i != p.vocroot && p.vocbuf[i] == p.vocbuf[k] {
							k++
							i++
						}
						i -= symbol
						if i >= p.length {
							if p.buf_size < LZ_BUF_SIZE {
								if uint16(cnode-rle_shift) > 0xfefe {
									cnode = p.vocarea[cnode]
									continue
								}
							}
							p.offset = cnode
							p.length = i
							if i == p.buf_size {
								break
							}
						}
					}
					cnode = p.vocarea[cnode]
				}
			}
			if rle > p.length {
				p.length = rle
				p.offset = uint16(p.vocbuf[symbol])
			} else {
				p.offset = ^(p.offset - rle_shift)
			}
			i = p.length - LZ_MIN_MATCH
			if i != 0 {
				p.cbuffer[cpos] = uint8(i - 1)
				cpos++
				p.cbuffer[cpos] = uint8(p.offset)
				cpos++
				p.cbuffer[cpos] = uint8(*_offset)
				p.buf_size -= p.length
			} else {
				cntx -= 3
				p.cntxs[cntx] = p.vocbuf[uint16(symbol-1)]
				cntx++
				p.cbuffer[0] |= 1
				p.cbuffer[cpos] = p.vocbuf[symbol]
				p.buf_size--
			}
		} else {
			cpos++
			p.cbuffer[cpos] = 0
			cpos++
			p.cbuffer[cpos] = 1
			if eoff {
				eofs = true
			}
		}
		cpos++
		p.flags--
		if p.flags == 0 || eofs {
			p.cbuffer[0] <<= p.flags
			for i = 0; i < uint16(cpos); i++ {
				if p.rc32_putc(p.cbuffer[i], p.cntxs[i]) {
					return true
				}
			}
			cntx = 1
			p.flags = 8
			cpos = 1
			if eofs {
				for j := 3; j >= 0; j-- {
					if p.wbuf(uint8(p.low >> (8 * j))) {
						return true
					}
				}
				_, err := p.ofile.Write(p.obuf[:p.wpos])
				if err != nil {
					return true
				}
				break
			}
		}
	}
	return false
}

func (p *packer) unpack() bool {
	var (
		c, cflags, symbol uint8 = 0, 0, 0
		rle_flag          bool  = false
	)
	p.length = 0
	for ; c < 4; c++ {
		p.hlp <<= 8
		*p._hlp = p.rbuf()
		if p.rpos == 0 {
			return true
		}
	}
	for true {
		if p.length != 0 {
			if !rle_flag {
				symbol = p.vocbuf[p.offset]
				p.offset++
			}
			p.vocbuf[p.vocroot] = symbol
			p.vocroot++
			p.length--
			if p.vocroot == 0 {
				w, err := p.ofile.Write(p.vocbuf[:])
				if err != nil || w != 0x10000 {
					return true
				}
			}
			continue
		}
		if p.flags != 0 {
			p.length = 1
			rle_flag = true
			if cflags&0x80 != 0 {
				if p.rc32_getc(&symbol, p.vocbuf[uint16(uint16((p.vocroot)-1))]) {
					return true
				}
			} else {
				for c = 1; c < 4; c++ {
					if p.rc32_getc(&p.cbuffer[c-1], c) {
						return true
					}
				}
				p.length = LZ_MIN_MATCH + 1 + uint16(p.cbuffer[0])
				p.offset = uint16(p.cbuffer[1])
				p.offset |= uint16(p.cbuffer[2]) << 8
				if p.offset >= 0x0100 {
					if p.offset == 0x0100 {
						break
					}
					p.offset = ^p.offset + p.vocroot + LZ_BUF_SIZE
					rle_flag = false
				} else {
					symbol = uint8(p.offset)
				}
			}
			cflags <<= 1
			p.flags <<= 1
			continue
		}
		if p.rc32_getc(&cflags, 0) {
			return true
		}
		p.flags = 0xff
	}
	if p.length != 0 {
		var s uint32
		if p.vocroot != 0 {
			s = uint32(p.vocroot)
		} else {
			s = 0x10000
		}
		w, err := p.ofile.Write(p.vocbuf[:s])
		if err != nil || w < int(s) {
			return true
		}
	}
	return false
}

func main() {
	var exitCode int = 0
	n := filepath.Base(os.Args[0])
	defer func() { os.Exit(exitCode) }()
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
		fmt.Println("Error: input file doesn't exist!")
		exitCode = 2
		return
	}
	if info.IsDir() {
		fmt.Println("Error: input file is a directory!")
		exitCode = 3
		return
	}
	if !info.Mode().IsRegular() {
		fmt.Println("Error: input file is not a regular file!")
		exitCode = 4
		return
	}
	if _, err := os.Stat(os.Args[3]); !errors.Is(err, os.ErrNotExist) {
		fmt.Println("Error: output file is already exist!")
		exitCode = 5
		return
	}
	var pack packer
	switch os.Args[1][0] {
	case 'c', 'd':
		pack.initialize()
		pack.ifile, err = os.OpenFile(os.Args[2], os.O_RDONLY, 0644)
		if err != nil {
			fmt.Println("Error: unable to open input file!")
			exitCode = 6
			return
		}
		pack.ofile, err = os.OpenFile(os.Args[3], os.O_CREATE|os.O_WRONLY, 0644)
		if err != nil {
			fmt.Println("Error: unable to create output file!")
			pack.ifile.Close()
			exitCode = 7
			return
		}
		if os.Args[1][0] == 'c' {
			if pack.pack() {
				fmt.Println("An unexpected error occurred while packing the file!")
				exitCode = 8
			}
		} else {
			if pack.unpack() {
				fmt.Println("An unexpected error occurred while unpacking the file!")
				exitCode = 9
			}
		}
		pack.ifile.Close()
		pack.ofile.Close()
	default:
		fmt.Println("Error: wrong mode!")
		goto usage
	}
}
