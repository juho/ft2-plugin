/**
 * @file ft2_plugin_about.c
 * @brief Exact port of about screen from ft2_about.c with starfield and waving logo
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_about.h"
#include "ft2_plugin_ui.h"

/* Constants from original ft2_about.c */
#define OLD_NUM_STARS 1000
#define NUM_STARS 1500
#define LOGO_ALPHA_PERCENTAGE 71
#define STARSHINE_ALPHA_PERCENTAGE 33
#define SINUS_PHASES 1024
#define ABOUT_SCREEN_X 3
#define ABOUT_SCREEN_Y 3
#define ABOUT_SCREEN_W 626
#define ABOUT_SCREEN_H 167

#ifndef PI
#define PI 3.14159265358979323846
#endif

/* 
 * FT2 original ran at 70Hz vblank. Plugin runs at 60Hz.
 * Scale rotation deltas to maintain visual speed: delta * (70/60).
 */
#define SCALE_VBLANK_DELTA(x) ((int32_t)round((x) * (70.0 / 60.0)))

/* Old starfield types (classic FT2) */
typedef struct
{
	int16_t x, y, z;
} oldVector_t;

typedef struct
{
	uint16_t x, y, z;
} oldRotate_t;

typedef struct
{
	oldVector_t x, y, z;
} oldMatrix_t;

/* New starfield types */
typedef struct
{
	float x, y, z;
} vector_t;

typedef struct
{
	vector_t x, y, z;
} matrix_t;

/* Maps depth (0-23) to palette index for old starfield. Index selects PAL_FORGRND variants. */
static const uint8_t starColConv[24] = { 2,2,2,2,2,2,2,2, 2,2,2,1,1,1,3,3, 3,3,3,3,3,3,3,3 };

/* Text strings - must use Latin-1 escapes for FT2 bitmap font compatibility */
static char *customText0 = "Original FT2 by Magnus \"Vogue\" H\224gdahl & Fredrik \"Mr.H\" Huss";
static char *customText1 = "Clone by Olav \"8bitbubsy\" S\233rensen (16-bits.org)";
static char *customText2 = "Plugin by Blamstrain/TPOLM (blamstrain.com)";
static char *customText3 = "";
static char customText4[256];

/* Plugin version string - defined by CMake, fallback for IDE parsing */
#ifndef FT2_PLUGIN_VERSION
#define FT2_PLUGIN_VERSION "0.0.0"
#endif

/* Static state */
static int16_t customText0X, customText0Y, customText1Y, customText2Y, customText3Y;
static int16_t customText4Y, customText1X, customText2X, customText3X, customText4X;
static int16_t sin16[SINUS_PHASES], zSpeed;
static int32_t lastStarScreenPos[OLD_NUM_STARS];
static const uint16_t logoAlpha16 = (65535 * LOGO_ALPHA_PERCENTAGE) / 100;
static const uint16_t starShineAlpha16 = (65535 * STARSHINE_ALPHA_PERCENTAGE) / 100;
static uint32_t sinp1, sinp2;
static oldVector_t oldStarPoints[OLD_NUM_STARS];
static oldRotate_t oldStarRotation;
static oldMatrix_t oldStarMatrix;
static vector_t starPoints[NUM_STARS], starRotation;
static matrix_t starMatrix;
static bool initialized = false;
static bool useNewAboutScreen = true;

/* Linear congruential PRNG. Matches standalone ft2_random.c for reproducible star positions. */
static uint32_t aboutRandSeed = 12345;
static int32_t randoml(int32_t limit)
{
	if (limit <= 0) return 0;
	aboutRandSeed *= 134775813;
	aboutRandSeed++;
	return (int32_t)(((int64_t)aboutRandSeed * limit) >> 32);
}

/* Blend two 32-bit pixels */
static uint32_t blendPixels(uint32_t pixelA, uint32_t pixelB, uint16_t alpha)
{
	const uint16_t invAlpha = alpha ^ 0xFFFF;

	const int32_t rA = (pixelA >> 16) & 0xFF;
	const int32_t gA = (pixelA >> 8) & 0xFF;
	const int32_t bA = pixelA & 0xFF;

	const int32_t rB = (pixelB >> 16) & 0xFF;
	const int32_t gB = (pixelB >> 8) & 0xFF;
	const int32_t bB = pixelB & 0xFF;

	const int32_t r = ((rA * invAlpha) + (rB * alpha)) >> 16;
	const int32_t g = ((gA * invAlpha) + (gB * alpha)) >> 16;
	const int32_t b = ((bA * invAlpha) + (bB * alpha)) >> 16;

	return 0xFF000000 | (r << 16) | (g << 8) | b;
}

