/*
 * Apple // emulator for Linux: C portion of Disk ][ emulation
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

/*
 * (De-)nibblizing routines sourced from AppleWin project.
 */

#include "common.h"

#if DISK_TRACING
static FILE *test_read_fp = NULL;
static FILE *test_write_fp = NULL;
#endif

extern uint8_t slot6_rom[256];

drive_t disk6;

static int stepper_phases = 0; // state bits for stepper magnet phases 0-3
static int skew_table_6_po[16] = { 0x00,0x08,0x01,0x09,0x02,0x0A,0x03,0x0B, 0x04,0x0C,0x05,0x0D,0x06,0x0E,0x07,0x0F }; // ProDOS order
static int skew_table_6_do[16] = { 0x00,0x07,0x0E,0x06,0x0D,0x05,0x0C,0x04, 0x0B,0x03,0x0A,0x02,0x09,0x01,0x08,0x0F }; // DOS order

static int translate_table_6[0x40] = {
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

static void cut_gz(char *name) {
    char *p = name + strlen(name) - 1;
    p--;
    p--;
    *p = '\0';
}

static bool is_gz(const char * const name) {
    size_t len = strlen( name );
    if (len > 3) {
        return (strcmp(name+len-3, ".gz") == 0);
    }
    return false;
}

static bool is_nib(const char * const name) {
    size_t len = strlen( name );
    if (is_gz(name)) {
        len -= 3;
    }
    if (!strncmp(name + len - 4, ".nib", 4)) {
        return true;
    }
    return false;
}

static bool is_po(const char * const name) {
    size_t len = strlen( name );
    if (is_gz(name)) {
        len -= 3;
    }
    if (!strncmp(name + len - 3, ".po", 3)) {
        return true;
    }
    return false;
}

#define NUM_SIXBIT_NIBS 342

static void nibblize_sector(const uint8_t *src, uint8_t *out) {
    SCOPE_TRACE_DISK("nibblize_sector");

    uint8_t work_buf[NUM_SIXBIT_NIBS];
    uint8_t *nib = work_buf;

    // Convert 256 8-bit bytes into 342 6-bit bytes
    {
        unsigned int counter = 0;
        uint8_t offset = 0xAC;
        while (offset != 0x02) {
            uint8_t value =        (( (*(src+offset)) & 0x01) << 1) | (( (*(src+offset)) & 0x02) >> 1);
            offset -= 0x56;
            value = (value << 2) | (( (*(src+offset)) & 0x01) << 1) | (( (*(src+offset)) & 0x02) >> 1);
            offset -= 0x56;
            value = (value << 2) | (( (*(src+offset)) & 0x01) << 1) | (( (*(src+offset)) & 0x02) >> 1);
            offset -= 0x53;
            *(nib++) = value << 2;
            ++counter;
        }
        *(nib-2) &= 0x3F;
        *(nib-1) &= 0x3F;
        ++counter;
        unsigned int loop = 0;
        while (loop < 0x100) {
            *(nib++) = *(src+(loop++));
            ++counter;
        }
        assert((counter == NUM_SIXBIT_NIBS+1) && "nibblizing counter overflow");
    }

    src = work_buf;
    uint8_t work_buf2[NUM_SIXBIT_NIBS+1];
    nib = work_buf2;

    // XOR the entire data block with itself offset by one byte, creating a final checksum byte
    {
        uint8_t savedval = 0;
        int loop = NUM_SIXBIT_NIBS;
        while (loop--) {
            *(nib++) = savedval ^ *src;
            savedval = *(src++);
        }
        *nib = savedval;
    }

    src = work_buf2;
    nib = NULL;

    // Convert the 6-bit bytes into disk bytes using a lookup table.  A valid disk byte is a byte that has the high bit
    // set, at least two adjacent bits set (excluding the high bit), and at most one pair of consecutive zero bits.
    {
        int loop = NUM_SIXBIT_NIBS+1;
        while (loop--) {
            *(out++) = translate_table_6[(*(src++)) >> 2];
        }
    }
}

static void denibblize_sector(const uint8_t *src, uint8_t *out) {
    SCOPE_TRACE_DISK("denibblize_sector");

    uint8_t work_buf[NUM_SIXBIT_NIBS+1];
    uint8_t *dsk = work_buf;

    static uint8_t denib[0x80] = { 0 };
    static bool tablegenerated = false;
    if (!tablegenerated) {
        unsigned int loop = 0;
        while (loop < 0x40) {
            denib[translate_table_6[loop]-0x80] = loop << 2;
            loop++;
        }
        tablegenerated = true;
    }

    // Convert disk bytes into 6-bit bytes
    {
        unsigned int loop = NUM_SIXBIT_NIBS+1;
        while (loop--) {
            *(dsk++) = denib[*(src++) & 0x7F];
        }
    }

#if DISK_TRACING
    if (test_write_fp) {
        fprintf(test_write_fp, "SIXBITNIBS:\n");
        for (unsigned int i=0; i<NUM_SIXBIT_NIBS+1; i++) {
            fprintf(test_write_fp, "%02X", work_buf[i]);
        }
        fprintf(test_write_fp, "\n");
    }
#endif

    src = work_buf;
    uint8_t work_buf2[NUM_SIXBIT_NIBS];
    dsk = work_buf2;

    // XOR the entire data block with itself offset by one byte to undo checksumming
    {
        uint8_t savedval  = 0;
        unsigned int loop = NUM_SIXBIT_NIBS;
        while (loop--) {
            *dsk = savedval ^ *(src++);
            savedval = *(dsk++);
        }
    }

#if DISK_TRACING
    if (test_write_fp) {
        fprintf(test_write_fp, "XORNIBS:\n");
        for (unsigned int i=0; i<NUM_SIXBIT_NIBS; i++) {
            fprintf(test_write_fp, "%02X", work_buf2[i]);
        }
        fprintf(test_write_fp, "\n");
    }
#endif

    // Convert 342 6-bit bytes into 256 8-bit bytes
    {
        uint8_t *lowbitsptr = work_buf2;
        uint8_t *sectorbase = work_buf2+0x56;
        uint8_t offset = 0xAC;
        unsigned int counter = 0;
        while (offset != 0x02) {
            assert(counter < 256 && "denibblizing counter overflow");
            if (offset >= 0xAC) {
                *(out+offset) = (*(sectorbase+offset) & 0xFC) | (((*lowbitsptr) & 0x80) >> 7) | (((*lowbitsptr) & 0x40) >> 5);
                ++counter;
            }
            offset -= 0x56;

            *(out+offset) = (*(sectorbase+offset) & 0xFC) | (((*lowbitsptr) & 0x20) >> 5) | (((*lowbitsptr) & 0x10) >> 3);
            ++counter;
            offset -= 0x56;

            *(out+offset) = (*(sectorbase+offset) & 0xFC) | (((*lowbitsptr) & 0x08) >> 3) | (((*lowbitsptr) & 0x04) >> 1);
            ++counter;
            offset -= 0x53;

            ++lowbitsptr;
        }
        assert(counter == 256 && "invalid bytes count");
    }

#ifdef DISK_TRACING
    if (test_write_fp) {
        fprintf(test_write_fp, "SECTOR:\n");
        for (unsigned int i = 0; i < 256; i++) {
            fprintf(test_write_fp, "%02X", out[i]);
        }
        fprintf(test_write_fp, "%s", "\n");
    }
#endif
}

#define CODE44A(a) ((((a)>> 1) & 0x55) | 0xAA)
#define CODE44B(b) (((b) & 0x55) | 0xAA)

static unsigned long nibblize_track(uint8_t *buf, int drive) {
    SCOPE_TRACE_DISK("nibblize_track");

    uint8_t *output = disk6.disk[drive].track_image;
    
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

    return output-disk6.disk[drive].track_image;
}

static void denibblize_track(int drive, uint8_t *dst) {
    SCOPE_TRACE_DISK("denibblize_track");

    // Searches through the track data for each sector and decodes it
#warning TODO FIXME inefficient -- refactor after moar tests =P

    uint8_t *trackimage = disk6.disk[drive].track_image;
#if DISK_TRACING
    if (test_write_fp) {
        fprintf(test_write_fp, "DSK OUT:\n");
    }
#endif

    unsigned int offset = 0;
    unsigned int partsleft = (NUM_SECTORS<<1) +1;
    int sector = -1;
    while (partsleft) {
        --partsleft;
        uint8_t prologue[3] = {0,0,0}; // D5AA..

        // Find prologue
        unsigned int count = 0;
        unsigned int loop = NIB_TRACK_SIZE;
        while (loop && (count < 3)) {
            --loop;
            if (count || (*(trackimage+offset) == 0xD5)) {
                prologue[count] = *(trackimage+offset);
                ++count;
            }
            ++offset;
            if (offset >= disk6.disk[drive].track_width) {
                offset = 0;
            }
        }

        if ((count == 3) && (prologue[1] = 0xAA)) {
#define DATA_BYTES_LEN (256+128)
            unsigned int secloop = 0;
            unsigned int tmpoff = offset;
            uint8_t work_buf[DATA_BYTES_LEN] = { 0 };
            uint8_t *nib = work_buf;
            while (secloop < DATA_BYTES_LEN) {
                *(nib+secloop) = *(trackimage+tmpoff);
                if (tmpoff >= disk6.disk[drive].track_width) {
                    tmpoff = 0;
                }
                ++secloop;
                ++tmpoff;
            }

            if (prologue[2] == 0x96) { // header
                sector = ((*(nib+4) & 0x55) << 1) | (*(nib+5) & 0x55);
            } else if (prologue[2] == 0xAD) { // data
                int sec_off = 256 * disk6.disk[drive].skew_table[ sector ];
                denibblize_sector(nib, dst+sec_off);
                sector = -1;
            }
        }
    }
}

static bool load_track_data(void) {
    SCOPE_TRACE_DISK("load_track_data");

    if (disk6.disk[disk6.drive].nibblized) {
        // .nib image
        int track_pos = NIB_TRACK_SIZE * (disk6.disk[disk6.drive].phase >> 1);
        fseek(disk6.disk[disk6.drive].fp, track_pos, SEEK_SET);

        if (fread(disk6.disk[disk6.drive].track_image, 1, NIB_TRACK_SIZE, disk6.disk[disk6.drive].fp) != NIB_TRACK_SIZE) {
            ERRLOG("nib image corrupted ...");
            return false;
        }
        disk6.disk[disk6.drive].track_width = NIB_TRACK_SIZE;
    } else {
        // .dsk, .do, .po images
        int track_pos = DSK_TRACK_SIZE * (disk6.disk[disk6.drive].phase >> 1);
        fseek(disk6.disk[disk6.drive].fp, track_pos, SEEK_SET);

        uint8_t buf[DSK_TRACK_SIZE];
        if (fread(buf, 1, DSK_TRACK_SIZE, disk6.disk[disk6.drive].fp) != DSK_TRACK_SIZE) {
            ERRLOG("dsk image corrupted ...");
            return false;
        }

        disk6.disk[disk6.drive].track_width = nibblize_track(buf, disk6.drive);
        if (disk6.disk[disk6.drive].track_width != NI2_TRACK_SIZE) {
#if CONFORMANT_TRACKS
            ERRLOG("Invalid dsk image creation...");
            return false;
#endif
        }
    }

    disk6.disk[disk6.drive].track_valid = true;
    disk6.disk[disk6.drive].run_byte = 0;
    return true;
}

static bool save_track_data(void) {
    SCOPE_TRACE_DISK("save_track_data");

    if (disk6.disk[disk6.drive].nibblized) {
        // .nib image
        int track_pos = NIB_TRACK_SIZE * (disk6.disk[disk6.drive].phase >> 1);
        fseek(disk6.disk[disk6.drive].fp, track_pos, SEEK_SET);
        if (fwrite(disk6.disk[disk6.drive].track_image, 1, NIB_TRACK_SIZE, disk6.disk[disk6.drive].fp) != NIB_TRACK_SIZE) {
            ERRLOG("could not write nib data ...");
            return false;
        }
    } else {
        // .dsk, .do, .po images
        uint8_t buf[DSK_TRACK_SIZE];
        denibblize_track(disk6.drive, buf);
        int track_pos = DSK_TRACK_SIZE * (disk6.disk[disk6.drive].phase >> 1);
        fseek(disk6.disk[disk6.drive].fp, track_pos, SEEK_SET);
        if (fwrite(buf, 1, DSK_TRACK_SIZE, disk6.disk[disk6.drive].fp) != DSK_TRACK_SIZE) {
            ERRLOG("could not write dsk data ...");
            return false;
        }
        fflush(disk6.disk[disk6.drive].fp);
    }

    disk6.disk[disk6.drive].track_dirty = false;
    return true;
}

// ----------------------------------------------------------------------------
// Emulator hooks

GLUE_C_READ(disk_read_write_byte)
{
    uint8_t value = 0xFF;
    do {
        if (disk6.disk[disk6.drive].fp == NULL) {
            break;
        }

        if (!disk6.disk[disk6.drive].track_valid) {
            if (!load_track_data()) {
                ERRLOG("OOPS, problem loading track data");
                break;
            }
        }

        if (disk6.ddrw) {
            if (disk6.disk[disk6.drive].is_protected) {
                value = 0x00;
                break;           /* Do not write if diskette is write protected */
            }

            if (disk6.disk_byte < 0x96) {
                ERRLOG("OOPS, attempting to write a non-nibblized byte");
                value = 0x00;
                break;
            }

#if DISK_TRACING
            if (test_write_fp) {
                fprintf(test_write_fp, "%02X", disk6.disk_byte);
            }
#endif
            disk6.disk[disk6.drive].track_image[disk6.disk[disk6.drive].run_byte] = disk6.disk_byte;
            disk6.disk[disk6.drive].track_dirty = true;
        } else {

            if (disk6.motor_off) { // ???
                if (disk6.motor_off > 99) {
                    ERRLOG("OOPS, potential disk motor issue");
                    value = 0x00;
                    break;
                } else {
                    disk6.motor_off++;
                }
            }

            value = disk6.disk[disk6.drive].track_image[disk6.disk[disk6.drive].run_byte];
#if DISK_TRACING
            if (test_read_fp) {
                fprintf(test_read_fp, "%02X", value);
            }
#endif
        }
    } while (0);

    ++disk6.disk[disk6.drive].run_byte;
    if (disk6.disk[disk6.drive].run_byte >= disk6.disk[disk6.drive].track_width) {
        disk6.disk[disk6.drive].run_byte = 0;
    }
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

    return value;
}

GLUE_C_READ(disk_read_phase)
{
    ea &= 0xFF;
    int phase = (ea>>1)&3;
    int phase_bit = (1 << phase);

    char *phase_str = NULL;
    if (ea & 1) {
        phase_str = "on ";
        stepper_phases |= phase_bit;
    } else {
        phase_str = "off";
        stepper_phases &= ~phase_bit;
    }
#if DISK_TRACING
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
        if (disk6.disk[disk6.drive].track_dirty) {
            save_track_data();
        }
        disk6.disk[disk6.drive].track_valid = false;
        disk6.disk[disk6.drive].phase += direction;

        if (disk6.disk[disk6.drive].phase<0) {
            disk6.disk[disk6.drive].phase=0;
        }

        if (disk6.disk[disk6.drive].phase>69) { // AppleWin uses 79 (extra tracks/phases)?
            disk6.disk[disk6.drive].phase=69;
        }

#if DISK_TRACING
        if (test_read_fp) {
            fprintf(test_read_fp, "NEW TRK:%d\n", (disk6.disk[disk6.drive].phase>>1));
        }
        if (test_write_fp) {
            fprintf(test_write_fp, "NEW TRK:%d\n", (disk6.disk[disk6.drive].phase>>1));
        }
#endif
    }

    return ea == 0xE0 ? 0xFF : floating_bus_hibit(1);
}

