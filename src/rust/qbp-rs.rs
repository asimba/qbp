/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
/***********************************************************************************************************/

use std::env::args;
use std::path::Path;
use std::fs::{File,metadata};
use std::{panic,ptr,usize};
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

union U16U8 {
  u: u16,
  c: [u8; 2],
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
  lowp: *mut u8,
  hlpp: *mut u8,
  flags: u8,
  pub ifile: File,
  pub ofile: File,
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
      lowp: ptr::null_mut(),
      hlpp: ptr::null_mut(),
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
    self.lowp=ptr::addr_of_mut!(self.low) as *mut u8;
    unsafe { self.lowp=self.lowp.add(3) };
    self.hlpp=ptr::addr_of_mut!(self.hlp) as *mut u8;
  }

  #[inline(always)]
  fn rbuf(&mut self,c: *mut u8)->bool {
    if self.rpos==self.icbuf {
      self.rpos=0;
      match self.ifile.read(&mut self.ibuf) {
        Ok(r) => self.icbuf=r as u32,
        Err(_) => return true,
      }
    }
    if self.icbuf>0 {
      unsafe { *c=self.ibuf[self.rpos as usize] };
      self.rpos+=1;
    }
    return false;
  }

  #[inline(always)]
  fn wbuf(&mut self,c: u8)->bool {
    if self.wpos==0x10000{
      self.wpos=0;
      match self.ofile.write(&self.obuf) {
        Ok(_) => {},
        Err(_) => return true,
      }
    }
    self.obuf[self.wpos as usize]=c;
    self.wpos+=1;
    return false;
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

  fn rc32_getc(&mut self,c: *mut u8,cntx: u8)->bool {
    while (self.low^(self.low+self.range))<0x1000000 || self.range<self.fcs[cntx as usize] as u32 || self.hlp<self.low {
      self.hlp<<=8;
      if self.rbuf(self.hlpp) {
        return true;
      }
      if self.rpos==0 {
        return false;
      };
      self.low<<=8;
      self.range<<=8;
      if self.range>!self.low  {
        self.range=!self.low;
      }
    }
    self.range/=self.fcs[cntx as usize] as u32;
    let count: u32=(self.hlp-self.low)/self.range;
    if count>=self.fcs[cntx as usize] as u32 {
      return true;
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
    unsafe { *c=self.symbol as u8};
    self.rc32_rescale(s,cntx);
    return false;
  }
  
  fn rc32_putc(&mut self,c: u8,cntx: u8)->bool {
    while (self.low^(self.low+self.range))<0x1000000 || self.range<self.fcs[cntx as usize] as u32 {
      if self.wbuf(unsafe { *self.lowp }) {
        return true;
      }
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
    return false;
  }

  pub fn pack(&mut self) {
    let mut i: u16;
    let mut rle: u16;
    let mut rle_shift: u16=0;
    let mut cnode: u16;
    let mut cpos: *mut u8=ptr::addr_of_mut!(self.cbuffer).cast();
    let w: *mut u8=cpos;
    unsafe { cpos=cpos.add(1) };
    let mut eoff: bool=false;
    let mut eofs: bool=false;
    let mut cntx: u8=1;
    self.flags=8;
    loop {
      if !eoff {
        if (LZ_BUF_SIZE-self.buf_size)>0 {
          if self.rbuf(ptr::addr_of!(self.vocbuf[self.vocroot as usize]) as *mut u8) {
            break;
          }
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
      let cnv: U16U8;
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
                if self.buf_size<LZ_BUF_SIZE {
                  if (cnode-rle_shift) as u16>0xfefe {
                    cnode=self.vocarea[cnode as usize];
                    continue;
                  }
                }
                self.offset=cnode;
                self.length=i;
                if i==self.buf_size {
                  break;
                }
              }
            }
            cnode=self.vocarea[cnode as usize];
          }
        }
        if rle>self.length {
          self.cntxs[cntx as usize]=1;
          cntx+=1;
          self.cntxs[cntx as usize]=2;
          cntx+=1;
          self.cntxs[cntx as usize]=3;
          cntx+=1;
          unsafe {
            *cpos=(rle-LZ_MIN_MATCH-1) as u8;
            cpos=cpos.add(1);
            cnv.u=self.vocbuf[self.symbol as usize] as u16;
            *cpos=cnv.c[0];
            cpos=cpos.add(1);
            *cpos=cnv.c[1];
            self.buf_size-=rle;
          }
        }
        else {
          if self.length>LZ_MIN_MATCH {
            self.cntxs[cntx as usize]=1;
            cntx+=1;
            self.cntxs[cntx as usize]=2;
            cntx+=1;
            self.cntxs[cntx as usize]=3;
            cntx+=1;
            unsafe {
              *cpos=(self.length-LZ_MIN_MATCH-1) as u8;
              cpos=cpos.add(1);
              cnv.u=!(self.offset-rle_shift);
              *cpos=cnv.c[0];
              cpos=cpos.add(1);
              *cpos=cnv.c[1];
              self.buf_size-=self.length;
            }
          }
          else {
            self.cntxs[cntx as usize]=self.vocbuf[((self.symbol-1) as u16) as usize];
            cntx+=1;
            self.cbuffer[0]|=0x01;
            unsafe { *cpos=self.vocbuf[self.symbol as usize] };
            self.buf_size-=1;
          }
        }
      }
      else {
        self.cntxs[cntx as usize]=1;
        cntx+=1;
        self.cntxs[cntx as usize]=2;
        cntx+=1;
        self.cntxs[cntx as usize]=3;
        cntx+=1;
        unsafe {
          cpos=cpos.add(1);
          cnv.u=0x0100;
          *cpos=cnv.c[0];
          cpos=cpos.add(1);
          *cpos=cnv.c[1];
          if eoff {
            eofs=true;
          }
        }
      }
      unsafe { cpos=cpos.add(1) };
      self.flags-=1;
      if self.flags==0 || eofs {
        self.cbuffer[0]<<=self.flags;
        for  i in 0..(cpos as u32 - w as u32) as u16 {
          if self.rc32_putc(self.cbuffer[i as usize],self.cntxs[i as usize]){
            return;
          }
        }
        cntx=1;
        self.flags=8;
        cpos=w;
        unsafe { cpos=cpos.add(1) };
        if eofs {
          i=4;
          while i>0 {
            if self.wbuf(unsafe { *self.lowp }) {
              return;
            }
            self.low<<=8;
            i-=1;
          }
          match self.ofile.write(&self.obuf[..self.wpos as usize]) {
            Ok(_) => break,
            Err(_) => return,
          }
        }
      }
    }
  }

  pub fn unpack(&mut self) {
    let mut cpos: *mut u8;
    let mut c: u8=0;
    let mut cflags: u8=0;
    let mut rle_flag: bool=false;
    for _i in 0..4 {
      self.hlp<<=8;
      if self.rbuf(self.hlpp) || self.rpos==0 {
        return;
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
        if self.vocroot==0 {
          match self.ofile.write(&self.vocbuf) {
            Ok(_) => {},
            Err(_) => return,
          }
        }
      }
      else {
        cpos=ptr::addr_of_mut!(self.cbuffer) as *mut u8;
        if self.flags==0 {
          if self.rc32_getc(cpos,0){
            return;
          }
          cflags=self.cbuffer[0];
          self.flags=8;
        }
        else {
          self.length=1;
          rle_flag=true;
          if cflags&0x80!=0 {
            if self.rc32_getc(cpos,self.vocbuf[((self.vocroot-1) as u16) as usize]) {
              return;
            }
            c=self.cbuffer[0];
          }
          else {
            for _i in 1..4 {
              if self.rc32_getc(cpos,_i) {
                return;
              }
              unsafe { cpos=cpos.add(1) };
            }
            cpos=ptr::addr_of_mut!(self.cbuffer) as *mut u8;
            let mut cnv: U16U8;
            cnv.u=0;
            unsafe {
              self.length=*cpos as u16;
              cpos=cpos.add(1);
              self.length+=LZ_MIN_MATCH+1;
              cnv.c[0]=*cpos as u8;
              cpos=cpos.add(1);
              cnv.c[1]=*cpos as u8;
              self.offset=cnv.u;
            }
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
    if self.length!=0 {
      if self.vocroot!=0 {
        match self.ofile.write(&self.vocbuf[..self.vocroot as usize]) {
          Ok(_) => return,
          Err(_) => return,
        }
      }
      else {
        match self.ofile.write(&self.vocbuf) {
          Ok(_) => return,
          Err(_) => return,
        }
      }
    }
  }

}

fn main(){
  panic::set_hook(Box::new(|_| { }));
  let argv: Vec<String>=args().collect();
  let argc=argv.len();
  if argc<4 {
    println!("qbp-rs file compressor");
    println!("to   compress use: qbp-rs c input output");
    println!("to decompress use: qbp-rs d input output");
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
  if cmd!='c' && cmd!='d' {
    println!("Wrong argument: \'{}\'!",cmd);
    panic!();
  }
  let mut p=Packer::new(&argv[2],&argv[3]);
  p.init();
  match cmd {
    'c' => p.pack(),
    'd' => p.unpack(),
    _ => (),
  }
}
