/* runstatic.c (2016-10-21) */
/* Copyright (C) 2016-2017 RIKEN R-CCS */

/* Preloader for Static Executables.  It execs an executable after
   setting a signal handler on SIGILL.  See "preloader.c". */

/* DO NOTHING.  Especially, do not access stdio, etc., which have
   copy-relocation.  It jumps into a shared-object, and the program
   text and data will be replaced. */

int
main(int argc, char *argv[])
{
    extern void yasve_main(int argc, char *argv[]);
    yasve_main(argc, argv);
}
