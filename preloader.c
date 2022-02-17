/* preloader.c (2016-10-21) */
/* Copyright (C) 2016-2017 RIKEN R-CCS */

/* Postloader for Static Executables.  It execs a given executable
   after setting a signal handler on SIGILL.  It is not necessary, if
   LD_PRELOAD can be used.  It uses the packages "elfutils-libelf" and
   "elfutils-libelf-devel". */

/* NOTE: Uses of standard libraries are avoided in the emulator.  It
   is because it loads a statically-linked executable in the memory
   space where a dynamically-linked executable is running.  Thus it
   has two copies of the libraries, and simultaneous use of them may
   introduce inconsistency.  */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>

#include <elf.h>
#include <nlist.h>
//#include <gelf.h>
#include <libelf.h>

#include "yasve.h"

#define FLOOR_TO_ALIGN(P, L) ((P) & ~((L) - 1));
#define CEILING_TO_ALIGN(P, L) (((P) + (L) - 1) & ~((L) - 1));

/* Maps a specified range of a file.  It is almost like mmap() but it
   zero-fills the extra space. */

static void
yasve_map_segment(Elf64_Addr addr, size_t filesz, size_t memsz,
		  int prot, int flags, off_t off, int fd,
		  Elf64_Addr pagesize)
{
    char *addrpg = (void *)FLOOR_TO_ALIGN(addr, pagesize);
    off_t shift = ((char *)addr - addrpg);
    assert(shift >= 0);
    size_t filesz1 = (filesz + (size_t)shift);
    off_t offset1 = (off - shift);
    assert(off >= shift);

    /*AHO*/
    /*prot |= PROT_WRITE;*/

    if (filesz != 0) {
	void *m = mmap(addrpg, filesz1, prot, flags, fd, offset1);
	if (m == MAP_FAILED) {
	    fprintf(stderr, "mmap(%p, 0x%zx, 0x%zx): %s.\n",
		    addrpg, filesz1, offset1, strerror(errno));
	    abort();
	}
    }

    char *zero = (void *)(addr + filesz);
    char *zeropg = (void *)CEILING_TO_ALIGN((Elf64_Addr)zero, pagesize);
    char *zeroend = (void *)(addr + memsz);

    if (filesz != 0 && (zeropg - zero) > 0 && (prot & PROT_WRITE) != 0) {
	memset(zero, 0, (size_t)(zeropg - zero));
    }

    if (zeroend > zeropg) {
	size_t zerosz = (size_t)(zeroend - zeropg);
	void *m = mmap(zeropg, zerosz, prot,
		       (MAP_ANON|MAP_PRIVATE|MAP_FIXED), -1, (off_t)0);
	if (m == MAP_FAILED) {
	    fprintf(stderr, "mmap(%p, 0x%zx, 0x%zx): %s.\n",
		    zeropg, zerosz, (off_t)0, strerror(errno));
	    abort();
	}
    }
}

/* Loads a statically-linked executable after setting a signal handler
   on SIGILL (a handler function is yasve_trap()).  Currently, argv[]
   are not passed.  It runs a start-up procedure of a SYSV binary; See
   "glibc-2.23/sysdeps/aarch64/start.S". */

