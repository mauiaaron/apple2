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

/*
 * (De-)nibblizing routines sourced from AppleWin project.
 */

#include "common.h"

#include <sys/mman.h>

#define READONLY_FD 0x00DEADFD

#if DISK_TRACING
static FILE *test_read_fp = NULL;
static FILE *test_write_fp = NULL;
#endif

extern uint8_t slot6_rom[256];

drive_t disk6;

static uint8_t disk_a[NIB_SIZE] = { 0 };
static uint8_t disk_a_raw[NIB_SIZE] = { 0 };
static uint8_t disk_b[NIB_SIZE] = { 0 };
static uint8_t disk_b_raw[NIB_SIZE] = { 0 };

#if TESTING
#   define STATIC
#else
#   define STATIC static
#endif

STATIC int stepper_phases = 0; // state bits for stepper magnet phases 0-3

STATIC int skew_table_6_po[16] = { 0x00,0x08,0x01,0x09,0x02,0x0A,0x03,0x0B, 0x04,0x0C,0x05,0x0D,0x06,0x0E,0x07,0x0F }; // ProDOS order
STATIC int skew_table_6_do[16] = { 0x00,0x07,0x0E,0x06,0x0D,0x05,0x0C,0x04, 0x0B,0x03,0x0A,0x02,0x09,0x01,0x08,0x0F }; // DOS order

static pthread_mutex_t insertion_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint8_t translate_table_6[0x40] = {
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
    /*
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x01,
    0x80, 0x80, 0x02, 0x03, 0x80, 0x04, 0x05, 0x06,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x07, 0x08,
    0x80, 0x80, 0x80, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0x80, 0x80, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
    0x80, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x1b, 0x80, 0x1c, 0x1d, 0x1e,
    0x80, 0x80, 0x80, 0x1f, 0x80, 0x80, 0x20, 0x21,
    0x80, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x29, 0x2a, 0x2b,
    0x80, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
    0x80, 0x80, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x80, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
    */
};

static uint8_t rev_translate_table_6[0x80] = { 0x01 };

static void _init_disk6(void) {
    LOG("Disk ][ emulation module early setup");
    memset(&disk6, 0x0, sizeof(disk6));
    disk6.disk[0].fd = -1;
    disk6.disk[1].fd = -1;
    disk6.disk[0].raw_image_data = MAP_FAILED;
    disk6.disk[1].raw_image_data = MAP_FAILED;

    for (unsigned int i=0; i<0x40; i++) {
        rev_translate_table_6[translate_table_6[i]-0x80] = i << 2;
    }
}

static __attribute__((constructor)) void __init_disk6(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_LATE, &_init_disk6);
}

static inline bool is_nib(const char * const name) {
    size_t len = strlen(name);
    if (len <= _NIBLEN) {
        return false;
    }
    if (is_gz(name)) {
        if (len <= _NIBLEN+_GZLEN) {
            return false;
        }
        len -= _GZLEN;
    }
    return strncmp(name+len-_NIBLEN, DISK_EXT_NIB, _NIBLEN) == 0;
}

static inline bool is_po(const char * const name) {
    size_t len = strlen( name );
    if (len <= _POLEN) {
        return false;
    }
    if (is_gz(name)) {
        if (len <= _POLEN+_GZLEN) {
            return false;
        }
        len -= _GZLEN;
    }
    return strncmp(name+len-_POLEN, DISK_EXT_PO, _POLEN) == 0;
}

#define SIXBIT_MASK 0x3F // 111111
#define SIXBIT_EXTRA_BYTES 0x56 // 86
#define SIXBIT_EXTRA_WRAP 0x53 // 86 + 86 + 83 == 255
#define SIXBIT_OFF_BEGIN (SIXBIT_EXTRA_BYTES<<1) // 0xAC
#define NUM_SIXBIT_NIBS (0x100 + SIXBIT_EXTRA_BYTES) // 256 + 86 == 342

#if DISK_TRACING
#define _DISK_TRACE_RAWSRC() \
    if (test_write_fp) { \
        fprintf(test_write_fp, "RAWBUF:\n"); \
        for (unsigned int i=0; i<NUM_SIXBIT_NIBS+1; i++) { \
            fprintf(test_write_fp, "%02X", src[i]); \
        } \
        fprintf(test_write_fp, "\n"); \
    }
#define _DISK_TRACE_SIXBITNIBS() \
    if (test_write_fp) { \
        fprintf(test_write_fp, "SIXBITNIBS:\n"); \
        for (unsigned int i=0; i<NUM_SIXBIT_NIBS+1; i++) { \
            fprintf(test_write_fp, "%02X", work_buf[i]); \
        } \
        fprintf(test_write_fp, "\n"); \
    }
#define _DISK_TRACE_XORNIBS() \
    if (test_write_fp) { \
        fprintf(test_write_fp, "XORNIBS:\n"); \
        for (unsigned int i=0; i<NUM_SIXBIT_NIBS; i++) { \
            fprintf(test_write_fp, "%02X", work_buf[i]); \
        } \
        fprintf(test_write_fp, "\n"); \
    }
#define _DISK_TRACE_SECDATA() \
    if (test_write_fp) { \
        fprintf(test_write_fp, "SECTOR:\n"); \
        for (unsigned int i = 0; i < 256; i++) { \
            fprintf(test_write_fp, "%02X", out[i]); \
        } \
        fprintf(test_write_fp, "%s", "\n"); \
    }
#else
#define _DISK_TRACE_RAWSRC()
#define _DISK_TRACE_SIXBITNIBS()
#define _DISK_TRACE_XORNIBS()
#define _DISK_TRACE_SECDATA()
#endif

