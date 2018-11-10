/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2017 Aaron Culliney
 *
 */

#include "common.h"

typedef struct backend_node_s {
    struct backend_node_s *next;
    long order;
    video_backend_s *backend;
} backend_node_s;

static bool video_initialized = false;
static bool null_backend_running = true;
static backend_node_s *head = NULL;
static video_backend_s *currentBackend = NULL;
static pthread_t render_thread_id = 0;

static unsigned int cyclesFrameLast = 0;
static unsigned int cyclesDirty = CYCLES_FRAME;
static unsigned long dirty = 0UL;
static bool reset_scanner = false;

#if VIDEO_TRACING
static FILE *video_trace_fp = NULL;
static unsigned long frameCount = 0UL;
static unsigned long frameBegin = 0UL;
static unsigned long frameEnd = UINT_MAX;
#endif

// ----------------------------------------------------------------------------
// video scanner & generator

// scanline text page offsets
static uint16_t vert_offset_txt[TEXT_ROWS + /*VBL:*/ 8 + /*extra:*/1] = {
    0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380, // 0-63  screen top
    0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380, // 64-127 screen middle
    0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380, // 128-191 screen bottom
    0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380, // 192-255 VBL...
    0x380,                                                  // 256,257,258,259,260,261
};

// scanline hires page offsets
static uint16_t vert_offset_hgr[SCANLINES_FRAME] = {
    0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,// 0-63 screen top
    0x0080,0x0480,0x0880,0x0C80,0x1080,0x1480,0x1880,0x1C80,
    0x0100,0x0500,0x0900,0x0D00,0x1100,0x1500,0x1900,0x1D00,
    0x0180,0x0580,0x0980,0x0D80,0x1180,0x1580,0x1980,0x1D80,
    0x0200,0x0600,0x0A00,0x0E00,0x1200,0x1600,0x1A00,0x1E00,
    0x0280,0x0680,0x0A80,0x0E80,0x1280,0x1680,0x1A80,0x1E80,
    0x0300,0x0700,0x0B00,0x0F00,0x1300,0x1700,0x1B00,0x1F00,
    0x0380,0x0780,0x0B80,0x0F80,0x1380,0x1780,0x1B80,0x1F80,

    0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,// 64-127 screen middle
    0x0080,0x0480,0x0880,0x0C80,0x1080,0x1480,0x1880,0x1C80,
    0x0100,0x0500,0x0900,0x0D00,0x1100,0x1500,0x1900,0x1D00,
    0x0180,0x0580,0x0980,0x0D80,0x1180,0x1580,0x1980,0x1D80,
    0x0200,0x0600,0x0A00,0x0E00,0x1200,0x1600,0x1A00,0x1E00,
    0x0280,0x0680,0x0A80,0x0E80,0x1280,0x1680,0x1A80,0x1E80,
    0x0300,0x0700,0x0B00,0x0F00,0x1300,0x1700,0x1B00,0x1F00,
    0x0380,0x0780,0x0B80,0x0F80,0x1380,0x1780,0x1B80,0x1F80,

    0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,// 128-191 screen bottom
    0x0080,0x0480,0x0880,0x0C80,0x1080,0x1480,0x1880,0x1C80,
    0x0100,0x0500,0x0900,0x0D00,0x1100,0x1500,0x1900,0x1D00,
    0x0180,0x0580,0x0980,0x0D80,0x1180,0x1580,0x1980,0x1D80,
    0x0200,0x0600,0x0A00,0x0E00,0x1200,0x1600,0x1A00,0x1E00,
    0x0280,0x0680,0x0A80,0x0E80,0x1280,0x1680,0x1A80,0x1E80,
    0x0300,0x0700,0x0B00,0x0F00,0x1300,0x1700,0x1B00,0x1F00,
    0x0380,0x0780,0x0B80,0x0F80,0x1380,0x1780,0x1B80,0x1F80,

    0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,// 192-255 VBL...
    0x0080,0x0480,0x0880,0x0C80,0x1080,0x1480,0x1880,0x1C80,
    0x0100,0x0500,0x0900,0x0D00,0x1100,0x1500,0x1900,0x1D00,
    0x0180,0x0580,0x0980,0x0D80,0x1180,0x1580,0x1980,0x1D80,
    0x0200,0x0600,0x0A00,0x0E00,0x1200,0x1600,0x1A00,0x1E00,
    0x0280,0x0680,0x0A80,0x0E80,0x1280,0x1680,0x1A80,0x1E80,
    0x0300,0x0700,0x0B00,0x0F00,0x1300,0x1700,0x1B00,0x1F00,
    0x0380,0x0780,0x0B80,0x0F80,0x1380,0x1780,0x1B80,0x1F80,

    0x0B80,0x0F80,0x1380,0x1780,0x1B80,0x1F80,              // 256,257,258,259,260,261
};

