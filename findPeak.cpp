#include "XOPStandardHeaders.h"			// Include ANSI headers, Mac headers, IgorXOP.h, XOP.h and XOPSupport.h
#include "SXMreader.h"
#include "float.h"

extern "C" int
findpeaks(findPeaksParams* p) {
	if (!WaveType(p->wave) == NT_FP32 || !WaveType(p->wave) == NT_FP64) {
		return NT_INCOMPATIBLE;
	}
	int result;
	char message[300];

	int numDimensionsPtr; // Number of dimensions in the wave
	CountInt dimensionSizes[MAX_DIMENSIONS + 1]; // Array of dimension sizes

	if (result = MDGetWaveDimensions(p->wave, &numDimensionsPtr, dimensionSizes)) {
		return result;
	}
	if (numDimensionsPtr != 3) {
		return GRAFWIN_TOO_BIG;
	}
	
	/*sprintf(message, "%d %d %d\n", dimensionSizes[ROWS], dimensionSizes[COLUMNS], dimensionSizes[LAYERS]);
	XOPNotice(message);*/


	char waveName[MAX_OBJ_NAME + 1];
	int err;
	double offset;
	double delta;

	char name[MAX_OBJ_NAME + 1];
	WaveName(p->wave, name);
	sprintf(message, "%s_[negtive_peaks]", name);
	CountInt dimensionSizes2[MAX_DIMENSIONS + 1];
	MemClear(dimensionSizes2, sizeof(dimensionSizes2));
	dimensionSizes2[ROWS] = dimensionSizes[ROWS];
	dimensionSizes2[COLUMNS] = dimensionSizes[COLUMNS];

	waveHndl test;
	if (result = MDMakeWave(&test, message, NULL, dimensionSizes2, NT_FP64, 1)) {
		return result;
	}

	sprintf(message, "%s_[positive_peaks]", name);
	waveHndl test2;
	if (result = MDMakeWave(&test2, message, NULL, dimensionSizes2, NT_FP64, 1)) {
		return result;
	}

	IndexInt indices[MAX_DIMENSIONS];
	IndexInt mincords[MAX_DIMENSIONS];
	double value[2];
	double out[2];
	out[0] = DBL_MIN;
	for (int x = 0; x < dimensionSizes2[ROWS]; x++) {
		for (int y = 0; y < dimensionSizes2[COLUMNS]; y++) {
		mincords[0] = x;
		mincords[1] = y;
		MDSetNumericWavePointValue(test, mincords, out);
		MDSetNumericWavePointValue(test2, mincords, out);
		}
	}

	double z0;
	double dz;
	MDGetWaveScaling(p->wave, LAYERS, &dz, &z0);
	for (int x = 0; x < dimensionSizes[ROWS]; x++) {
		for (int y = 0; y < dimensionSizes[COLUMNS]; y++) {
			indices[0] = x;
			indices[1] = y;
			mincords[0] = x;
			mincords[1] = y;
			double foundPeakNeg = DBL_MIN;
			int foundPeakZNeg = 0;
			double foundPeakPos = DBL_MIN;
			int foundPeakZPos = 0;
			for (int z = 0; z < dimensionSizes[LAYERS]; z++) {
				indices[2] = z;
				MDGetNumericWavePointValue(p->wave, indices, value);
				//sprintf(message, "Peak:%e PeakZ:%d Value:%e (x = %d, y = %d)\n", foundPeak, foundPeakZ, value[0], x, y);
				//XOPNotice(message);
				if (z0 + dz * z < 0) {
					if (foundPeakNeg < value[0]) {
						foundPeakNeg = value[0];
						foundPeakZNeg = z;
					}
				}
				else
				{
					if (foundPeakPos < value[0]) {
						foundPeakPos = value[0];
						foundPeakZPos = z;
					}
				}
			}



			double offset = fabs(z0 + dz * foundPeakZNeg - z0 + dz * foundPeakZPos) / 2;
			double input[2];
			input[0] = (z0 + dz * foundPeakZNeg - offset) / 100;
			MDSetNumericWavePointValue(test, mincords, input);
			input[0] = (z0 + dz * foundPeakZPos + offset) / 100;
			MDSetNumericWavePointValue(test2, mincords, input);

		}
	}
	
	MDGetWaveScaling(p->wave, ROWS, &delta, &offset);
	if (err = MDSetWaveScaling(test, ROWS, &delta, &offset)) {
		return err;
	}
	if (err = MDSetWaveScaling(test2, ROWS, &delta, &offset)) {
		return err;
	}
	MDGetWaveScaling(p->wave, COLUMNS, &delta, &offset);
	if (err = MDSetWaveScaling(test, COLUMNS, &delta, &offset)) {
		return err;
	}
	if (err = MDSetWaveScaling(test2, COLUMNS, &delta, &offset)) {
		return err;
	}
	p->result = test2;

	return 0;
}