static void nibblize_sector(const uint8_t * const src, uint8_t * const out) {
    SCOPE_TRACE_DISK("nibblize_sector");

    uint8_t work_buf[NUM_SIXBIT_NIBS+1];

    // Convert 256 8-bit bytes into 342 6-bit bytes
    unsigned int counter = 0;
    uint8_t offset = SIXBIT_OFF_BEGIN;
    while (offset != 0x02) {
        uint8_t value =        ((src[offset] & 0x01) << 1) | ((src[offset] & 0x02) >> 1);
        offset -= SIXBIT_EXTRA_BYTES;
        value = (value << 2) | ((src[offset] & 0x01) << 1) | ((src[offset] & 0x02) >> 1);
        offset -= SIXBIT_EXTRA_BYTES;
        value = (value << 2) | ((src[offset] & 0x01) << 1) | ((src[offset] & 0x02) >> 1);
        offset -= SIXBIT_EXTRA_WRAP;
        work_buf[counter] = value << 2;
        ++counter;
    }
    assert(counter == SIXBIT_EXTRA_BYTES && "nibblizing counter about to overflow");
    work_buf[counter-2] &= SIXBIT_MASK;
    work_buf[counter-1] &= SIXBIT_MASK;
    memcpy(&work_buf[counter], src, 0x100);

    // XOR the entire data block with itself offset by one byte, creating a final checksum byte
    uint8_t savedval = 0;
    for (unsigned int i=0; i<NUM_SIXBIT_NIBS; i++) {
        uint8_t prevsaved = savedval ^ work_buf[i];
        savedval = work_buf[i];
        work_buf[i] = prevsaved;
    }
    work_buf[NUM_SIXBIT_NIBS] = savedval;

    // Convert the 6-bit bytes into disk bytes using a lookup table.  A valid disk byte is a byte that has the high bit
    // set, at least two adjacent bits set (excluding the high bit), and at most one pair of consecutive zero bits.
    for (unsigned int i=0; i<NUM_SIXBIT_NIBS+1; i++) {
        out[i] = translate_table_6[work_buf[i] >> 2];
    }
}

static void denibblize_sector(const uint8_t * const src, uint8_t * const out) {
    SCOPE_TRACE_DISK("denibblize_sector");

    _DISK_TRACE_RAWSRC();
    uint8_t work_buf[NUM_SIXBIT_NIBS+1];

    // Convert disk bytes into 6-bit bytes
    for (unsigned int i=0; i<(NUM_SIXBIT_NIBS+1); i++) {
        work_buf[i] = rev_translate_table_6[src[i] & 0x7F];
        assert(work_buf[i] != 0x1);
    }
    _DISK_TRACE_SIXBITNIBS();

    // XOR the entire data block with itself offset by one byte to undo checksumming
    uint8_t savedval = 0;
    for (unsigned int i=0; i<NUM_SIXBIT_NIBS; i++) {
        work_buf[i] = savedval ^ work_buf[i];
        savedval = work_buf[i];
    }
    _DISK_TRACE_XORNIBS();

    // Convert 342 6-bit bytes into 256 8-bit bytes

    uint8_t *lowbitsptr = work_buf;
    uint8_t *sectorbase = work_buf+SIXBIT_EXTRA_BYTES;
    uint8_t offset = SIXBIT_OFF_BEGIN;
    unsigned int counter = 0;
    for (unsigned int i=0; i<SIXBIT_EXTRA_BYTES; i++) {
        if (offset >= SIXBIT_OFF_BEGIN) {
            out[offset] = (sectorbase[offset] & 0xFC) | ((lowbitsptr[i] & 0x80) >> 7) | ((lowbitsptr[i] & 0x40) >> 5);
            ++counter;
        }
        offset -= SIXBIT_EXTRA_BYTES;
        out[offset]     = (sectorbase[offset] & 0xFC) | ((lowbitsptr[i] & 0x20) >> 5) | ((lowbitsptr[i] & 0x10) >> 3);
        ++counter;
        offset -= SIXBIT_EXTRA_BYTES;
        out[offset]     = (sectorbase[offset] & 0xFC) | ((lowbitsptr[i] & 0x08) >> 3) | ((lowbitsptr[i] & 0x04) >> 1);
        ++counter;
        offset -= SIXBIT_EXTRA_WRAP;
    }
    assert(counter == 0x100 && "invalid bytes count");
    assert(offset == 2 && "invalid bytes count");
    _DISK_TRACE_SECDATA();
}

#define CODE44A(a) ((((a)>> 1) & 0x55) | 0xAA)
#define CODE44B(b) (((b) & 0x55) | 0xAA)