/* Blend a pixel at coordinates */
static void blendPixelsXY(ft2_video_t *video, int32_t x, int32_t y, 
                          int32_t pixelB_r, int32_t pixelB_g, int32_t pixelB_b, uint16_t alpha)
{
	if (video == NULL || video->frameBuffer == NULL)
		return;

	if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H)
		return;

	uint32_t *p = &video->frameBuffer[(y * SCREEN_W) + x];
	const uint32_t pixelA = *p;
	const uint16_t invAlpha = alpha ^ 0xFFFF;

	const int32_t rA = (pixelA >> 16) & 0xFF;
	const int32_t gA = (pixelA >> 8) & 0xFF;
	const int32_t bA = pixelA & 0xFF;

	const int32_t r = ((rA * invAlpha) + (pixelB_r * alpha)) >> 16;
	const int32_t g = ((gA * invAlpha) + (pixelB_g * alpha)) >> 16;
	const int32_t b = ((bA * invAlpha) + (pixelB_b * alpha)) >> 16;

	*p = 0xFF000000 | (r << 16) | (g << 8) | b;
}

/* Build 3x3 rotation matrix from Euler angles for old (classic FT2) starfield. */
static void oldRotateStarfieldMatrix(void)
{
	const int16_t sa = (int16_t)round(32767.0 * sin(oldStarRotation.x * (2.0 * PI / 65536.0)));
	const int16_t ca = (int16_t)round(32767.0 * cos(oldStarRotation.x * (2.0 * PI / 65536.0)));
	const int16_t sb = (int16_t)round(32767.0 * sin(oldStarRotation.y * (2.0 * PI / 65536.0)));
	const int16_t cb = (int16_t)round(32767.0 * cos(oldStarRotation.y * (2.0 * PI / 65536.0)));
	const int16_t sc = (int16_t)round(32767.0 * sin(oldStarRotation.z * (2.0 * PI / 65536.0)));
	const int16_t cc = (int16_t)round(32767.0 * cos(oldStarRotation.z * (2.0 * PI / 65536.0)));

	oldStarMatrix.x.x = ((ca * cc) >> 16) + (((sc * ((sa * sb) >> 16)) >> 16) << 1);
	oldStarMatrix.y.x = (sa * cb) >> 16;
	oldStarMatrix.z.x = (((cc * ((sa * sb) >> 16)) >> 16) << 1) - ((ca * sc) >> 16);

	oldStarMatrix.x.y = (((sc * ((ca * sb) >> 16)) >> 16) << 1) - ((sa * cc) >> 16);
	oldStarMatrix.y.y = (ca * cb) >> 16;
	oldStarMatrix.z.y = ((sa * sc) >> 16) + (((cc * ((ca * sb) >> 16)) >> 16) << 1);

	oldStarMatrix.x.z = (cb * sc) >> 16;
	oldStarMatrix.y.z = 0 - (sb >> 1);
	oldStarMatrix.z.z = (cb * cc) >> 16;
}

/* Render old (classic FT2) starfield: 1000 stars with integer math, pixel-erase. */
static void oldStarfield(ft2_video_t *video)
{
	if (video == NULL || video->frameBuffer == NULL)
		return;

	oldVector_t *star = oldStarPoints;
	for (int32_t i = 0; i < OLD_NUM_STARS; i++, star++)
	{
		/* Erase last star pixel */
		int32_t screenBufferPos = lastStarScreenPos[i];
		if (screenBufferPos >= 0 && screenBufferPos < SCREEN_W * SCREEN_H)
		{
			video->frameBuffer[screenBufferPos] = video->palette[PAL_BCKGRND];
			lastStarScreenPos[i] = -1;
		}

		star->z += zSpeed; /* int16_t overflow wraps stars to back of field */

		int16_t z = (((oldStarMatrix.x.z * star->x) >> 16) + ((oldStarMatrix.y.z * star->y) >> 16) + ((oldStarMatrix.z.z * star->z) >> 16)) + 9000;
		if (z <= 100) continue;
		
		int32_t y = ((oldStarMatrix.x.y * star->x) >> 16) + ((oldStarMatrix.y.y * star->y) >> 16) + ((oldStarMatrix.z.y * star->z) >> 16);
		y = (int16_t)((y << 7) / z) + 84;
		if ((uint16_t)y > 173-6) continue;

		int32_t x = ((oldStarMatrix.x.x * star->x) >> 16) + ((oldStarMatrix.y.x * star->y) >> 16) + ((oldStarMatrix.z.x * star->z) >> 16);
		x = (int16_t)((((x >> 2) + x) << 7) / z) + (320-8);
		if ((uint16_t)x >= 640-16)
			continue;

		/* Render star pixel only if the pixel under it is the background color */
		screenBufferPos = ((y + 4) * SCREEN_W) + (x + 4);
		if (screenBufferPos >= 0 && screenBufferPos < SCREEN_W * SCREEN_H)
		{
			if ((video->frameBuffer[screenBufferPos] >> 24) == PAL_BCKGRND)
			{
				const uint8_t col = ((uint8_t)~(z >> 8) >> 3) - (22 - 8);
				if (col < 24)
				{
					video->frameBuffer[screenBufferPos] = video->palette[starColConv[col]];
					lastStarScreenPos[i] = screenBufferPos;
				}
			}
		}
	}
}

