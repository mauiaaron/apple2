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

#define DSK_SIZE 143360
#define NIB_SIZE 232960
#define NI2_SIZE 223440

#define NUM_TRACKS 35
#define NUM_SECTORS 16

#define DSK_TRACK_SIZE 0x1000 // DOS order, ProDOS order
#define NIB_TRACK_SIZE 0x1A00 // NIB format
#define NI2_TRACK_SIZE 0x18F0 // NI2 format

#define PHASE_BYTES (NIB_TRACK_SIZE/2)
#define NIB_SEC_SIZE (NIB_TRACK_SIZE/NUM_SECTORS)

#define DSK_VOLUME 254
#define FILE_NAME_SZ (PATH_MAX>>2)

typedef struct diskette_t {
    uint8_t track_image[NIB_TRACK_SIZE];
    char file_name[FILE_NAME_SZ];
    bool nibblized;
    bool is_protected;
    bool track_valid;
    bool track_dirty;
    int *skew_table;
    int sector;
    long nib_count;
    int phase;
    int run_byte;
    FILE *fp;
} diskette_t;

typedef struct drive_t {
    struct timespec motor_time;
    int motor_off;
    int drive;
    int ddrw;
    int disk_byte;
    int exor_value;
    diskette_t disk[2];
} drive_t;

extern drive_t disk6;

void c_init_6(void);
const char *c_new_diskette_6(int drive, const char * const file_name, int force);
const char *c_eject_6(int drive);
void disk_io_initialize(unsigned int slot);

#if DISK_TRACING
void c_toggle_disk_trace_6(const char *read_file, const char *write_file);
void c_begin_disk_trace_6(const char *read_file, const char *write_file);
void c_end_disk_trace_6(void);
#endif

#endif