static unsigned long nibblize_track(const uint8_t * const buf, int drive, uint8_t *output) {
    SCOPE_TRACE_DISK("nibblize_track");

    uint8_t * const begin_track = output;

#if CONFORMANT_TRACKS
    // Write track-beginning gap containing 48 self-sync bytes
    for (unsigned int i=0; i<48; i++) {
        *(output++) = 0xFF;
    }
#else
    // NOTE : original apple2emul used X sync bytes here and disk loading becomes much faster at a cost of conformance
    // for certain disk images.  For resource-constrained mobile/wearable devices, this is prolly the right path.
    for (unsigned int i=0; i<8; i++) {
        *(output++) = 0xFF;
    }
#endif

    unsigned int sector = 0;
    while (sector < 16) {
        // --- Address field

        // Prologue
        *(output)++ = 0xD5;
        *(output)++ = 0xAA;
        *(output)++ = 0x96;

        // Volume   (4-and-4 encoded)
        *(output)++ = CODE44A(DSK_VOLUME);
        *(output)++ = CODE44B(DSK_VOLUME);

        // Track    (4-and-4 encoded)
        int track = (disk6.disk[drive].phase>>1);
        *(output)++ = CODE44A(track);
        *(output)++ = CODE44B(track);

        // Sector   (4-and-4 encoded)
        *(output)++ = CODE44A(sector);
        *(output)++ = CODE44B(sector);

        // Checksum (4-and-4 encoded)
        uint8_t checksum = (DSK_VOLUME ^ track ^ sector);
        *(output)++ = CODE44A(checksum);
        *(output)++ = CODE44B(checksum);

        // Epilogue
        *(output)++ = 0xDE;
        *(output)++ = 0xAA;
        *(output)++ = 0xEB;

        // Gap of 6 self-sync bytes
        for (unsigned int i=0; i<6; i++) {
            *(output++) = 0xFF;
        }

        // --- Data field

        // Prologue
        *(output)++ = 0xD5;
        *(output)++ = 0xAA;
        *(output)++ = 0xAD;

        // 343 6-bit bytes of nibblized data + 6-bit checksum
        int sec_off = 256 * disk6.disk[drive].skew_table[ sector ];
        nibblize_sector(buf+sec_off, output);
        output += NUM_SIXBIT_NIBS+1;

        // Epilogue
        *(output)++ = 0xDE;
        *(output)++ = 0xAA;
        *(output)++ = 0xEB;

#if CONFORMANT_TRACKS
        // Sector gap of 27 self-sync bytes
        for (unsigned int i=0; i<27; i++) {
            *(output++) = 0xFF;
        }
#else
        // NOTE : original apple2emul used X self-sync bytes here
        for (unsigned int i=0; i<8; i++) {
            *(output++) = 0xFF;
        }
#endif

        ++sector;
    }

    return output - begin_track;
}

static void denibblize_track(const uint8_t * const src, int drive, uint8_t * const dst) {
    SCOPE_TRACE_DISK("denibblize_track");

    // Searches through the track data for each sector and decodes it

    const uint8_t * const trackimage = src;
#if DISK_TRACING
    if (test_write_fp) {
        fprintf(test_write_fp, "DSK OUT:\n");
    }
#endif

    unsigned int offset = 0;
    int sector = -1;

    const unsigned int trackwidth = disk6.disk[drive].track_width;

    // iterate over 2x sectors (accounting for header and data sections)
    for (unsigned int sct2=0; sct2<(NUM_SECTORS<<1)+1; sct2++) {
        uint8_t prologue[3] = {0,0,0}; // D5AA..

        // Find the beginning of a header or data prologue
        for (unsigned int i=0, idx=0; i<NIB_TRACK_SIZE; i++) { // FIXME: probably can change this to NIB_SEC_SIZE*2 ...
            uint8_t byte = trackimage[offset];
            if (idx || (byte == 0xD5)) {
                prologue[idx] = byte;
                ++idx;
            }
            offset = (offset+1) % trackwidth;
            if (idx >= 3) {
                break;
            }
        }

        if (prologue[1] != 0xAA) {
            continue;
        }

#define SCTOFF 0x4
        if (prologue[2] == 0x96) {
            // found header prologue : extract sector
            offset = (offset+SCTOFF) % trackwidth;
            sector = ((trackimage[offset] & 0x55) << 1);
            offset = (offset+1) % trackwidth;
            sector |= (trackimage[offset] & 0x55);
            offset = (offset+1) % trackwidth;
            continue;
        }
        if (UNLIKELY(prologue[2] != 0xAD)) {
            RELEASE_LOG("OMG, found mid-track 0xD5 byte...");
            continue;
        }

        // found data prologue : copy and write data to sector

        uint8_t work_buf[NUM_SIXBIT_NIBS+1];
        for (unsigned int idx=0; idx<(NUM_SIXBIT_NIBS+1); idx++) {
            work_buf[idx] = trackimage[offset];
            offset = (offset+1) % trackwidth;
        }
        assert(sector >= 0 && sector < 16 && "invalid previous nibblization");
        int sec_off = 256 * disk6.disk[drive].skew_table[ sector ];
        denibblize_sector(work_buf, dst+sec_off);
        sector = -1;
    }
}

static size_t load_track_data(int drive) {
    SCOPE_TRACE_DISK("load_track_data");

    size_t expected = 0;

    if (disk6.disk[drive].nibblized) {
        expected = NIB_TRACK_SIZE;
    } else {
        // .dsk, .do, .po images
        unsigned int trk = (disk6.disk[drive].phase >> 1);
        uintptr_t dskoff = DSK_TRACK_SIZE * trk;
        uintptr_t niboff = NIB_TRACK_SIZE * trk;
        expected = nibblize_track(disk6.disk[drive].raw_image_data+dskoff, drive, disk6.disk[drive].nib_image_data+niboff);
    }

    return expected;
}

static void save_track_data(int drive) {
    SCOPE_TRACE_DISK("save_track_data");

    unsigned int trk = (disk6.disk[drive].phase >> 1);
    uintptr_t niboff = NIB_TRACK_SIZE * trk;

    if (disk6.disk[drive].nibblized) {
        /*
        int ret = -1;
        TEMP_FAILURE_RETRY(ret = msync(disk6.disk[drive].raw_image_data+niboff, NIB_TRACK_SIZE, MS_SYNC));
        if (ret) {
            ERRLOG("Error syncing file %s", disk6.disk[drive].file_name);
        }
        */
    } else {
        // .dsk, .do, .po images
        uintptr_t dskoff = DSK_TRACK_SIZE * trk;
        denibblize_track(/*src:*/disk6.disk[drive].nib_image_data+niboff, drive, /*dst:*/disk6.disk[drive].raw_image_data+dskoff);
        /*
        int ret = -1;
        TEMP_FAILURE_RETRY(ret = msync(disk6.disk[drive].raw_image_data+dskoff, DSK_TRACK_SIZE, MS_SYNC));
        if (ret) {
            ERRLOG("Error syncing file %s", disk6.disk[drive].file_name);
        }
        */
    }

    disk6.disk[drive].track_dirty = false;
}

