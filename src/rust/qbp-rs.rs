/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
/***********************************************************************************************************/

use std::env::args;
use std::path::Path;
use std::fs::{File,metadata};
use std::{panic,usize};
use std::io::{Read, Write};

const LZ_BUF_SIZE: u16=259;
const LZ_CAPACITY: u8=24;
const LZ_MIN_MATCH: u16=3;

#[derive(Copy, Clone)]
struct IOpntr {
  i: u16,
  o: u16,
}

#[derive(Copy, Clone)]
union Vocpntr {
  p: IOpntr,
  v: u32,
}

pub struct Packer {
  ibuf: [u8; 0x10000],
  obuf: [u8; 0x10000],
  vocbuf: [u8; 0x10000],
  cbuffer: [u8; (LZ_CAPACITY+1) as usize],
  cntxs: [u8; (LZ_CAPACITY+1) as usize],
  vocarea: [u16; 0x10000],
  hashes: [u16; 0x10000],
  vocindx: [Vocpntr; 0x10000],
  frequency: Vec<Vec<u16>>,
  fcs: [u16; 256],
  icbuf: u32,
  wpos: u32,
  rpos: u32,
  low: u32,
  hlp: u32,
  range: u32,
  buf_size: u16,
  voclast: u16,
  vocroot: u16,
  offset: u16,
  length: u16,
  symbol: u16,
  hs: u16,
  flags: u8,
  pub ifile: File,
  pub ofile: File,
}

macro_rules! write_err {
  ($x:expr) => {
    {
      println!("Couldn't write output file: {}",$x);
      panic!()
    }
  }
}

macro_rules! read_err {
  () => {
    {
      println!("Couldn't read input file!");
      panic!();
    }
  }
}

impl Packer {
  pub fn new(i: &str,o: &str)->Packer {
    Packer{
      ibuf: [0 as u8; 0x10000],
      obuf: [0 as u8; 0x10000],
      vocbuf: [0 as u8; 0x10000],
      cbuffer: [0 as u8; (LZ_CAPACITY+1) as usize],
      cntxs: [0 as u8; (LZ_CAPACITY+1) as usize],
      vocarea: [0 as u16; 0x10000],
      hashes: [0 as u16; 0x10000],
      vocindx: [Vocpntr{v:1 as u32,}; 0x10000],
      frequency: vec![vec![0 as u16; 256]; 256],
      fcs: [0 as u16; 256],
      icbuf: 0,wpos: 0,rpos: 0,low: 0,hlp: 0,range: 0,
      buf_size: 0,voclast: 0,vocroot: 0,offset: 0,length: 0,symbol: 0,hs: 0,
      flags: 0,
      ifile:match File::open(&Path::new(i)){
        Err(why)=>{
          println!("Couldn't open input file: {}",why);
          panic!();
        },
        Ok(ifile)=>ifile,
      },
      ofile:match File::create(&Path::new(o)){
        Err(why)=>{
          println!("Couldn't open output file: {}",why);
          panic!();
        },
        Ok(ofile)=>ofile,
      },
    }
  }

  pub fn init(&mut self) {
    self.buf_size=0;
    self.flags=0;
    self.vocroot=0;
    self.cbuffer[0]=0;
    self.cntxs[0]=0;
    self.low=0;
    self.hlp=0;
    self.icbuf=0;
    self.wpos=0;
    self.rpos=0;
    self.voclast=0xfffd;
    self.range=0xffffffff;
    for i in 0..256{
      for j in 0..256{
        self.frequency[i][j]=1 as u16;
      }
      self.fcs[i]=256;
    }
    for i in 0..0x10000{
      self.vocbuf[i]=0xff;
      self.hashes[i]=0;
      self.vocarea[i]=((i as u16)+1) as u16;
    };
    self.vocindx[0].p.i=0;
    self.vocindx[0].p.o=0xfffc;
    self.vocarea[0xfffc]=0xfffc;
    self.vocarea[0xfffd]=0xfffd;
    self.vocarea[0xfffe]=0xfffe;
    self.vocarea[0xffff]=0xffff;
    self.hs=0x00ff;
  }

