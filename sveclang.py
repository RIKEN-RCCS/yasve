#!/usr/bin/env python

"""CC Driver"""

# It drops all optimization flags; Optimization by clang makes it
# fail.

# It runs the sequence:
# (1) clang -O0 -emit-llvm -S --target=aarch64-arm-none-eabi AHO.c
# (2) opt -mattr=+sve -O3 -force-scalable-vectorization AHO.ll
# (3) llc
# (3') (as -march=armv8-a+sve)
# (4) ld ...

# MEMO:
# [commnd] {-c; -S; -E}
# [singular] {-o outfile}
# [frontend/backend] {-Olevel; -g}
# [frontend] {-s; -Idirectory; -Uname; -Dname[=value]}
# [link] {-Ldirectory; -llibrary}
# [prefix] {-mllvm arg; -Wc,arg}
# [file suffix] {.c; .ll; .s; .o; .a; .so}

import warnings
import time
import sys
import os
import subprocess

def path_parts(s):
    (d, f) = os.path.split(s)
    (r, e) = os.path.splitext(f)
    return (d, r, e)

def make_path((d, r, e)):
    if (d != ""):
        return (d + "/" + r + e)
    else:
        return (r + e)

# Convers an option with multiple tokens to a single token.  It
# converts "-mllvm_option" => "-mllvm,option", etc.

def normalize_args(args0):
    args = []
    i = 0
    while (i < len(args0)):
        s = args0[i]
        if (s in ["-mllvm", "-o"]):
            if ((i + 1) >= len(args0)):
                raise RuntimeError("Option (" + s + ") needs an argument")
            args.append(s + "," + args0[i + 1])
            i = i + 1
        else:
            args.append(s)
        i = i + 1
    return args

args0 = normalize_args(sys.argv[1:])

# MODE is one of {"-E", "-S", "-c", "-x"} for (preprocessing),
# compiling, assembling, and linking, performed in this order.

mode = ""
aout = ""
gen_options = []
cc_files = []
cc_options = []
ll_files = []
ll_options = []
as_files = []
as_options = []
ld_args = []

def scan_args(args):
    global mode, aout
    run_mode = []
    out_files = []
    for s in args:
        if (s in ["-c", "-E", "-S"]):
            run_mode.append(s)
        elif (s[:3] in ["-o,"]):
            out_files.append(s[3:])
        elif (s[:2] in ["-O"]):
            pass
        elif (s[:2] in ["-g"]):
            gen_options.append(s)
        elif (s[:2] in ["-l", "-L"]):
            ld_args.append(s)
        elif (s[:4] == "-Wl,"):
            ld_args.append(string.split(s[4:], ","))
        elif (s[:4] == "-Wa,"):
            as_options.append(string.split(s[4:], ","))
        elif (s[:7] == "-mllvm,"):
            ll_options.append(s[7:]);
        elif (s.startswith("-")):
            cc_options.append(s)
        else:
            (d, r, e) = path_parts(s)
            if (e == ".c"):
                cc_files.append((d, r, e))
                ld_args.append((r + ".o"))
            elif (e == ".ll"):
                ll_files.append((d, r, e))
                ld_args.append((r + ".o"))
            elif (e == ".s"):
                as_files.append((d, r, e))
                ld_args.append((r + ".o"))
            elif (e in [".o", ".a", ".so"]):
                ld_args.append(s)
            elif (e == ""):
                raise RuntimeError("No file extension in: " + s)
            else:
                raise RuntimeError("Unknown file extension: (" + e + ")")
    if (len(run_mode) > 1):
        raise RuntimeError("Multiple options:" + "".join(run_mode))
    if (len(run_mode) == 0):
        mode = "-x"
    else:
        mode = run_mode[0]
    if (len(out_files) > 1):
        raise RuntimeError("Multiple output files: " + "".join(out_files))
    if (len(out_files) == 0):
        aout = "a.out"
    else:
        aout = out_files[0]
    return ()

scan_args(args0)

