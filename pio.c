/*
 * Copyright 2002 by Albert Cahalan; all rights reserved.
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 * 
 * Vincent v.li@f5.com 2013-02-23
 * pio.c is taken from pmap.c to walk through process table to scan
 * /proc/#/io and pipe to pio.pl for I/O usage sorting
 * TODO: write pure C code for I/O usage sorting without pio.pl help 
 * ideally like top command
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include "proc/readproc.h"
#include "proc/version.h"
#include "proc/escape.h"

static void usage(void) NORETURN;
static void usage(void){
  fprintf(stderr,
    "Usage: pio /proc/<pid> or /proc/*\n"
  );
  exit(1);
}

static unsigned shm_minor = ~0u;

static void discover_shm_minor(void){
  void *addr;
  int shmid;
  char mapbuf[256];

  if(!freopen("/proc/self/maps", "r", stdin)) return;

  // create
  shmid = shmget(IPC_PRIVATE, 42, IPC_CREAT | 0666);
  if(shmid==-1) return; // failed; oh well
  // attach
  addr = shmat(shmid, NULL, SHM_RDONLY);
  if(addr==(void*)-1) goto out_destroy;

  while(fgets(mapbuf, sizeof mapbuf, stdin)){
    char flags[32];
    char *tmp; // to clean up unprintables
    unsigned KLONG start, end;
    unsigned long long file_offset, inode;
    unsigned dev_major, dev_minor;
    sscanf(mapbuf,"%"KLF"x-%"KLF"x %31s %Lx %x:%x %Lu", &start, &end, flags, &file_offset, &dev_major, &dev_minor, &inode);
    tmp = strchr(mapbuf,'\n');
    if(tmp) *tmp='\0';
    tmp = mapbuf;
    while(*tmp){
      if(!isprint(*tmp)) *tmp='?';
      tmp++;
    }
    if(start > (unsigned long)addr) continue;
    if(dev_major) continue;
    if(flags[3] != 's') continue;
    if(strstr(mapbuf,"/SYSV")){
      shm_minor = dev_minor;
      break;
    }
  }

  if(shmdt(addr)) perror("shmdt");

out_destroy:
  if(shmctl(shmid, IPC_RMID, NULL)) perror("IPC_RMID");

  return;
}


static int one_proc(proc_t *p){
  char buf[32];
  char mapbuf[9600];
  char cmdbuf[512];

  int maxcmd = 0xfffff;

  sprintf(buf,"/proc/%u/io",p->tgid);
  if(!freopen(buf, "r", stdin)) return 1;

  escape_command(cmdbuf, p, sizeof cmdbuf, &maxcmd, ESC_ARGS|ESC_BRACKETS);
  printf("%u:   %s\n", p->tgid, cmdbuf);

  while(fgets(mapbuf,sizeof mapbuf,stdin)){
    printf("%s", mapbuf);
    
  }

  return 0;
}


int main(int argc, char *argv[]){
  unsigned *pidlist;
  unsigned count = 0;
  PROCTAB* PT;
  proc_t p;
  int ret = 0;

  if(argc<2) usage();
  pidlist = malloc(sizeof(unsigned)*argc);  // a bit more than needed perhaps

  while(*++argv){
      char *walk = *argv;
      char *endp;
      unsigned long pid;
      if(!strncmp("/proc/",walk,6)){
        walk += 6;
        // user allowed to do: pio /proc/*
        if(*walk<'0' || *walk>'9') continue;
      }
      if(*walk<'0' || *walk>'9') usage();
      pid = strtoul(walk, &endp, 0);
      if(pid<1ul || pid>0x7ffffffful || *endp) usage();
      pidlist[count++] = pid;
  }

  if(count<1) usage();   // no processes

  discover_shm_minor();

  pidlist[count] = 0;  // old libproc interface is zero-terminated
  PT = openproc(PROC_FILLSTAT|PROC_FILLARG|PROC_PID, pidlist);
  while(readproc(PT, &p)){
    ret |= one_proc(&p);
    if(p.cmdline) free((void*)*p.cmdline);
    count--;
  }
  closeproc(PT);

  if(count) ret |= 42;  // didn't find all processes asked for
  return ret;
}