static inline void animate_disk_track_sector(void) {
    if (video_animations && video_animations->animation_showTrackSector) {
        static int previous_sect = 0;
        int sect_width = disk6.disk[disk6.drive].track_width>>4; // div-by-16
        do {
            if (UNLIKELY(sect_width <= 0)) {
                break;
            }
            int sect = disk6.disk[disk6.drive].run_byte/sect_width;
            if (sect != previous_sect) {
                previous_sect = sect;
                video_animations->animation_showTrackSector(disk6.drive, disk6.disk[disk6.drive].phase>>1, sect);
            }
        } while (0);
    }
}

// ----------------------------------------------------------------------------
// Emulator hooks

static void _disk_readWriteByte(void) {
    do {
        if (disk6.disk[disk6.drive].fd < 0) {
            ////ERRLOG_THROTTLE("OOPS, attempt to read byte from NULL image in drive (%d)", disk6.drive+1);
            disk6.disk_byte = 0xFF;
            break;
        }

        if (!disk6.disk[disk6.drive].track_valid) {
            assert(!disk6.disk[disk6.drive].track_dirty);
            // TODO FIXME ... methinks we shouldn't need to reload, but :
            //  * testing shows different intermediate results (SIXBITNIBS, etc)
            //  * could be instability between the {de,}nibblize routines
            size_t track_width = load_track_data(disk6.drive);
            if (track_width != disk6.disk[disk6.drive].track_width) {
                ////ERRLOG_THROTTLE("OOPS, problem loading track data");
                disk6.disk_byte = 0xFF;
                break;
            }
            disk6.disk[disk6.drive].track_valid = true;
            disk6.disk[disk6.drive].run_byte = 0;
        }

        uintptr_t track_idx = NIB_TRACK_SIZE * (disk6.disk[disk6.drive].phase >> 1);
        track_idx += disk6.disk[disk6.drive].run_byte;
        if (disk6.ddrw) {
            if (disk6.disk[disk6.drive].is_protected) {
                break; // Do not write if diskette is write protected
            }

            if (disk6.disk_byte < 0x96) {
                ////ERRLOG_THROTTLE("OOPS, attempting to write a non-nibblized byte");
                break;
            }

#if DISK_TRACING
            if (test_write_fp) {
                fprintf(test_write_fp, "%02X", disk6.disk_byte);
            }
#endif

            disk6.disk[disk6.drive].nib_image_data[track_idx] = disk6.disk_byte;
            disk6.disk[disk6.drive].track_dirty = true;
        } else {

            if (disk6.motor_off) { // !!! FIXME TODO ... introduce a proper spin-down, cribbing from AppleWin
                if (disk6.motor_off > 99) {
                    ////ERRLOG_THROTTLE("OOPS, potential disk motor issue");
                    disk6.disk_byte = 0xFF;
                    break;
                } else {
                    disk6.motor_off++;
                }
            }

            disk6.disk_byte = disk6.disk[disk6.drive].nib_image_data[track_idx];
#if DISK_TRACING
            if (test_read_fp) {
                fprintf(test_read_fp, "%02X", disk6.disk_byte);
            }
#endif
        }
    } while (0);

    ++disk6.disk[disk6.drive].run_byte;
    if (disk6.disk[disk6.drive].run_byte >= disk6.disk[disk6.drive].track_width) {
        disk6.disk[disk6.drive].run_byte = 0;
    }

    animate_disk_track_sector();

#if DISK_TRACING
    if ((disk6.disk[disk6.drive].run_byte % NIB_SEC_SIZE) == 0) {
        if (disk6.ddrw) {
            if (test_write_fp) {
                fprintf(test_write_fp, "%s", "\n");
            }
        } else {
            if (test_read_fp) {
                fprintf(test_read_fp, "%s", "\n");
            }
        }
    }
#endif
}

static void _disk6_phaseChange(uint16_t ea) {
    ea &= 0xFF;
    int phase = (ea>>1)&3;
    int phase_bit = (1 << phase);

    if (ea & 1) {
        stepper_phases |= phase_bit;
    } else {
        stepper_phases &= ~phase_bit;
    }
#if DISK_TRACING
    char *phase_str = NULL;
    if (ea & 1) {
        phase_str = "on ";
    } else {
        phase_str = "off";
    }
    if (test_read_fp) {
        fprintf(test_read_fp,  "\ntrack %02X phases %X phase %d %s address $C0E%X\n", disk6.disk[disk6.drive].phase, stepper_phases, phase, phase_str, ea&0xF);
    }
    if (test_write_fp) {
        fprintf(test_write_fp, "\ntrack %02X phases %X phase %d %s address $C0E%X\n", disk6.disk[disk6.drive].phase, stepper_phases, phase, phase_str, ea&0xF);
    }
#endif

    // Disk ][ Magnet causes stepping effect:
    //  - move only when the magnet opposite the cog is off
    //  - move in the direction of an adjacent magnet if one is on
    //  - do not move if both adjacent magnets are on
    int direction = 0;
    int cur_phase = disk6.disk[disk6.drive].phase;
    if (stepper_phases & (1 << ((cur_phase + 1) & 3))) {
        direction += 1;
    }
    if (stepper_phases & (1 << ((cur_phase + 3) & 3))) {
        direction -= 1;
    }

    if (direction) {
        int next_phase = cur_phase + direction;

        if (next_phase < 0) {
            next_phase = 0;
        }

        if (next_phase > 69) { // AppleWin uses 79 (extra tracks/phases)?
            next_phase = 69;
        }

        if ((cur_phase >> 1) != (next_phase >> 1)) {
            if (disk6.disk[disk6.drive].track_dirty) {
                save_track_data(disk6.drive);
            }
            disk6.disk[disk6.drive].track_valid = false;
        }

        disk6.disk[disk6.drive].phase = next_phase;

#if DISK_TRACING
        if (test_read_fp) {
            fprintf(test_read_fp, "NEW TRK:%d\n", (disk6.disk[disk6.drive].phase>>1));
        }
        if (test_write_fp) {
            fprintf(test_write_fp, "NEW TRK:%d\n", (disk6.disk[disk6.drive].phase>>1));
        }
#endif

        animate_disk_track_sector();
    }
}

