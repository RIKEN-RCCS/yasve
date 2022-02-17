#!/bin/sh

## This extracts the SVE-V1 part of the opcode table in binutils
## ("opcodes/aarch64-tbl.h").  This code depends on the version of
## binutils; The attached table is taken from binutils-2.37.  The
## separator strings are: "__/*_SVE_instructions.__*/" and
## "__/*_SVE2_instructions.__*/".  In addition, it fixes each entry by
## unquoting a name string and dropping a trailing comma.

echo "/* Copyright (C) 2012-2021 Free Software Foundation, Inc. */" > insn.c
echo "/* SPDX-License-Identifier: LGPL-3.0-or-later */" >> insn.c
echo "/* This is a part of an instruction table in cpcodes/aarch64-tbl.h. */" >> insn.c
sed -n '/^  \/\* SVE instructions.  \*\/$/,/^  \/\* SVE2 instructions.  \*\/$/p' \
    < aarch64-tbl.h | \
    grep -v F_ALIAS | \
    sed -e 's/"\([a-zA-Z0-9]*\)"/\1/g' \
	-e 's/{}/OP_SVE_NIL/' \
	-e 's/compact, 0x05a18000, 0xffbfe000/compact, 0x05218000, 0xff3fe000/' \
	-e 's/sdiv, 0x04940000, 0xffbfe000/sdiv, 0x04140000, 0xff3fe000/' \
	-e 's/sdivr, 0x04960000, 0xffbfe000/sdivr, 0x04160000, 0xff3fe000/' \
	-e 's/sxth, 0x0492a000, 0xffbfe000/sxth, 0x0412a000, 0xff3fe000/' \
	-e 's/udivr, 0x04970000, 0xffbfe000/udivr, 0x04170000, 0xff3fe000/' \
	-e 's/uxth, 0x0493a000, 0xffbfe000/uxth, 0x0413a000, 0xff3fe000/' \
	-e 's/,[ \t]*$//' >> insn.c
