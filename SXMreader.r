#include "XOPStandardHeaders.r"

resource 'vers' (1) {						/* XOP version info */
	0x01, 0x00, final, 0x00, 0,				/* version bytes and country integer */
	"1.00",
	"1.00, Copyright 1993-2018 WaveMetrics, Inc., all rights reserved."
};

resource 'vers' (2) {						/* Igor version info */
	0x08, 0x00, release, 0x00, 0,			/* version bytes and country integer */
	"8.00",
	"(for Igor 8.00 or later)"
};

resource 'STR#' (1100) {					/* custom error messages */
	{
		/* [1] */
		"SXMreader requires Igor 8.00 or later",
	}
};

/* no menu item */

resource 'XOPI' (1100) {
	XOP_VERSION,							// XOP protocol version.
	DEV_SYS_CODE,							// Code for development system used to make XOP
	XOP_FEATURE_FLAGS,						// Tells Igor about XOP features
	XOPI_RESERVED,							// Reserved - must be zero.
	XOP_TOOLKIT_VERSION,					// XOP Toolkit version.
};

resource 'XOPF' (1100) {
	{
		"SXMreaderAdd",						/* function name */
		F_UTIL | F_EXTERNAL,				/* function category */
		NT_FP64,							/* return value type */			
		{
			NT_FP64,						/* parameter types */
			NT_FP64,
		},
		
		"SXMreaderDiv",						/* function name */
		F_UTIL | F_EXTERNAL,				/* function category */
		NT_FP64,							/* return value type */			
		{
			NT_FP64,						/* parameter types */
			NT_FP64,
		},

		"SXMreaderComplexConjugate",			/* function name */
		F_CMPLX | F_EXTERNAL,				/* function category */
		NT_FP64 | NT_CMPLX,					/* return value type = double precision complex */			
		{
			NT_FP64 | NT_CMPLX,				/* double precision complex parameter */			
		},
	}
};
