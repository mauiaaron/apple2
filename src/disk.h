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

#define ERR_IMAGE_NOT_EXPECTED_SIZE "disk image is not expected size"
#define ERR_STAT_FAILED "disk image unreadable for stat"
#define ERR_CANNOT_OPEN "could not open disk image"
#define ERR_MMAP_FAILED "disk image unreadable for mmap"

#define NUM_TRACKS 35
#define NUM_SECTORS 16

#define DSK_TRACK_SIZE 0x1000 // DOS order, ProDOS order
#define NIB_TRACK_SIZE 0x1A00 // NIB format
#define NI2_TRACK_SIZE 0x18F0 // NI2 format

#define DSK_SIZE (NUM_TRACKS * DSK_TRACK_SIZE) // 143360
#define NIB_SIZE (NUM_TRACKS * NIB_TRACK_SIZE) // 232960
#define NI2_SIZE (NUM_TRACKS * NI2_TRACK_SIZE) // 223440

#define PHASE_BYTES (NIB_TRACK_SIZE/2)
#define NIB_SEC_SIZE (NIB_TRACK_SIZE/NUM_SECTORS)

#define DSK_VOLUME 254

#define DISK_EXT_DSK ".dsk"
#define _DSKLEN (sizeof(DISK_EXT_DSK)-1)
#define DISK_EXT_DO  ".do"
#define _DOLEN (sizeof(DISK_EXT_DO)-1)
#define DISK_EXT_PO  ".po"
#define _POLEN (sizeof(DISK_EXT_PO)-1)
#define DISK_EXT_NIB ".nib"
#define _NIBLEN (sizeof(DISK_EXT_NIB)-1)
#define DISK_EXT_GZ  ".gz"
#define _GZLEN (sizeof(DISK_EXT_GZ)-1)

typedef struct diskette_t {
    uint8_t track_image[NIB_TRACK_SIZE];
    char *file_name;
    bool nibblized;
    bool is_protected;
    bool track_valid;
    bool track_dirty;
    int *skew_table;
    int sector;
    long track_width;
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

// initialize emulated 5.25 Disk ][ module
extern void disk6_init(void);

// insert 5.25 disk image file
extern const char *disk6_insert(int drive, const char * const file_name, int force);

// eject 5.25 disk image file
extern const char *disk6_eject(int drive);

// flush all I/O
extern void disk6_flush(int drive);

#if DISK_TRACING
void c_toggle_disk_trace_6(const char *read_file, const char *write_file);
void c_begin_disk_trace_6(const char *read_file, const char *write_file);
void c_end_disk_trace_6(void);
#endif

#endif
