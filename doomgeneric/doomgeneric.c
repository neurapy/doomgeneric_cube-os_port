#include "doomgeneric.h"

#include "m_argv.h"
#include <stdio.h>
#include <stdlib.h>

pixel_t *DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain(void);

void doomgeneric_Create(int argc, char **argv)
{
	// save arguments
	myargc = argc;
	myargv = argv;

	M_FindResponseFile();

	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(*DG_ScreenBuffer));
	if (DG_ScreenBuffer == NULL) {
		fprintf(stderr, "doomgeneric_Create: failed to allocate screen buffer\n");
		exit(1);
	}

	DG_Init();

	D_DoomMain();
}
