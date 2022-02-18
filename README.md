# YASVE -- Yet Another ARM SVE Emulator (v2.1)

Copyright (C) 2020-2022 RIKEN R-CCS

__YASVE comes with ABSOLUTELY NO WARRANTY.__

YASVE is a trivial SIGILL trap handler for ARM SVE instructions.  It
is to run SVE code on AArch64 machines without SVE.  On trapping an
SVE instruction, a signal handler emulates it.  It is like ARM's
ARMIE.

## Usage

Running "make" creates a dynamic-linking library **libyasve.so**.
libyasve.so can be used with LD_PRELOAD.  It installs a SIGILL trap
handler at a start up in a constructor initializer.

```
$ LD_PRELOAD=libyasve.so ./a.out
```

YASVE includes a utility tool **runstatic**.  It can be used in
situations when a binary is statically linked and relinking with YASVE
is difficult.  runstatic places a "a.out" image in a large hole in the
address space of runstatic, and jumps to the entry point of the
"a.out" image.

Prerequisite packages (in Ubuntu):
* libelf-dev
* elfutils-libelf
* elfutils-libelf-devel

These are only necessary in building runstatic.

## Limitations

Programs depending on the cache line size may not work properly.  It
is uncontrollable.  Compilers may generate code depending on the cache
line size.  Note that the cache line size of A64FX is not 64 bytes
(unlike virtually all machines).

## Deficiencies

* No FP16 instructions are implemented.
* Many instructions are not implemented.
* Almost all of the emulation code are not tested.
* Only code generated by gcc-9.3 are tested.

## Source code

* [action.c](action.c): instuction definition
* [insn.c](insn.c): a part of the instruciton table from binutils
* [yasve.c](yasve.c): opcode dispatcher
* [preloader.c](preloader.c): trap handler setter for statically linked a.out

YASVE includes the instruction table "aarch64-tbl.h" taken from
binutils-2.37.  YASVE uses an extracted file "insn.c" which consists
only of the SVE-1 part of the table.  The source of binutils is
available in GNU git repository.

## Resources

"Arm Architecture Reference Manual for A-profile architecture" in

* https://developer.arm.com/architectures/cpu-architecture/a-profile/docs
* https://developer.arm.com/documentation/ddi0487/latest

## License

The license is the LGPL.  The codes of operations are direct
translations of the pseudocode in the architecture manual.  The
instruction table was taken from binutils, which is copyrighted by FSF
(opcodes/aarch64-tbl.h).  The spinlock code was taken from Linux,
which is copyrighted by ARM (arch/arm64/include/asm/spinlock.h).
