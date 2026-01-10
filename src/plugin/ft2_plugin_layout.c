/**
 * @file ft2_plugin_layout.c
 * @brief Main screen layout drawing.
 *
 * Draws the FT2 main screen: position editor, song/pattern controls, menus,
 * status bar, instrument switcher. Two modes: normal and extended pattern editor.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "ft2_plugin_layout.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_instrsw.h"
#include "ft2_plugin_scopes.h"
#include "ft2_plugin_config.h"
#include "ft2_plugin_help.h"
#include "ft2_plugin_nibbles.h"
#include "ft2_plugin_diskop.h"
#include "ft2_plugin_textbox.h"
#include "ft2_plugin_ui.h"
#include "ft2_instance.h"

/* Layout dimensions */
#define POSED_X     0
#define POSED_Y     0
#define POSED_W     112
#define POSED_H     77
#define SONGPATT_X  112
#define SONGPATT_Y  32
#define STATUS_X    0
#define STATUS_Y    77
#define STATUS_W    291
#define STATUS_H    15
#define LEFTMENU_X  291
#define LEFTMENU_Y  0
#define LEFTMENU_W  65
#define LEFTMENU_H  173
#define RIGHTMENU_X 356
#define RIGHTMENU_Y 0
#define RIGHTMENU_W 65
#define RIGHTMENU_H 173

/* Draw position editor: 5 rows showing song position and pattern numbers */
void drawPosEdNums(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL || bmp == NULL) return;

	int16_t songPos = inst->replayer.song.songPos;
	if (songPos >= inst->replayer.song.songLength)
		songPos = inst->replayer.song.songLength - 1;

	bool extended = inst->uiState.extendedPatternEditor;

	/* Clear display areas (different heights for normal vs extended) */
	if (extended) {
		clearRect(video, 8, 4, 39, 16);
		fillRect(video, 8, 23, 39, 7, PAL_DESKTOP);
		clearRect(video, 8, 33, 39, 16);
	} else {
		clearRect(video, 8, 4, 39, 15);
		fillRect(video, 8, 22, 39, 7, PAL_DESKTOP);
		clearRect(video, 8, 32, 39, 15);
	}

	const uint32_t color1 = video->palette[PAL_PATTEXT];
	const uint32_t color2 = video->palette[PAL_FORGRND];
	int16_t rowH = extended ? 9 : 8;

	/* Top two entries (before current) */
	for (int16_t y = 0; y < 2; y++) {
		int16_t entry = songPos - (2 - y);
		if (entry < 0) continue;
		int16_t yPos = 4 + (y * rowH);
		pattTwoHexOut(video, bmp, 8, yPos, (uint8_t)entry, color1);
		pattTwoHexOut(video, bmp, 32, yPos, inst->replayer.song.orders[entry], color1);
	}

	/* Current position (highlighted) */
	int16_t midY = extended ? 23 : 22;
	pattTwoHexOut(video, bmp, 8, midY, (uint8_t)songPos, color2);
	pattTwoHexOut(video, bmp, 32, midY, inst->replayer.song.orders[songPos], color2);

	/* Bottom two entries (after current) */
	for (int16_t y = 0; y < 2; y++) {
		int16_t entry = songPos + (1 + y);
		if (entry >= inst->replayer.song.songLength) break;
		int16_t yPos = (extended ? 33 : 32) + (y * rowH);
		pattTwoHexOut(video, bmp, 8, yPos, (uint8_t)entry, color1);
		pattTwoHexOut(video, bmp, 32, yPos, inst->replayer.song.orders[entry], color1);
	}
}

void drawSongLength(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	int16_t x = inst->uiState.extendedPatternEditor ? 165 : 59;
	int16_t y = inst->uiState.extendedPatternEditor ? 5 : 52;
	hexOutBg(video, bmp, x, y, PAL_FORGRND, PAL_DESKTOP, (uint8_t)inst->replayer.song.songLength, 2);
}