  #[inline(always)]
  fn rbuf(&mut self)->u8 {
    if self.rpos==self.icbuf {
      self.rpos=0;
      match self.ifile.read(&mut self.ibuf) {
        Ok(r) => self.icbuf=r as u32,
        Err(why)=>{
          println!("Couldn't read input file: {}",why);
          panic!();
        },
      }
    }
    let mut c: u8=0;
    if self.icbuf>0 {
      c=self.ibuf[self.rpos as usize];
      self.rpos+=1;
    }
    c
  }

  #[inline(always)]
  fn wbuf(&mut self,c: u8) {
    if self.wpos==0x10000{
      self.wpos=0;
      match self.ofile.write(&self.obuf) {
        Ok(_) => {},
        Err(why) => write_err!(why),
      }
    }
    self.obuf[self.wpos as usize]=c;
    self.wpos+=1;
  }

  #[inline(always)]
  fn rc32_rescale(&mut self,s: u32,cntx: u8) {
    self.low+=s*self.range;
    self.range*=self.frequency[cntx as usize][self.symbol as usize] as u32;
    self.frequency[cntx as usize][self.symbol as usize]+=1;
    self.fcs[cntx as usize]+=1;
    if self.fcs[cntx as usize]==0 {
      let mut fc: u16=0;
      for i in 0..256 {
        self.frequency[cntx as usize][i]=(self.frequency[cntx as usize][i]>>1)|(self.frequency[cntx as usize][i]&1);
        fc+=self.frequency[cntx as usize][i];
      }
      self.fcs[cntx as usize]=fc;
    }
  }

  fn rc32_getc(&mut self,cntx: u8) {
    while (self.low^(self.low+self.range))<0x1000000 || self.range<self.fcs[cntx as usize] as u32 || self.hlp<self.low {
      self.hlp=(self.hlp<<8)|(self.rbuf() as u32);
      if self.rpos==0 {
        read_err!()
      }
      self.low<<=8;
      self.range<<=8;
      if self.range>!self.low  {
        self.range=!self.low;
      }
    }
    self.range/=self.fcs[cntx as usize] as u32;
    let count: u32=(self.hlp-self.low)/self.range;
    if count>=self.fcs[cntx as usize] as u32 {
      read_err!()
    }
    let mut s: u32=0;
    for i in 0..256 {
      s+=self.frequency[cntx as usize][i as usize] as u32;
      if s>count {
        self.symbol=i as u16;
        break;
      };
    };
    s-=self.frequency[cntx as usize][self.symbol as usize] as u32;
    self.rc32_rescale(s,cntx);
  }
  
  fn rc32_putc(&mut self,c: u8,cntx: u8) {
    while (self.low^(self.low+self.range))<0x1000000 || self.range<self.fcs[cntx as usize] as u32 {
      self.wbuf((self.low>>24) as u8);
      self.low<<=8;
      self.range<<=8;
      if self.range>!self.low  {
        self.range=!self.low;
      }
    }
    self.symbol=c as u16;
    self.range/=self.fcs[cntx as usize] as u32;
    let mut s: u32=0;
    for i in 0..self.symbol {
      s+=self.frequency[cntx as usize][i as usize] as u32;
    }
    self.rc32_rescale(s,cntx);
  }