if (False):
    print ";;mode=", mode
    print ";;aout=", aout
    print ";;gen_options=", gen_options
    print ";;cc_files=", cc_files
    print ";;cc_options=", cc_options
    print ";;ll_files=", ll_files
    print ";;ll_options=", ll_options
    print ";;as_files=", as_files
    print ";;as_options=", as_options
    print ";;ld_args=", ld_args

if (False):
    dryrun = ["echo"]
else:
    dryrun = []

#
# STEP CLANG (.c => .ll)
#

cc_template = ["/opt/a64sve/bin/clang"
               #, "-cc1" , "-target-feature" , "+sve"
               , "-S" , "-emit-llvm" , "-O0"
               , "--target=aarch64-arm-none-eabi+sve"
               , "-ffast-math"
               #, "-enable-unsafe-fp-math"
               , "-Rpass=loop-vectorize"
               , "-Rpass-missed=loop-vectorize"
               , "-Rpass-analysis=loop-vectorize"]

def cc_run(files, options):
    for (d, r, e) in files:
        cmd = dryrun[:]
        cmd.extend(cc_template)
        cmd.extend(options)
        cmd.extend([make_path((d, r, e)), "-o", make_path(("", r, ".ll"))])
        print "CC=", cmd
        cc = subprocess.call(cmd)
        if (cc != 0):
            raise RuntimeError("clang returns: " + str(cc))

options_cc = gen_options[:]
options_cc.extend(cc_options)
if (mode in ["-S", "-c", "-x"]):
    cc_run(cc_files, options_cc)

#
# STEP OPT (.ll => .ll2)
#

opt_template = ["/opt/a64sve/bin/opt", "-S"
                #, "-march=armv8-a+sve"
                , "-mattr=+sve" , "-mattr=+neon"
                , "-O3" , "-ffast-math=1" , "-fp-contract=fast"
                , "-enable-unsafe-fp-math"
                #, "-print-all-options"
                , "-recip=all"
                #, "-recip=divf,divd,vec-divf,vec-divd"
                , "-force-scalable-vectorization"
                , "-force-vector-predication"
                , "-enable-non-consecutive-stride-ind-vars"
                , "-vectorize-loops=1"
                , "-vectorize-slp=1"
                #, "-force-vector-width=2"
                #, "-force-vector-interleave=2"
                #, "-sl-small-loop-cost=2"
                #, "-enable-laa-uncounted-loops"
                #, "-sl-enable-lv-uncounted-loops"
                #, "-aarch64-setffr-optimize"
                , "-pass-remarks=loop-vectorize"
                , "-pass-remarks-missed=loop-vectorize"
                , "-pass-remarks-analysis=loop-vectorize"]

def opt_run(files, options):
    for (d, r, e) in files:
        cmd = dryrun[:]
        cmd.extend(opt_template)
        cmd.extend(options)
        cmd.extend([make_path((d, r, ".ll")),
                    "-o", make_path((d, r, ".ll2"))])
        print "OPT=", cmd
        cc = subprocess.call(cmd)
        if (cc != 0):
            raise RuntimeError("opt returns: " + str(cc0))

files_opt = cc_files[:]
files_opt.extend(ll_files)
options_opt = gen_options[:]
options_opt.extend(ll_options)
if (mode in ["-S", "-c", "-x"]):
    opt_run(files_opt, options_opt)

#
# STEP LLC (.ll2 => .o)
#

llc_template = ["/opt/a64sve/bin/llc"
                #, "-recip=all"
                , "-mtriple=aarch64--linux-gnu" , "-mattr=+sve"
                , "-O3" , "-ffast-math" , "-fp-contract=fast"
                , "-enable-unsafe-fp-math"]

