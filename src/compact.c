/* 
 * Apple // emulator for Linux: Memory optimizer
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software 
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER. 
 *
 */

#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "misc.h"
#include "cpu.h"

/* This code is not essential to emulator operation.  It (ab)uses the 
 * virtual-memory system so that repetitive parts of the memory-access
 * indirection table are compressed. This should hopefully relieve load 
 * on the processor's cache and boost speed.
 * 
 * This file is far more complicated than it needs to be. For the present 
 * purpose --- emulating an Apple // series machine on an 386 box --- I 
 * could have hard-wired the compaction specifically for the Apple layout and 
 * i386 page size. 
 *
 * But it's more flexible - it needs no change to be used in an emulator
 * for Nintendo, C64, etc.  Support for host processors with different
 * page sizes will just be matter of changing the PSIZE and TPAGES macros. 
 *
 */

#define TSIZE 	524288		/* Size of the entire table */
#define PSIZE	4096		/* Size of a page */
#define TPAGES  128		/* pages in table */

#ifndef HAVE_MMAP

void precompact(void){}
void compact(void){}

#else /* HAVE_MMAP */

static int compaction_file = -1;

void pre_compact(void)
{
    const char *tmpdir;
    char *filename;
    char *x;

    /* Destroy any old mapping file */
    close(compaction_file); 

    /* Reset the mapping on the table to normal, in case compaction has 
     * been done before.  This also tests if the kernel is advanced enough
     * to support this stunt.
     */
    x = mmap((void *) cpu65_vmem,
             TSIZE,
             PROT_READ|PROT_WRITE,
             MAP_ANONYMOUS|MAP_FIXED|MAP_PRIVATE,
             0,
             0);
    
    if (x == MAP_FAILED)
    {
        /* mmap failed.
         * For now we assume this is because we are running on a system 
         * which doesn't support cookie-cutter mmap operations.
         *
         * We set compaction_file to -1 and return. This tells compact to
         * do nothing. (The emu still works.)
         */
	printf("System does not appear to support fancy mmap\n"
               "(error: %m)\n");

        compaction_file = -1;
        return;
    }
   
    /* Create a fresh new mapping file */
    tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    filename = alloca(strlen(tmpdir) + 9);
    strcpy(filename,tmpdir);
    strcat(filename,"/a2-XXXXXX");
    compaction_file = mkstemp(filename);

    if (!compaction_file)
    {
        fprintf(stderr,"cannot open temporary file (%m)\n");
        exit(EXIT_FAILURE);
    };

    unlink(filename);
    ftruncate(compaction_file,TSIZE);  /* might not be 100% portable */ 
   
    /* If the ftruncate doesn't work (Single Unix does not require it 
     * to work for the extending case), try this instead:
     *
     *  lseek(compaction_file,TSIZE-1,SEEK_SET);
     *  write(compaction_file,"",1);
     */

}
    
void compact(void)
{
    int i,j,n;
    char *work;
    char *x;

    /* Give up if the first mmap didn't work out */
    if (compaction_file == -1) return;

    work = mmap(0,
                TSIZE,
                PROT_READ|PROT_WRITE,
                MAP_FILE|MAP_SHARED,
                compaction_file,
                0);

    if (work == MAP_FAILED)
    {
        fprintf(stderr,"mmap failure (%m)");
        exit(EXIT_FAILURE);
    }

    n = 0;
    i = TPAGES;
    
    while (i--)
    {
        j = n;
        while (j-- && memcmp(work+j*PSIZE,
                             ((void *) cpu65_vmem)+i*PSIZE,
                             PSIZE));
        if (j == -1)
        {
            memcpy(work+n*PSIZE,
                   ((void *) cpu65_vmem)+i*PSIZE,
                   PSIZE);
            j = n++;
        }

        x = mmap(((void *) cpu65_vmem)+i*PSIZE,
             PSIZE,
             PROT_READ|PROT_WRITE,
             MAP_FIXED|MAP_FILE|MAP_SHARED,
             compaction_file,
             j*PSIZE);

        if (work == MAP_FAILED)
        {
            fprintf(stderr,"mmap failure (%m)");
            exit(EXIT_FAILURE);
        }
    }

    munmap(work,TSIZE);

}

#endif /* HAVE_MMAP */