static void _disk6_motorControl(uint16_t ea) {
    clock_gettime(CLOCK_MONOTONIC, &disk6.motor_time);
    int turnOn = (ea & 0x1);
    disk6.motor_off = turnOn ? 0 : 1;
}

static void _disk6_driveSelect(uint16_t ea) {
    int driveB = (ea & 0x1);
    disk6.drive = driveB ? 1 : 0;
}

static void _disk_readLatch(void) {
    if (!disk6.motor_off && !disk6.ddrw) {
        // UNIMPLEMENTED : phase 1 on forces write protect in the Disk II drive (UTA2E page 9-7)
        if (disk6.disk[disk6.drive].is_protected) {
            disk6.disk_byte |= 0x80;
        } else {
            disk6.disk_byte &= 0x7F;
        }
    }
}

static void _disk_modeSelect(uint16_t ea) {
    disk6.ddrw = (ea & 0x1);
}

GLUE_C_READ(disk6_ioRead)
{
    uint8_t sw = ea & 0xf;
    if (sw <= 0x7) {  // C0E0 - C0E7
        _disk6_phaseChange(ea);
    } else {
        switch (sw) {
            case 0x8: // C0E8
            case 0x9: // C0E9
                _disk6_motorControl(ea);
                break;
            case 0xA: // C0EA
            case 0xB: // C0EB
                _disk6_driveSelect(ea);
                break;
            case 0xC: // C0EC
                _disk_readWriteByte();
                break;
            case 0xD: // C0ED
                _disk_readLatch();
                break;
            case 0xE: // C0EE
            case 0xF: // C0EF
                _disk_modeSelect(ea);
                break;
            default:
                assert(false && "all cases should be handled");
                break;
        }
    }

    // Even addresses returns the latch (UTA2E Table 9.1)
    return (ea & 1) ? floating_bus() : disk6.disk_byte;
}

GLUE_C_WRITE(disk6_ioWrite)
{
    uint8_t sw = ea & 0xf;
    if (sw < 0x7) {   // C0E0 - C0E7
        _disk6_phaseChange(ea);
    } else {
        switch (sw) {
            case 0x8: // C0E8
            case 0x9: // C0E9
                _disk6_motorControl(ea);
                break;
            case 0xA: // C0EA
            case 0xB: // C0EB
                _disk6_driveSelect(ea);
                break;
            case 0xC: // C0EC
                _disk_readWriteByte();
                break;
            case 0xD: // C0ED
                // _disk_readLatch(); -- do not call on write
                break;
            case 0xE: // C0EE
            case 0xF: // C0EF
                _disk_modeSelect(ea);
                break;
            default:
                assert(false && "all cases should be handled");
                break;
        }
    }

    // NOTE : any address writes the latch via sequencer LD command (74LS323 datasheet)
    if (disk6.ddrw) {
        disk6.disk_byte = b;
    }
}

// ----------------------------------------------------------------------------

void disk6_init(void) {

    disk6_flush(0);
    disk6_flush(1);

    // load Disk II ROM
    memcpy(apple_ii_64k[0] + 0xC600, slot6_rom, 0x100);

    // disk softswitches
    // 0xC0Xi : X = slot 0x6 + 0x8 == 0xE

    for (unsigned int i = 0xC0E0; i < 0xC0F0; i++) {
        cpu65_vmem_r[i] = disk6_ioRead;
        cpu65_vmem_w[i] = disk6_ioWrite;
    }

    stepper_phases = 0;

    disk6.disk[0].phase = disk6.disk[1].phase = 0;
    disk6.disk[0].run_byte = disk6.disk[1].run_byte = 0;
    disk6.disk[0].track_valid = disk6.disk[1].track_valid = false;
    disk6.disk[0].track_dirty = disk6.disk[1].track_dirty = false;
    disk6.motor_time = (struct timespec){ 0 };
    disk6.motor_off = 1;
    disk6.drive = 0;
    disk6.ddrw = 0;
}