static uint16_t *vert_offset[NUM_DRAWPAGE_MODES] = { &vert_offset_txt[0], &vert_offset_hgr[0] };

// scanline horizontal offsets (UtAIIe 5-12, 5-15+)
static uint8_t scan_offset[5][CYCLES_SCANLINE] =
{
    {   // 0-63 screen top
        0x68,
        0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,// 1-8
        0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,// 9-16
        0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,// 17-24
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,// 25-32
        0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,// 33-40
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,// 41-48
        0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,// 49-56
        0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,// 57-64
    },
    {   // 64-127 screen middle
        0x10,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
        0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
        0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
        0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
        0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
        0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
        0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
        0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
    },
    {   // 128-191 screen bottom
        0x38,
        0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
        0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
        0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
        0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
        0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
        0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
        0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
        0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
    },
    {   // 192-255 VBL
        0x60,
        0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
        0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
        0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
        0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
        0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
    },
    {   // 256,257,258,259,260,261
        0x60,
        0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
        0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
        0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
        0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
        0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
    },
};

// ----------------------------------------------------------------------------

void video_init(void) {
    video_initialized = true;

    ASSERT_NOT_ON_CPU_THREAD();
    LOG("(re)setting render_thread_id : %lu -> %lu", (unsigned long)render_thread_id, (unsigned long)pthread_self());
    render_thread_id = pthread_self();

    currentBackend->init((void*)0);
}

void _video_setRenderThread(pthread_t id) {
    LOG("setting render_thread_id : %lu -> %lu", (unsigned long)render_thread_id, (unsigned long)id);
    render_thread_id = id;
}

bool video_isRenderThread(void) {
    return (pthread_self() == render_thread_id);
}

void video_shutdown(void) {

#if MOBILE_DEVICE
    // WARNING : shutdown should occur on the render thread.  Platform code (iOS, Android) should ensure this is called
    // from within a render pass...
    assert(!render_thread_id || pthread_self() == render_thread_id);
#endif

    currentBackend->shutdown();
}

void video_render(void) {
    ASSERT_ON_UI_THREAD();
    currentBackend->render();
}

void video_main_loop(void) {
    currentBackend->main_loop();
}

void video_flashText(void) {
    currentBackend->flashText();
}

bool video_isDirty(unsigned long flags) {
    return !!(dirty & flags);
}

static void _setScannerDirty() {
    if (cyclesDirty == 0) {
        cyclesFrameLast = cycles_video_frame;
    }

    unsigned int hCount = cyclesFrameLast % CYCLES_SCANLINE;
    unsigned int colCount = (CYCLES_SCANLINE - hCount) % CYCLES_SCANLINE;
    if (cyclesDirty == 0) {
        cyclesFrameLast -= hCount;
        assert(cyclesFrameLast < (CYCLES_FRAME<<1)); // should be no underflow
        assert(cyclesFrameLast % CYCLES_SCANLINE == 0);
        colCount = CYCLES_SCANLINE;

        // previously we were not dirty:
        //  - next call to video_scannerUpdate() will load scanline[] buffer
        //  - next+1 call to video_scannerUpdate() will begin with new cyclesDirty count
    }

    cyclesDirty = CYCLES_FRAME + colCount;
    assert((cyclesDirty >= CYCLES_FRAME) && (cyclesDirty <= CYCLES_FRAME + CYCLES_SCANLINE));
    assert((cyclesFrameLast + cyclesDirty) % CYCLES_SCANLINE == 0);
}

void video_setDirty(unsigned long flags) {
    __sync_fetch_and_or(&dirty, flags);
    if (flags & A2_DIRTY_FLAG) {
        ASSERT_ON_CPU_THREAD();

        // NOTE without knowing any specific information about the nature of the video update, the scanner needs to render 1.X full frames to make sure we've correct rendered the change ...
        timing_checkpointCycles();
        _setScannerDirty();

        video_scannerUpdate();
        assert(cyclesFrameLast == cycles_video_frame);
    }
}

unsigned long video_clearDirty(unsigned long flags) {
    return __sync_fetch_and_and(&dirty, ~flags);
}

// ----------------------------------------------------------------------------
// video scanner & generator routines