/* Build 3x3 rotation matrix from Euler angles for new starfield (float precision). */
static void rotateStarfieldMatrix(void)
{
	const float F_2PI = (float)(2.0 * PI);

	const float rx2p = starRotation.x * F_2PI;
	const float xsin = sinf(rx2p);
	const float xcos = cosf(rx2p);

	const float ry2p = starRotation.y * F_2PI;
	const float ysin = sinf(ry2p);
	const float ycos = cosf(ry2p);

	const float rz2p = starRotation.z * F_2PI;
	const float zsin = sinf(rz2p);
	const float zcos = cosf(rz2p);

	starMatrix.x.x = (xcos * zcos) + (zsin * xsin * ysin);
	starMatrix.y.x = xsin * ycos;
	starMatrix.z.x = (zcos * xsin * ysin) - (xcos * zsin);

	starMatrix.x.y = (zsin * xcos * ysin) - (xsin * zcos);
	starMatrix.y.y = xcos * ycos;
	starMatrix.z.y = (xsin * zsin) + (zcos * xcos * ysin);

	starMatrix.x.z = ycos * zsin;
	starMatrix.y.z = 0.0f - ysin;
	starMatrix.z.z = ycos * zcos;
}

/* Render new starfield: 1500 stars with float math, anti-aliased glow effect. */
static void starfield(ft2_video_t *video)
{
	if (video == NULL || video->frameBuffer == NULL)
		return;

	vector_t *star = starPoints;
	for (int16_t i = 0; i < NUM_STARS; i++, star++)
	{
		star->z += 0.0001f;
		if (star->z >= 0.5f)
			star->z -= 1.0f;

		const float z = (starMatrix.x.z * star->x) + (starMatrix.y.z * star->y) + (starMatrix.z.z * star->z) + 0.5f;
		if (z <= 0.0f)
			continue;

		float y = (((starMatrix.x.y * star->x) + (starMatrix.y.y * star->y) + (starMatrix.z.y * star->z)) / z) * 400.0f;
		const int32_t outY = (ABOUT_SCREEN_Y + (ABOUT_SCREEN_H / 2)) + (int32_t)y;
		if (outY < ABOUT_SCREEN_Y || outY >= ABOUT_SCREEN_Y + ABOUT_SCREEN_H)
			continue;

		float x = (((starMatrix.x.x * star->x) + (starMatrix.y.x * star->y) + (starMatrix.z.x * star->z)) / z) * 400.0f;
		const int32_t outX = (ABOUT_SCREEN_X + (ABOUT_SCREEN_W / 2)) + (int32_t)x;
		if (outX < ABOUT_SCREEN_X || outX >= ABOUT_SCREEN_X + ABOUT_SCREEN_W)
			continue;

		int32_t intensity255 = (int32_t)(z * 256.0f);
		if (intensity255 > 255)
			intensity255 = 255;
		intensity255 ^= 255;

		/* Add a tint of blue to the star pixel */
		int32_t r = intensity255 - 79;
		if (r < 0) r = 0;

		int32_t g = intensity255 - 38;
		if (g < 0) g = 0;

		int32_t b = intensity255 + 64;
		if (b > 255) b = 255;

		/* Plot and blend sides of star (basic shine effect) */
		if (outX - 1 >= ABOUT_SCREEN_X)
			blendPixelsXY(video, outX - 1, outY, r, g, b, starShineAlpha16);

		if (outX + 1 < ABOUT_SCREEN_X + ABOUT_SCREEN_W)
			blendPixelsXY(video, outX + 1, outY, r, g, b, starShineAlpha16);

		if (outY - 1 >= ABOUT_SCREEN_Y)
			blendPixelsXY(video, outX, outY - 1, r, g, b, starShineAlpha16);

		if (outY + 1 < ABOUT_SCREEN_Y + ABOUT_SCREEN_H)
			blendPixelsXY(video, outX, outY + 1, r, g, b, starShineAlpha16);

		/* Plot center pixel */
		video->frameBuffer[(outY * SCREEN_W) + outX] = 0xFF000000 | (r << 16) | (g << 8) | b;
	}
}