void
yasve_main(int argc, char *argv[])
{
#define YASVE_MAX_PHNUM (64)
    Elf64_Phdr phdrsave[YASVE_MAX_PHNUM];

    /* Call gettid() via syscall() once to make the dynamic-linker
       happy (not sure its necessity). */

    if (1) {
	(void)syscall(178);
    }

    /* (Installing a signal handler is done by ctor initializer). */

    if (0) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = (SA_SIGINFO|SA_ONSTACK);
	sa.sa_sigaction = yasve_trap;
	int cc = sigaction(SIGILL, &sa, 0);
	if (cc == -1) {
	    fprintf(stderr, "sigaction(SIGILL): %s.\n", strerror(errno));
	    abort();
	}
    }

    /* Loads an executable. */

    char *file = argv[1];

    {
	/*int pagesize0 = getpagesize();*/
	errno = 0;
	long pgsz = sysconf(_SC_PAGESIZE);
	assert(pgsz != -1 && errno == 0);
	Elf64_Addr pagesize = (Elf64_Addr)pgsz;

	unsigned int ev = elf_version(EV_CURRENT);
	if (ev == EV_NONE) {
	    fprintf(stderr, "Warning: Used libelf.so is out of date.\n");
	    fflush(0);
	}

	int fd = open(file, O_RDONLY);
	if (fd == -1) {
	    char *s = strerror(errno);
	    fprintf(stderr, "open(%s): %s\n", file, s);
	    fflush(0);
	    abort();
	}

	Elf *elf = elf_begin(fd, ELF_C_READ, 0);
	if (elf == 0) {
	    const char *s = elf_errmsg(elf_errno());
	    fprintf(stderr, "elf_begin(): %s\n", s);
	    fflush(0);
	    abort();
	}

	Elf_Kind ek = elf_kind(elf);
	if (ek != ELF_K_ELF) {
	    fprintf(stderr, "File is not an elf binary (%s).\n", file);
	    fflush(0);
	    abort();
	}

	char *eid = elf_getident(elf, 0);
	if (eid == 0) {
	    const char *s = elf_errmsg(elf_errno());
	    fprintf(stderr, "elf_getident(): %s.\n", s);
	    fflush(0);
	    abort();
	}
	if (eid[EI_CLASS] != ELFCLASS64) {
	    fprintf(stderr, "File is not a 64bit elf binary (%s).\n", file);
	    fflush(0);
	    abort();
	}

	Elf64_Ehdr *ehdr = elf64_getehdr(elf);
	if (ehdr == 0) {
	    const char *s = elf_errmsg(elf_errno());
	    fprintf(stderr, "elf64_getehdr(): %s.\n", s);
	    fflush(0);
	    abort();
	}

	Elf64_Phdr *phdr0 = elf64_getphdr(elf);
	if (phdr0 == 0) {
	    const char *s = elf_errmsg(elf_errno());
	    fprintf(stderr, "elf_getphdr(): %s.\n", s);
	    fflush(0);
	    abort();
	}

	_Bool dynamic;
	dynamic = 0;
	for (int i = 0; i < ehdr->e_phnum; i++) {
	    switch (phdr0[i].p_type) {
	    case PT_NULL:
		/*ignore*/
		break;
	    case PT_LOAD:
		break;
	    case PT_DYNAMIC:
		dynamic = 1;
		break;
	    case PT_INTERP:
		dynamic = 1;
		break;
	    case PT_NOTE:
		/*ignore*/
		break;
	    case PT_SHLIB:
		/*ignore*/
		break;
	    case PT_PHDR:
		dynamic = 1;
		break;
	    case PT_TLS:
		break;
	    case PT_NUM:
		/*?*/
		break;
	    case PT_GNU_EH_FRAME:
		break;
	    case PT_GNU_STACK:
		/*ignore*/
		break;
	    case PT_GNU_RELRO:
		/*ignore*/
		break;
	    default:
		printf("Ignore phdr p_type[%d]=%x.\n", i, phdr0[i].p_type);
		break;
	    }
	}
	if (dynamic) {
	    fprintf(stderr, "File is not a static binary (%s).\n", file);
	    fflush(0);
	    abort();
	}

	assert(ehdr->e_phnum <= YASVE_MAX_PHNUM);
	void *entrypoint = (void *)ehdr->e_entry;
	int phnum = ehdr->e_phnum;
	Elf64_Phdr *phdr = phdrsave;
	for (int i = 0; i < ehdr->e_phnum; i++) {
	    Elf64_Phdr *ph = &phdr0[i];
	    memcpy(&phdr[i], ph, sizeof(Elf64_Phdr));
	}

	int rcnt = elf_end(elf);
	assert(rcnt == 0);

	/* The area of TLS is included in data. */

	for (int i = 0; i < phnum; i++) {
	    Elf64_Phdr *ph = &phdr[i];
	    switch (ph->p_type) {
	    case PT_LOAD:
		/*case PT_TLS:*/
	    case PT_GNU_EH_FRAME: {
		if (ph->p_memsz != 0) {
		    assert(ph->p_filesz <= ph->p_memsz);
		    assert(ph->p_paddr == ph->p_vaddr);
		    int prot;
		    prot = PROT_READ;
		    if ((ph->p_flags & PF_W) != 0) {
			prot |= PROT_WRITE;
		    }
		    if ((ph->p_flags & PF_X) != 0) {
			prot |= PROT_EXEC;
		    }
		    int flags = (MAP_FIXED|MAP_PRIVATE);
		    yasve_map_segment(ph->p_vaddr, ph->p_filesz, ph->p_memsz,
				      prot, flags, (off_t)ph->p_offset, fd,
				      pagesize);
		}
	    }
	    }
	}

	//sleep(3600);

	void *ep = entrypoint;
	void *sp = (argv - 1);
	__asm__ __volatile__("/* (Jump to the entry point) */");
	__asm__ __volatile__("mov sp, %0" : : "r" (sp));
	__asm__ __volatile__("mov x1, %0" : : "r" (ep));
	__asm__ __volatile__("mov x0, 0");
	__asm__ __volatile__("br x1");
	__asm__ __volatile__("hlt 0 /*?*/");

	fprintf(stderr, "BAD! Calling entry point returns.\n"); fflush(0);
	abort();
    }

    exit(1);
}