void drawSongLoopStart(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	int16_t x = inst->uiState.extendedPatternEditor ? 165 : 59;
	int16_t y = inst->uiState.extendedPatternEditor ? 19 : 64;
	hexOutBg(video, bmp, x, y, PAL_FORGRND, PAL_DESKTOP, (uint8_t)inst->replayer.song.songLoopStart, 2);
}

/* Decimal string lookup tables */
static const char *dec2StrTab[100] = {
	"00","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15",
	"16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31",
	"32","33","34","35","36","37","38","39","40","41","42","43","44","45","46","47",
	"48","49","50","51","52","53","54","55","56","57","58","59","60","61","62","63",
	"64","65","66","67","68","69","70","71","72","73","74","75","76","77","78","79",
	"80","81","82","83","84","85","86","87","88","89","90","91","92","93","94","95",
	"96","97","98","99"
};

/* 3-digit decimal string lookup table (matches original dec3StrTab) */
static const char *dec3StrTab[256] = {
	"000","001","002","003","004","005","006","007","008","009","010","011","012","013","014","015",
	"016","017","018","019","020","021","022","023","024","025","026","027","028","029","030","031",
	"032","033","034","035","036","037","038","039","040","041","042","043","044","045","046","047",
	"048","049","050","051","052","053","054","055","056","057","058","059","060","061","062","063",
	"064","065","066","067","068","069","070","071","072","073","074","075","076","077","078","079",
	"080","081","082","083","084","085","086","087","088","089","090","091","092","093","094","095",
	"096","097","098","099","100","101","102","103","104","105","106","107","108","109","110","111",
	"112","113","114","115","116","117","118","119","120","121","122","123","124","125","126","127",
	"128","129","130","131","132","133","134","135","136","137","138","139","140","141","142","143",
	"144","145","146","147","148","149","150","151","152","153","154","155","156","157","158","159",
	"160","161","162","163","164","165","166","167","168","169","170","171","172","173","174","175",
	"176","177","178","179","180","181","182","183","184","185","186","187","188","189","190","191",
	"192","193","194","195","196","197","198","199","200","201","202","203","204","205","206","207",
	"208","209","210","211","212","213","214","215","216","217","218","219","220","221","222","223",
	"224","225","226","227","228","229","230","231","232","233","234","235","236","237","238","239",
	"240","241","242","243","244","245","246","247","248","249","250","251","252","253","254","255"
};

/* BPM display. Grayed when synced from DAW, shows native BPM in parens. */
void drawSongBPM(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	if (inst->uiState.extendedPatternEditor) return;

	uint16_t bpm = inst->replayer.song.BPM;
	if (bpm > 255) bpm = 255;
	uint8_t fgColor = inst->config.syncBpmFromDAW ? PAL_DSKTOP2 : PAL_FORGRND;
	textOutFixed(video, bmp, 145, 36, fgColor, PAL_DESKTOP, dec3StrTab[bpm]);

	if (inst->config.syncBpmFromDAW && inst->config.savedBpm > 0) {
		uint16_t nativeBpm = inst->config.savedBpm;
		if (nativeBpm > 255) nativeBpm = 255;
		char nativeStr[8];
		snprintf(nativeStr, sizeof(nativeStr), "(%s)", dec3StrTab[nativeBpm]);
		textOutFixed(video, bmp, 168, 36, fgColor, PAL_DESKTOP, nativeStr);
	} else {
		fillRect(video, 168, 36, 35, 8, PAL_DESKTOP);
	}
}

/* Speed display. Grayed when Fxx changes disabled. */
void drawSongSpeed(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	if (inst->uiState.extendedPatternEditor) return;
	uint16_t spd = inst->replayer.song.speed;
	uint8_t fgColor = inst->config.allowFxxSpeedChanges ? PAL_FORGRND : PAL_DSKTOP2;
	if (spd > 99) spd = 99;
	textOutFixed(video, bmp, 152, 50, fgColor, PAL_DESKTOP, dec2StrTab[spd]);
}

