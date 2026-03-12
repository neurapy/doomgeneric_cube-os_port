#include "i_video.h"

#include "config.h"
#include "d_event.h"
#include "d_main.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_system.h"
#include "m_argv.h"
#include "tables.h"
#include "v_video.h"
#include "z_zone.h"
#include <config.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

//#define CMAP256

struct FB_BitField {
	uint32_t offset; /* beginning of bitfield	*/
	uint32_t length; /* length of bitfield		*/
};

struct FB_ScreenInfo {
	uint32_t xres; /* visible resolution		*/
	uint32_t yres;
	uint32_t xres_virtual; /* virtual resolution		*/
	uint32_t yres_virtual;

	uint32_t bits_per_pixel; /* guess what			*/

	/* >1 = FOURCC			*/
	struct FB_BitField red; /* bitfield in s_Fb mem if true color, */
	struct FB_BitField green; /* else only length is significant */
	struct FB_BitField blue;
	struct FB_BitField transp; /* transparency			*/
};

static struct FB_ScreenInfo s_Fb;
int			    fb_scaling = 1;
int			    usemouse   = 0;

#ifdef CMAP256

boolean	     palette_changed;
struct color colors[256];

#else // CMAP256

static struct color colors[256];

#endif // CMAP256

void I_GetEvent(void);

// The screen buffer; this is modified to draw things to the screen

byte *I_VideoBuffer = NULL;

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible;

// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.

float mouse_acceleration = 2.0;
int   mouse_threshold	 = 10;

// Gamma correction level to use

int usegamma = 0;

typedef struct {
	byte r;
	byte g;
	byte b;
} col_t;

static uint16_t rgb565_palette[256];
static uint16_t fb16_palette[256];
static uint32_t fb32_palette[256];

static void I_SetFbFormat32(uint32_t red_offset, uint32_t green_offset, uint32_t blue_offset)
{
	s_Fb.bits_per_pixel = 32;

	s_Fb.blue.length   = 8;
	s_Fb.green.length  = 8;
	s_Fb.red.length	   = 8;
	s_Fb.transp.length = 8;

	s_Fb.red.offset	   = red_offset;
	s_Fb.green.offset  = green_offset;
	s_Fb.blue.offset   = blue_offset;
	s_Fb.transp.offset = 24;
}

static void cmap_to_fb(uint8_t *out, const uint8_t *in, int in_pixels)
{
	if (s_Fb.bits_per_pixel == 16) {
		uint16_t *out16 = (uint16_t *)out;

		if (fb_scaling == 1) {
			for (int i = 0; i < in_pixels; i++)
				out16[i] = fb16_palette[in[i]];
			return;
		}

		if (fb_scaling == 2) {
			for (int i = 0; i < in_pixels; i++) {
				uint16_t pix = fb16_palette[in[i]];
				out16[0]     = pix;
				out16[1]     = pix;
				out16 += 2;
			}
			return;
		}

		for (int i = 0; i < in_pixels; i++) {
			uint16_t pix = fb16_palette[in[i]];
			for (int k = 0; k < fb_scaling; k++)
				*out16++ = pix;
		}
		return;
	}

	if (s_Fb.bits_per_pixel == 32) {
		uint32_t *out32 = (uint32_t *)out;

		if (fb_scaling == 1) {
			for (int i = 0; i < in_pixels; i++)
				out32[i] = fb32_palette[in[i]];
			return;
		}

		if (fb_scaling == 2) {
			for (int i = 0; i < in_pixels; i++) {
				uint32_t pix = fb32_palette[in[i]];
				out32[0]     = pix;
				out32[1]     = pix;
				out32 += 2;
			}
			return;
		}

		for (int i = 0; i < in_pixels; i++) {
			uint32_t pix = fb32_palette[in[i]];
			for (int k = 0; k < fb_scaling; k++)
				*out32++ = pix;
		}
		return;
	}

	// no clue how to convert this
	I_Error("No idea how to convert %lu bpp pixels", (unsigned long)s_Fb.bits_per_pixel);
}

