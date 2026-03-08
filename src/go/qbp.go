/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
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

	"github.com/asimba/qbp/tree/master/src/go/src/packer"
)

func main() {
	var (
		pack         packer.Commoncompress
		exitCode     int = 0
		wg           sync.WaitGroup
		ifile, ofile *os.File
		info         os.FileInfo
		stat         chan [2]int
		err          error = nil
	)
	n := filepath.Base(os.Args[0])
	help := func() {
		fmt.Print("\n",
			"--------------------------------------------------------------\n",
			"\t\t      qbp file compressor\n",
			"--------------------------------------------------------------\n",
			"Usage:\n",
			"--------------------------------------------------------------\n\t",
			n, " <c|d>[s] <input_file|-> <output_file|->\n",
			"\t------------------------------------------------------\n",
			"\texample:\n\t\t", n, " cs input_file_name output_file_name\n",
			"\t\t", n, " c input_file_name output_file_name\n",
			"\t\t", n, " dl input_file_name output_file_name\n",
			"\t------------------------------------------------------\n",
			"\t'c' - compress mode\n",
			"\t'd' - decompress mode\n",
			"\t'l' - LZSS+RLE (de)compression method (w/o RC32)\n",
			"\t's' - suppress progress output\n",
			"\t'-' - use stdin/stdout for input/output\n",
			"--------------------------------------------------------------\n")
	}
	defer func() {
		if err != nil {
			if v, ok := err.(packer.PackError); ok {
				if exitCode = int(v); exitCode != 0 {
					if exitCode == 1 {
						help()
					} else {
						log.Println(err.Error())
					}
				}
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
		err = packer.PackError(packer.ErrWrongMode)
		return
	}
	help()
	return
normal:
	if os.Args[2] != "-" {
		info, err = os.Stat(os.Args[2])
		if errors.Is(err, os.ErrNotExist) {
			err = packer.PackError(packer.ErrInputNotFound)
			return
		}
		if info.IsDir() {
			err = packer.PackError(packer.ErrIsDirectory)
			return
		}
		if !info.Mode().IsRegular() {
			err = packer.PackError(packer.ErrNotRegular)
			return
		}
	} else {
		ifile = os.Stdin
	}
	var (
		_unset  bool = true
		_mode   bool = true
		_method int  = packer.RC32
		_silent bool = true
	)
	for _, v := range os.Args[1] {
		switch v {
		case 'c':
			if _unset {
				_unset = false
			} else {
				err = packer.PackError(packer.ErrWrongMode)
				return
			}
		case 'd':
			if _unset {
				_unset = false
				_mode = false
			} else {
				err = packer.PackError(packer.ErrWrongMode)
				return
			}
		case 's':
			_silent = false
		case 'l':
			_method = packer.LZ16
		default:
			err = packer.PackError(packer.ErrWrongMode)
			return
		}
	}
	if _unset {
		err = packer.PackError(packer.ErrWrongMode)
		return
	}
	if ifile == nil {
		if ifile, err = os.OpenFile(os.Args[2], os.O_RDONLY, 0644); err != nil {
			err = packer.PackError(packer.ErrInputOpen)
			return
		}
		defer ifile.Close()
	}
	if os.Args[3] != "-" {
		if ofile, err = os.OpenFile(os.Args[3], os.O_CREATE|os.O_WRONLY|os.O_EXCL, 0644); err != nil {
			if os.IsExist(err) {
				err = packer.PackError(packer.ErrOutputExists)
			} else {
				err = packer.PackError(packer.ErrOutputCreate)
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
	if _silent {
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
	if _mode {
		if pack = packer.NewCompressor(ifile, ofile, stat, _method); pack.GetErr() != 0 {
			return
		}
	} else {
		if pack = packer.NewDecompressor(ifile, ofile, stat, _method); pack.GetErr() != 0 {
			return
		}
	}
	err = pack.Proceed()
}