def llc_run(files, options, asmobj):
    for (d, r, e) in files:
        cmd = dryrun[:]
        cmd.extend(llc_template)
        if (asmobj == "obj"):
            cmd.extend(["-filetype=obj"])
            cmd.extend([make_path((d, r, ".ll2")),
                        "-o", make_path(("", r, ".o"))])
        elif (asmobj == "asm"):
            cmd.extend(["-filetype=asm"])
            cmd.extend([make_path((d, r, ".ll2")),
                        "-o", make_path(("", r, ".s"))])
        else:
            raise RuntimeError("(internal) bad argument")
        print "LLC=", cmd
        cc = subprocess.call(cmd)
        if (cc != 0):
            raise RuntimeError("llc returns: " + str(cc))

#p0 = subprocess.Popen(cmd0, stdout=subprocess.PIPE)
#p1 = subprocess.Popen(cmd1, stdin=p0.stdout)
#cc0 = p0.wait()
#cc1 = p1.wait()
#if (cc0 != 0):
#raise RuntimeError("llc returns: " + str(cc0))
#if (cc1 != 0):
#raise RuntimeError("as returns: " + str(cc1))

files_llc = cc_files[:]
files_llc.extend(ll_files)
options_llc = gen_options[:]
options_llc.extend(ll_options)
if (mode in ["-c", "-x"]):
    llc_run(files_llc, options_llc, "obj")
elif (mode in ["-S"]):
    llc_run(files_llc, options_llc, "asm")

# STEP (as .s => .o)

as_template = ["/opt/a64sve/bin/as", "-march=armv8-a+sve"]

def as_run(files, options):
    for (d, r, e) in files:
        cmd = dryrun[:]
        cmd.extend(as_template)
        cmd.extend([make_path((d, r, e))])
        cmd.extend(["-o", make_path(("", r, ".o"))])
        print "AS=", cmd
        cc = subprocess.call(cmd)
        if (cc != 0):
            raise RuntimeError("as returns: " + str(cc))

files_as = as_files[:]
options_as = gen_options[:]
options_as.extend(as_options)
as_run(files_as, options_as)

#
# STEP (ld .o => a.out)
#

ld_template0 = [
    "/opt/a64sve/bin/ld",
    "--hash-style=gnu",
    "--no-add-needed",
    "--eh-frame-hdr",
    "-m", "aarch64linux",
    "-dynamic-linker", "/lib/ld-linux-aarch64.so.1"]

ld_template1 = [
    "/usr/lib/gcc/aarch64-redhat-linux/4.8.5/../../../../lib64/crt1.o",
    "/usr/lib/gcc/aarch64-redhat-linux/4.8.5/../../../../lib64/crti.o",
    "/usr/lib/gcc/aarch64-redhat-linux/4.8.5/crtbegin.o",
    "-L/usr/lib/gcc/aarch64-redhat-linux/4.8.5",
    "-L/usr/lib/gcc/aarch64-redhat-linux/4.8.5/../../../../lib64",
    "-L/lib/../lib64",
    "-L/usr/lib/../lib64",
    "-L/usr/lib/gcc/aarch64-redhat-linux/4.8.5/../../..",
    "-L/opt/a64sve/bin/../lib",
    "-L/lib",
    "-L/usr/lib"]

ld_template2 = [
    "-lgcc",
    "--as-needed",
    "-lgcc_s",
    "--no-as-needed",
    "-lc",
    "-lgcc",
    "--as-needed",
    "-lgcc_s",
    "--no-as-needed",
    "/usr/lib/gcc/aarch64-redhat-linux/4.8.5/crtend.o",
    "/usr/lib/gcc/aarch64-redhat-linux/4.8.5/../../../../lib64/crtn.o"]

def ld_run(args):
    cmd = dryrun[:]
    cmd.extend(ld_template0)
    cmd.extend(["-o", aout])
    cmd.extend(ld_template1)
    cmd.extend(args)
    #cmd.extend(["/opt/a64sve/lib/yasve.o"])
    cmd.extend(["yasve.o"])
    cmd.extend(ld_template2)
    print "LD=", cmd
    cc = subprocess.call(cmd)
    if (cc != 0):
        raise RuntimeError("ld returns: " + str(cc))

if (mode == "-x"):
    ld_run(ld_args)
