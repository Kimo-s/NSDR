#include "stdio.h"
#include "stdlib.h"


/*
	SXMreader.h -- equates for SXMreader XOP
*/

/* SXMreader custom error codes */
#define OLD_IGOR 1 + FIRST_XOP_ERR

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct readSXMFileParams {
	Handle waveName;				// Set the name of the wave variable
	Handle fp;						// File refernce handle
	waveHndl result;				// wave result
};
typedef struct readSXMFileParams readSXMFileParams;
#pragma pack()		// Reset structure alignment to default.

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct DynesFitParams {
	double temperature;
	double integralLimit;
	waveHndl wave;					// wave handle
	waveHndl result;				// wave result
};
typedef struct DynesFitParams dynesFitParams;
#pragma pack()		// Reset structure alignment to default.

#pragma pack(2)		// All structures passed to Igor are two-byte aligned.
struct findPeaksParams {
	waveHndl wave;
	waveHndl result;				// wave result
};
typedef struct findPeaksParams findPeaksParams;
#pragma pack()		// Reset structure alignment to default.

#pragma pack(2)		
struct removebackgroundParams {
	waveHndl offsetYwave;
	waveHndl offsetXwave;
	waveHndl modYwave;
	waveHndl modXwave;
	waveHndl result;				
};
typedef struct removebackgroundParams removebackgroundParams;
#pragma pack()		

/* Prototypes */
HOST_IMPORT int XOPMain(IORecHandle ioRecHandle);
extern "C" int dynesFit(dynesFitParams * p);
extern "C" int findpeaks(findPeaksParams * p);
extern "C" int dynesFitGrid(dynesFitParams * p);
extern "C" int readDATFile(readSXMFileParams * p);
extern "C" int removebackground(removebackgroundParams * p);
extern "C" int read3ds(readSXMFileParams * p);
float ReverseFloat(const float inFloat);

#define SXM 1
#define DAT 2
#define FILE_3DS 3

#define WAVE_NOT_GRID FIRST_XOP_ERR + 1
#define MISMATCHING_SIZES FIRST_XOP_ERR + 4


struct data {
	size_t n;
	double* t;
	double* y;
};

struct functionInput {
	double Gamma;
	double Del;
	double V_0;
	double xdata;
};
