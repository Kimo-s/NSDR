#include "XOPStandardHeaders.h"			// Include ANSI headers, Mac headers, IgorXOP.h, XOP.h and XOPSupport.h
#include "SXMreader.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"



extern "C" int
removebackground(removebackgroundParams * p) {
	int result;

	int modXdimnum; // Number of dimensions in the wave
	CountInt modXdimsizes[MAX_DIMENSIONS + 1]; // Array of dimension sizes
	if (result = MDGetWaveDimensions(p->modXwave, &modXdimnum, modXdimsizes)) {
		return result;
	}

	int modYdimnum; // Number of dimensions in the wave
	CountInt modYdimsizes[MAX_DIMENSIONS + 1]; // Array of dimension sizes
	if (result = MDGetWaveDimensions(p->modYwave, &modYdimnum, modYdimsizes)) {
		return result;
	}

	int offsetmodXdimnum; // Number of dimensions in the wave
	CountInt offsetmodXdimsizes[MAX_DIMENSIONS + 1]; // Array of dimension sizes
	if (result = MDGetWaveDimensions(p->offsetXwave, &offsetmodXdimnum, offsetmodXdimsizes)) {
		return result;
	}

	int offsetmodYdimnum; // Number of dimensions in the wave
	CountInt offsetmodYdimsizes[MAX_DIMENSIONS + 1]; // Array of dimension sizes
	if (result = MDGetWaveDimensions(p->offsetYwave, &offsetmodYdimnum, offsetmodYdimsizes)) {
		return result;
	}


	// Must be same number of points
	if (modYdimsizes[0] != modXdimsizes[0] && offsetmodYdimsizes[0] != offsetmodXdimsizes[0] && offsetmodYdimsizes[0] != modYdimsizes[0]) {
		return MISMATCHING_SIZES;
	}
	int n = modYdimsizes[0];  // Number of points
	int m = modYdimsizes[1];


	IndexInt indices[MAX_DIMENSIONS];
	double value1[2];
	double value2[2];
	double offsetavg = 0;
	for (int i = 0; i < n; i++) {
		indices[0] = i;
		MDGetNumericWavePointValue(p->offsetXwave, indices, value1);
		MDGetNumericWavePointValue(p->offsetYwave, indices, value2);
		offsetavg = offsetavg + sqrt(value1[0] * value1[0] + value2[0] * value2[0]);
	}
	offsetavg = offsetavg / n;


	int err;
	double offset;
	double delta;
	char waveName[MAX_OBJ_NAME + 1];
	char name[MAX_OBJ_NAME + 1];
	WaveName(p->modXwave, name);
	sprintf(waveName, "%s_[RemovedBackground]", name);
	if (result = MDMakeWave(&p->result, waveName, NULL, modYdimsizes, NT_FP64, 1)) {
		return result;
	}

	if (m == 0) {
		m = 1;
		modYdimsizes[LAYERS] = 1;
	}

	double valueX[2];
	double valueY[2];
	for (int x = 0; x < n; x++) {
		for (int y = 0; y < m; y++) {
			indices[0] = x;
			indices[1] = y;
			for (int z = 0; z < modYdimsizes[LAYERS]; z++) {
				indices[2] = z;
				MDGetNumericWavePointValue(p->modXwave, indices, valueX);
				MDGetNumericWavePointValue(p->modYwave, indices, valueY);
				valueX[0] = sqrt((valueX[0] * valueX[0]) + (valueY[0] * valueY[0])) - offsetavg;
				MDSetNumericWavePointValue(p->result, indices, valueX);
			}

		}
	}


	// Copy the scalling to the result wave
	MDGetWaveScaling(p->modYwave, ROWS, &delta, &offset);
	if (err = MDSetWaveScaling(p->result, ROWS, &delta, &offset)) {
		return err;
	}
	MDGetWaveScaling(p->modYwave, COLUMNS, &delta, &offset);
	if (err = MDSetWaveScaling(p->result, COLUMNS, &delta, &offset)) {
		return err;
	}
	MDGetWaveScaling(p->modYwave, LAYERS, &delta, &offset);
	if (err = MDSetWaveScaling(p->result, LAYERS, &delta, &offset)) {
		return err;
	}

	return 0;
}