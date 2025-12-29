/**
 * @file ft2_plugin_pushbuttons.c
 * @brief Push button implementation for the FT2 plugin UI.
 * 
 * Ported from ft2_pushbuttons.c - exact coordinates preserved.
 * Modified for multi-instance support: visibility/state stored per-instance.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_widgets.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"

#define BUTTON_GFX_BMP_WIDTH 90

pushButton_t pushButtons[NUM_PUSHBUTTONS] =
{
	/* Reserved pushbuttons */
	{ 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 },

	/* Position editor */
	/*x,  y,  w,  h,  p, d, text #1,           text #2, funcOnDown, funcOnUp */
	{ 55,  2, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 55, 36, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 74,  2, 35, 16, 1, 6, "Ins.",            NULL,    NULL,       NULL },
	{ 74, 19, 18, 13, 1, 6, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 91, 19, 18, 13, 1, 6, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 74, 33, 35, 16, 1, 6, "Del.",            NULL,    NULL,       NULL },
	{ 74, 50, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 91, 50, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 74, 62, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 91, 62, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },

	/* Song/Pattern */
	/*x,   y,  w,  h,  p, d, text #1,           text #2, funcOnDown, funcOnUp */
	{ 168, 34, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 185, 34, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 168, 48, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 185, 48, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 168, 62, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 185, 62, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 253, 34, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 270, 34, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 253, 48, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 270, 48, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 209, 62, 40, 13, 0, 0, "Expd.",           NULL,    NULL,       NULL },
	{ 248, 62, 40, 13, 0, 0, "Srnk.",           NULL,    NULL,       NULL },

	/* Logo */
	/*x,   y, w,   h,  p, d, text #1, text #2, funcOnDown, funcOnUp */
	{ 112, 0, 154, 32, 0, 0, NULL,    NULL,    NULL,       NULL },
	{ 266, 0,  25, 32, 0, 0, NULL,    NULL,    NULL,       NULL },

	/* Main menu */
	/*x,   y,   w,  h,  p, d, text #1,      text #2, funcOnDown, funcOnUp */
	{ 294,   2, 59, 16, 0, 0, "About",      NULL,    NULL,       NULL },
	{ 294,  19, 59, 16, 0, 0, "Nibbles",    NULL,    NULL,       NULL },
	{ 294,  36, 59, 16, 0, 0, "Zap",        NULL,    NULL,       NULL },
	{ 294,  53, 59, 16, 0, 0, "Trim",       NULL,    NULL,       NULL },
	{ 294,  70, 59, 16, 0, 0, "Extend",     NULL,    NULL,       NULL },
	{ 294,  87, 59, 16, 0, 0, "Transps.",   NULL,    NULL,       NULL },
	{ 294, 104, 59, 16, 0, 0, "I.E.Ext.",   NULL,    NULL,       NULL },
	{ 294, 121, 59, 16, 0, 0, "S.E.Ext.",   NULL,    NULL,       NULL },
	{ 294, 138, 59, 16, 0, 0, "Adv. Edit",  NULL,    NULL,       NULL },
	{ 294, 155, 30, 16, 0, 0, "Add",        NULL,    NULL,       NULL },
	{ 323, 155, 30, 16, 0, 0, "Sub",        NULL,    NULL,       NULL },
	{ 359,   2, 59, 16, 0, 0, "Play sng.",  NULL,    NULL,       NULL },
	{ 359,  19, 59, 16, 0, 0, "Play ptn.",  NULL,    NULL,       NULL },
	{ 359,  36, 59, 16, 0, 0, "Stop",       NULL,    NULL,       NULL },
	{ 359,  53, 59, 16, 0, 0, "Rec. sng.",  NULL,    NULL,       NULL },
	{ 359,  70, 59, 16, 0, 0, "Rec. ptn.",  NULL,    NULL,       NULL },
	{ 359,  87, 59, 16, 0, 0, "Disk op.",   NULL,    NULL,       NULL },
	{ 359, 104, 59, 16, 0, 0, "Instr. Ed.", NULL,    NULL,       NULL },
	{ 359, 121, 59, 16, 0, 0, "Smp. Ed.",   NULL,    NULL,       NULL },
	{ 359, 138, 59, 16, 0, 0, "Config",     NULL,    NULL,       NULL },
	{ 359, 155, 59, 16, 0, 0, "Help",       NULL,    NULL,       NULL },
	{ 115,  35, 46, 16, 0, 0, "Exit",       NULL,    NULL,       NULL },

	/* Instrument switcher */
	/*x,   y,   w,  h,  p, d, text #1,           text #2, funcOnDown, funcOnUp */
	{ 590,   2, 39, 16, 0, 0, "01-08",           NULL,    NULL,       NULL },
	{ 590,  19, 39, 16, 0, 0, "09-10",           NULL,    NULL,       NULL },
	{ 590,  36, 39, 16, 0, 0, "11-18",           NULL,    NULL,       NULL },
	{ 590,  53, 39, 16, 0, 0, "19-20",           NULL,    NULL,       NULL },
	{ 590,  73, 39, 16, 0, 0, "21-28",           NULL,    NULL,       NULL },
	{ 590,  90, 39, 16, 0, 0, "29-30",           NULL,    NULL,       NULL },
	{ 590, 107, 39, 16, 0, 0, "31-38",           NULL,    NULL,       NULL },
	{ 590, 124, 39, 16, 0, 0, "39-40",           NULL,    NULL,       NULL },
	{ 590,   2, 39, 16, 0, 0, "41-48",           NULL,    NULL,       NULL },
	{ 590,  19, 39, 16, 0, 0, "49-50",           NULL,    NULL,       NULL },
	{ 590,  36, 39, 16, 0, 0, "51-58",           NULL,    NULL,       NULL },
	{ 590,  53, 39, 16, 0, 0, "59-60",           NULL,    NULL,       NULL },
	{ 590,  73, 39, 16, 0, 0, "61-68",           NULL,    NULL,       NULL },
	{ 590,  90, 39, 16, 0, 0, "69-70",           NULL,    NULL,       NULL },
	{ 590, 107, 39, 16, 0, 0, "71-78",           NULL,    NULL,       NULL },
	{ 590, 124, 39, 16, 0, 0, "79-80",           NULL,    NULL,       NULL },
	{ 590, 144, 39, 27, 0, 0, "Swap",            "Bank",  NULL,       NULL },
	{ 566,  99, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 566, 140, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },

	/* Nibbles */
	/*x,   y,   w,  h,  p, d, text #1, text #2, funcOnDown, funcOnUp */
	{ 568, 104, 61, 16, 0, 0, "Play",  NULL,    NULL,       NULL },
	{ 568, 121, 61, 16, 0, 0, "Help",  NULL,    NULL,       NULL },
	{ 568, 138, 61, 16, 0, 0, "Highs", NULL,    NULL,       NULL },
	{ 568, 155, 61, 16, 0, 0, "Exit",  NULL,    NULL,       NULL },

	/* Advanced edit */
	/*x,  y,   w,  h,  p, d, text #1,   text #2, funcOnDown, funcOnUp */
	{  3, 138, 51, 16, 0, 0, "Track",   NULL,    NULL,       NULL },
	{ 55, 138, 52, 16, 0, 0, "Pattern", NULL,    NULL,       NULL },
	{  3, 155, 51, 16, 0, 0, "Song",    NULL,    NULL,       NULL },
	{ 55, 155, 52, 16, 0, 0, "Block",   NULL,    NULL,       NULL },

	/* About */
	/*x, y,   w,  h,  p, d, text #1, text #2, funcOnDown, funcOnUp */
	{ 4, 153, 59, 16, 0, 0, "Exit",  NULL,    NULL,       NULL },

	/* Help */
	/*x,   y,   w,  h,  p, d, text #1,           text #2, funcOnDown, funcOnUp */
	{   3, 155, 59, 16, 0, 0, "Exit",            NULL,    NULL,       NULL },
	{ 611,   2, 18, 13, 1, 3, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 611, 158, 18, 13, 1, 3, ARROW_DOWN_STRING, NULL,    NULL,       NULL },

	/* Pattern editor */
	/*x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown, funcOnUp */
	{   3, 385, 25, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 604, 385, 25, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },

	/* Transpose */
	/*x,   y,   w,  h,  p, d, text #1, text #2, funcOnDown, funcOnUp */
	{  56, 110, 21, 16, 0, 0, "up",    NULL,    NULL,       NULL },
	{  76, 110, 21, 16, 0, 0, "dn",    NULL,    NULL,       NULL },
	{  98, 110, 36, 16, 0, 0, "12up",  NULL,    NULL,       NULL },
	{ 133, 110, 36, 16, 0, 0, "12dn",  NULL,    NULL,       NULL },
	{ 175, 110, 21, 16, 0, 0, "up",    NULL,    NULL,       NULL },
	{ 195, 110, 21, 16, 0, 0, "dn",    NULL,    NULL,       NULL },
	{ 217, 110, 36, 16, 0, 0, "12up",  NULL,    NULL,       NULL },
	{ 252, 110, 36, 16, 0, 0, "12dn",  NULL,    NULL,       NULL },
	{  56, 125, 21, 16, 0, 0, "up",    NULL,    NULL,       NULL },
	{  76, 125, 21, 16, 0, 0, "dn",    NULL,    NULL,       NULL },
	{  98, 125, 36, 16, 0, 0, "12up",  NULL,    NULL,       NULL },
	{ 133, 125, 36, 16, 0, 0, "12dn",  NULL,    NULL,       NULL },
	{ 175, 125, 21, 16, 0, 0, "up",    NULL,    NULL,       NULL },
	{ 195, 125, 21, 16, 0, 0, "dn",    NULL,    NULL,       NULL },
	{ 217, 125, 36, 16, 0, 0, "12up",  NULL,    NULL,       NULL },
	{ 252, 125, 36, 16, 0, 0, "12dn",  NULL,    NULL,       NULL },
	{  56, 140, 21, 16, 0, 0, "up",    NULL,    NULL,       NULL },
	{  76, 140, 21, 16, 0, 0, "dn",    NULL,    NULL,       NULL },
	{  98, 140, 36, 16, 0, 0, "12up",  NULL,    NULL,       NULL },
	{ 133, 140, 36, 16, 0, 0, "12dn",  NULL,    NULL,       NULL },
	{ 175, 140, 21, 16, 0, 0, "up",    NULL,    NULL,       NULL },
	{ 195, 140, 21, 16, 0, 0, "dn",    NULL,    NULL,       NULL },
	{ 217, 140, 36, 16, 0, 0, "12up",  NULL,    NULL,       NULL },
	{ 252, 140, 36, 16, 0, 0, "12dn",  NULL,    NULL,       NULL },
	{  56, 155, 21, 16, 0, 0, "up",    NULL,    NULL,       NULL },
	{  76, 155, 21, 16, 0, 0, "dn",    NULL,    NULL,       NULL },
	{  98, 155, 36, 16, 0, 0, "12up",  NULL,    NULL,       NULL },
	{ 133, 155, 36, 16, 0, 0, "12dn",  NULL,    NULL,       NULL },
	{ 175, 155, 21, 16, 0, 0, "up",    NULL,    NULL,       NULL },
	{ 195, 155, 21, 16, 0, 0, "dn",    NULL,    NULL,       NULL },
	{ 217, 155, 36, 16, 0, 0, "12up",  NULL,    NULL,       NULL },
	{ 252, 155, 36, 16, 0, 0, "12dn",  NULL,    NULL,       NULL },

	/* Sample editor */
	/*x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown, funcOnUp */
	{   3, 331, 23, 13, 1, 3, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 331, 23, 13, 1, 3, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{  38, 356, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{  38, 368, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{   3, 382, 53, 16, 0, 0, "Stop",             NULL,    NULL,       NULL },
	{  57, 348, 55, 16, 0, 0, "Wave",             NULL,    NULL,       NULL },
	{  57, 365, 55, 16, 0, 0, "Range",            NULL,    NULL,       NULL },
	{  57, 382, 55, 16, 0, 0, "Display",          NULL,    NULL,       NULL },
	{ 118, 348, 63, 16, 0, 0, "Show r.",          NULL,    NULL,       NULL },
	{ 118, 365, 63, 16, 0, 0, "Range all",        NULL,    NULL,       NULL },
	{ 118, 382, 63, 16, 0, 0, "Sample",           NULL,    NULL,       NULL },
	{ 182, 348, 63, 16, 0, 0, "Zoom out",         NULL,    NULL,       NULL },
	{ 182, 365, 63, 16, 0, 0, "Show all",         NULL,    NULL,       NULL },
	{ 182, 382, 63, 16, 0, 0, "Save rng.",        NULL,    NULL,       NULL },
	{ 251, 348, 43, 16, 0, 0, "Cut",              NULL,    NULL,       NULL },
	{ 251, 365, 43, 16, 0, 0, "Copy",             NULL,    NULL,       NULL },
	{ 251, 382, 43, 16, 0, 0, "Paste",            NULL,    NULL,       NULL },
	{ 300, 348, 50, 16, 0, 0, "Crop",             NULL,    NULL,       NULL },
	{ 300, 365, 50, 16, 0, 0, "Volume",           NULL,    NULL,       NULL },
	{ 300, 382, 50, 16, 0, 0, "Effects",          NULL,    NULL,       NULL },
	{ 430, 348, 54, 16, 0, 0, "Exit",             NULL,    NULL,       NULL },
	{ 594, 348, 35, 13, 0, 0, "Clr S.",           NULL,    NULL,       NULL },
	{ 594, 360, 35, 13, 0, 0, "Min.",             NULL,    NULL,       NULL },
	{ 594, 373, 18, 13, 2, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 611, 373, 18, 13, 2, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 594, 385, 18, 13, 2, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 611, 385, 18, 13, 2, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },

	/* Sample editor effects */
	/*x,   y,   w,  h,  p, d, text #1,     text #2, funcOnDown, funcOnUp */
	{  78, 350, 18, 13, 2, 2, ARROW_UP_STRING,   NULL, NULL, NULL },
	{  95, 350, 18, 13, 2, 2, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{   3, 365, 54, 16, 0, 0, "Triangle",        NULL, NULL, NULL },
	{  59, 365, 54, 16, 0, 0, "Saw",             NULL, NULL, NULL },
	{   3, 382, 54, 16, 0, 0, "Sine",            NULL, NULL, NULL },
	{  59, 382, 54, 16, 0, 0, "Square",          NULL, NULL, NULL },
	{ 192, 350, 18, 13, 1, 2, ARROW_UP_STRING,   NULL, NULL, NULL },
	{ 209, 350, 18, 13, 1, 2, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 119, 365, 53, 16, 0, 0, "lp filter",       NULL, NULL, NULL },
	{ 174, 365, 53, 16, 0, 0, "hp filter",       NULL, NULL, NULL },
	{ 269, 350, 13, 13, 0, 0, "-",               NULL, NULL, NULL },
	{ 281, 350, 13, 13, 0, 0, "+",               NULL, NULL, NULL },
	{ 269, 367, 13, 13, 0, 0, "-",               NULL, NULL, NULL },
	{ 281, 367, 13, 13, 0, 0, "+",               NULL, NULL, NULL },
	{ 233, 382, 61, 16, 0, 0, "Amplitude",       NULL, NULL, NULL },
	{ 300, 348, 50, 16, 0, 0, "Undo",            NULL, NULL, NULL },
	{ 300, 365, 50, 16, 0, 0, "X-Fade",          NULL, NULL, NULL },
	{ 300, 382, 50, 16, 0, 0, "Back...",         NULL, NULL, NULL },

	/* Sample editor extension */
	/*x,   y,   w,  h,  p, d, text #1,     text #2, funcOnDown, funcOnUp */
	{   3, 138, 52, 16, 0, 0, "Clr. c.bf", NULL,    NULL,       NULL },
	{  56, 138, 49, 16, 0, 0, "Sign",      NULL,    NULL,       NULL },
	{ 106, 138, 49, 16, 0, 0, "Echo",      NULL,    NULL,       NULL },
	{   3, 155, 52, 16, 0, 0, "Backw.",    NULL,    NULL,       NULL },
	{  56, 155, 49, 16, 0, 0, "B. swap",   NULL,    NULL,       NULL },
	{ 106, 155, 49, 16, 0, 0, "Fix DC",    NULL,    NULL,       NULL },
	{ 161, 121, 60, 16, 0, 0, "Copy ins.", NULL,    NULL,       NULL },
	{ 222, 121, 66, 16, 0, 0, "Copy smp.", NULL,    NULL,       NULL },
	{ 161, 138, 60, 16, 0, 0, "Xchg ins.", NULL,    NULL,       NULL },
	{ 222, 138, 66, 16, 0, 0, "Xchg smp.", NULL,    NULL,       NULL },
	{ 161, 155, 60, 16, 0, 0, "Resample",  NULL,    NULL,       NULL },
	{ 222, 155, 66, 16, 0, 0, "Mix smp.",  NULL,    NULL,       NULL },

	/* Instrument editor */
	/*x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown, funcOnUp */
	{ 200, 175, 23, 12, 0, 0, SMALL_1_STRING,     NULL,    NULL,       NULL },
	{ 222, 175, 24, 12, 0, 0, SMALL_2_STRING,     NULL,    NULL,       NULL },
	{ 245, 175, 24, 12, 0, 0, SMALL_3_STRING,     NULL,    NULL,       NULL },
	{ 268, 175, 24, 12, 0, 0, SMALL_4_STRING,     NULL,    NULL,       NULL },
	{ 291, 175, 24, 12, 0, 0, SMALL_5_STRING,     NULL,    NULL,       NULL },
	{ 314, 175, 24, 12, 0, 0, SMALL_6_STRING,     NULL,    NULL,       NULL },
	{ 200, 262, 23, 12, 0, 0, SMALL_1_STRING,     NULL,    NULL,       NULL },
	{ 222, 262, 24, 12, 0, 0, SMALL_2_STRING,     NULL,    NULL,       NULL },
	{ 245, 262, 24, 12, 0, 0, SMALL_3_STRING,     NULL,    NULL,       NULL },
	{ 268, 262, 24, 12, 0, 0, SMALL_4_STRING,     NULL,    NULL,       NULL },
	{ 291, 262, 24, 12, 0, 0, SMALL_5_STRING,     NULL,    NULL,       NULL },
	{ 314, 262, 24, 12, 0, 0, SMALL_6_STRING,     NULL,    NULL,       NULL },
	{ 570, 276, 59, 16, 0, 0, "Exit",             NULL,    NULL,       NULL },
	{ 341, 175, 47, 16, 1, 4, "Add",              NULL,    NULL,       NULL },
	{ 389, 175, 46, 16, 1, 4, "Del",              NULL,    NULL,       NULL },
	{ 398, 204, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 416, 204, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 398, 231, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 416, 231, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 398, 245, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 416, 245, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 341, 262, 47, 16, 1, 4, "Add",              NULL,    NULL,       NULL },
	{ 389, 262, 46, 16, 1, 4, "Del",              NULL,    NULL,       NULL },
	{ 398, 291, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 416, 291, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 398, 318, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 416, 318, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 398, 332, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 416, 332, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 521, 175, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 175, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 521, 189, 23, 13, 2, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 189, 23, 13, 2, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 521, 203, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 203, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 521, 220, 23, 13, 2, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 220, 23, 13, 2, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 521, 234, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 234, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 521, 248, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 248, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 521, 262, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 262, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 441, 312, 94, 16, 1, 4, "Octave up",        NULL,    NULL,       NULL },
	{ 536, 312, 93, 16, 1, 4, "Halftone up",      NULL,    NULL,       NULL },
	{ 441, 329, 94, 16, 1, 4, "Octave down",      NULL,    NULL,       NULL },
	{ 536, 329, 93, 16, 1, 4, "Halftone down",    NULL,    NULL,       NULL },

	/* Instrument editor extension */
	/*x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown, funcOnUp */
	{ 172, 130, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 265, 130, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 172, 144, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 265, 144, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 172, 158, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 265, 158, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },

	/* Trim screen */
	/*x,   y,   w,  h,  p, d, text #1,     text #2, funcOnDown, funcOnUp */
	{ 139, 155, 74, 16, 0, 0, "Calculate", NULL,    NULL,       NULL },
	{ 214, 155, 74, 16, 0, 0, "Trim",      NULL,    NULL,       NULL },

	/* Config left panel */
	/*x, y,   w,   h,  p, d, text #1,         text #2, funcOnDown, funcOnUp */
	{ 3, 104, 104, 16, 0, 0, "Reset config",  NULL,    NULL,       NULL },
	{ 3, 121, 104, 16, 0, 0, "Load config",   NULL,    NULL,       NULL },
	{ 3, 138, 104, 16, 0, 0, "Save config",   NULL,    NULL,       NULL },
	{ 3, 155, 104, 16, 0, 0, "Exit",          NULL,    NULL,       NULL },

	/* Config audio */
	/*x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown, funcOnUp */
	{ 326,   2, 57, 13, 0, 0, "Re-scan",          NULL,    NULL,       NULL },
	{ 365,  16, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 365,  72, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 365, 103, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 365, 137, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 512, 117, 21, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 608, 117, 21, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 512, 143, 21, 13, 1, 0, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 608, 143, 21, 13, 1, 0, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },

	/* Config layout */
	/*x,   y,  w,  h,  p, d, text #1,            text #2, funcOnDown, funcOnUp */
	{ 513, 15, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 15, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 513, 29, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 29, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 513, 43, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 43, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },
	{ 513, 71, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 606, 71, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },

	/* Config miscellaneous */
	/*x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown, funcOnUp */
	{ 370, 120, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 387, 120, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 594, 106, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 611, 106, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 594, 120, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    NULL,       NULL },
	{ 611, 120, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    NULL,       NULL },
	{ 556, 158, 22, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    NULL,       NULL },
	{ 607, 158, 22, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    NULL,       NULL },

	/* Disk op */
	/*x,   y,   w,  h,  p, d, text #1,           text #2, funcOnDown, funcOnUp */
	{  70,   2, 58, 16, 0, 0, "Save",            NULL,    NULL,       NULL },
	{  70,  19, 58, 16, 0, 0, "Delete",          NULL,    NULL,       NULL },
	{  70,  36, 58, 16, 0, 0, "Rename",          NULL,    NULL,       NULL },
	{  70,  53, 58, 16, 0, 0, "Make dir.",       NULL,    NULL,       NULL },
	{  70,  70, 58, 16, 0, 0, "Refresh",         NULL,    NULL,       NULL },
	{  70,  87, 58, 16, 0, 0, "Set path",        NULL,    NULL,       NULL },
	{  70, 104, 58, 16, 0, 0, "Show all",        NULL,    NULL,       NULL },
	{  70, 121, 58, 19, 0, 0, "Exit",            NULL,    NULL,       NULL },
	{ 134,  16, 31, 12, 0, 0, "/",               NULL,    NULL,       NULL },
	{ 134,   2, 31, 13, 0, 0, "../",             NULL,    NULL,       NULL },
	{ 134,  30, 31, 12, 0, 0, "Home",            NULL,    NULL,       NULL },
	{ 335,   2, 18, 13, 1, 3, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 335, 158, 18, 13, 1, 3, ARROW_DOWN_STRING, NULL,    NULL,       NULL },

	/* WAV renderer */
	/*x,   y,   w,  h,  p, d, text #1,           text #2, funcOnDown, funcOnUp */
	{   3, 111, 53, 43, 0, 0, "Export",          NULL,    NULL,       NULL },
	{   3, 155, 53, 16, 0, 0, "Exit",            NULL,    NULL,       NULL },
	{ 253, 114, 18, 13, 1, 6, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 270, 114, 18, 13, 1, 6, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 253, 128, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 270, 128, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 138, 142, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 155, 142, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },
	{ 253, 142, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    NULL,       NULL },
	{ 270, 142, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    NULL,       NULL },

	/* Channel output routing buttons (32 channels x 2 buttons each) */
	/* Column 1 (Ch 1-11) */
	{ 172, 43, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 43, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 172, 54, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 54, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 172, 65, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 65, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 172, 76, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 76, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 172, 87, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 87, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 172, 98, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 98, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 172, 109, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 109, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 172, 120, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 120, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 172, 131, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 131, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 172, 142, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 142, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 172, 153, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 188, 153, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	/* Column 2 (Ch 12-22) */
	{ 332, 43, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 43, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 332, 54, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 54, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 332, 65, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 65, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 332, 76, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 76, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 332, 87, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 87, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 332, 98, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 98, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 332, 109, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 109, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 332, 120, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 120, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 332, 131, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 131, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 332, 142, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 142, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 332, 153, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 348, 153, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	/* Column 3 (Ch 23-32) */
	{ 492, 43, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 508, 43, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 492, 54, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 508, 54, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 492, 65, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 508, 65, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 492, 76, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 508, 76, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 492, 87, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 508, 87, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 492, 98, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 508, 98, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 492, 109, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 508, 109, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 492, 120, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 508, 120, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 492, 131, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 508, 131, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL },
	{ 492, 142, 16, 11, 1, 4, ARROW_UP_STRING, NULL, NULL, NULL }, { 508, 142, 16, 11, 1, 4, ARROW_DOWN_STRING, NULL, NULL, NULL }
};

void initPushButtons(void)
{
	/* Initialize constant data only (visibility/state now per-instance in ft2_widgets_t) */
	for (int i = 0; i < NUM_PUSHBUTTONS; i++)
	{
		/* Logo and badge buttons use bitmap graphics */
		if (i == PB_LOGO || i == PB_BADGE)
		{
			pushButtons[i].bitmapFlag = true;
		}
		else
		{
			pushButtons[i].bitmapFlag = false;
			pushButtons[i].bitmapUnpressed = NULL;
			pushButtons[i].bitmapPressed = NULL;
		}
	}
}

void drawPushButton(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t pushButtonID)
{
	if (widgets == NULL || pushButtonID >= NUM_PUSHBUTTONS)
		return;

	if (!widgets->pushButtonVisible[pushButtonID])
		return;

	pushButton_t *b = &pushButtons[pushButtonID];
	uint8_t state = widgets->pushButtonState[pushButtonID];
	uint16_t x = b->x;
	uint16_t y = b->y;
	uint16_t w = b->w;
	uint16_t h = b->h;

	if (w < 4 || h < 4)
		return;

	if (b->bitmapFlag && b->bitmapUnpressed != NULL && b->bitmapPressed != NULL)
	{
		blitFast(video, x, y, (state == PUSHBUTTON_UNPRESSED) ? b->bitmapUnpressed : b->bitmapPressed, w, h);
		return;
	}

	/* Fill button background */
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	/* Draw outer border */
	hLine(video, x,         y,         w, PAL_BCKGRND);
	hLine(video, x,         y + h - 1, w, PAL_BCKGRND);
	vLine(video, x,         y,         h, PAL_BCKGRND);
	vLine(video, x + w - 1, y,         h, PAL_BCKGRND);

	/* Draw inner borders */
	if (state == PUSHBUTTON_UNPRESSED)
	{
		/* Top left corner inner border */
		hLine(video, x + 1, y + 1, w - 3, PAL_BUTTON1);
		vLine(video, x + 1, y + 2, h - 4, PAL_BUTTON1);

		/* Bottom right corner inner border */
		hLine(video, x + 1,     y + h - 2, w - 2, PAL_BUTTON2);
		vLine(video, x + w - 2, y + 1,     h - 3, PAL_BUTTON2);
	}
	else
	{
		/* Top left corner inner border */
		hLine(video, x + 1, y + 1, w - 2, PAL_BUTTON2);
		vLine(video, x + 1, y + 2, h - 3, PAL_BUTTON2);
	}

	/* Render button text */
	if (b->caption != NULL && b->caption[0] != '\0')
	{
		/* Check for special button graphics (arrow chars etc) */
		if ((uint8_t)b->caption[0] < 32 && b->caption[1] == '\0')
		{
			uint16_t textW = 8;
			uint8_t ch = (uint8_t)b->caption[0];

			if (ch == 0x01 || ch == 0x02) /* arrow up/down */
				textW = 6;
			else if (ch == 0x03 || ch == 0x04) /* arrow left/right */
				textW = 7;
			else if (ch >= 0x05 && ch <= 0x0A) /* small digits 1-6 */
				textW = 5;

			uint16_t textX = x + ((w - textW) / 2);
			uint16_t textY = y + ((h - 8) / 2);

			if (state == PUSHBUTTON_PRESSED)
			{
				textX++;
				textY++;
			}

			/* Blit button graphics from buttonGfx */
			if (bmp != NULL && bmp->buttonGfx != NULL)
			{
				const uint8_t *src8 = &bmp->buttonGfx[(ch - 1) * 8];
				uint32_t *dst32 = &video->frameBuffer[(textY * SCREEN_W) + textX];

				for (int row = 0; row < 8; row++)
				{
					for (int col = 0; col < textW; col++)
					{
						if (src8[col] != 0)
							dst32[col] = video->palette[PAL_BTNTEXT];
					}
					src8 += BUTTON_GFX_BMP_WIDTH;
					dst32 += SCREEN_W;
				}
			}
		}
		else
		{
			/* Normal text */
			uint16_t textW = textWidth(b->caption);
			uint16_t textX = x + ((w - textW) / 2);
			uint16_t textY = y + ((h - 8) / 2);

			/* If there's a second caption, adjust positions */
			if (b->caption2 != NULL && b->caption2[0] != '\0')
			{
				uint16_t text2W = textWidth(b->caption2);
				uint16_t text2X = x + ((w - text2W) / 2);
				uint16_t text2Y = textY + 6;

				if (state == PUSHBUTTON_PRESSED)
					textOut(video, bmp, text2X + 1, text2Y + 1, PAL_BTNTEXT, b->caption2);
				else
					textOut(video, bmp, text2X, text2Y, PAL_BTNTEXT, b->caption2);

				textY -= 5;
			}

			if (state == PUSHBUTTON_PRESSED)
				textOut(video, bmp, textX + 1, textY + 1, PAL_BTNTEXT, b->caption);
			else
				textOut(video, bmp, textX, textY, PAL_BTNTEXT, b->caption);
		}
	}
}

void showPushButton(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t pushButtonID)
{
	if (widgets == NULL || pushButtonID >= NUM_PUSHBUTTONS)
		return;

	widgets->pushButtonVisible[pushButtonID] = true;
	drawPushButton(widgets, video, bmp, pushButtonID);
}

void hidePushButton(struct ft2_widgets_t *widgets, uint16_t pushButtonID)
{
	if (widgets == NULL || pushButtonID >= NUM_PUSHBUTTONS)
		return;

	widgets->pushButtonState[pushButtonID] = PUSHBUTTON_UNPRESSED;
	widgets->pushButtonVisible[pushButtonID] = false;
}

int16_t testPushButtonMouseDown(struct ft2_widgets_t *widgets, struct ft2_instance_t *inst, int32_t mouseX, int32_t mouseY, bool sysReqShown)
{
	if (widgets == NULL)
		return -1;

	uint16_t start, end;

	if (sysReqShown)
	{
		start = 0;
		end = 8;
	}
	else
	{
		start = 8;
		end = NUM_PUSHBUTTONS;
	}

	for (uint16_t i = start; i < end; i++)
	{
		if (!widgets->pushButtonVisible[i])
			continue;

		pushButton_t *pb = &pushButtons[i];
		if (mouseX >= pb->x && mouseX < pb->x + pb->w &&
		    mouseY >= pb->y && mouseY < pb->y + pb->h)
		{
			widgets->pushButtonState[i] = PUSHBUTTON_PRESSED;
			
			/* Call callback immediately on first press (matching original FT2 behavior) */
			if (pb->callbackFuncOnDown != NULL)
				pb->callbackFuncOnDown(inst);
			
			return (int16_t)i;
		}
	}

	return -1;
}

int16_t testPushButtonMouseRelease(struct ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY, int16_t lastButtonID, bool runCallback)
{
	if (widgets == NULL || lastButtonID < 0 || lastButtonID >= NUM_PUSHBUTTONS)
		return -1;

	if (!widgets->pushButtonVisible[lastButtonID])
		return -1;

	pushButton_t *pb = &pushButtons[lastButtonID];
	widgets->pushButtonState[lastButtonID] = PUSHBUTTON_UNPRESSED;
	drawPushButton(widgets, video, bmp, lastButtonID);

	if (mouseX >= pb->x && mouseX < pb->x + pb->w &&
	    mouseY >= pb->y && mouseY < pb->y + pb->h)
	{
		if (runCallback && pb->callbackFuncOnUp != NULL)
			pb->callbackFuncOnUp(inst);

		return lastButtonID;
	}

	return -1;
}

void handlePushButtonWhileMouseDown(struct ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY, int16_t buttonID,
	bool *firstTimePressingButton, uint8_t *buttonCounter)
{
	if (widgets == NULL || buttonID < 0 || buttonID >= NUM_PUSHBUTTONS)
		return;

	if (!widgets->pushButtonVisible[buttonID])
		return;

	pushButton_t *pb = &pushButtons[buttonID];

	/* Check if mouse is still over the button */
	bool mouseOverButton = (mouseX >= pb->x && mouseX < pb->x + pb->w &&
	                        mouseY >= pb->y && mouseY < pb->y + pb->h);

	/* Update button state and visual */
	uint8_t newState = mouseOverButton ? PUSHBUTTON_PRESSED : PUSHBUTTON_UNPRESSED;
	if (widgets->pushButtonState[buttonID] != newState)
	{
		widgets->pushButtonState[buttonID] = newState;
		drawPushButton(widgets, video, bmp, buttonID);
	}

	/* Handle repeat functionality - match original FT2 behavior from handlePushButtonsWhileMouseDown() */
	if (widgets->pushButtonState[buttonID] != PUSHBUTTON_PRESSED || pb->callbackFuncOnDown == NULL)
		return;

	/* Pre-delay: long delay before repeat starts (BUTTON_DOWN_DELAY = 25 in original) */
	if (pb->preDelay && *firstTimePressingButton)
	{
		if (++(*buttonCounter) >= BUTTON_DOWN_DELAY)
		{
			*buttonCounter = 0;
			*firstTimePressingButton = false;
		}
		return;
	}

	/* Calculate delay frames for repeat rate */
	uint8_t buttonDelay = pb->delayFrames;
	if (buttonDelay == 0)
		buttonDelay = 1;
	
	/* Increment counter and call callback when it wraps */
	if (++(*buttonCounter) >= buttonDelay)
	{
		*buttonCounter = 0;
		pb->callbackFuncOnDown(inst);
	}
}

void changeLogoType(ft2_widgets_t *widgets, const struct ft2_bmp_t *bmp, uint8_t logoType)
{
	if (bmp == NULL || bmp->ft2LogoBadges == NULL)
		return;

	pushButtons[PB_LOGO].bitmapFlag = true;

	if (logoType == 0)
	{
		pushButtons[PB_LOGO].bitmapUnpressed = &bmp->ft2LogoBadges[(154 * 32) * 0];
		pushButtons[PB_LOGO].bitmapPressed = &bmp->ft2LogoBadges[(154 * 32) * 1];
	}
	else
	{
		pushButtons[PB_LOGO].bitmapUnpressed = &bmp->ft2LogoBadges[(154 * 32) * 2];
		pushButtons[PB_LOGO].bitmapPressed = &bmp->ft2LogoBadges[(154 * 32) * 3];
	}

	if (widgets != NULL)
	{
		widgets->pushButtonState[PB_LOGO] = PUSHBUTTON_UNPRESSED;
		widgets->pushButtonVisible[PB_LOGO] = true;
	}
}

void changeBadgeType(ft2_widgets_t *widgets, const struct ft2_bmp_t *bmp, uint8_t badgeType)
{
	if (bmp == NULL || bmp->ft2ByBadges == NULL)
		return;

	pushButtons[PB_BADGE].bitmapFlag = true;

	/* Each badge is 25x32 pixels, 4 frames total:
	 * Frame 0: type 0 unpressed, Frame 1: type 0 pressed
	 * Frame 2: type 1 unpressed, Frame 3: type 1 pressed */
	if (badgeType == 0)
	{
		pushButtons[PB_BADGE].bitmapUnpressed = &bmp->ft2ByBadges[(25 * 32) * 0];
		pushButtons[PB_BADGE].bitmapPressed = &bmp->ft2ByBadges[(25 * 32) * 1];
	}
	else
	{
		pushButtons[PB_BADGE].bitmapUnpressed = &bmp->ft2ByBadges[(25 * 32) * 2];
		pushButtons[PB_BADGE].bitmapPressed = &bmp->ft2ByBadges[(25 * 32) * 3];
	}

	if (widgets != NULL)
	{
		widgets->pushButtonState[PB_BADGE] = PUSHBUTTON_UNPRESSED;
		widgets->pushButtonVisible[PB_BADGE] = true;
	}
}