static inline uint16_t _getScannerAddress(drawpage_mode_t mode, int page, unsigned int vCount, unsigned int hCount) {
    uint16_t baseOff = (0x1 << 10) << (mode * 3) << page; // 0x400 or 0x2000 + page
    unsigned int vCount3 = vCount >> ((1 - (mode & 0x1)) * 3); // TEXT:vCount>>3 GRAPHICS:vCount
    return baseOff + vert_offset[mode][vCount3] + scan_offset[vCount>>6][hCount];
}

static drawpage_mode_t _currentMode(unsigned int vCount) {
    // FIXME TODO ... this is currently incorrect in VBL for MIXED
    drawpage_mode_t mode = (vCount < SCANLINES_MIX) ? video_currentMainMode(run_args.softswitches) : video_currentMixedMode(run_args.softswitches);
    return mode;
}

static void _flushScanline(uint8_t *scanline, unsigned int scanrow, unsigned int scancol, unsigned int scanend) {
    scan_data_t scandata = {
        .scanline = &scanline[0],
        .softswitches = run_args.softswitches,
        .scanrow = scanrow,
        .scancol = scancol,
        .scanend = scanend,
    };
    currentBackend->flushScanline(&scandata);
}

static void _endOfVideoFrame() {
    assert(cyclesFrameLast == cycles_video_frame);
    cyclesFrameLast -= CYCLES_FRAME;
    cycles_video_frame -= CYCLES_FRAME;

    static uint8_t textFlashCounter = 0;
    textFlashCounter = (textFlashCounter+1) & 0xf;
    if (!textFlashCounter) {
        video_flashText();
        if (!(run_args.softswitches & SS_80COL) && (run_args.softswitches & (SS_TEXT|SS_MIXED))) {
            _setScannerDirty();
        }
    }

    // TODO FIXME : modularize these (and moar) handlers for video frame completion
    MB_EndOfVideoFrame();

    //  UtAIIe 3-17 :
    //  - keyboard auto-repeat ...
    //  - power-up reset timing ...
}

void video_scannerReset(void) {
    ASSERT_ON_CPU_THREAD();
    reset_scanner = true;
}