const char *disk6_eject(int drive) {

#if !TESTING
#   if __APPLE__
#       warning FIXME TODO ...
#   else
    assert(cpu_isPaused() && "CPU must be paused for disk ejection");
#   endif
#endif
    assert(drive == 0 || drive == 1);

    const char *err = NULL;

    pthread_mutex_lock(&insertion_mutex);

    if ((disk6.disk[drive].fd > 0) && !disk6.disk[drive].is_protected) {
        disk6_flush(drive);

        int ret = -1;
        off_t compressed_size = -1;

        if (disk6.disk[drive].was_gzipped) {

            // backup uncompressed data ...
            uint8_t *compressed_data = drive == 0 ? &disk_a_raw[0] : &disk_b_raw[0];

            // re-compress in place ...
            err = zlib_deflate_buffer(/*src:*/disk6.disk[drive].raw_image_data, disk6.disk[drive].whole_len, /*dst:*/compressed_data, &compressed_size);
            if (err) {
                ERRLOG("OOPS, error deflating %s : %s", disk6.disk[drive].file_name, err);
            }

            if (compressed_size > 0) {
                assert(compressed_size < disk6.disk[drive].whole_len);

                // overwrite portion of mmap()'d file with compressed data ...
                memcpy(/*dst:*/disk6.disk[drive].raw_image_data, /*src:*/compressed_data, compressed_size);

                TEMP_FAILURE_RETRY(ret = msync(disk6.disk[drive].raw_image_data, disk6.disk[drive].whole_len, MS_SYNC));
                if (ret) {
                    ERRLOG("Error syncing file %s", disk6.disk[drive].file_name);
                }
            }
        }

        TEMP_FAILURE_RETRY(ret = munmap(disk6.disk[drive].raw_image_data, disk6.disk[drive].whole_len));
        if (ret) {
            ERRLOG("Error munmap()ping file %s", disk6.disk[drive].file_name);
        }

        if (compressed_size > 0) {
            TEMP_FAILURE_RETRY(ret = ftruncate(disk6.disk[drive].fd, compressed_size));
            if (ret == -1) {
                ERRLOG("OOPS, cannot truncate file descriptor!");
            }
        }

        TEMP_FAILURE_RETRY(ret = fsync(disk6.disk[drive].fd));
        if (ret) {
            ERRLOG("Error fsync()ing file %s", disk6.disk[drive].file_name);
        }

        TEMP_FAILURE_RETRY(ret = close(disk6.disk[drive].fd));
        if (ret) {
            ERRLOG("Error close()ing file %s", disk6.disk[drive].file_name);
        }
    }

    FREE(disk6.disk[drive].file_name);

    pthread_mutex_unlock(&insertion_mutex);

    disk6.disk[drive].fd = -1;
    disk6.disk[drive].raw_image_data = MAP_FAILED;
    disk6.disk[drive].whole_len = 0;
    disk6.disk[drive].nib_image_data = NULL;
    disk6.disk[drive].nibblized = false;
    disk6.disk[drive].is_protected = false;
    disk6.disk[drive].was_gzipped = false;
    disk6.disk[drive].track_valid = false;
    disk6.disk[drive].track_dirty = false;
    disk6.disk[drive].skew_table = NULL;
    disk6.disk[drive].track_width = 0;
    // WARNING DO NOT RESET certain disk parameters on simple eject.  We need to retain state in the case where an image
    // was "re-inserted" ... e.g. Drol load screen)
    //disk6.disk[drive].phase = 0;
    //disk6.disk[drive].run_byte = 0;

    //disk6.motor_time = (struct timespec){ 0 };
    //disk6.motor_off = 1;
    //disk6.drive = 0;
    //disk6.ddrw = 0;

    return err;
}

const char *disk6_insert(int fd, int drive, const char * const file_name, int readonly) {

#if !TESTING
#   if __APPLE__
#       warning FIXME TODO ...
#   else
    assert(cpu_isPaused() && "CPU must be paused for disk insertion");
#   endif
#endif
    assert(drive == 0 || drive == 1);

    disk6_eject(drive);

    disk6.disk[drive].file_name = STRDUP(file_name);

    int expected = NIB_SIZE;
    disk6.disk[drive].nibblized = true;
    if (!is_nib(disk6.disk[drive].file_name)) {
        expected = DSK_SIZE;
        disk6.disk[drive].nibblized = false;
        disk6.disk[drive].skew_table = skew_table_6_do;
        if (is_po(disk6.disk[drive].file_name)) {
            disk6.disk[drive].skew_table = skew_table_6_po;
        }
    }

    pthread_mutex_lock(&insertion_mutex);

    const char *err = NULL;
    do {
        disk6.disk[drive].is_protected = readonly;
        disk6.disk[drive].whole_len = expected;

        if (readonly) {
            // disk images inserted readonly use raw_image_data buffer ...
            disk6.disk[drive].fd = READONLY_FD; // we're essentially done with the real FD, although it still signifies ...
            disk6.disk[drive].raw_image_data = (drive==0) ? &disk_a_raw[0] : &disk_b_raw[0];
            err = zlib_inflate_to_buffer(fd, expected, disk6.disk[drive].raw_image_data);
        } else {
            // disk images inserted read/write are mmap'd/inflated in place ...
            TEMP_FAILURE_RETRY(fd = dup(fd));
            if (fd == -1) {
                ERRLOG("OOPS, could not dup() file descriptor %d", fd);
                err = ERR_CANNOT_DUP;
                break;
            }
            disk6.disk[drive].fd = fd;

            err = zlib_inflate_inplace(disk6.disk[drive].fd, expected, &(disk6.disk[drive].was_gzipped));
            if (err) {
                ERRLOG("OOPS, An error occurred when attempting to inflate/load a disk image [%s] : [%s]", file_name, err);
                break;
            }

            TEMP_FAILURE_RETRY(disk6.disk[drive].raw_image_data = mmap(NULL, disk6.disk[drive].whole_len, (readonly ? PROT_READ : PROT_READ|PROT_WRITE), MAP_SHARED|MAP_FILE, disk6.disk[drive].fd, /*offset:*/0));
            if (disk6.disk[drive].raw_image_data == MAP_FAILED) {
                ERRLOG("OOPS, could not mmap file %s", disk6.disk[drive].file_name);
                err = ERR_MMAP_FAILED;
                break;
            }
        }

        // direct pass-thru to raw_image_data (for NIB)
        disk6.disk[drive].nib_image_data = disk6.disk[drive].raw_image_data;
        disk6.disk[drive].track_width = NIB_TRACK_SIZE;

        int lastphase = disk6.disk[drive].phase;

        if (!disk6.disk[drive].nibblized) {
            // DSK/DO/PO require nibblizing on read (and denibblizing on write) ...

            disk6.disk[drive].nib_image_data = (drive==0) ? &disk_a[0] : &disk_b[0];
            disk6.disk[drive].track_width = 0;

            for (unsigned int trk=0; trk<NUM_TRACKS; trk++) {

                disk6.disk[drive].phase = (trk<<1); // HACK : load_track_data()/nibblize_track() expects this set properly
                size_t track_width = load_track_data(drive);
                if (disk6.disk[drive].nibblized) {
                    assert(track_width == NIB_TRACK_SIZE);
                } else {
                    assert(track_width <= NIB_TRACK_SIZE);
#if CONFORMANT_TRACKS
                    if (track_width != NI2_TRACK_SIZE) {
                        ERRLOG("Invalid dsk image creation...");
                    }
#endif
                    if (!disk6.disk[drive].track_width) {
                        disk6.disk[drive].track_width = track_width;
                    } else {
                        assert((disk6.disk[drive].track_width == track_width) && "track width should match for all tracks");
                    }
                }
            }
        }

        disk6.disk[drive].phase = lastphase; // needed for some images when "re-inserted" ... e.g. Drol load screen
    } while (0);

    pthread_mutex_unlock(&insertion_mutex);

    if (err) {
        disk6_eject(drive);
    }

    return err;
}

