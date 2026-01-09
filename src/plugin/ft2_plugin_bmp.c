/**
 * @file ft2_plugin_bmp.c
 * @brief BMP asset loader for the FT2 plugin.
 * 
 * This is a port of ft2_bmp.c that works without SDL dependencies.
 * It decodes RLE-compressed 4-bit BMPs to usable pixel data.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_video.h"

/* Include all BMP data from gfxdata */
#include "gfxdata/ft2_bmp_fonts.c"
#include "gfxdata/ft2_bmp_gui.c"
#include "gfxdata/ft2_bmp_logo.c"
#include "gfxdata/ft2_bmp_instr.c"
#include "gfxdata/ft2_bmp_looppins.c"
#include "gfxdata/ft2_bmp_scopes.c"
#include "gfxdata/ft2_bmp_midi.c"
#include "gfxdata/ft2_bmp_mouse.c"
#include "gfxdata/ft2_bmp_nibbles.c"

/* BMP biCompression field values */
enum
{
	COMP_RGB = 0,   /* Uncompressed (not supported by these loaders) */
	COMP_RLE8 = 1,  /* 8-bit run-length encoding */
	COMP_RLE4 = 2   /* 4-bit run-length encoding */
};

/* BMP header structure */
typedef struct bmpHeader_t
{
	uint32_t bfSize;
	uint16_t bfReserved1;
	uint16_t bfReserved2;
	uint32_t bfOffBits;
	uint32_t biSize;
	int32_t biWidth;
	int32_t biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	int32_t biXPelsPerMeter;
	int32_t biYPelsPerMeter;
	int32_t biClrUsed;
	int32_t biClrImportant;
} bmpHeader_t;

/*
 * BMP colors mapped to FT2 palette indices. The original FT2 bitmaps use
 * specific RGB values that we translate to logical palette entries
 * (PAL_BCKGRND, PAL_FORGRND, etc.) for theme support.
 */
#define NUM_CUSTOM_PALS 17
static const uint32_t bmpCustomPalette[NUM_CUSTOM_PALS] =
{
	0x000000, 0x5397FF, 0x000067, 0x4BFFFF, 0xAB7787,
	0xFFFFFF, 0x7F7F7F, 0xABCDEF, 0x733747, 0xF7CBDB,
	0x434343, 0xD3D3D3, 0xFFFF00, 0xC0FFEE, 0xC0FFEE,
	0xC0FFEE, 0xFF0000
};

/* Map a 32-bit RGB color to an FT2 palette index (0-16), or PAL_TRANSPR if unknown. */
static int8_t getFT2PalNrFromPixel(uint32_t pixel32)
{
	for (int32_t i = 0; i < NUM_CUSTOM_PALS; i++)
	{
		if (pixel32 == bmpCustomPalette[i])
			return (int8_t)i;
	}
	return PAL_TRANSPR;
}

/*
 * Decode RLE-compressed BMP to 32-bit ARGB. Used for full-color assets
 * like the about screen logo. Handles RLE4 and RLE8 compression.
 */
static uint32_t *loadBMPTo32Bit(const uint8_t *src)
{
	int32_t len, byte, palIdx;
	uint32_t *tmp32, color, color2, pal[256];

	const bmpHeader_t *hdr = (const bmpHeader_t *)&src[2];
	const uint8_t *pData = &src[hdr->bfOffBits];
	const int32_t colorsInBitmap = 1 << hdr->biBitCount;

	if (hdr->biCompression == COMP_RGB || hdr->biClrUsed > 256 || colorsInBitmap > 256)
		return NULL;

	uint32_t *outData = (uint32_t *)malloc(hdr->biWidth * hdr->biHeight * sizeof(uint32_t));
	if (outData == NULL)
		return NULL;

	const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;
	memcpy(pal, &src[0x36], palEntries * sizeof(uint32_t));

	for (int32_t i = 0; i < hdr->biWidth * hdr->biHeight; i++)
		outData[i] = pal[0];

	const int32_t lineEnd = hdr->biWidth;
	const uint8_t *src8 = pData;
	uint32_t *dst32 = outData;
	int32_t x = 0;
	int32_t y = hdr->biHeight - 1; /* BMP stores bottom-up */

	/*
	 * RLE decode loop. Format: [count][data] pairs.
	 * count=0 is escape: 0=EOL, 1=EOF, 2=delta, else=absolute run.
	 */
	while (true)
	{
		byte = *src8++;
		if (byte == 0)
		{
			byte = *src8++;
			if (byte == 0) /* End of line */
			{
				x = 0;
				y--;
			}
			else if (byte == 1) /* End of bitmap */
			{
				break;
			}
			else if (byte == 2) /* Delta: skip pixels */
			{
				x += *src8++;
				y -= *src8++;
			}
			else /* Absolute run: literal bytes follow */
			{
				if (hdr->biCompression == COMP_RLE8)
				{
					tmp32 = &dst32[(y * hdr->biWidth) + x];
					for (int32_t i = 0; i < byte; i++)
						*tmp32++ = pal[*src8++];

					if (byte & 1)
						src8++;

					x += byte;
				}
				else
				{
					len = byte >> 1;
					tmp32 = &dst32[y * hdr->biWidth];
					for (int32_t i = 0; i < len; i++)
					{
						palIdx = *src8++;
						tmp32[x++] = pal[palIdx >> 4];
						if (x < lineEnd)
							tmp32[x++] = pal[palIdx & 0xF];
					}

					if (((byte + 1) >> 1) & 1)
						src8++;
				}
			}
		}
		else
		{
			palIdx = *src8++;

			if (hdr->biCompression == COMP_RLE8)
			{
				color = pal[palIdx];
				tmp32 = &dst32[(y * hdr->biWidth) + x];
				for (int32_t i = 0; i < byte; i++)
					*tmp32++ = color;

				x += byte;
			}
			else
			{
				color = pal[palIdx >> 4];
				color2 = pal[palIdx & 0x0F];

				len = byte >> 1;
				tmp32 = &dst32[y * hdr->biWidth];
				for (int32_t i = 0; i < len; i++)
				{
					tmp32[x++] = color;
					if (x < lineEnd)
						tmp32[x++] = color2;
				}
			}
		}
	}

	return outData;
}