// Call to advance the video scanner and generator when the following events occur:
//  1: Just prior writing to active video memory
//  2: Just prior toggling a video softswitch
//  3: After cpu65_run()
void video_scannerUpdate(void) {

    static uint8_t scanline[CYCLES_VIS<<1] = { 0 }; // 80 columns of data ...
    static unsigned int scancol = 0;
    static unsigned int scanidx = 0;

    ASSERT_ON_CPU_THREAD();

    if (UNLIKELY(reset_scanner)) {
        reset_scanner = false;
        cyclesFrameLast = 0;
        cyclesDirty = CYCLES_FRAME;
        scancol = 0;
        scanidx = 0;
    }

    timing_checkpointCycles(); // keep this here ... even if speaker or mockingboard previously checked ...
    if (cyclesDirty == 0) {
        cyclesFrameLast = cycles_video_frame;
        if (cycles_video_frame >= CYCLES_FRAME) {
            _endOfVideoFrame();
        }
        return;
    }
    unsigned int hCount = cyclesFrameLast % CYCLES_SCANLINE;
    unsigned int vCount = (cyclesFrameLast / CYCLES_SCANLINE) % SCANLINES_FRAME;
    /*if (scancol + scanidx) {
        assert(hCount - CYCLES_VIS_BEGIN == (scancol + scanidx));
    }*/

    assert(cycles_video_frame >= cyclesFrameLast);
    unsigned int cyclesCount = cycles_video_frame - cyclesFrameLast;
    cyclesFrameLast = cycles_video_frame;

    unsigned int page = video_currentPage(run_args.softswitches);

    uint8_t aux = 0x0;
    uint8_t mbd = 0x0;
    uint16_t addr = 0x0;

    drawpage_mode_t mode = _currentMode(vCount);
    cyclesCount = (cyclesCount <= cyclesDirty) ? cyclesCount : cyclesDirty;
    for (unsigned int i=0; i<cyclesCount; i++, --cyclesDirty) {
        const bool isVisible = ((hCount >= CYCLES_VIS_BEGIN) && (vCount < SCANLINES_VBL_BEGIN));

#if VIDEO_TRACING
        char *type = "xBL";
#else
        if (isVisible)
#endif
        {
            addr = _getScannerAddress(mode, page, vCount, hCount);
            aux = apple_ii_64k[1][addr];
            mbd = apple_ii_64k[0][addr];
        }

        if (isVisible) {
#if VIDEO_TRACING
            type = "VIS";
#endif
            unsigned int idx = (scancol<<1)+(scanidx<<1);
            assert(idx < ((CYCLES_VIS<<1) - 1));
            scanline[idx+0] = aux;
            scanline[idx+1] = mbd;
            ++scanidx;
        }

#if VIDEO_TRACING
        if (video_trace_fp && (frameBegin <= frameCount && frameCount <= frameEnd)) {
            char buf[16] = { 0 };

            uint8_t c = keys_apple2ASCII(mbd, NULL);
            if (c <= 0x1F || c >= 0x7F) {
                c = ' ';
            }
            snprintf(buf, sizeof(buf), "%c", c);
            fprintf(video_trace_fp, "%03u %s %04X/0:%02X:%s ", vCount, type, addr, mbd, buf);

            c = keys_apple2ASCII(aux, NULL);
            if (c <= 0x1F || c >= 0x7F) {
                c = ' ';
            }
            snprintf(buf, sizeof(buf), "%c", c);
            fprintf(video_trace_fp, "/1:%02X:%s (%lu) ", aux, buf, frameCount);

            vm_printSoftwitches(video_trace_fp, /*output_mem:*/false, /*output_pseudo:*/false);
            fprintf(video_trace_fp, "%s", "\n");
        }
#endif

        ++hCount;
        /*if (scancol + scanidx) {
            assert(hCount - CYCLES_VIS_BEGIN == (scancol + scanidx));
        }*/

        if (hCount == CYCLES_SCANLINE) {

            if (vCount < SCANLINES_VBL_BEGIN) {
                // complete scanline flush ...
                unsigned int scanend = scancol+scanidx;
                assert(scanend == CYCLES_VIS);
                _flushScanline(scanline, /*scanrow:*/vCount, scancol, scanend);
            }

            // begin new scanline ...
            hCount = 0;
            scancol = 0;
            scanidx = 0;
            ++vCount;

            if (vCount == SCANLINES_FRAME) {
                // begin new frame ...
                assert(cyclesFrameLast >= CYCLES_FRAME);
                vCount = 0;
                _endOfVideoFrame();
                currentBackend->frameComplete();
#if VIDEO_TRACING
                ++frameCount;
#endif
            }

            mode = _currentMode(vCount);
        }
    }

    if ((scanidx > 0) && (vCount < SCANLINES_VBL_BEGIN)) {
        // incomplete scanline flush ...
        unsigned int scanend = scancol+scanidx;
        assert(scanend < CYCLES_VIS);
        _flushScanline(scanline, /*scanrow:*/vCount, scancol, scanend);
        scancol = scanend;
        scanidx = 0;
    }
    if (cyclesDirty == 0) {
        scancol = 0;
        scanidx = 0;
        video_clearDirty(A2_DIRTY_FLAG);
        currentBackend->frameComplete();
    }

    assert(cyclesDirty < (CYCLES_FRAME<<1)); // should be no underflow
}

uint16_t video_scannerAddress(bool *ptrIsVBL) {
    // get video scanner read position
    timing_checkpointCycles();
    unsigned int hCount = cycles_video_frame % CYCLES_SCANLINE;
    unsigned int vCount = (cycles_video_frame / CYCLES_SCANLINE) % SCANLINES_FRAME;

    if (ptrIsVBL) {
        *ptrIsVBL = (vCount >= SCANLINES_VBL_BEGIN);
    }

    // AppleWin : Required for ANSI STORY (end credits) vert scrolling mid-scanline mixed mode: DGR80, TEXT80, DGR80
    hCount -= 2;
    if ((int)hCount < 0) {
        hCount += CYCLES_SCANLINE;
        --vCount;
        if ((int)vCount < 0) {
            vCount = SCANLINES_FRAME-1;
        }
    }

    unsigned int page = video_currentPage(run_args.softswitches);
    drawpage_mode_t mode = (vCount < SCANLINES_VIS) ? video_currentMainMode(run_args.softswitches) : video_currentMixedMode(run_args.softswitches);
    uint16_t addr = _getScannerAddress(mode, page, vCount, hCount);
    return addr;
}

uint8_t floating_bus(void) {
    uint16_t scanner_addr = video_scannerAddress(NULL);
    return apple_ii_64k[0][scanner_addr];
}