void disk6_flush(int drive) {
    assert(drive == 0 || drive == 1);

    if (disk6.disk[drive].fd < 0) {
        return;
    }

    if (disk6.disk[drive].is_protected) {
        return;
    }

    if (disk6.disk[drive].raw_image_data == MAP_FAILED) {
        return;
    }

    if (disk6.disk[drive].track_dirty) {
        LOG("WARNING : flushing previous session for drive (%d)...", drive+1);
        save_track_data(drive);
    }

    __sync_synchronize();

    LOG("FLUSHING disk image I/O");

    int ret = -1;
    TEMP_FAILURE_RETRY(ret = msync(disk6.disk[drive].raw_image_data, disk6.disk[drive].whole_len, MS_SYNC));
    if (ret) {
        ERRLOG("Error syncing file %s", disk6.disk[drive].file_name);
    }
}

bool disk6_saveState(StateHelper_s *helper) {
    bool saved = false;
    int fd = helper->fd;

    do {
        uint8_t state = 0x0;

        state = (uint8_t)disk6.motor_off;
        if (!helper->save(fd, &state, 1)) {
            break;
        }
        LOG("SAVE motor_off = %02x", state);

        state = (uint8_t)disk6.drive;
        if (!helper->save(fd, &state, 1)) {
            break;
        }
        LOG("SAVE drive = %02x", state);

        state = (uint8_t)disk6.ddrw;
        if (!helper->save(fd, &state, 1)) {
            break;
        }
        LOG("SAVE ddrw = %02x", state);

        state = (uint8_t)disk6.disk_byte;
        if (!helper->save(fd, &state, 1)) {
            break;
        }
        LOG("SAVE disk_byte = %02x", state);

        // Drive A/B

        bool saved_drives = false;
        for (unsigned long i=0; i<3; i++) {
            if (i >= 2) {
                saved_drives = true;
                break;
            }

            disk6_flush(i);

            state = (uint8_t)disk6.disk[i].is_protected;
            if (!helper->save(fd, &state, 1)) {
                break;
            }
            LOG("SAVE is_protected[%lu] = %02x", i, state);

            uint8_t serialized[4] = { 0 };

            if (disk6.disk[i].file_name != NULL) {
                uint32_t namelen = strlen(disk6.disk[i].file_name);
                serialized[0] = (uint8_t)((namelen & 0xFF000000) >> 24);
                serialized[1] = (uint8_t)((namelen & 0xFF0000  ) >> 16);
                serialized[2] = (uint8_t)((namelen & 0xFF00    ) >>  8);
                serialized[3] = (uint8_t)((namelen & 0xFF      ) >>  0);
                if (!helper->save(fd, serialized, 4)) {
                    break;
                }

                if (!helper->save(fd, (const uint8_t *)disk6.disk[i].file_name, namelen)) {
                    break;
                }

                LOG("SAVE disk[%lu] : (%u) %s", i, namelen, disk6.disk[i].file_name);
            } else {
                memset(serialized, 0x0, sizeof(serialized));
                if (!helper->save(fd, serialized, 4)) {
                    break;
                }
                LOG("SAVE disk[%lu] (0) <NULL>", i);
            }

            state = (uint8_t)stepper_phases;
            if (!helper->save(fd, &state, 1)) {
                break;
            }
            LOG("SAVE stepper_phases[%lu] = %02x", i, stepper_phases);

            state = disk6.disk[i].was_gzipped;
            if (!helper->save(fd, &state, 1)) {
                break;
            }

            state = (uint8_t)disk6.disk[i].phase;
            if (!helper->save(fd, &state, 1)) {
                break;
            }
            LOG("SAVE phase[%lu] = %02x", i, state);

            serialized[0] = (uint8_t)((disk6.disk[i].run_byte & 0xFF00) >>  8);
            serialized[1] = (uint8_t)((disk6.disk[i].run_byte & 0xFF  ) >>  0);
            if (!helper->save(fd, serialized, 2)) {
                break;
            }
            LOG("SAVE run_byte[%lu] = %04x", i, disk6.disk[i].run_byte);
        }

        if (!saved_drives) {
            break;
        }

        saved = true;
    } while (0);

    return saved;
}