/*
 * Decode RLE4-compressed BMP to 1-bit mask (0 = background, 1 = foreground).
 * Used for fonts and button graphics where only shape matters.
 */
static uint8_t *loadBMPTo1Bit(const uint8_t *src)
{
	uint8_t palIdx, color, color2, *tmp8;
	int32_t len, byte, i;
	uint32_t pal[16];

	const bmpHeader_t *hdr = (const bmpHeader_t *)&src[2];
	const uint8_t *pData = &src[hdr->bfOffBits];
	const int32_t colorsInBitmap = 1 << hdr->biBitCount;

	if (hdr->biCompression != COMP_RLE4 || hdr->biClrUsed > 16 || colorsInBitmap > 16)
		return NULL;

	uint8_t *outData = (uint8_t *)malloc(hdr->biWidth * hdr->biHeight * sizeof(uint8_t));
	if (outData == NULL)
		return NULL;

	const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;
	memcpy(pal, &src[0x36], palEntries * sizeof(uint32_t));

	color = !!pal[0]; /* Non-zero palette color = foreground (1) */
	for (i = 0; i < hdr->biWidth * hdr->biHeight; i++)
		outData[i] = color;

	const int32_t lineEnd = hdr->biWidth;
	const uint8_t *src8 = pData;
	uint8_t *dst8 = outData;
	int32_t x = 0;
	int32_t y = hdr->biHeight - 1; /* BMP stores bottom-up */

	while (true)
	{
		byte = *src8++;
		if (byte == 0)
		{
			byte = *src8++;
			if (byte == 0)
			{
				x = 0;
				y--;
			}
			else if (byte == 1)
			{
				break;
			}
			else if (byte == 2)
			{
				x += *src8++;
				y -= *src8++;
			}
			else
			{
				len = byte >> 1;
				tmp8 = &dst8[y * hdr->biWidth];
				for (i = 0; i < len; i++)
				{
					palIdx = *src8++;
					tmp8[x++] = !!pal[palIdx >> 4];
					if (x < lineEnd)
						tmp8[x++] = !!pal[palIdx & 0xF];
				}

				if (((byte + 1) >> 1) & 1)
					src8++;
			}
		}
		else
		{
			palIdx = *src8++;
			color = !!pal[palIdx >> 4];
			color2 = !!pal[palIdx & 0x0F];

			len = byte >> 1;
			tmp8 = &dst8[y * hdr->biWidth];
			for (i = 0; i < len; i++)
			{
				tmp8[x++] = color;
				if (x < lineEnd)
					tmp8[x++] = color2;
			}
		}
	}

	return outData;
}

/*
 * Decode RLE4-compressed BMP to FT2 palette indices.
 * Used for UI graphics that need to respond to theme colors.
 */