GLUE_C_READ(disk_read_motor_off)
{
    clock_gettime(CLOCK_MONOTONIC, &disk6.motor_time);
    disk6.motor_off = 1;
    return floating_bus_hibit(1);
}

GLUE_C_READ(disk_read_motor_on)
{
    clock_gettime(CLOCK_MONOTONIC, &disk6.motor_time);
    disk6.motor_off = 0;
    return floating_bus_hibit(1);
}

GLUE_C_READ(disk_read_select_a)
{
    disk6.drive = 0;
    return floating_bus();
}

GLUE_C_READ(disk_read_select_b)
{
    disk6.drive = 1;
    return floating_bus();
}

GLUE_C_READ(disk_read_latch)
{
    return disk6.drive;
}

GLUE_C_READ(disk_read_prepare_in)
{
    disk6.ddrw = 0;
    return floating_bus_hibit(disk6.disk[disk6.drive].is_protected);
}

GLUE_C_READ(disk_read_prepare_out)
{
    disk6.ddrw = 1;
    return floating_bus_hibit(1);
}

GLUE_C_WRITE(disk_write_latch)
{
    disk6.disk_byte = b;
    //return b;
}

// ----------------------------------------------------------------------------

void disk_io_initialize(unsigned int slot) {
    FILE *f;
    int i;
    char temp[PATH_MAX];

    assert(slot == 6);

    /* load Disk II rom */
    memcpy(apple_ii_64k[0] + 0xC600, slot6_rom, 0x100);

    // disk softswitches
    // 0xC0Xi : X = slot 0x6 + 0x8 == 0xE
    cpu65_vmem_r[0xC0E0] = cpu65_vmem_r[0xC0E2] = cpu65_vmem_r[0xC0E4] = cpu65_vmem_r[0xC0E6] = disk_read_phase;
    cpu65_vmem_r[0xC0E1] = cpu65_vmem_r[0xC0E3] = cpu65_vmem_r[0xC0E5] = cpu65_vmem_r[0xC0E7] = disk_read_phase;

    cpu65_vmem_r[0xC0E8] = disk_read_motor_off;
    cpu65_vmem_r[0xC0E9] = disk_read_motor_on;
    cpu65_vmem_r[0xC0EA] = disk_read_select_a;
    cpu65_vmem_r[0xC0EB] = disk_read_select_b;
    cpu65_vmem_r[0xC0EC] = disk_read_write_byte;
    cpu65_vmem_r[0xC0ED] = disk_read_latch;
    cpu65_vmem_r[0xC0EE] = disk_read_prepare_in;
    cpu65_vmem_r[0xC0EF] = disk_read_prepare_out;

    for (i = 0xC0E0; i < 0xC0F0; i++) {
        cpu65_vmem_w[i] = cpu65_vmem_r[i];
    }

    cpu65_vmem_w[0xC0ED] = disk_write_latch;
}