static int32_t Sqr(int32_t x)
{
	return x * x;
}

void ft2_about_init(void)
{
	if (initialized)
		return;

	/* Initialize new star positions - matching original randoml usage */
	vector_t *star = starPoints;
	for (int32_t i = 0; i < NUM_STARS; i++, star++)
	{
		star->x = (float)((randoml(INT32_MAX) - (INT32_MAX/2)) * (1.0 / INT32_MAX));
		star->y = (float)((randoml(INT32_MAX) - (INT32_MAX/2)) * (1.0 / INT32_MAX));
		star->z = (float)((randoml(INT32_MAX) - (INT32_MAX/2)) * (1.0 / INT32_MAX));
	}

	/* Initialize sinus phases */
	sinp1 = 0;
	sinp2 = SINUS_PHASES / 4; /* Cosine offset */

	/* Pre-calc sinus table - matching original */
	for (int32_t i = 0; i < SINUS_PHASES; i++)
		sin16[i] = (int16_t)round(32767.0 * sin(i * PI * 2.0 / SINUS_PHASES));

	/* Build initial matrix */
	rotateStarfieldMatrix();

	/* Format version string - matching original style */
	sprintf(customText4, "v%s (%s)", FT2_PLUGIN_VERSION, __DATE__);

	/* Calculate text positions - using proper textWidth() for variable-width font */
	/* Line 1: Original FT2 authors (centered) */
	customText0X = (SCREEN_W - textWidth(customText0)) / 2;
	customText0Y = 157 - 28;
	/* Line 2: Clone by Olav (centered) */
	customText1X = (SCREEN_W - textWidth(customText1)) / 2;
	customText1Y = 157 - 16;
	/* Line 3: Plugin by Blamstrain (centered) */
	customText2X = (SCREEN_W - textWidth(customText2)) / 2;
	customText2Y = 157 - 4;
	/* Version (right-justified) */
	customText4X = (SCREEN_W - 8) - textWidth(customText4);
	customText4Y = 157 - 4;

	initialized = true;
}

void ft2_about_show(ft2_widgets_t *widgets, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (video == NULL)
		return;

	if (!initialized)
		ft2_about_init();

	/* Draw framework */
	drawFramework(video, 0, 0, 632, 173, FRAMEWORK_TYPE1);
	drawFramework(video, 2, 2, 628, 169, FRAMEWORK_TYPE2);

	/* Show buttons */
	if (widgets != NULL)
	{
		showPushButton(widgets, video, bmp, PB_GITHUB_ABOUT);
		showPushButton(widgets, video, bmp, PB_EXIT_ABOUT);
	}

	if (!useNewAboutScreen)
	{
		/* Initialize old starfield with random pattern */
		oldVector_t *s = oldStarPoints;

		const int32_t type = randoml(4);
		switch (type)
		{
			case 0: /* Classic "space stars" */
			{
				zSpeed = 309;
				for (int32_t i = 0; i < OLD_NUM_STARS; i++, s++)
				{
					s->z = (int16_t)randoml(0xFFFF) - 0x8000;
					s->y = (int16_t)randoml(0xFFFF) - 0x8000;
					s->x = (int16_t)randoml(0xFFFF) - 0x8000;
				}
			}
			break;

			case 1: /* Galaxy */
			{
				zSpeed = 0;
				for (int32_t i = 0; i < OLD_NUM_STARS; i++, s++)
				{
					if (i < OLD_NUM_STARS/4)
					{
						s->z = (int16_t)randoml(0xFFFF) - 0x8000;
						s->y = (int16_t)randoml(0xFFFF) - 0x8000;
						s->x = (int16_t)randoml(0xFFFF) - 0x8000;
					}
					else
					{
						int32_t r = randoml(30000);
						int32_t n = randoml(5);
						int32_t w = ((2 * randoml(2)) - 1) * Sqr(randoml(1000));
						double ww = (((PI * 2.0) / 5.0) * n) + (r * (1.0 / 12000.0)) + (w * (1.0 / 3000000.0));
						int32_t h = ((Sqr(r) / 30000) * ((int32_t)randoml(10000) - 5000)) / 12000;

						s->x = (int16_t)(r * cos(ww));
						s->y = (int16_t)(r * sin(ww));
						s->z = (int16_t)h;
					}
				}
			}
			break;

			case 2:
			case 3: /* Spiral */
			{
				zSpeed = 0;
				for (int32_t i = 0; i < OLD_NUM_STARS; i++, s++)
				{
					int32_t r = (int32_t)round(sqrt(randoml(500) * 500));
					int32_t w = randoml(3000);
					double ww = ((w * 8) + r) * (1.0 / 16.0);

					const int16_t z =  (int16_t)round(32767.0 * cos(w  * (2.0 * PI / 1024.0)));
					const int16_t y =  (int16_t)round(32767.0 * sin(w  * (2.0 * PI / 1024.0)));
					const int16_t x = ((int16_t)round(32767.0 * cos(ww * (2.0 * PI / 1024.0)))) / 4;

					s->z = (int16_t)((z * (w + r)) / 3500);
					s->y = (int16_t)((y * (w + r)) / 3500);
					s->x = (int16_t)((x * r) / 500);
				}
			}
			break;

			default:
				break;
		}

		oldStarRotation.x = 0;
		oldStarRotation.y = 748;
		oldStarRotation.z = 200;

		for (int32_t i = 0; i < OLD_NUM_STARS; i++)
			lastStarScreenPos[i] = -1;

		/* Blit the old FT2 logo */
		if (bmp != NULL && bmp->ft2OldAboutLogo != NULL)
			blit(video, 91, 31, bmp->ft2OldAboutLogo, ABOUT_OLD_LOGO_W, ABOUT_OLD_LOGO_H);
	}
}