static uint8_t *loadBMPTo4BitPal(const uint8_t *src)
{
	uint8_t palIdx, *tmp8, pal1, pal2;
	int32_t len, byte, i;
	uint32_t pal[16];

	const bmpHeader_t *hdr = (const bmpHeader_t *)&src[2];
	const uint8_t *pData = &src[hdr->bfOffBits];
	const int32_t colorsInBitmap = 1 << hdr->biBitCount;

	if (hdr->biCompression != COMP_RLE4 || hdr->biClrUsed > 16 || colorsInBitmap > 16)
		return NULL;

	uint8_t *outData = (uint8_t *)malloc(hdr->biWidth * hdr->biHeight * sizeof(uint8_t));
	if (outData == NULL)
		return NULL;

	const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;
	memcpy(pal, &src[0x36], palEntries * sizeof(uint32_t));

	palIdx = getFT2PalNrFromPixel(pal[0]);
	for (i = 0; i < hdr->biWidth * hdr->biHeight; i++)
		outData[i] = palIdx;

	const int32_t lineEnd = hdr->biWidth;
	const uint8_t *src8 = pData;
	uint8_t *dst8 = outData;
	int32_t x = 0;
	int32_t y = hdr->biHeight - 1; /* BMP stores bottom-up */

	while (true)
	{
		byte = *src8++;
		if (byte == 0)
		{
			byte = *src8++;
			if (byte == 0)
			{
				x = 0;
				y--;
			}
			else if (byte == 1)
			{
				break;
			}
			else if (byte == 2)
			{
				x += *src8++;
				y -= *src8++;
			}
			else
			{
				tmp8 = &dst8[y * hdr->biWidth];
				len = byte >> 1;
				for (i = 0; i < len; i++)
				{
					palIdx = *src8++;
					tmp8[x++] = getFT2PalNrFromPixel(pal[palIdx >> 4]);
					if (x < lineEnd)
						tmp8[x++] = getFT2PalNrFromPixel(pal[palIdx & 0xF]);
				}

				if (((byte + 1) >> 1) & 1)
					src8++;
			}
		}
		else
		{
			palIdx = *src8++;
			pal1 = getFT2PalNrFromPixel(pal[palIdx >> 4]);
			pal2 = getFT2PalNrFromPixel(pal[palIdx & 0x0F]);

			tmp8 = &dst8[y * hdr->biWidth];
			len = byte >> 1;
			for (i = 0; i < len; i++)
			{
				tmp8[x++] = pal1;
				if (x < lineEnd)
					tmp8[x++] = pal2;
			}
		}
	}

	return outData;
}

/* Load all embedded BMP assets. Returns false on allocation failure. */
bool ft2_bmp_load(ft2_bmp_t *bmp)
{
	if (bmp == NULL)
		return false;

	memset(bmp, 0, sizeof(ft2_bmp_t));

	/* 4-bit palette indexed: UI elements that use theme colors */
	bmp->ft2OldAboutLogo = loadBMPTo4BitPal(ft2OldAboutLogoBMP);
	bmp->ft2LogoBadges = loadBMPTo4BitPal(ft2LogoBadgesBMP);
	bmp->ft2ByBadges = loadBMPTo4BitPal(ft2ByBadgesBMP);
	bmp->midiLogo = loadBMPTo4BitPal(midiLogoBMP);
	bmp->nibblesLogo = loadBMPTo4BitPal(nibblesLogoBMP);
	bmp->nibblesStages = loadBMPTo4BitPal(nibblesStagesBMP);
	bmp->loopPins = loadBMPTo4BitPal(loopPinsBMP);
	bmp->mouseCursors = loadBMPTo4BitPal(mouseCursorsBMP);
	bmp->mouseCursorBusyClock = loadBMPTo4BitPal(mouseCursorBusyClockBMP);
	bmp->mouseCursorBusyGlass = loadBMPTo4BitPal(mouseCursorBusyGlassBMP);
	bmp->whitePianoKeys = loadBMPTo4BitPal(whitePianoKeysBMP);
	bmp->blackPianoKeys = loadBMPTo4BitPal(blackPianoKeysBMP);
	bmp->vibratoWaveforms = loadBMPTo4BitPal(vibratoWaveformsBMP);
	bmp->scopeRec = loadBMPTo4BitPal(scopeRecBMP);
	bmp->scopeMute = loadBMPTo4BitPal(scopeMuteBMP);
	bmp->radiobuttonGfx = loadBMPTo4BitPal(radiobuttonGfxBMP);
	bmp->checkboxGfx = loadBMPTo4BitPal(checkboxGfxBMP);

	/* 32-bit ARGB: full-color images */
	bmp->ft2AboutLogo = loadBMPTo32Bit(ft2AboutLogoBMP);

	/* 1-bit masks: fonts and buttons (rendered with current FG color) */
	bmp->buttonGfx = loadBMPTo1Bit(buttonGfxBMP);
	bmp->font1 = loadBMPTo1Bit(font1BMP);
	bmp->font2 = loadBMPTo1Bit(font2BMP);
	bmp->font3 = loadBMPTo1Bit(font3BMP);
	bmp->font4 = loadBMPTo1Bit(font4BMP);
	bmp->font6 = loadBMPTo1Bit(font6BMP);
	bmp->font7 = loadBMPTo1Bit(font7BMP);
	bmp->font8 = loadBMPTo1Bit(font8BMP);

	if (bmp->ft2OldAboutLogo == NULL || bmp->ft2AboutLogo == NULL ||
		bmp->buttonGfx == NULL || bmp->font1 == NULL || bmp->font2 == NULL ||
		bmp->font3 == NULL || bmp->font4 == NULL || bmp->font6 == NULL ||
		bmp->font7 == NULL || bmp->font8 == NULL || bmp->ft2LogoBadges == NULL ||
		bmp->ft2ByBadges == NULL || bmp->midiLogo == NULL || bmp->nibblesLogo == NULL ||
		bmp->nibblesStages == NULL || bmp->loopPins == NULL || bmp->mouseCursors == NULL ||
		bmp->mouseCursorBusyClock == NULL || bmp->mouseCursorBusyGlass == NULL ||
		bmp->whitePianoKeys == NULL || bmp->blackPianoKeys == NULL ||
		bmp->vibratoWaveforms == NULL || bmp->scopeRec == NULL || bmp->scopeMute == NULL ||
		bmp->radiobuttonGfx == NULL || bmp->checkboxGfx == NULL)
	{
		ft2_bmp_free(bmp);
		return false;
	}

	return true;
}

