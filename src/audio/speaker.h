/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

#ifndef _SPEAKER_H_
#define _SPEAKER_H_

// leaky detail : max amplitude should be <= SHRT_MAX/2 to not overflow/clip 16bit samples when simple additive mixing
// between speaker and mockingboard
#define SPKR_DATA_INIT (SHRT_MAX>>3) // 0x0FFF

void speaker_init(void);
void speaker_destroy(void);
void speaker_reset(void);
void speaker_flush(void);
void speaker_setVolumeZeroToTen(unsigned long goesToTen);
bool speaker_isActive(void);

/*
 * returns the machine cycles per sample
 *  - for example, emulator running at normal speed: CLK_6502 / 44.1kHz == ~23
 */
double speaker_cyclesPerSample(void);

#endif /* whole file */

