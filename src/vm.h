/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#ifndef _VM_H_
#define _VM_H_

/* ----------------------------------------------------------------------------
        Apple //e 64K VM

        (Two byte addresses are represented with least significant
         byte first, e.g. address FA59 is represented as 59 FA)

        This is not meant to be definitive.
        Verify against _Understanding the Apple IIe_ by Jim Sather

        Address         Description
        -----------     -----------
        0000 - 00FF     Zero page RAM
        0100 - 01FF     Stack
        0200 - 03EF     RAM
        03F0 - 03F1     Address for BRK instruction (normally 59 FA = FA59)
        03F2 - 03F3     Soft entry vector (normally BF 9D = 9DBF)
        03F4            Power-up byte
        03F5 - 03F7     Jump instruction to the subroutine which handles
                        Applesoft II &-commands (normally 4C 58 FF = JMP FF58)
        03F8 - 03FA     Jump instruction to the subroutine which handles User
                        CTRL-Y commands (normally 4C 65 FF = JMP FF65)
        03FB - 03FD     Jump instruction to the subroutine which handles
                        Non-Maskable Interrupts (NMI) (normally 4C 65 FF = JMP FF65)
        03FE - 03FF     Address of the subroutine which handles Interrupt
                        ReQuests (IRQ) (normally 65 FF = FF65)
        0400 - 07FF     Basically primary video text page
        0478 - 047F     I/O Scratchpad RAM Addresses for Slot 0 - 7
        04F8 - 04FF                  - " " -
        0578 - 057F                  - " " -
        05F8 - 05FF                  - " " -
        0678 - 067F                  - " " -
        06F8 - 06FF                  - " " -
        0778 - 077F                  - " " -
        07F8 - 07FF                  - " " -
        05F8            Holds the slot number of the disk controller card from
                        which DOS was booted. ??? VERIFY...
        07F8            Holds the slot number (CX, X = Slot #) of the slot that
                        is currently active. ??? VERIFY...
        0800 - 0BFF     Secondary video text page
        0C00 - 1FFF     Plain RAM
        2000 - 3FFF     Primary video hires page (RAM)
        4000 - 5FFF     Secondary video hires page (RAM)
        6000 - BFFF     Plain RAM (Normally the OS is loaded into ~9C00 - BFFF)
        C000 - C00F     Keyboard data (C00X contains the character ASCII code of
                        the pressed key. The value is available at any C00X address)
        C010 - C01F     Clear Keyboard strobe
        C020 - C02F     Cassette output toggle
        C030 - C03F     Speaker toggle
        C040 - C04F     Utility strobe
        C050            Set graphics mode
        C051            Set text mode
        C052            Set all text or graphics
        C053            Mix text and graphics
        C054            Display primary page
        C055            Display secondary page
        C056            Display low-res graphics
        C057            Display hi-res graphics
        C058 - C05F     Annunciator outputs
        C060            Cassette input
        C061 - C063     Pushbutton inputs (button 1, 2 and 3)
        C064 - C067     Game controller inputs
        C068 - C06F     Same as C060 - C067
        C070 - C07F     Game controller strobe
        C080 - C08F     Slot 0 I/O space (usually a language card)
        C080            Reset language card
                            * Read enabled
                            * Write disabled
                            * Read from language card
        C081            --- First access
                            * Read mode disabled
                            * Read from ROM
                        --- On second+ access
                            * Write mode enabled
                            * Write to language card
        C082            --- (Disable language card)
                            * Read mode disabled
                            * Write mode disabled
                            * Read from ROM
        C083            --- First access
                            * Read mode enabled
                            * Read from language card
                        --- On second+ access
                            * Write mode enabled
                            * Write to language card
        C088 - C08B     Same as C080 - C083, but switch to second bank, i.e.
                        map addresses D000-DFFF to other 4K area.
        C100 - C1FF     Slot 1 PROM
        C200 - C2FF     Slot 2 PROM
        C300 - C3FF     Slot 3 PROM
        C400 - C4FF     Slot 4 PROM
        C500 - C5FF     Slot 5 PROM
        C600 - C6FF     Slot 6 PROM
        C700 - C7FF     Slot 7 PROM
        C800 - CFFF     Expansion ROM (for peripheral cards)
        CFFF            Disable access to expansion ROM for
                        ALL peripheral cards.
        D000 - DFFF     ROM or 4K RAM if language card is enabled. However,
                        there are TWO 4K banks that can be mapped onto addresses
                        D000 - DFFF.  See C088 - C08B.
        E000 - FFFF     ROM or 8K RAM if language card is enabled.
---------------------------------------------------------------------------- */

#if !defined(__ASSEMBLER__)

// 128k bank-switched main memory
extern uint8_t apple_ii_64k[2][65536];

// language card memory and settings
extern uint8_t language_card[2][8192];
extern uint8_t language_banks[2][8192];

void vm_initialize(void);

void vm_reinitializeAudio(void);

extern bool vm_saveState(StateHelper_s *helper);
extern bool vm_loadState(StateHelper_s *helper);

#if VM_TRACING
void vm_trace_begin(const char *vm_file);
void vm_trace_end(void);
void vm_trace_toggle(const char *vm_file);
bool vm_trace_is_ignored(uint16_t ea);
#endif

void vm_printSoftwitches(FILE *fp, bool output_mem, bool output_pseudo);
#endif // !defined(__ASSEMBLER__)

// softswitch flag bits

#define         SS_TEXT         0x00000001
#define         SS_MIXED        0x00000002
#define         SS_HIRES        0x00000004
#define         SS_PAGE2        0x00000008
#define         SS_BANK2        0x00000010
#define         SS_LCRAM        0x00000020
#define         SS_LCSEC        0x00000040 // Pseudo-softswitch : enabled if 2+ reads have occurred
#define         SS_LCWRT        0x00000080 // Pseudo-softswitch : LC write enable
#define         SS_80STORE      0x00000100
#define         SS_80COL        0x00000200
#define         SS_RAMRD        0x00000400
#define         SS_RAMWRT       0x00000800
#define         SS_ALTZP        0x00001000
#define         SS_DHIRES       0x00002000
#define         SS_IOUDIS       0x00004000
#define         SS_CXROM        0x00008000
#define         SS_C3ROM        0x00010000
#define         SS_ALTCHAR      0x00020000

// Pseudo soft switches.  These are actually functions of other SSes, but are tiresome to calculate as needed.
#define         SS_SCREEN       0x00040000 /* PAGE2 && !80STORE */
#define         SS_TEXTRD       0x00080000 /* (PAGE2 && 80STORE) ||
                                              (RAMRD && !80STORE) */
#define         SS_TEXTWRT      0x00100000 /* (PAGE2 && 80STORE) ||
                                              (RAMWRT && !80STORE) */
#define         SS_HGRRD        0x00200000 /* (PAGE2 && 80STORE && HIRES) ||
                                              (RAMRD && !(80STORE && HIRES) */
#define         SS_HGRWRT       0x00400000 /* (PAGE2 && 80STORE && HIRES) ||
                                              (RAMWRT && !(80STORE && HIRES)) */

#endif // whole file
