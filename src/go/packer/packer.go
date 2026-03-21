/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
/***********************************************************************************************************/

package packer

import (
	"fmt"
	"io"
)

const (
	RC32 = iota
	LZ16
	ErrWrongMode
	ErrInputNotFound
	ErrIsDirectory
	ErrNotRegular
	ErrOutputExists
	ErrInputOpen
	ErrOutputCreate
	ErrWrite
	ErrRead
	ErrCorrupt
	LZ_BUF_SIZE  uint16 = 259
	LZ_CAPACITY  uint8  = 25
	LZ_MIN_MATCH uint16 = 3
	LZ_EOF       uint16 = 0x100
	IO_BUF_SIZE  int    = 0x10000
	VOC_SIZE     int    = IO_BUF_SIZE
)

type PackError int
type unit_func func()

func (e PackError) Error() string {
	switch e {
	case ErrWrongMode:
		return "Error: wrong mode!"
	case ErrInputNotFound:
		return "Error: input file doesn't exist!"
	case ErrIsDirectory:
		return "Error: input file is a directory!"
	case ErrNotRegular:
		return "Error: input file is not a regular file!"
	case ErrOutputExists:
		return "Error: output file is already exist!"
	case ErrInputOpen:
		return "Error: unable to open input file!"
	case ErrOutputCreate:
		return "Error: unable to create output file!"
	case ErrWrite:
		return "Error writing output file!"
	case ErrRead:
		return "Error reading input file!"
	case ErrCorrupt:
		return "Error: the input file is corrupt or of an unknown/unsupported type!"
	default:
		return fmt.Sprintf("De/Compression error code %d", e)
	}
}

type iotype interface {
	Read([]byte) (int, error)
	Write([]byte) (int, error)
}

type vocpntr struct {
	in   uint16
	out  uint16
	skip bool
}

type commonwork struct {
	ibuf      []uint8
	obuf      []uint8
	vocbuf    [VOC_SIZE]uint8
	frequency [][257]uint16
	icbuf     int
	rpos      int
	wpos      int
	low       uint32
	rnge      uint32
	vocroot   uint16
	length    uint16
	offset    uint16
	flags     uint8
	ifile     iotype
	ofile     iotype
	eof       bool
	err       int
	stat      chan [2]int
}

type compressor struct {
	commonwork
	cbuffer [LZ_CAPACITY]uint8
	cntxs   [LZ_CAPACITY]uint8
	vocarea [VOC_SIZE]uint16
	hashes  [VOC_SIZE]uint16
	vocindx [VOC_SIZE]vocpntr
	voclast uint16
	cpos    uint8
	done    bool
	queue   unit_func
	encode  unit_func
}

type decompressor struct {
	commonwork
	hlp      uint32
	cbuffer  [4]uint8
	cflags   uint8
	rle_flag bool
	getflags unit_func
	getbyte  unit_func
	getpair  unit_func
}

func fill[T any](slice []T, val T) {
	for i := range slice {
		slice[i] = val
	}
}

func (p *commonwork) progress(i, o int) {
	if p.stat != nil {
		p.stat <- [2]int{i, o}
	}
}

func (p *commonwork) Wbuf(c uint8) {
	if p.obuf == nil {
		p.err = ErrWrite
		return
	}
	if p.wpos == IO_BUF_SIZE {
		p.wpos = 0
		if _, err := p.ofile.Write(p.obuf); err != nil {
			p.err = ErrWrite
			return
		}
		p.progress(0, IO_BUF_SIZE)
	}
	p.obuf[p.wpos] = c
	p.wpos++
}

func (p *commonwork) Rbuf() (c uint8) {
	if p.rpos == p.icbuf {
		p.rpos = 0
		var err error
		if p.icbuf, err = p.ifile.Read(p.ibuf); p.icbuf == 0 {
			if err != nil && err != io.EOF {
				p.err = ErrRead
			}
			return
		}
		p.progress(p.icbuf, 0)
	}
	c = p.ibuf[p.rpos]
	p.rpos++
	return
}

func (p *commonwork) init(ifile, ofile iotype, stat chan [2]int) {
	p.ibuf = make([]uint8, IO_BUF_SIZE)
	p.ifile, p.ofile, p.stat = ifile, ofile, stat
}

func (p *commonwork) init_frequency() {
	p.rnge = 0xffffffff
	p.frequency = make([][257]uint16, 256)
	for i := range p.frequency {
		fill(p.frequency[i][:], 1)
		p.frequency[i][256] = 256
	}
}