void drawGlobalVol(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	uint16_t y = inst->uiState.extendedPatternEditor ? 56 : 80;
	uint8_t vol = inst->replayer.song.globalVolume;
	if (vol > 64) vol = 64;
	textOutFixed(video, bmp, 87, y, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[vol]);
}

void drawEditPattern(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	int16_t x = inst->uiState.extendedPatternEditor ? 252 : 237;
	int16_t y = inst->uiState.extendedPatternEditor ? 39 : 36;
	hexOutBg(video, bmp, x, y, PAL_FORGRND, PAL_DESKTOP, inst->editor.editPattern, 2);
}

void drawPatternLength(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	int16_t x = inst->uiState.extendedPatternEditor ? 326 : 230;
	int16_t y = inst->uiState.extendedPatternEditor ? 39 : 50;
	uint16_t len = inst->replayer.patternNumRows[inst->editor.editPattern];
	hexOutBg(video, bmp, x, y, PAL_FORGRND, PAL_DESKTOP, len, 3);
}

void drawIDAdd(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	if (inst->uiState.extendedPatternEditor) return;
	uint8_t add = inst->editor.editRowSkip;
	if (add > 16) add = 16;
	textOutFixed(video, bmp, 152, 64, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[add]);
}

void drawOctave(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	fillRect(video, 238, 64, 16, 8, PAL_DESKTOP);
	charOut(video, bmp, 238, 64, PAL_FORGRND, '0' + inst->editor.curOctave);
}

void drawPlaybackTime(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;

	uint32_t totalSeconds = inst->replayer.song.playbackSeconds;
	uint8_t hours = totalSeconds / 3600;
	totalSeconds -= hours * 3600;
	uint8_t minutes = totalSeconds / 60;
	uint8_t seconds = totalSeconds - (minutes * 60);

	uint16_t x = inst->uiState.extendedPatternEditor ? 576 : 235;
	uint16_t y = inst->uiState.extendedPatternEditor ? 56 : 80;

	textOutFixed(video, bmp, x + 0, y, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[hours % 100]);
	textOutFixed(video, bmp, x + 20, y, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[minutes]);
	textOutFixed(video, bmp, x + 40, y, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[seconds]);
}

void drawSongName(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	if (inst->uiState.extendedPatternEditor) return;
	drawFramework(video, 421, 155, 166, 18, FRAMEWORK_TYPE1);
	drawFramework(video, 423, 157, 162, 14, FRAMEWORK_TYPE2);
	ft2_textbox_draw(video, bmp, TB_SONG_NAME, inst);
}

/* ---------- Main screen sections ---------- */