static bool _disk6_loadState(StateHelper_s *helper, JSON_ref *json) {
    bool loaded = false;
    int fd = helper->fd;

    do {
        if (json != NULL) {
            if (!json_createFromString("{}", json)) {
                LOG("OOPS, could not create JSON!");
                break;
            }

            json_mapSetStringValue(*json, "diskA", "");
            json_mapSetStringValue(*json, "diskB", "");
            json_mapSetBoolValue(*json, "readOnlyA", "true");
            json_mapSetBoolValue(*json, "readOnlyB", "true");
        }

        const bool changeState = (json == NULL);

        uint8_t state = 0x0;

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        if (changeState) {
            disk6.motor_off = state;
        }

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        if (changeState) {
            disk6.drive = state;
        }

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        if (changeState) {
            disk6.ddrw = state;
        }

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        if (changeState) {
            disk6.disk_byte = state;
        }

        // Drive A/B

        bool loaded_drives = false;

        for (unsigned long i=0; i<3; i++) {
            if (i >= 2) {
                loaded_drives = true;
                break;
            }

            if (!helper->load(fd, &state, 1)) {
                break;
            }
            bool is_protected = (bool)state;

            uint8_t serialized[4] = { 0 };
            if (!helper->load(fd, serialized, 4)) {
                break;
            }
            uint32_t namelen = 0x0;
            namelen  = (uint32_t)(serialized[0] << 24);
            namelen |= (uint32_t)(serialized[1] << 16);
            namelen |= (uint32_t)(serialized[2] <<  8);
            namelen |= (uint32_t)(serialized[3] <<  0);

            if (changeState) {
                disk6_eject(i);
            }

            if (namelen) {
                unsigned long gzlen = (_GZLEN+1);
                char *namebuf = MALLOC(namelen+gzlen+1);
                if (!helper->load(fd, (uint8_t *)namebuf, namelen)) {
                    FREE(namebuf);
                    break;
                }
                namebuf[namelen] = '\0';

                if (changeState) {
                    if (disk6_insert(i == 0 ? helper->diskFdA : helper->diskFdB, /*drive:*/i, /*file_name:*/namebuf, /*readonly:*/is_protected)) {
                        LOG("OOPS loadState : proceeding despite cannot load disk : %s", namebuf);
                        //FREE(namebuf); break; -- ignore error with inserting disk and proceed
                    }
                } else {
                    if (i == 0) {
                        json_mapSetStringValue(*json, "diskA", namebuf);
                        json_mapSetBoolValue(*json, "readOnlyA", is_protected);
                    } else {
                        json_mapSetStringValue(*json, "diskB", namebuf);
                        json_mapSetBoolValue(*json, "readOnlyB", is_protected);
                    }
                }

                FREE(namebuf);
            }

            if (!helper->load(fd, &state, 1)) {
                break;
            }
            if (changeState) {
                stepper_phases = state & 0x3; // HACK NOTE : this is unnecessarily encoded twice ...
            }

            // format A2V3+ : was_gzipped, (otherwise placeholder)
            if (!helper->load(fd, &state, 1)) {
                break;
            }
            if (helper->version >= 3) {
                disk6.disk[i].was_gzipped = (state != 0);

                // remember if image was gzipped
                prefs_setBoolValue(PREF_DOMAIN_VM, i == 0 ? PREF_DISK_DRIVEA_GZ : PREF_DISK_DRIVEB_GZ, disk6.disk[i].was_gzipped);
            }

            if (!helper->load(fd, &state, 1)) {
                break;
            }
            if (changeState) {
                disk6.disk[i].phase = state;
            }

            if (!helper->load(fd, serialized, 2)) {
                break;
            }
            if (changeState) {
                disk6.disk[i].run_byte  = (uint32_t)(serialized[0] << 8);
                disk6.disk[i].run_byte |= (uint32_t)(serialized[1] << 0);
            }
        }

        if (changeState && !loaded_drives) {
            disk6_eject(0);
            disk6_eject(1);
            break;
        }

        loaded = true;
    } while (0);

    return loaded;
}

bool disk6_loadState(StateHelper_s *helper) {
    return _disk6_loadState(helper, NULL);
}

bool disk6_stateExtractDiskPaths(StateHelper_s *helper, JSON_ref *json) {
    return _disk6_loadState(helper, json);
}

#if DISK_TRACING
void disk6_traceBegin(const char *read_file, const char *write_file) {
    if (read_file) {
        TEMP_FAILURE_RETRY_FOPEN(test_read_fp = fopen(read_file, "w"));
    }
    if (write_file) {
        TEMP_FAILURE_RETRY_FOPEN(test_write_fp = fopen(write_file, "w"));
    }
}

void disk6_traceEnd(void) {
    if (test_read_fp) {
        TEMP_FAILURE_RETRY(fflush(test_read_fp));
        TEMP_FAILURE_RETRY(fclose(test_read_fp));
        test_read_fp = NULL;
    }
    if (test_write_fp) {
        TEMP_FAILURE_RETRY(fflush(test_write_fp));
        TEMP_FAILURE_RETRY(fclose(test_write_fp));
        test_write_fp = NULL;
    }
}

void disk6_traceToggle(const char *read_file, const char *write_file) {
    if (test_read_fp) {
        disk6_traceEnd();
    } else {
        disk6_traceBegin(read_file, write_file);
    }
}
#endif

