/*******************************************************************
lower level fft stuff including routines called in fftext.c and fft2d.c
*******************************************************************/
/* inline declarations modified by RBD for C99 compiler */


#include <stdint.h>
#include "fftlib.h"
#include <math.h>
#define	MCACHE	(11-(sizeof(float)/8))	// fft's with M bigger than this bust primary cache
#ifdef WIN32
#define inline __inline
#endif

// some math constants to 40 decimal places
#define MYPI		3.141592653589793238462643383279502884197F	// pi
#define MYROOT2 	1.414213562373095048801688724209698078569F	// sqrt(2)
#define MYCOSPID8	0.9238795325112867561281831893967882868224F	// cos(pi/8)
#define MYSINPID8	0.3826834323650897717284599840303988667613F	// sin(pi/8)


/*************************************************
routines to initialize tables used by fft routines
**************************************************/

void fftCosInit(long M, float *Utbl){
/* Compute Utbl, the cosine table for ffts	*/
/* of size (pow(2,M)/4 +1)	*/
/* INPUTS */
/* M = log2 of fft size	*/
/* OUTPUTS */
/* *Utbl = cosine table	*/
unsigned long fftN = POW2(M);
unsigned long i1;
	Utbl[0] = 1.0;
	for (i1 = 1; i1 < fftN/4; i1++)
		Utbl[i1] = (float) cos( (2.0 * MYPI * i1) / fftN );
	Utbl[fftN/4] = 0.0;
}

void fftBRInit(long M, short *BRLow){
/* Compute BRLow, the bit reversed table for ffts	*/
/* of size pow(2,M/2 -1)	*/
/* INPUTS */
/* M = log2 of fft size	*/
/* OUTPUTS */
/* *BRLow = bit reversed counter table	*/
long	Mroot_1 = M / 2 - 1;
long	Nroot_1 = POW2(Mroot_1);
long i1;
long bitsum;
long bitmask;
long bit;
for (i1 = 0; i1 < Nroot_1; i1++){
	bitsum = 0;
	bitmask = 1;
	for (bit=1; bit <= Mroot_1; bitmask<<=1, bit++)
		if (i1 & bitmask)
			bitsum = bitsum + (Nroot_1 >> bit);
	BRLow[i1] = (short) bitsum;
};
}

/************************************************
parts of ffts1
****************