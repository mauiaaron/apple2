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

#define SPKR_SAMPLE_RATE 22050
#define SPKR_DATA_INIT 0x4000

void speaker_init(void);
void speaker_destroy(void);
void speaker_reset(void);
void speaker_flush(void);
void speaker_set_volume(int16_t amplitude);
bool speaker_is_active(void);

/*
 * returns the machine cycles per sample
 *  - for emulator running at normal speed: CLK_6502 / SPKR_SAMPLE_RATE == ~46
 */
double speaker_cycles_per_sample(void);

#endif /* whole file */