#if VIDEO_TRACING
void video_scannerTraceBegin(const char *trace_file, unsigned long count) {
    if (video_trace_fp) {
        video_scannerTraceEnd();
    }
    if (trace_file) {
        video_trace_fp = fopen(trace_file, "w");
        frameCount = 0UL;
        frameBegin = 0UL;
        frameEnd = UINT_MAX;
        if (count > 0) {
            frameBegin = frameCount+1;
            frameEnd = frameCount+count;
        }
    }
}

void video_scannerTraceEnd(void) {
    if (video_trace_fp) {
        fflush(video_trace_fp);
        fclose(video_trace_fp);
        video_trace_fp = NULL;
        frameCount = 0UL;
        frameBegin = 0UL;
        frameEnd = UINT_MAX;
    }
}

void video_scannerTraceCheckpoint(void) {
    if (video_trace_fp) {
        fflush(video_trace_fp);
    }
}

bool video_scannerTraceShouldStop(void) {
    return frameCount > frameEnd;
}
#endif

// ----------------------------------------------------------------------------
// state save & restore

bool video_saveState(StateHelper_s *helper) {
    bool saved = false;
    int fd = helper->fd;

    do {
        uint8_t state = 0x0;
        if (!helper->save(fd, &state, 1)) {
            break;
        }
        LOG("SAVE (no-op) video__current_page = %02x", state);

        saved = true;
    } while (0);

    return saved;
}

bool video_loadState(StateHelper_s *helper) {
    bool loaded = false;
    int fd = helper->fd;

    do {
        uint8_t state = 0x0;

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        LOG("LOAD (no-op) video__current_page = %02x", state);

        loaded = true;
    } while (0);

    return loaded;
}

// ----------------------------------------------------------------------------
// Video backend registration and selection

void video_registerBackend(video_backend_s *backend, long order) {
    assert(!video_initialized); // backends cannot be registered after we've picked one to use

    backend_node_s *node = MALLOC(sizeof(backend_node_s));
    assert(node);
    node->next = NULL;
    node->order = order;
    node->backend = backend;

    backend_node_s *p0 = NULL;
    backend_node_s *p = head;
    while (p && (order > p->order)) {
        p0 = p;
        p = p->next;
    }
    if (p0) {
        p0->next = node;
    } else {
        head = node;
    }
    node->next = p;

    currentBackend = head->backend;
}

void video_printBackends(FILE *out) {
    backend_node_s *p = head;
    int count = 0;
    while (p) {
        const char *name = p->backend->name();
        if (count++) {
            fprintf(out, "|");
        }
        fprintf(out, "%s", name);
        p = p->next;
    }
}

static const char *_null_backend_name(void);
void video_chooseBackend(const char *name) {
    if (!name) {
        name = _null_backend_name();
    }

    backend_node_s *p = head;
    while (p) {
        const char *bname = p->backend->name();
        if (strcasecmp(name, bname) == 0) {
            currentBackend = p->backend;
            LOG("Setting current video backend to %s", name);
            break;
        }
        p = p->next;
    }
}

video_animation_s *video_getAnimationDriver(void) {
    return currentBackend->anim;
}

video_backend_s *video_getCurrentBackend(void) {
    return currentBackend;
}

// ----------------------------------------------------------------------------
// NULL video backend ...

static const char *_null_backend_name(void) {
    return "none";
}

static void _null_backend_init(void *context) {
}

static void _null_backend_main_loop(void) {
    while (null_backend_running) {
        sleep(1);
    }
}

static void _null_backend_render(void) {
}

static void _null_backend_shutdown(void) {
    null_backend_running = false;
}

#if INTERFACE_CLASSIC
static void _null_backend_plotChar(const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const uint8_t c) {
}

static void _null_backend_plotLine(const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const char *message) {
}
#endif

static __attribute__((constructor)) void _init_video(void) {
    static video_backend_s null_backend = { 0 };
    null_backend.name      = &_null_backend_name;
    null_backend.init      = &_null_backend_init;
    null_backend.main_loop = &_null_backend_main_loop;
    null_backend.render    = &_null_backend_render;
    null_backend.shutdown  = &_null_backend_shutdown;

#if INTERFACE_CLASSIC
    null_backend.plotChar  = &_null_backend_plotChar;
    null_backend.plotLine  = &_null_backend_plotLine;
#endif

    // Allow headless testing ...
    null_backend.flashText = &display_flashText;
    null_backend.flushScanline = &display_flushScanline;
    null_backend.frameComplete = &display_frameComplete;

    static video_animation_s _null_animations = { 0 };
    null_backend.anim = &_null_animations;
    video_registerBackend(&null_backend, VID_PRIO_NULL);
}