void ft2_about_render_frame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (video == NULL)
		return;

	if (!initialized)
		ft2_about_init();

	if (useNewAboutScreen)
	{
		/* Clear the starfield area with black (like standalone) */
		clearRect(video, ABOUT_SCREEN_X, ABOUT_SCREEN_Y, ABOUT_SCREEN_W, ABOUT_SCREEN_H);

		/* Render 3D starfield */
		starfield(video);

		/* Update rotation */
		starRotation.x -= 0.0003f;
		starRotation.y -= 0.0002f;
		starRotation.z += 0.0001f;
		rotateStarfieldMatrix();

		/* Render waving FT2 logo */
		if (bmp != NULL && bmp->ft2AboutLogo != NULL)
		{
			uint32_t *dstPtr = video->frameBuffer + (ABOUT_SCREEN_Y * SCREEN_W) + ABOUT_SCREEN_X;
			for (int32_t y = 0; y < ABOUT_SCREEN_H; y++, dstPtr += SCREEN_W)
			{
				for (int32_t x = 0; x < ABOUT_SCREEN_W; x++)
				{
					int32_t srcX = (x - ((ABOUT_SCREEN_W - ABOUT_LOGO_W) / 2)) + (sin16[(sinp1 + x) & (SINUS_PHASES - 1)] >> 10);
					int32_t srcY = (y - ((ABOUT_SCREEN_H - ABOUT_LOGO_H) / 2) + 20) + (sin16[(sinp2 + y + x + x) & (SINUS_PHASES - 1)] >> 11);

					if ((uint32_t)srcX < ABOUT_LOGO_W && (uint32_t)srcY < ABOUT_LOGO_H)
					{
						const uint32_t logoPixel = bmp->ft2AboutLogo[(srcY * ABOUT_LOGO_W) + srcX];
						if (logoPixel != 0x00FF00) /* Transparency */
							dstPtr[x] = blendPixels(dstPtr[x], logoPixel, logoAlpha16);
					}
				}
			}
		}

		/* Update sinus phases */
		sinp1 = (sinp1 + 2) & (SINUS_PHASES - 1);
		sinp2 = (sinp2 + 3) & (SINUS_PHASES - 1);

		/* Render static texts */
		if (bmp != NULL)
		{
			textOut(video, bmp, customText0X, customText0Y, PAL_FORGRND, customText0);
			textOut(video, bmp, customText1X, customText1Y, PAL_FORGRND, customText1);
			textOut(video, bmp, customText2X, customText2Y, PAL_FORGRND, customText2);
			textOut(video, bmp, customText4X, customText4Y, PAL_FORGRND, customText4);
		}
	}
	else
	{
		/* Original FT2 about screen */
		oldStarRotation.x += SCALE_VBLANK_DELTA(3 * 64);
		oldStarRotation.y += SCALE_VBLANK_DELTA(2 * 64);
		oldStarRotation.z -= SCALE_VBLANK_DELTA(1 * 64);
		oldRotateStarfieldMatrix();

		oldStarfield(video);
	}
}

void ft2_about_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_about_render_frame(video, bmp);
}

void ft2_about_set_mode(bool newMode)
{
	useNewAboutScreen = newMode;
}

bool ft2_about_get_mode(void)
{
	return useNewAboutScreen;
}