void c_init_6(void) {
    disk6.disk[0].phase = disk6.disk[1].phase = 0;
    disk6.disk[0].track_valid = disk6.disk[1].track_valid = 0;
    disk6.motor_time = (struct timespec){ 0 };
    disk6.motor_off = 1;
    disk6.drive = 0;
    disk6.ddrw = 0;
}

const char *c_eject_6(int drive) {

    const char *err = NULL;

    // foo.dsk -> foo.dsk.gz
    err = def(disk6.disk[drive].file_name, is_nib(disk6.disk[drive].file_name) ? NIB_SIZE : DSK_SIZE);
    if (err) {
        ERRLOG("OOPS: An error occurred when attempting to compress a disk image : %s", err);
    } else {
        unlink(disk6.disk[drive].file_name);
    }

    disk6.disk[drive].nibblized = 0;
    sprintf(disk6.disk[drive].file_name, "%s", "");
    if (disk6.disk[drive].fp) {
        fclose(disk6.disk[drive].fp);
        disk6.disk[drive].fp = NULL;
        disk6.disk[drive].track_width = 0;
    }

    return err;
}

const char *c_new_diskette_6(int drive, const char * const raw_file_name, int force) {
    struct stat buf;

    if (disk6.disk[drive].fp) {
        c_eject_6(drive);
    }

    /* uncompress the gziped disk */
    char *file_name = strdup(raw_file_name);
    if (is_gz(file_name)) {
        int rawcount = 0;
        const char *err = inf(file_name, &rawcount); // foo.dsk.gz -> foo.dsk
        if (!err) {
            int expected = is_nib(file_name) ? NIB_SIZE : DSK_SIZE;
            if (rawcount != expected) {
                err = "disk image is not expected size!";
            }
        }
        if (err) {
            ERRLOG("OOPS: An error occurred when attempting to inflate/load a disk image : %s", err);
            free(file_name);
            return err;
        }
        if (unlink(file_name)) { // temporarily remove .gz file
            ERRLOG("OOPS, cannot unlink %s", file_name);
        }

        cut_gz(file_name);
    }

    strncpy(disk6.disk[drive].file_name, file_name, FILE_NAME_SZ-1);
    disk6.disk[drive].file_name[FILE_NAME_SZ-1] = '\0';
    disk6.disk[drive].nibblized = is_nib(file_name);
    disk6.disk[drive].skew_table = skew_table_6_do;
    if (is_po(file_name)) {
        disk6.disk[drive].skew_table = skew_table_6_po;
    }
    free(file_name);
    file_name = NULL;

    if (disk6.disk[drive].fp) {
        fclose(disk6.disk[drive].fp);
        disk6.disk[drive].fp = NULL;
    }

    if (stat(disk6.disk[drive].file_name, &buf) < 0) {
        disk6.disk[drive].fp = NULL;
        c_eject_6(drive);
        return "disk unreadable 1";
    } else {
        if (!force) {
            disk6.disk[drive].fp = fopen(disk6.disk[drive].file_name, "r+");
            disk6.disk[drive].is_protected = false;
        }

        if ((disk6.disk[drive].fp == NULL) || (force)) {
            disk6.disk[drive].fp = fopen(disk6.disk[drive].file_name, "r");
            disk6.disk[drive].is_protected = true;    /* disk is write protected! */
        }

        if (disk6.disk[drive].fp == NULL) {
            /* Failed to open file. */
            c_eject_6(drive);
            return "disk unreadable 2";
        }

        /* seek to current head position. */
        fseek(disk6.disk[drive].fp, PHASE_BYTES * disk6.disk[drive].phase, SEEK_SET);
    }

    disk6.disk[drive].sector = 0;
    disk6.disk[drive].track_valid = false;
    disk6.disk[drive].track_width = 0;
    disk6.disk[drive].run_byte = 0;
    stepper_phases = 0;

    return NULL;
}

#if DISK_TRACING
void c_begin_disk_trace_6(const char *read_file, const char *write_file) {
    if (read_file) {
        test_read_fp = fopen(read_file, "w");
    }
    if (write_file) {
        test_write_fp = fopen(write_file, "w");
    }
}

void c_end_disk_trace_6(void) {
    if (test_read_fp) {
        fflush(test_read_fp);
        fclose(test_read_fp);
        test_read_fp = NULL;
    }
    if (test_write_fp) {
        fflush(test_write_fp);
        fclose(test_write_fp);
        test_write_fp = NULL;
    }
}

void c_toggle_disk_trace_6(const char *read_file, const char *write_file) {
    if (test_read_fp) {
        c_end_disk_trace_6();
    } else {
        c_begin_disk_trace_6(read_file, write_file);
    }
}
#endif