  pub fn pack(&mut self) {
    let mut i: u16;
    let mut rle: u16;
    let mut rle_shift: u16=0;
    let mut cnode: u16;
    let mut cpos: usize=1;
    let mut eoff: bool=false;
    let mut eofs: bool=false;
    self.flags=8;
    loop {
      if !eoff {
        if (LZ_BUF_SIZE-self.buf_size)>0 {
          self.vocbuf[self.vocroot as usize]=self.rbuf();
          if self.rpos==0 {
            eoff=true;
            continue;
          }
          else {
            if self.vocarea[self.vocroot as usize]==self.vocroot {
              self.vocindx[self.hashes[self.vocroot as usize] as usize].v=1;
            }
            else {
              self.vocindx[self.hashes[self.vocroot as usize] as usize].p.i=self.vocarea[self.vocroot as usize];
            }
            self.vocarea[self.vocroot as usize]=self.vocroot;
            self.hs^=self.vocbuf[self.vocroot as usize] as u16;
            self.hs=(self.hs<<4)|(self.hs>>12);
            self.hashes[self.voclast as usize]=self.hs;
            self.hs^=self.vocbuf[self.voclast as usize] as u16;
            let indx=self.hashes[self.voclast as usize] as usize;
            unsafe {
              if self.vocindx[indx].v==1 {
                self.vocindx[indx].p.i=self.voclast;
              }
              else {
                self.vocarea[self.vocindx[indx].p.o as usize]=self.voclast;
              }
              self.vocindx[indx].p.o=self.voclast;
            }
            self.voclast+=1;
            self.vocroot+=1;
            self.buf_size+=1;
            continue;
          }
        }
      }
      self.cbuffer[0]<<=1;
      self.symbol=self.vocroot-self.buf_size;
      rle=self.symbol;
      if self.buf_size>0 {
        rle+=1;
        while rle!=self.vocroot && self.vocbuf[self.symbol as usize]==self.vocbuf[rle as usize] {
          rle+=1;
        }
        rle-=self.symbol;
        self.length=LZ_MIN_MATCH;
        if self.buf_size>LZ_MIN_MATCH && rle<self.buf_size {
          unsafe { cnode=self.vocindx[self.hashes[self.symbol as usize] as usize].p.i };
          rle_shift=self.vocroot+LZ_BUF_SIZE-self.buf_size;
          while cnode!=self.symbol {
            if self.vocbuf[((self.symbol+self.length) as u16) as usize]==
              self.vocbuf[((cnode+self.length) as u16) as usize] {
              i=self.symbol;
              let mut k: u16=cnode;
              while i!=self.vocroot && self.vocbuf[i as usize]==self.vocbuf[k as usize] {
                k+=1;
                i+=1;
              }
              i-=self.symbol;
              if i>=self.length {
                if (cnode-rle_shift) as u16<=0xfefe {
                  self.offset=cnode;
                  self.length=i;
                  if i==self.buf_size {
                    break;
                  }
                }
              }
            }
            cnode=self.vocarea[cnode as usize];
          }
        }
        if rle>self.length {
          self.cbuffer[cpos]=(rle-LZ_MIN_MATCH-1) as u8;
          self.cntxs[cpos]=1;
          cpos+=1;
          self.cbuffer[cpos]=self.vocbuf[self.symbol as usize] as u8;
          self.cntxs[cpos]=2;
          cpos+=1;
          self.cbuffer[cpos]=0;
          self.cntxs[cpos]=3;
          self.buf_size-=rle;
        }
        else {
          if self.length>LZ_MIN_MATCH {
            self.cbuffer[cpos]=(self.length-LZ_MIN_MATCH-1) as u8;
            self.cntxs[cpos]=1;
            cpos+=1;
            self.offset=!(self.offset-rle_shift);
            self.cbuffer[cpos]=self.offset as u8;
            self.cntxs[cpos]=2;
            cpos+=1;
            self.cbuffer[cpos]=(self.offset>>8) as u8;
            self.cntxs[cpos]=3;
            self.buf_size-=self.length;
          }
          else {
            self.cntxs[cpos]=self.vocbuf[((self.symbol-1) as u16) as usize];
            self.cbuffer[cpos]=self.vocbuf[self.symbol as usize];
            self.cbuffer[0]|=1;
            self.buf_size-=1;
          }
        }
      }
      else {
        self.cntxs[cpos]=1;
        cpos+=1;
        self.cbuffer[cpos]=0;
        self.cntxs[cpos]=2;
        cpos+=1;
        self.cbuffer[cpos]=1;
        self.cntxs[cpos]=3;
        if eoff {
          eofs=true;
        }
      }
      cpos+=1;
      self.flags-=1;
      if self.flags==0 || eofs {
        self.cbuffer[0]<<=self.flags;
        for  i in 0..cpos as usize {
          self.rc32_putc(self.cbuffer[i],self.cntxs[i]);
        }
        self.flags=8;
        cpos=1;
        if eofs {
          i=4;
          while i>0 {
            self.wbuf((self.low>>24) as u8);
            self.low<<=8;
            i-=1;
          }
          match self.ofile.write(&self.obuf[..self.wpos as usize]) {
            Ok(_) => break,
            Err(why) =>  write_err!(why),
          }
        }
      }
    }
  }

