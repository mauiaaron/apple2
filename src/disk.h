/*
 * Apple // emulator for Linux: Defines for Disk ][ emulation
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


#ifndef A2_DISK_H
#define A2_DISK_H

#include "common.h"

struct diskette
{
    char file_name[1024];
    int compressed;
    int nibblized;
    int is_protected;
    int phase_change;
    int sector;
    long file_size;
    int phase;
    int run_byte;
    FILE *fp;
    int file_pos;
};

struct drive
{
    int motor;
    int drive;
    int ddrw;
    int disk_byte;
    int volume;
    int checksum;
    int exor_value;
    unsigned char disk_data[258];
    struct diskette disk[2];
};

extern struct drive disk6;

void            c_init_6();
int             c_new_diskette_6(int, char*, int, int, int);
void            c_eject_6(int);
void            disk_io_initialize(unsigned int slot);

void            disk_read_nop(),
disk_read_phase(),
disk_read_motor_off(),
disk_read_motor_on(),
disk_read_select_a(),
disk_read_select_b(),
disk_read_byte(),
disk_read_latch(),
disk_write_latch(),
disk_read_prepare_in(),
disk_read_prepare_out();

#endif