func (p *compressor) initialize(ifile, ofile iotype, stat chan [2]int, mode int) {
	p.init(ifile, ofile, stat)
	p.obuf = make([]uint8, IO_BUF_SIZE)
	for i := range VOC_SIZE {
		p.vocbuf[i], p.vocindx[i].skip, p.vocarea[i] = 0xff, true, uint16(i)+1
	}
	p.vocindx[0].out, p.vocindx[0].skip = 0xfffc, false
	p.flags, p.cpos, p.voclast = 8, 1, 0xfffc
	p.vocarea[0xfffc], p.vocarea[0xfffd], p.vocarea[0xfffe], p.vocarea[0xffff] = 0xfffc, 0xfffd, 0xfffe, 0xffff
	if mode == RC32 {
		p.init_frequency()
		p.queue, p.encode = p.rc32_queue, p.rc32
	} else {
		p.queue, p.encode = p.lz16_queue, func() {
			for i := range p.cpos {
				if p.Wbuf(p.cbuffer[i]); p.err != 0 {
					break
				}
			}
		}
	}
}

func (p *decompressor) initialize(ifile, ofile iotype, stat chan [2]int, mode int) {
	p.init(ifile, ofile, stat)
	p.rnge = 0xffffffff
	fill(p.vocbuf[:], 0xff)
	if mode == RC32 {
		p.init_frequency()
		for range 4 {
			p.hlp <<= 8
			if p.hlp |= uint32(p.Rbuf()); p.rpos == 0 {
				p.err = ErrRead
				return
			}
		}
		p.getflags = func() {
			p.rc32(&p.cflags, 0)
		}
		p.getbyte = func() {
			p.rc32(&p.cbuffer[0], p.vocbuf[uint16((p.vocroot)-1)])
		}
		p.getpair = func() {
			for c := 1; c < 4; c++ {
				p.rc32(&p.cbuffer[c], uint8(c))
			}
		}
	} else {
		p.getflags = func() {
			if p.cflags = p.Rbuf(); p.rpos == 0 {
				p.err = ErrRead
			}
		}
		p.getbyte = func() {
			if p.cbuffer[0] = p.Rbuf(); p.rpos == 0 {
				p.err = ErrRead
			}
		}
		p.getpair = func() {
			for c := 1; c < 4; c++ {
				if p.cbuffer[c] = p.Rbuf(); p.rpos == 0 {
					p.err = ErrRead
					break
				}
			}
		}
	}
}

func (p *commonwork) frequency_rescale(f *[257]uint16, c uint8, s uint32) {
	p.low += s * p.rnge
	p.rnge *= uint32((*f)[c])
	(*f)[c]++
	if (*f)[256]++; (*f)[256] == 0 {
		for i := range 256 {
			(*f)[i] = ((*f)[i] >> 1) | ((*f)[i] & 1)
			(*f)[256] += (*f)[i]
		}
	}
}

func (p *commonwork) range_shift() {
	p.low <<= 8
	p.rnge <<= 8
	if p.rnge > ^p.low {
		p.rnge = ^p.low
	}
}

func (p *compressor) rc32() {
	for v := range p.cpos {
		f := &p.frequency[p.cntxs[v]]
		for p.low^(p.low+p.rnge) < 0x1000000 || p.rnge < uint32((*f)[256]) {
			if p.Wbuf(uint8(p.low >> 24)); p.err != 0 {
				return
			}
			p.range_shift()
		}
		var s uint16
		for i := range uint32(p.cbuffer[v]) {
			s += (*f)[i]
		}
		p.rnge /= uint32((*f)[256])
		p.frequency_rescale(f, p.cbuffer[v], uint32(s))
	}
}

func (p *decompressor) rc32(c *uint8, cntx uint8) {
	f := &p.frequency[cntx]
	for p.hlp < p.low || p.low^(p.low+p.rnge) < 0x1000000 || p.rnge < uint32((*f)[256]) {
		p.hlp <<= 8
		if p.hlp |= uint32(p.Rbuf()); p.rpos == 0 {
			p.err = ErrRead
			return
		}
		p.range_shift()
	}
	p.rnge /= uint32((*f)[256])
	if i := uint16((p.hlp - p.low) / p.rnge); i < (*f)[256] {
		var j uint8
		s := (*f)[j]
		for {
			if s > i {
				*c = j
				p.frequency_rescale(f, *c, uint32(s-(*f)[j]))
				break
			}
			j++
			s += (*f)[j]
		}
	} else {
		p.err = ErrCorrupt
	}
}

func (p *commonwork) finalize() error {
	if _, err := p.ofile.Write(p.obuf[:p.wpos]); err != nil {
		p.err = ErrWrite
		return PackError(p.err)
	}
	p.progress(0, int(p.wpos))
	return nil
}

func (p *compressor) append(b uint8) {
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
}

func (p *compressor) search() (symbol, rle, length, offset uint16) {
	var rle_shift, i, k uint16
	length = LZ_MIN_MATCH
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
	rle = uint16(length - LZ_MIN_MATCH)
	return
}

