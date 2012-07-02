
// AY Chip related functions
//
// (c) 2012, TS-Labs inc.
// All rights fucked out
//
// Sajnálom, de kizökkentetted a felséges urat


#include <stdio.h>
#include <string.h>
#include "types.h"
#include "ay-arm.h"
#include "ay.h"


// --- Variables declaration -----
		AY_Regs AY[AY_CHIPS_MAX];	// Registers for virtual AY chips
		U8		AYChipNum = AY_CHIPS_DEF;
		AYCtrl	AYControl;
extern	DAC_t	DACOut;
		U8		div = 0;


// --- Arrays -----
	U8	env_tab[256] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
	};

	// U8	env_start[16] = {
	// };

// --- Functions -----

// Initialize AY Chips
void AY_Init() {
	int i;
	// Registers nulling
	for (i=0; i<AY_CHIPS_MAX; i++) {
		memset(&AY[i], 0, sizeof(AY_Regs));
	}
}


// Iterate next tick for all AY-chips
// Called at F_osc/16 IRQ
void AY_Tick() {
	int i;
	DAC_Sum Out, Sum;
	Sum.l = Sum.r = 0;

	// here write data to DACs
	
	// YM chip (called at Fosc/8)
	if (AYControl.i.type) {
		// In this tick only call envelope
		if (div) {
			for (i=0; i<AYChipNum; i++) {
				AY_Tick_env(i, 31, 1);
				Out = AY_DAC_Sum(i);
				Sum.l += Out.l;
				Sum.r += Out.r;
			}
		}
		
		// In this tick have to call also sound generators
		else {
			for (i=0; i<AYChipNum; i++) {
				AY_Tick_tone(i);
				AY_Tick_noise(i);
				AY_Tick_env(i, 31, 1);
				Out = AY_DAC_Sum(i);
				Sum.l += Out.l;
				Sum.r += Out.r;
			}
		}
		
		div = ~div;
	}
	
	// AY chip (called at Fosc/16)
	else {
		for (i=0; i<AYChipNum; i++) {
			AY_Tick_tone(i);
			AY_Tick_noise(i);
			AY_Tick_env(i, 30, 2);
			Out = AY_DAC_Sum(i);
			Sum.l += Out.l;
			Sum.r += Out.r;
		}
	}
	
	DACOut.l = Sum.l;
	DACOut.r = Sum.r;

}


// Iterate next tick of AY Tone Generators
__inline void AY_Tick_tone(int n) {
	if (AY[n].ctr_tn0 < AY[n].TF0.h)
		AY[n].ctr_tn0++;
	else {
		AY[n].ph_tn0 ^= 0x01;
		AY[n].ctr_tn0 = 1;
	}

	if (AY[n].ctr_tn1 < AY[n].TF1.h)
		AY[n].ctr_tn1++;
	else {
		AY[n].ph_tn1 ^= 0x01;
		AY[n].ctr_tn1 = 1;
	}

	if (AY[n].ctr_tn2 < AY[n].TF2.h)
		AY[n].ctr_tn2++;
	else {
		AY[n].ph_tn2 ^= 0x01;
		AY[n].ctr_tn2 = 1;
	}
}


// Iterate next tick of AY Noise Generator
__inline void AY_Tick_noise(int n) {
	if (AY[n].ctr_ns < AY[n].NF)
		AY[n].ctr_ns++;
	else {
		AY[n].sd_ns = ((AY[n].sd_ns << 1) | 1) ^ (((AY[n].sd_ns >> 16) ^ (AY[n].sd_ns >> 13)) & 1);	// bit16 is ised as 
		// ^^^ This was spizded from 'ZXTUNES' project by Vitamin/CAIG
		AY[n].ctr_ns = 1;		// !!! check this on a real chip !!!
	}
}


// Iterate next tick of AY Volume Envelope Generator
__inline void AY_Tick_env(int n, int tab_lim, int tab_step) {
	
	// Process envelope tick
	if (!AY[n].env_rld) {
		if (AY[n].ctr_ev < AY[n].EP.h) {
			AY[n].ctr_ev++;
			return;
		}
		else {
			AY[n].ctr_ev = 1;
			AY_Env_Proc(n, tab_lim, tab_step);
			return;
		}
	}
	
	// Reload envelope
	else {
		AY[n].env_rld = 0;
		AY[n].env_ph = (AY[n].EC & 0x04) ? 1 : 0;		// 'Attack'
		AY[n].env_ctr = AY[n].env_ph ? 0 : tab_lim;
		AY[n].env_st = 1;
		// AY[n].env_vol = env_tab[AY[n].env_ctr + (AY[n].env_sel * 32)];
		return;
	}
}

__inline void AY_Env_Proc(int n, int tab_lim, int tab_step) {
	if (AY[n].env_st) {
	
		AY[n].env_ctr = AY[n].env_ph ? (AY[n].env_ctr + tab_step) : (AY[n].env_ctr - tab_step); 
		
		if (AY[n].env_ctr > 31) {
		
			if (!(AY[n].EC & 0x08)) {	// 'Continue'
				AY[n].env_ctr = 0;
				AY[n].env_st = 0;
			}
			
			else {
				if (AY[n].EC & 0x02)		// 'Alternate'
					AY[n].env_ph ^= 0x01;
				
				if (AY[n].EC & 0x01) {		// 'Hold'
					AY[n].env_st = 0;
					}
				
				AY[n].env_ctr = (AY[n].env_ph ^ AY[n].env_st) ? tab_lim : 0;
			}
		}
	}
}


// DAC output
__inline DAC_Sum AY_DAC_Sum(int n) {
	U8 out0;
	U8 out1;
	U8 out2;
	U8 ns;
	U8 env_vol;
	DAC_Sum Out;
	
	ns = 0 != (AY[n].sd_ns & 0x10000);
	env_vol = env_tab[AY[n].env_ctr];
	
	// outputs per each PSG channel, unsigned, 5 bit
	out0 = ((AY[n].ph_tn0 & AY[n].MX.i.t0) ^ (ns & AY[n].MX.i.n0) ^ 0x01) * ((AY[n].V0 & 0x10) ? env_vol : (AY[n].V0 << 1));
	out1 = ((AY[n].ph_tn1 & AY[n].MX.i.t1) ^ (ns & AY[n].MX.i.n1) ^ 0x01) * ((AY[n].V1 & 0x10) ? env_vol : (AY[n].V1 << 1));
	out2 = ((AY[n].ph_tn2 & AY[n].MX.i.t2) ^ (ns & AY[n].MX.i.n2) ^ 0x01) * ((AY[n].V2 & 0x10) ? env_vol : (AY[n].V2 << 1));
	
	// outputs per each audio channel, unsigned, sum of 3 values, each 11 bits (5 value + 6 volume)
	Out.l = out0 * AY[n].vol_0l + out1 * AY[n].vol_1l + out2 * AY[n].vol_2l;
	Out.r = out0 * AY[n].vol_0r + out1 * AY[n].vol_1r + out2 * AY[n].vol_2r;
	
	return Out;
}