void ft2_bmp_free(ft2_bmp_t *bmp)
{
	if (bmp == NULL)
		return;

	if (bmp->ft2OldAboutLogo) { free(bmp->ft2OldAboutLogo); bmp->ft2OldAboutLogo = NULL; }
	if (bmp->ft2AboutLogo) { free(bmp->ft2AboutLogo); bmp->ft2AboutLogo = NULL; }
	if (bmp->buttonGfx) { free(bmp->buttonGfx); bmp->buttonGfx = NULL; }
	if (bmp->font1) { free(bmp->font1); bmp->font1 = NULL; }
	if (bmp->font2) { free(bmp->font2); bmp->font2 = NULL; }
	if (bmp->font3) { free(bmp->font3); bmp->font3 = NULL; }
	if (bmp->font4) { free(bmp->font4); bmp->font4 = NULL; }
	if (bmp->font6) { free(bmp->font6); bmp->font6 = NULL; }
	if (bmp->font7) { free(bmp->font7); bmp->font7 = NULL; }
	if (bmp->font8) { free(bmp->font8); bmp->font8 = NULL; }
	if (bmp->ft2LogoBadges) { free(bmp->ft2LogoBadges); bmp->ft2LogoBadges = NULL; }
	if (bmp->ft2ByBadges) { free(bmp->ft2ByBadges); bmp->ft2ByBadges = NULL; }
	if (bmp->midiLogo) { free(bmp->midiLogo); bmp->midiLogo = NULL; }
	if (bmp->nibblesLogo) { free(bmp->nibblesLogo); bmp->nibblesLogo = NULL; }
	if (bmp->nibblesStages) { free(bmp->nibblesStages); bmp->nibblesStages = NULL; }
	if (bmp->loopPins) { free(bmp->loopPins); bmp->loopPins = NULL; }
	if (bmp->mouseCursors) { free(bmp->mouseCursors); bmp->mouseCursors = NULL; }
	if (bmp->mouseCursorBusyClock) { free(bmp->mouseCursorBusyClock); bmp->mouseCursorBusyClock = NULL; }
	if (bmp->mouseCursorBusyGlass) { free(bmp->mouseCursorBusyGlass); bmp->mouseCursorBusyGlass = NULL; }
	if (bmp->whitePianoKeys) { free(bmp->whitePianoKeys); bmp->whitePianoKeys = NULL; }
	if (bmp->blackPianoKeys) { free(bmp->blackPianoKeys); bmp->blackPianoKeys = NULL; }
	if (bmp->vibratoWaveforms) { free(bmp->vibratoWaveforms); bmp->vibratoWaveforms = NULL; }
	if (bmp->scopeRec) { free(bmp->scopeRec); bmp->scopeRec = NULL; }
	if (bmp->scopeMute) { free(bmp->scopeMute); bmp->scopeMute = NULL; }
	if (bmp->radiobuttonGfx) { free(bmp->radiobuttonGfx); bmp->radiobuttonGfx = NULL; }
	if (bmp->checkboxGfx) { free(bmp->checkboxGfx); bmp->checkboxGfx = NULL; }
}

