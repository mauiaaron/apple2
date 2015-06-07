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

#ifndef AY8910_H
#define AY8910_H

#define MAX_8910 4

//-------------------------------------
// MAME interface

void _AYWriteReg(int chip, int r, int v);
//void AY8910_write_ym(int chip, int addr, int data);
void AY8910_reset(int chip);
void AY8910Update(int chip, int16_t** buffer, int nNumSamples);

void AY8910_InitAll(int nClock, int nSampleRate);
void AY8910_InitClock(int nClock);
BYTE* AY8910_GetRegsPtr(UINT uChip);

void AY8910UpdateSetCycles();

//-------------------------------------
// FUSE stuff

typedef ULONG libspectrum_dword;
typedef uint8_t libspectrum_byte;
typedef SHORT libspectrum_signed_word;

struct CAY8910;

/* max. number of sub-frame AY port writes allowed;
 * given the number of port writes theoretically possible in a
 * 50th I think this should be plenty.
 */
#define AY_CHANGE_MAX		8000

//class CAY8910
//{
//public:
	void CAY8910_init(struct CAY8910 *_this);

	void sound_ay_init(struct CAY8910 *_this);
	void sound_init(struct CAY8910 *_this, const char *device);
	void sound_ay_write(struct CAY8910 *_this, int reg, int val, libspectrum_dword now);
	void sound_ay_reset(struct CAY8910 *_this);
	void sound_frame(struct CAY8910 *_this);
	BYTE* GetAYRegsPtr(struct CAY8910 *_this);
	void SetCLK(double CLK);

//private:
//	void sound_end(struct CAY8910 *_this, void);
//	void sound_ay_overlay(struct CAY8910 *_this, void);


typedef struct ay_change_tag
{
    libspectrum_dword tstates;
    unsigned short ofs;
    unsigned char reg, val;
} ay_change_tag;

//private:
typedef struct CAY8910
{
	/* foo_subcycles are fixed-point with low 16 bits as fractional part.
	 * The other bits count as the chip does.
	 */
	unsigned int ay_tone_tick[3], ay_tone_high[3], ay_noise_tick;
	unsigned int ay_tone_subcycles, ay_env_subcycles;
	unsigned int ay_env_internal_tick, ay_env_tick;
	unsigned int ay_tick_incr;
	unsigned int ay_tone_period[3], ay_noise_period, ay_env_period;

	//static int beeper_last_subpos[2] = { 0, 0 };

	/* Local copy of the AY registers */
	libspectrum_byte sound_ay_registers[16];

	ay_change_tag ay_change[ AY_CHANGE_MAX ];
	int ay_change_count;

	// statics from sound_ay_overlay()
	int rng;
	int noise_toggle;
	int env_first, env_rev, env_counter;
} CAY8910;

// Vars shared between all AY's
extern double m_fCurrentCLK_AY8910;

#endif