static void I_ConvertFrameBuffer(void)
{
	PERF_USCOPE();
	int	       y;
	int	       x_offset, x_offset_end;
	unsigned int   bytes_per_pixel;
	unsigned char *line_in, *line_out;

	/* Offsets in case FB is bigger than DOOM */
	/* 600 = s_Fb heigt, 200 screenheight */
	/* 2048 =s_Fb width, 320 screenwidth */
	bytes_per_pixel = s_Fb.bits_per_pixel / 8;
	x_offset	= (((s_Fb.xres - (SCREENWIDTH * fb_scaling)) * bytes_per_pixel)) / 2;
	x_offset_end	= ((s_Fb.xres - (SCREENWIDTH * fb_scaling)) * bytes_per_pixel) - x_offset;

	/* DRAW SCREEN */
	line_in	 = (unsigned char *)I_VideoBuffer;
	line_out = (unsigned char *)DG_ScreenBuffer;

	if (s_Fb.bits_per_pixel != 8 && fb_scaling == 2 && x_offset == 0 && x_offset_end == 0) {
		size_t row_bytes = (size_t)s_Fb.xres * bytes_per_pixel;

		for (y = 0; y < SCREENHEIGHT; y++) {
			cmap_to_fb(line_out, line_in, SCREENWIDTH);
			memcpy(line_out + row_bytes, line_out, row_bytes);
			line_out += row_bytes * 2u;
			line_in += SCREENWIDTH;
		}
		return;
	}

	y = SCREENHEIGHT;

	while (y--) {
		int i;
		for (i = 0; i < fb_scaling; i++) {
			line_out += x_offset;
#ifdef CMAP256
			if (fb_scaling == 1) {
				memcpy(line_out, line_in, SCREENWIDTH);
			} else {
				int j;

				for (j = 0; j < SCREENWIDTH; j++) {
					int k;
					for (k = 0; k < fb_scaling; k++) {
						line_out[j * fb_scaling + k] = line_in[j];
					}
				}
			}
#else
			cmap_to_fb((void *)line_out, (void *)line_in, SCREENWIDTH);
#endif
			line_out += (SCREENWIDTH * fb_scaling * bytes_per_pixel) + x_offset_end;
		}
		line_in += SCREENWIDTH;
	}
}