  pub fn unpack(&mut self) {
    let mut cpos: usize;
    let mut c: u8=0;
    let mut cflags: u8=0;
    let mut rle_flag: bool=false;
    let mut flush: bool=false;
    for _i in 0..4 {
      self.hlp=(self.hlp<<8)|(self.rbuf() as u32);
      if self.rpos==0 {
        read_err!()
      }
    }
    loop {
      if self.length!=0 {
        if rle_flag==false {
          c=self.vocbuf[self.offset as usize];
          self.offset+=1;
        }
        self.vocbuf[self.vocroot as usize]=c;
        self.vocroot+=1;
        self.length-=1;
        flush=true;
        if self.vocroot==0 {
          match self.ofile.write(&self.vocbuf) {
            Ok(_) => { flush=false; },
            Err(why) => write_err!(why),
          }
        }
      }
      else {
        if self.flags==0 {
          self.rc32_getc(0);
          cflags=self.symbol as u8;
          self.flags=8;
        }
        else {
          self.length=1;
          rle_flag=true;
          if cflags&0x80!=0 {
            self.rc32_getc(self.vocbuf[((self.vocroot-1) as u16) as usize]);
            c=self.symbol as u8;
          }
          else {
            cpos=0;
            for _i in 1..4 {
              self.rc32_getc(_i);
              self.cbuffer[cpos]=self.symbol as u8;
              cpos+=1;
            }
            cpos=0;
            self.offset=0;
            self.length=self.cbuffer[cpos] as u16;
            cpos+=1;
            self.length+=LZ_MIN_MATCH+1;
            self.offset|=self.cbuffer[cpos] as u16;
            cpos+=1;
            self.offset|=(self.cbuffer[cpos] as u16)<<8;
            if self.offset<0x0100 {
              c=self.offset as u8;
            }
            else {
              if self.offset==0x0100 {
                break;
              }
              self.offset=!self.offset+(self.vocroot+LZ_BUF_SIZE) as u16;
              rle_flag=false;
            }
          }
          cflags<<=1;
          self.flags-=1;
        }
      }
    }
    if self.length!=0 && flush {
      if self.vocroot!=0 {
        match self.ofile.write(&self.vocbuf[..self.vocroot as usize]) {
          Ok(_) => return,
          Err(why) => write_err!(why),
        }
      }
      else {
        match self.ofile.write(&self.vocbuf) {
          Ok(_) => return,
          Err(why) => write_err!(why),
        }
      }
    }
  }
}

fn main(){
  panic::set_hook(Box::new(|_| { }));
  let argv: Vec<String>=args().collect();
  let argc=argv.len();
  let name=Path::new(&argv[0]);
  if argc<4 {
    println!("qbp file compressor");
    println!("to   compress use: {} c input output",name.file_name().unwrap().to_str().unwrap());
    println!("to decompress use: {} d input output",name.file_name().unwrap().to_str().unwrap());
    panic!();
  }
  if !Path::new(&argv[2]).exists() {
    println!("{} does not exists!",argv[2]);
    panic!();
  }
  if !metadata(&argv[2]).unwrap().is_file() {
    println!("{} not an ordinary file!",argv[2]);
    panic!();
  }
  if Path::new(&argv[3]).exists() {
    println!("{} already exists!",argv[3]);
    panic!();
  }
  let cmd=argv[1].chars().nth(0).unwrap();
  match cmd {
    'c' => {
      let mut p=Packer::new(&argv[2],&argv[3]);
      p.init();
      p.pack();
    },
    'd' => {
      let mut p=Packer::new(&argv[2],&argv[3]);
      p.init();
      p.unpack();
    },
    _ => {
      println!("Wrong argument: \'{}\'!",cmd);
      panic!();
    },
  }
}