func (p *compressor) rc32_queue() {
	if p.cbuffer[0] <<= 1; p.length != 0 {
		if symbol, rle, length, offset := p.search(); rle != 0 {
			p.cbuffer[p.cpos], p.cntxs[p.cpos] = uint8(rle-1), 1
			p.cpos++
			p.cbuffer[p.cpos], p.cntxs[p.cpos] = uint8(offset), 2
			p.cpos++
			p.cbuffer[p.cpos], p.cntxs[p.cpos] = uint8(offset>>8), 3
			p.length -= length
		} else {
			p.cbuffer[p.cpos], p.cntxs[p.cpos] = p.vocbuf[symbol], p.vocbuf[uint16(symbol-1)]
			p.cbuffer[0] |= 1
			p.length--
		}
	} else {
		p.cntxs[p.cpos] = 1
		p.cpos++
		p.cbuffer[p.cpos], p.cntxs[p.cpos] = 0, 2
		p.cpos++
		p.cbuffer[p.cpos], p.cntxs[p.cpos] = 1, 3
		p.done = true
	}
	p.cpos++
	p.flags--
}

func (p *compressor) lz16_queue() {
	if p.cbuffer[0] <<= 1; p.length != 0 {
		if symbol, rle, length, offset := p.search(); rle != 0 {
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
		p.done = true
	}
	p.cpos++
	p.flags--
}

func (p *compressor) PutC(b uint8) {
	for {
		if p.length != LZ_BUF_SIZE && !p.eof {
			p.append(b)
			break
		}
		if p.queue(); p.flags != 0 && !p.done {
			continue
		}
		p.cbuffer[0] <<= p.flags
		p.encode()
		if p.flags, p.cpos = 8, 1; p.err != 0 || p.done {
			break
		}
	}
}

func (p *decompressor) GetC() uint8 {
getc_loop:
	for {
		if p.length != 0 {
			if p.rle_flag {
				p.vocbuf[p.vocroot] = uint8(p.offset)
			} else {
				p.vocbuf[p.vocroot] = p.vocbuf[p.offset]
				p.offset++
			}
			p.vocroot++
			p.length--
			break
		}
		if p.flags != 0 {
			p.length, p.rle_flag = 1, true
			if p.cflags>>7 != 0 {
				if p.getbyte(); p.err != 0 {
					break
				}
				p.offset = uint16(p.cbuffer[0])
			} else {
				if p.getpair(); p.err != 0 {
					break
				}
				p.length = LZ_MIN_MATCH + 1 + uint16(p.cbuffer[1])
				p.offset = uint16(p.cbuffer[2]) | uint16(p.cbuffer[3])<<8
				switch {
				case p.offset > LZ_EOF:
					p.offset, p.rle_flag = ^p.offset+p.vocroot+LZ_BUF_SIZE, false
				case p.offset == LZ_EOF:
					p.eof = true
					break getc_loop
				}
			}
			p.cflags <<= 1
			p.flags--
			continue
		}
		p.flags = 8
		if p.getflags(); p.err != 0 {
			break
		}
	}
	return uint8(p.offset)
}

func (p *decompressor) Proceed() error {
	var (
		c     int  = VOC_SIZE
		flush bool = false
	)
	for {
		if p.GetC(); p.err != 0 {
			break
		} else {
			if !p.eof {
				flush = true
			}
		}
		if p.vocroot == 0 || p.eof {
			if p.vocroot != 0 {
				c = int(p.vocroot)
			}
			if flush {
				if _, err := p.ofile.Write(p.vocbuf[:c]); err != nil {
					p.err = ErrWrite
					break
				}
				flush = false
				p.progress(0, c)
			}
			if p.eof {
				break
			}
		}
	}
	return PackError(p.err)
}

func (p *compressor) Proceed() error {
	for {
		if b := p.Rbuf(); p.rpos == 0 {
			return p.Finalize()
		} else if p.PutC(b); p.err != 0 {
			return PackError(p.err)
		}
	}
}

func (p *compressor) Finalize() error {
	p.eof = true
	for p.length != 0 {
		if p.PutC(0); p.err != 0 {
			return PackError(p.err)
		}
	}
	if p.rnge != 0 {
		for j := 3; j >= 0; j-- {
			if p.Wbuf(uint8(p.low >> (8 * j))); p.err != 0 {
				return PackError(p.err)
			}
		}
	}
	return p.finalize()
}

func (p *decompressor) Finalize() error {
	return p.finalize()
}

func (p *commonwork) GetErr() int {
	return p.err
}

type Commoncompress interface {
	Finalize() error
	Wbuf(uint8)
	Rbuf() uint8
	Proceed() error
	GetErr() int
}

type Compressor interface {
	Commoncompress
	PutC(uint8)
}

type Decompressor interface {
	Commoncompress
	GetC() uint8
}

func NewCompressor(ifile, ofile iotype, stat chan [2]int, mode int) Compressor {
	pack := &compressor{}
	if mode != RC32 && mode != LZ16 {
		pack.err = ErrWrongMode
		return nil
	}
	pack.initialize(ifile, ofile, stat, mode)
	return pack
}

func NewDecompressor(ifile, ofile iotype, stat chan [2]int, mode int) Decompressor {
	pack := &decompressor{}
	if mode != RC32 && mode != LZ16 {
		pack.err = ErrWrongMode
		return nil
	}
	pack.initialize(ifile, ofile, stat, mode)
	return pack
}