/* Top-left: position editor, logo, left menu, song/pattern controls, status bar */
void drawTopLeftMainScreen(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, bool restoreScreens)
{
	if (inst == NULL || video == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	(void)restoreScreens;

	/* Position editor */
	drawFramework(video, POSED_X, POSED_Y, POSED_W, POSED_H, FRAMEWORK_TYPE1);
	drawFramework(video, 2, 2, 51, 19, FRAMEWORK_TYPE2);
	drawFramework(video, 2, 30, 51, 19, FRAMEWORK_TYPE2);
	textOutShadow(video, bmp, 4, 52, PAL_FORGRND, PAL_DSKTOP2, "Songlen.");
	textOutShadow(video, bmp, 4, 64, PAL_FORGRND, PAL_DSKTOP2, "Repstart");
	drawPosEdNums(inst, video, bmp);
	drawSongLength(inst, video, bmp);
	drawSongLoopStart(inst, video, bmp);
	showScrollBar(widgets, video, SB_POS_ED);
	showPushButton(widgets, video, bmp, PB_POSED_POS_UP);
	showPushButton(widgets, video, bmp, PB_POSED_POS_DOWN);
	showPushButton(widgets, video, bmp, PB_POSED_INS);
	showPushButton(widgets, video, bmp, PB_POSED_PATT_UP);
	showPushButton(widgets, video, bmp, PB_POSED_PATT_DOWN);
	showPushButton(widgets, video, bmp, PB_POSED_DEL);
	showPushButton(widgets, video, bmp, PB_POSED_LEN_UP);
	showPushButton(widgets, video, bmp, PB_POSED_LEN_DOWN);
	showPushButton(widgets, video, bmp, PB_POSED_REP_UP);
	showPushButton(widgets, video, bmp, PB_POSED_REP_DOWN);

	/* Logo buttons */
	changeLogoType(widgets, bmp, inst->config.id_FastLogo);
	changeBadgeType(widgets, bmp, inst->config.id_TritonProd);
	showPushButton(widgets, video, bmp, PB_LOGO);
	showPushButton(widgets, video, bmp, PB_BADGE);

	/* Left menu */
	drawFramework(video, LEFTMENU_X, LEFTMENU_Y, LEFTMENU_W, LEFTMENU_H, FRAMEWORK_TYPE1);
	showPushButton(widgets, video, bmp, PB_ABOUT);
	showPushButton(widgets, video, bmp, PB_NIBBLES);
	showPushButton(widgets, video, bmp, PB_KILL);
	showPushButton(widgets, video, bmp, PB_TRIM);
	showPushButton(widgets, video, bmp, PB_EXTEND_VIEW);
	showPushButton(widgets, video, bmp, PB_TRANSPOSE);
	showPushButton(widgets, video, bmp, PB_INST_ED_EXT);
	showPushButton(widgets, video, bmp, PB_SMP_ED_EXT);
	showPushButton(widgets, video, bmp, PB_ADV_EDIT);
	showPushButton(widgets, video, bmp, PB_ADD_CHANNELS);
	showPushButton(widgets, video, bmp, PB_SUB_CHANNELS);

	/* Song/pattern controls */
	drawFramework(video, 112, 32, 94, 45, FRAMEWORK_TYPE1);
	drawFramework(video, 206, 32, 85, 45, FRAMEWORK_TYPE1);
	if (!inst->config.syncBpmFromDAW) {
		showPushButton(widgets, video, bmp, PB_BPM_UP);
		showPushButton(widgets, video, bmp, PB_BPM_DOWN);
	}
	showPushButton(widgets, video, bmp, PB_SPEED_UP);
	showPushButton(widgets, video, bmp, PB_SPEED_DOWN);
	/* Lock speed buttons when Fxx disabled (toggle 3-6) */
	if (!inst->config.allowFxxSpeedChanges) {
		widgets->pushButtonLocked[PB_SPEED_UP] = (inst->config.lockedSpeed == 6);
		widgets->pushButtonLocked[PB_SPEED_DOWN] = (inst->config.lockedSpeed == 3);
	} else {
		widgets->pushButtonLocked[PB_SPEED_UP] = false;
		widgets->pushButtonLocked[PB_SPEED_DOWN] = false;
	}
	showPushButton(widgets, video, bmp, PB_EDITADD_UP);
	showPushButton(widgets, video, bmp, PB_EDITADD_DOWN);
	showPushButton(widgets, video, bmp, PB_PATT_UP);
	showPushButton(widgets, video, bmp, PB_PATT_DOWN);
	showPushButton(widgets, video, bmp, PB_PATTLEN_UP);
	showPushButton(widgets, video, bmp, PB_PATTLEN_DOWN);
	showPushButton(widgets, video, bmp, PB_PATT_EXPAND);
	showPushButton(widgets, video, bmp, PB_PATT_SHRINK);
	textOutShadow(video, bmp, 116, 36, PAL_FORGRND, PAL_DSKTOP2, "BPM");
	textOutShadow(video, bmp, 116, 50, PAL_FORGRND, PAL_DSKTOP2, "Spd.");
	textOutShadow(video, bmp, 116, 64, PAL_FORGRND, PAL_DSKTOP2, "Add.");
	textOutShadow(video, bmp, 210, 36, PAL_FORGRND, PAL_DSKTOP2, "Ptn.");
	textOutShadow(video, bmp, 210, 50, PAL_FORGRND, PAL_DSKTOP2, "Ln.");
	drawSongBPM(inst, video, bmp);
	drawSongSpeed(inst, video, bmp);
	drawEditPattern(inst, video, bmp);
	drawPatternLength(inst, video, bmp);
	drawIDAdd(inst, video, bmp);

	/* Status bar */
	drawFramework(video, STATUS_X, STATUS_Y, STATUS_W, STATUS_H, FRAMEWORK_TYPE1);
	textOutShadow(video, bmp, 4, 80, PAL_FORGRND, PAL_DSKTOP2, "Global volume");
	drawGlobalVol(inst, video, bmp);
	textOutShadow(video, bmp, 204, 80, PAL_FORGRND, PAL_DSKTOP2, "Time");
	charOutShadow(video, bmp, 250, 80, PAL_FORGRND, PAL_DSKTOP2, ':');
	charOutShadow(video, bmp, 270, 80, PAL_FORGRND, PAL_DSKTOP2, ':');
	drawPlaybackTime(inst, video, bmp);
}

/* Top-right: right menu buttons, instrument switcher, song name */
void drawTopRightMainScreen(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;

	drawFramework(video, RIGHTMENU_X, RIGHTMENU_Y, RIGHTMENU_W, RIGHTMENU_H, FRAMEWORK_TYPE1);
	showPushButton(widgets, video, bmp, PB_PLAY_SONG);
	showPushButton(widgets, video, bmp, PB_PLAY_PATT);
	showPushButton(widgets, video, bmp, PB_STOP);
	showPushButton(widgets, video, bmp, PB_RECORD_SONG);
	showPushButton(widgets, video, bmp, PB_RECORD_PATT);
	showPushButton(widgets, video, bmp, PB_DISK_OP);
	showPushButton(widgets, video, bmp, PB_INST_ED);
	showPushButton(widgets, video, bmp, PB_SMP_ED);
	showPushButton(widgets, video, bmp, PB_CONFIG);
	showPushButton(widgets, video, bmp, PB_HELP);

	inst->uiState.instrSwitcherShown = true;
	showInstrumentSwitcher(inst, video, bmp);
	drawSongName(inst, video, bmp);
}

/* Extended pattern editor: compact top bar with pos editor, song info, instruments */
void drawTopScreenExtended(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;

	/* Frameworks */
	drawFramework(video, 0, 0, 112, 53, FRAMEWORK_TYPE1);
	drawFramework(video, 2, 2, 51, 20, FRAMEWORK_TYPE2);
	drawFramework(video, 2, 31, 51, 20, FRAMEWORK_TYPE2);
	drawFramework(video, 112, 0, 106, 33, FRAMEWORK_TYPE1);
	drawFramework(video, 112, 33, 106, 20, FRAMEWORK_TYPE1);
	drawFramework(video, 218, 0, 168, 53, FRAMEWORK_TYPE1);
	drawFramework(video, 386, 0, 246, 53, FRAMEWORK_TYPE1);
	drawFramework(video, 388, 2, 118, 49, FRAMEWORK_TYPE2);
	drawFramework(video, 509, 2, 118, 49, FRAMEWORK_TYPE2);
	drawFramework(video, 0, 53, SCREEN_W, 15, FRAMEWORK_TYPE1);

	/* Labels */
	textOutShadow(video, bmp, 116, 5, PAL_FORGRND, PAL_DSKTOP2, "Sng.len.");
	textOutShadow(video, bmp, 116, 19, PAL_FORGRND, PAL_DSKTOP2, "Repst.");
	textOutShadow(video, bmp, 222, 39, PAL_FORGRND, PAL_DSKTOP2, "Ptn.");
	textOutShadow(video, bmp, 305, 39, PAL_FORGRND, PAL_DSKTOP2, "Ln.");
	textOutShadow(video, bmp, 4, 56, PAL_FORGRND, PAL_DSKTOP2, "Global volume");
	textOutShadow(video, bmp, 545, 56, PAL_FORGRND, PAL_DSKTOP2, "Time");
	charOutShadow(video, bmp, 591, 56, PAL_FORGRND, PAL_DSKTOP2, ':');
	charOutShadow(video, bmp, 611, 56, PAL_FORGRND, PAL_DSKTOP2, ':');

	/* Widgets */
	showScrollBar(widgets, video, SB_POS_ED);
	showPushButton(widgets, video, bmp, PB_POSED_POS_UP);
	showPushButton(widgets, video, bmp, PB_POSED_POS_DOWN);
	showPushButton(widgets, video, bmp, PB_POSED_INS);
	showPushButton(widgets, video, bmp, PB_POSED_PATT_UP);
	showPushButton(widgets, video, bmp, PB_POSED_PATT_DOWN);
	showPushButton(widgets, video, bmp, PB_POSED_DEL);
	showPushButton(widgets, video, bmp, PB_POSED_LEN_UP);
	showPushButton(widgets, video, bmp, PB_POSED_LEN_DOWN);
	showPushButton(widgets, video, bmp, PB_POSED_REP_UP);
	showPushButton(widgets, video, bmp, PB_POSED_REP_DOWN);
	showPushButton(widgets, video, bmp, PB_SWAP_BANK);
	showPushButton(widgets, video, bmp, PB_PATT_UP);
	showPushButton(widgets, video, bmp, PB_PATT_DOWN);
	showPushButton(widgets, video, bmp, PB_PATTLEN_UP);
	showPushButton(widgets, video, bmp, PB_PATTLEN_DOWN);
	showPushButton(widgets, video, bmp, PB_EXIT_EXT_PATT);

	/* Values */
	drawPosEdNums(inst, video, bmp);
	drawSongLength(inst, video, bmp);
	drawSongLoopStart(inst, video, bmp);
	drawEditPattern(inst, video, bmp);
	drawPatternLength(inst, video, bmp);
	drawGlobalVol(inst, video, bmp);
	drawPlaybackTime(inst, video, bmp);

	inst->uiState.instrSwitcherShown = true;
	showInstrumentSwitcher(inst, video, bmp);
	inst->uiState.updatePosSections = true;
}

/* Main entry: dispatch to appropriate screen based on UI state */
void drawTopScreen(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, bool restoreScreens)
{
	if (inst == NULL || video == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->uiState.scopesShown = false;

	if (inst->uiState.extendedPatternEditor) {
		drawTopScreenExtended(inst, video, bmp);
		return;
	}

	if (inst->uiState.aboutScreenShown) {
		drawFramework(video, 0, 0, 632, 173, FRAMEWORK_TYPE1);
		drawFramework(video, 2, 2, 628, 169, FRAMEWORK_TYPE2);
		showPushButton(widgets, video, bmp, PB_EXIT_ABOUT);
	} else if (inst->uiState.configScreenShown) {
		drawConfigScreen(inst, video, bmp);
	} else if (inst->uiState.helpScreenShown) {
		drawHelpScreen(inst, video, bmp);
	} else if (inst->uiState.nibblesShown) {
		ft2_nibbles_show(inst, video, bmp);
		if (inst->nibbles.playing) ft2_nibbles_redraw(inst, video, bmp);
		else if (inst->uiState.nibblesHelpShown) ft2_nibbles_show_help(inst, video, bmp);
		else if (inst->uiState.nibblesHighScoresShown) ft2_nibbles_show_highscores(inst, video, bmp);
	} else if (inst->uiState.diskOpShown) {
		drawDiskOpScreen(inst, video, bmp);
		drawTopRightMainScreen(inst, video, bmp);
	} else {
		drawTopLeftMainScreen(inst, video, bmp, restoreScreens);
		drawTopRightMainScreen(inst, video, bmp);
		inst->uiState.scopesShown = true;
	}
}

/* Bottom screen placeholder (actual drawing handled by respective modules) */
void drawBottomScreen(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;
	/* Pattern/instrument/sample editors handle their own drawing */
}

/* Full GUI redraw */
void drawGUILayout(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	setScrollBarPos(inst, widgets, video, SB_POS_ED, 0, false);
	drawTopScreen(inst, video, bmp, false);
	drawBottomScreen(inst, video, bmp);
	inst->uiState.updatePosSections = true;
}

