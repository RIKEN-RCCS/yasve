# YASVE -- Yet Another ARM SVE Emulator (v2.1)

Copyright (C) 2020-2022 RIKEN R-CCS

__YASVE comes with ABSOLUTELY NO WARRANTY.__

YASVE is a trivial SIGILL trap handler for ARM SVE instructions.  On
trapping an SVE instruction, it emulates it.  It is like ARM's ARMIE.

## Usage

Running make creates a dynamic-linking library "libyasve.so".
"libyasve.so" can be used with LD_PRELOAD.  The library installs a
SIGILL trap handler at start up in a constructor initializer.

```
$ LD_PRELOAD=libyasve.so ./a.out
```

YASVE includes a utility tool "runstatic".  It can be used in
situations when the binary is statically-linked and relinking with
YASVE is difficult.  "runstatic" places the binary image of "a.out" in
the large hole in the address space of "runstatic", and jumps to the
entry point of the "a.out" image.

Prerequisite packages (in Ubuntu):
* libelf-dev
* elfutils-libelf
* elfutils-libelf-devel

These are only necessary in building "runstatic".

## Limitations

Programs depending on the cache line size may not work properly.  It
is uncontrollable.

## Source code

* [action.c](action.c): instuction definition
* [insn.c](insn.c): a part of the instruciton table in binutils
* [yasve.c](yasve.c): opcode dispatcher
* [preloader.c](preloader.c): trap handler setting for static linked a.out

YASVE includes the instruction table "aarch64-tbl.h" taken from
binutils-2_37.  YASVE uses an extracted file "insn.c" which consists
only of the SVE-1 part of the table.  The source of binutils is is
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