void I_InitGraphics(void)
{
	int   i, gfxmodeparm;
	char *mode;
	uint32_t native_depth	     = 0;
	uint32_t native_pixel_order = 0;
	int	  have_native_format =
		DG_GetNativePixelFormat(&native_depth, &native_pixel_order);

	memset(&s_Fb, 0, sizeof(struct FB_ScreenInfo));
	s_Fb.xres	  = DOOMGENERIC_RESX;
	s_Fb.yres	  = DOOMGENERIC_RESY;
	s_Fb.xres_virtual = s_Fb.xres;
	s_Fb.yres_virtual = s_Fb.yres;

#ifdef CMAP256

	s_Fb.bits_per_pixel = 8;

#else // CMAP256

	gfxmodeparm = M_CheckParmWithArgs("-gfxmode", 1);

	if (gfxmodeparm) {
		mode = myargv[gfxmodeparm + 1];
	} else if (have_native_format && native_depth == 32) {
		mode = native_pixel_order == 1u ? "rgba8888" : "bgra8888";
	} else {
		mode = "rgba8888";
	}

	if (strcmp(mode, "rgba8888") == 0) {
		I_SetFbFormat32(16, 8, 0);
	} else if (strcmp(mode, "bgra8888") == 0) {
		I_SetFbFormat32(0, 8, 16);
	} else if (strcmp(mode, "rgb565") == 0) {
		s_Fb.bits_per_pixel = 16;

		s_Fb.blue.length   = 5;
		s_Fb.green.length  = 6;
		s_Fb.red.length	   = 5;
		s_Fb.transp.length = 0;

		s_Fb.blue.offset   = 0;
		s_Fb.green.offset  = 5;
		s_Fb.red.offset	   = 11;
		s_Fb.transp.offset = 16;
	} else
		I_Error("Unknown gfxmode value: %s\n", mode);

#endif // CMAP256

	printf("I_InitGraphics: framebuffer: x_res: %lu, y_res: %lu, x_virtual: %lu, "
	       "y_virtual: %lu, bpp: %lu\n",
	       (unsigned long)s_Fb.xres, (unsigned long)s_Fb.yres, (unsigned long)s_Fb.xres_virtual,
	       (unsigned long)s_Fb.yres_virtual, (unsigned long)s_Fb.bits_per_pixel);

	printf("I_InitGraphics: framebuffer: RGBA: %lu%lu%lu%lu, red_off: %lu, green_off: %lu, "
	       "blue_off: %lu, transp_off: %lu\n",
	       (unsigned long)s_Fb.red.length, (unsigned long)s_Fb.green.length,
	       (unsigned long)s_Fb.blue.length, (unsigned long)s_Fb.transp.length,
	       (unsigned long)s_Fb.red.offset, (unsigned long)s_Fb.green.offset,
	       (unsigned long)s_Fb.blue.offset, (unsigned long)s_Fb.transp.offset);

	printf("I_InitGraphics: DOOM screen size: w x h: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);

	i = M_CheckParmWithArgs("-scaling", 1);
	if (i > 0) {
		i	   = atoi(myargv[i + 1]);
		fb_scaling = i;
		printf("I_InitGraphics: Scaling factor: %d\n", fb_scaling);
	} else {
		fb_scaling = s_Fb.xres / SCREENWIDTH;
		if (s_Fb.yres / SCREENHEIGHT < fb_scaling)
			fb_scaling = s_Fb.yres / SCREENHEIGHT;
		printf("I_InitGraphics: Auto-scaling factor: %d\n", fb_scaling);
	}

	/* Allocate screen to draw to */
	I_VideoBuffer = (byte *)Z_Malloc(SCREENWIDTH * SCREENHEIGHT, PU_STATIC,
					 NULL); // For DOOM to draw on

	screenvisible = true;

	extern void I_InitInput(void);
	I_InitInput();
}

void I_ShutdownGraphics(void)
{
	Z_Free(I_VideoBuffer);
}

void I_StartFrame(void)
{
}

void I_StartTic(void)
{
	I_GetEvent();
}

void I_UpdateNoBlit(void)
{
}

//
// I_FinishUpdate
//

void I_FinishUpdate(void)
{
	I_ConvertFrameBuffer();
	DG_DrawFrame();
}

//
// I_ReadScreen
//
void I_ReadScreen(byte *scr)
{
	memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

//
// I_SetPalette
//
#define GFX_RGB565(r, g, b) \
	((((r & 0xF8) >> 3) << 11) | (((g & 0xFC) >> 2) << 5) | ((b & 0xF8) >> 3))
#define GFX_RGB565_R(color) ((0xF800 & color) >> 11)
#define GFX_RGB565_G(color) ((0x07E0 & color) >> 5)
#define GFX_RGB565_B(color) (0x001F & color)

void I_SetPalette(byte *palette)
{
	int i;
	//col_t* c;

	//for (i = 0; i < 256; i++)
	//{
	//	c = (col_t*)palette;

	//	rgb565_palette[i] = GFX_RGB565(gammatable[usegamma][c->r],
	//								   gammatable[usegamma][c->g],
	//								   gammatable[usegamma][c->b]);

	//	palette += 3;
	//}

	/* performance boost:
     * map to the right pixel format over here! */

	for (i = 0; i < 256; ++i) {
		colors[i].a = 0;
		colors[i].r = gammatable[usegamma][*palette++];
		colors[i].g = gammatable[usegamma][*palette++];
		colors[i].b = gammatable[usegamma][*palette++];

		rgb565_palette[i] = GFX_RGB565(colors[i].r, colors[i].g, colors[i].b);
		fb16_palette[i]	  = rgb565_palette[i];
		fb32_palette[i]	  = ((uint32_t)colors[i].r << s_Fb.red.offset) |
				  ((uint32_t)colors[i].g << s_Fb.green.offset) |
				  ((uint32_t)colors[i].b << s_Fb.blue.offset);

#ifdef SYS_BIG_ENDIAN
		fb16_palette[i] = swapeLE16(fb16_palette[i]);
		fb32_palette[i] = swapLE32(fb32_palette[i]);
#endif
	}

#ifdef CMAP256

	palette_changed = true;

#endif // CMAP256
}

// Given an RGB value, find the closest matching palette index.

int I_GetPaletteIndex(int r, int g, int b)
{
	int   best, best_diff, diff;
	int   i;
	col_t color;

	best	  = 0;
	best_diff = INT_MAX;

	for (i = 0; i < 256; ++i) {
		color.r = GFX_RGB565_R(rgb565_palette[i]);
		color.g = GFX_RGB565_G(rgb565_palette[i]);
		color.b = GFX_RGB565_B(rgb565_palette[i]);

		diff = (r - color.r) * (r - color.r) + (g - color.g) * (g - color.g) +
		       (b - color.b) * (b - color.b);

		if (diff < best_diff) {
			best	  = i;
			best_diff = diff;
		}

		if (diff == 0) {
			break;
		}
	}

	return best;
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

void I_SetWindowTitle(char *title)
{
	DG_SetWindowTitle(title);
}

void I_GraphicsCheckCommandLine(void)
{
}

void I_SetGrabMouseCallback(grabmouse_callback_t func)
{
}

void I_EnableLoadingDisk(void)
{
}

void I_BindVideoVariables(void)
{
}

void I_DisplayFPSDots(boolean dots_on)
{
}

void I_CheckIsScreensaver(void)
{
}
