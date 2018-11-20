/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#ifndef A2_DISK_H
#define A2_DISK_H

#include "common.h"

#define ERR_IMAGE_NOT_EXPECTED_SIZE "disk image is not expected size"
#define ERR_CANNOT_DUP "could not dup() disk image file descriptor"
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

typedef struct diskette_t {
    char *file_name;
    int fd;
    uint8_t *raw_image_data;
    unsigned int whole_len;
    uint8_t *nib_image_data;
    bool nibblized;
    bool is_protected;
    bool track_valid;
    bool track_dirty;
    int *skew_table;
    unsigned int track_width;
    int phase;
    int run_byte;
} diskette_t;

typedef struct drive_t {
    struct timespec motor_time;
    int motor_off;
    int drive;
    int ddrw;
    int disk_byte;
    diskette_t disk[2];
} drive_t;

extern drive_t disk6;

// initialize emulated 5.25 Disk ][ module
extern void disk6_init(void);

// insert 5.25 disk image file from file descriptor (internally dup()'d so caller may close() the passed fd after
// invocation).  file_name need NOT be a path, and is important only to determine the image type via file extension
extern const char *disk6_insert(int fd, int drive, const char * const file_name, int readonly);

// eject 5.25 disk image file
extern const char *disk6_eject(int drive);

// flush all I/O
extern void disk6_flush(int drive);

extern bool disk6_saveState(StateHelper_s *helper);
extern bool disk6_loadState(StateHelper_s *helper);
extern bool disk6_stateExtractDiskPaths(StateHelper_s *helper, JSON_ref json);

#if DISK_TRACING
void disk6_traceToggle(const char *read_file, const char *write_file);
void disk6_traceBegin(const char *read_file, const char *write_file);
void disk6_traceEnd(void);
#endif

#endif
