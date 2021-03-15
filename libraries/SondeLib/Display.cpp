#include <U8x8lib.h>
#include <U8g2lib.h>
#include <SPIFFS.h>
#include <axp20x.h>
#include <MicroNMEA.h>
#include "Display.h"
#include "Sonde.h"
#include "math.h"


int readLine(Stream &stream, char *buffer, int maxlen);

extern const char *version_name;
extern const char *version_id;

#include <fonts/FreeMono9pt7b.h>
#include <fonts/FreeMono12pt7b.h>
#include <fonts/FreeSans9pt7b.h>
#include <fonts/FreeSans12pt7b.h>
#include <fonts/Picopixel.h>

extern Sonde sonde;

extern MicroNMEA nmea;

extern AXP20X_Class axp;
extern bool axp192_found;
extern SemaphoreHandle_t axpSemaphore;


SPIClass spiDisp(HSPI);

const char *sondeTypeStr[NSondeTypes] = { "DFM6", "DFM9", "RS41", "RS92", "M10 ", "M20 ", "DFM " };
const char sondeTypeChar[NSondeTypes] = { '6', '9', '4', 'R', 'M', '2', 'D' };

byte myIP_tiles[8*11];
static uint8_t ap_tile[8]={0x00,0x04,0x22,0x92, 0x92, 0x22, 0x04, 0x00};

static const uint8_t font[10][5]={
  0x3E, 0x51, 0x49, 0x45, 0x3E,   // 0
  0x00, 0x42, 0x7F, 0x40, 0x00,   // 1
  0x42, 0x61, 0x51, 0x49, 0x46,   // 2
  0x21, 0x41, 0x45, 0x4B, 0x31,   // 3
  0x18, 0x14, 0x12, 0x7F, 0x10,   // 4
  0x27, 0x45, 0x45, 0x45, 0x39,   // 5
  0x3C, 0x4A, 0x49, 0x49, 0x30,   // 6
  0x01, 0x01, 0x79, 0x05, 0x03,   // 7
  0x36, 0x49, 0x49, 0x49, 0x36,   // 8
  0x06, 0x49, 0x39, 0x29, 0x1E }; // 9;  .=0x40


static unsigned char kmh_tiles[] U8X8_PROGMEM = {
   0x1F, 0x04, 0x0A, 0x11, 0x00, 0x1F, 0x02, 0x04, 0x42, 0x3F, 0x10, 0x08, 0xFC, 0x22, 0x20, 0xF8
   };
static unsigned char ms_tiles[] U8X8_PROGMEM = {
   0x1F, 0x02, 0x04, 0x02, 0x1F, 0x40, 0x20, 0x10, 0x08, 0x04, 0x12, 0xA8, 0xA8, 0xA4, 0x40, 0x00
   };
static unsigned char stattiles[5][4] =  {
   0x00, 0x1F, 0x00, 0x00 ,   // | == ok
   0x00, 0x10, 0x10, 0x00 ,   // . == no header found
   0x1F, 0x15, 0x15, 0x00 ,   // E == decode error
   0x00, 0x00, 0x00, 0x00 ,   // ' ' == unknown/unassigned
   0x07, 0x05, 0x07, 0x00 };  // ° = rx ok, but no valid position
static unsigned char stattilesXL[5][5] =  {
   0x00, 0x7F, 0x00, 0x00, 0x00,  // | == ok
   0x00, 0x40, 0x40, 0x00, 0x00,  // . == no header found
   0x7F, 0x49, 0x49, 0x49, 0x00,  // E == decode error
   0x00, 0x00, 0x00, 0x00, 0x00,   // ' ' == unknown/unassigned
   0x07, 0x05, 0x07, 0x00, 0x00 };  // ° = rx ok, but no valid position (not yet used?)


//static uint8_t halfdb_tile[8]={0x80, 0x27, 0x45, 0x45, 0x45, 0x39, 0x00, 0x00};

static uint8_t halfdb_tile1[8]={0x00, 0x38, 0x28, 0x28, 0x28, 0xC8, 0x00, 0x00};
static uint8_t halfdb_tile2[8]={0x00, 0x11, 0x02, 0x02, 0x02, 0x01, 0x00, 0x00};

//static uint8_t empty_tile[8]={0x80, 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x00};

static uint8_t empty_tile1[8]={0x00, 0xF0, 0x88, 0x48, 0x28, 0xF0, 0x00, 0x00};
static uint8_t empty_tile2[8]={0x00, 0x11, 0x02, 0x02, 0x02, 0x01, 0x00, 0x00};

//static uint8_t gps_tile[8]={0x3E, 0x77, 0x63, 0x77, 0x3E, 0x1C, 0x08, 0x00};
static uint8_t gps_tile[8]={0x00, 0x0E, 0x1F, 0x3B, 0x71, 0x3B, 0x1F, 0x0E};
static uint8_t nogps_tile[8]={0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41, 0x00};

static uint8_t deg_tile[8]={0x00, 0x06,0x09, 0x09, 0x06, 0x00, 0x00, 0x00};


/* Description of display layouts.
 * for each display, the content is described by a DispEntry structure
 * timeout values are in milliseconds, for view activ, rx signal present, no rx signal present
 * for each displey, actions (switching to different sonde or different view) can be defined
 * based on key presses or on expired timeouts
 */
DispEntry searchLayout[] = {
	{0, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawText, "Scan:"},
	{0, 8, FONT_LARGE, -1, 0xFFFF, 0, disp.drawType, NULL},
	{3, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawFreq, " MHz"},
	{5, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawSite, "l"},
	{7, 5, 0, -1, 0xFFFF, 0, disp.drawIP, NULL},	
	{-1, -1, -1, 0, 0, 0, NULL, NULL},
};
int16_t searchTimeouts[] = { -1, 0, 0 };
uint8_t searchActions[] = {
	ACT_NONE,
	ACT_DISPLAY_DEFAULT, ACT_NONE, ACT_DISPLAY_SPECTRUM, ACT_DISPLAY_WIFI,
	ACT_NONE, ACT_NONE, ACT_NONE, ACT_NONE,
	ACT_NONE, ACT_DISPLAY_DEFAULT, ACT_NEXTSONDE};
DispEntry legacyLayout[] = {
	{0, 5, FONT_SMALL, -1, 0xFFFF, 0, disp.drawFreq, " MHz"},
	{1, 8, FONT_SMALL, -1, 0xFFFF, 0, disp.drawAFC, NULL},
	{0, 0, FONT_SMALL, -1, 0xFFFF, 0, disp.drawType, NULL},
	{1, 0, FONT_SMALL, -1, 0xFFFF, 0, disp.drawID, NULL},
	{2, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawLat, NULL},
	{4, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawLon, NULL},
	{2, 10, FONT_SMALL, -1, 0xFFFF, 0, disp.drawAlt, NULL},
	{3, 10, FONT_SMALL, -1, 0xFFFF, 0, disp.drawHS, NULL},
	{4, 9, FONT_SMALL, -1, 0xFFFF, 0, disp.drawVS, NULL},
	{6, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawRSSI, NULL},
	{6, 7, 0, -1, 0xFFFF, 0, disp.drawQS, NULL},
	{7, 0, FONT_SMALL, -1, 0xFFFF, 0, disp.drawVBat, "%"},
	{7, 5, 0, -1, 0xFFFF, 0, disp.drawIP, NULL},	
	{-1, -1, -1, 0, 0, 0, NULL, NULL},
};
int16_t legacyTimeouts[] = { -1, -1, 20000 };
uint8_t legacyActions[] = {
	ACT_NONE,
	ACT_NEXTSONDE, ACT_DISPLAY(0), ACT_DISPLAY_SPECTRUM, ACT_DISPLAY_WIFI,
	ACT_DISPLAY(2), ACT_NONE, ACT_NONE, ACT_NONE,
	ACT_NONE, ACT_NONE, ACT_DISPLAY(0)};
DispEntry fieldLayout[] = {
	{2, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawLat, NULL},
	{4, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawLon, NULL},
	{3, 10, FONT_SMALL, -1, 0xFFFF, 0, disp.drawHS, NULL},
	{4, 9, FONT_SMALL, -1, 0xFFFF, 0, disp.drawVS, NULL},
	{0, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawID, NULL},
	{6, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawAlt, NULL},
	{6, 7, 0, -1, 0xFFFF, 0, disp.drawQS, NULL},
	{-1, -1, -1, 0, 0, 0, NULL, NULL},
};

int16_t fieldTimeouts[] = { -1, -1, -1 };
uint8_t fieldActions[] = {
	ACT_NONE,
	ACT_NEXTSONDE, ACT_DISPLAY(0), ACT_DISPLAY_SPECTRUM, ACT_DISPLAY_WIFI,
	ACT_DISPLAY(4), ACT_NONE, ACT_NONE, ACT_NONE,
	ACT_NONE, ACT_NONE, ACT_NONE};
DispEntry field2Layout[] = {
	{2, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawLat, NULL},
	{4, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawLon, NULL},
	{1, 12, FONT_SMALL, -1, 0xFFFF, 0, disp.drawType, NULL},
	{0, 9, FONT_SMALL, -1, 0xFFFF, 0, disp.drawFreq, ""},
	{3, 10, FONT_SMALL, -1, 0xFFFF, 0, disp.drawHS, NULL},
	{4, 9, FONT_SMALL, -1, 0xFFFF, 0, disp.drawVS, NULL},
	{0, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawID, NULL},
	{6, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawAlt, NULL},
	{6, 7, 0, -1, 0xFFFF, 0, disp.drawQS, NULL},
	{-1, -1, -1, 0, 0, 0, NULL, NULL},
};
uint8_t field2Actions[] = {
	ACT_NONE,
	ACT_NEXTSONDE, ACT_DISPLAY(0), ACT_DISPLAY_SPECTRUM, ACT_DISPLAY_WIFI,
	ACT_DISPLAY(1), ACT_NONE, ACT_NONE, ACT_NONE,
	ACT_NONE, ACT_NONE, ACT_NONE};
DispEntry gpsLayout[] = {
	{0, 0, FONT_LARGE, -1, 0xFFFF, 0, disp.drawID, NULL},
	{2, 0, FONT_SMALL, -1, 0xFFFF, 0, disp.drawLat, NULL},
	{3, 0, FONT_SMALL, -1, 0xFFFF, 0, disp.drawLon, NULL},
	{4, 0, FONT_SMALL, -1, 0xFFFF, 0, disp.drawAlt, NULL},
	//{6, 0, FONT_SMALL, -1, 0xFFFF, 0, disp.drawGPS, "V"},
	{6, 1, FONT_SMALL, -1, 0xFFFF, 0,disp.drawGPS, "A"},
	{6, 8, FONT_SMALL, -1, 0xFFFF, 0,disp.drawGPS, "O"},
	{7, 0, FONT_SMALL, -1, 0xFFFF, 0, disp.drawGPS, "D"},
	{7, 8, FONT_SMALL, -1, 0xFFFF, 0, disp.drawGPS, "I"},
	{-1, -1, -1, 0, 0, 0, NULL, NULL},
};
uint8_t gpsActions[] = {
	ACT_NONE,
	ACT_NEXTSONDE, ACT_DISPLAY(0), ACT_DISPLAY_SPECTRUM, ACT_DISPLAY_WIFI,
	ACT_DISPLAY(1), ACT_NONE, ACT_NONE, ACT_NONE,
	ACT_NONE, ACT_NONE, ACT_NONE};

DispInfo staticLayouts[5] = {
  { searchLayout, searchActions, searchTimeouts, "StaticSearch" },
  { legacyLayout, legacyActions, legacyTimeouts, "StaticLegacy" },
  { fieldLayout, fieldActions, fieldTimeouts, "StaticField1" },
  { field2Layout, field2Actions, fieldTimeouts, "StaticFiel2" },
  { gpsLayout, gpsActions, fieldTimeouts, "StaticGPS" } };


/////////////// Wrapper code for various display

// ALLFONTS requires 30k extra flash memory... for now there is still enough space :)
#define ALLFONTS 1
static const uint8_t *fl[] = { 
                u8x8_font_chroma48medium8_r,        // 0 ** default small
                u8x8_font_7x14_1x2_f,               // 1 ** default large
#ifdef ALLFONTS
                u8x8_font_amstrad_cpc_extended_f,   // 2
                u8x8_font_5x7_f,                    // 3
                u8x8_font_5x8_f,                    // 4
                u8x8_font_8x13_1x2_f,               // 5
                u8x8_font_8x13B_1x2_f,              // 6
                u8x8_font_7x14B_1x2_f,              // 7
                u8x8_font_artossans8_r,             // 8
                u8x8_font_artosserif8_r,            // 9
                u8x8_font_torussansbold8_r,         // 10 
                u8x8_font_victoriabold8_r,          // 11
                u8x8_font_victoriamedium8_r,        // 12
                u8x8_font_pressstart2p_f,           // 13
                u8x8_font_pcsenior_f,               // 14
                u8x8_font_pxplusibmcgathin_f,       // 15
                u8x8_font_pxplusibmcga_f,           // 16
                u8x8_font_pxplustandynewtv_f,       // 17
#endif
};


void U8x8Display::begin() {
	Serial.printf("Init SSD1306 display %d %d\n", sonde.config.oled_scl, sonde.config.oled_sda);
	//u8x8 = new U8X8_SSD1306_128X64_NONAME_SW_I2C(/* clock=*/ sonde.config.oled_scl, /* data=*/ sonde.config.oled_sda, /* reset=*/ sonde.config.oled_rst); // Unbuffered, basic graphics, software I2C
	if (_type==2) {
               	u8x8 = new U8X8_SH1106_128X64_NONAME_HW_I2C(/* reset=*/ sonde.config.oled_rst, /* clock=*/ sonde.config.oled_scl, /* data=*/ sonde.config.oled_sda); // Unbuffered, basic graphics, software I2C
	} else { //__type==0 or anything else
		u8x8 = new U8X8_SSD1306_128X64_NONAME_HW_I2C(/* reset=*/ sonde.config.oled_rst, /* clock=*/ sonde.config.oled_scl, /* data=*/ sonde.config.oled_sda); // Unbuffered, basic graphics, software I2C
	} 
	u8x8->begin();
	if(sonde.config.oled_orient==3) u8x8->setFlipMode(true);

	fontlist = fl;
	nfonts = sizeof(fl)/sizeof(uint8_t *);
	Serial.printf("Size of font list is %d\n", nfonts);
}

void U8x8Display::clear() {
	u8x8->clear();
}


// For u8x8 oled display: 0=small font, 1=large font 7x14
void U8x8Display::setFont(uint8_t fontindex) {
	if(fontindex>=nfonts) fontindex=0; // prevent overflow
	u8x8->setFont( fontlist[fontindex] );
}

void U8x8Display::getDispSize(uint8_t *height, uint8_t *width, uint8_t *lineskip, uint8_t *colskip) {
	// TODO: maybe we should decided depending on font size (single/double?)
        if(height) *height = 8;
        if(width) *width = 16;
	if(lineskip) *lineskip = 1;
	if(colskip) *colskip = 1;
}

void U8x8Display::drawString(uint8_t x, uint8_t y, const char *s, int16_t width, uint16_t fg, uint16_t bg) {
	u8x8->drawString(x, y, s);
}

void U8x8Display::drawTile(uint8_t x, uint8_t y, uint8_t cnt, uint8_t *tile_ptr) {
	u8x8->drawTile(x, y, cnt, tile_ptr);
}

void U8x8Display::drawBitmap(uint16_t x1, uint16_t y1, const uint16_t* bitmap, int16_t w, int16_t h) {
	// not supported
}
void U8x8Display::drawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color, bool fill) {
	// not supported (yet)
}

void U8x8Display::welcome() {
	u8x8->clear();
  	setFont(FONT_LARGE);
  	drawString(0, 1, version_name);
  	drawString(11, 1, version_id);
  	setFont(FONT_SMALL);
	drawString(0, 4, "RSx,DFMx,M10&20");
  	drawString(0, 6, "by Vigor & Xav ");
  	//drawString(11, 6, version_id);
}

static String previp;
void U8x8Display::drawIP(uint8_t x, uint8_t y, int16_t width, uint16_t fg, uint16_t bg) {
	if(!previp.equals(sonde.ipaddr)) {
		// ip address has changed
		// create tiles
  		memset(myIP_tiles, 0, 11*8);
		int len = sonde.ipaddr.length();
		const char *ip = sonde.ipaddr.c_str();
		int pix = (len-3)*6+6;
		int tp = 80-pix+8;
		//if(sonde.isAP) memcpy(myIP_tiles+(tp<16?0:8), ap_tile, 8);
		memcpy(myIP_tiles+(tp<16?0:8), ap_tile, 8);
		for(int i=0; i<len; i++) {
			if(ip[i]=='.') { myIP_tiles[tp++]=0x40; myIP_tiles[tp++]=0x00; }
        		else {
          			int idx = ip[i]-'0';
				memcpy(myIP_tiles+tp, &font[idx], 5);
				myIP_tiles[tp+5] = 0;
				tp+=6;
        		}
		}
		while(tp<8*10) { myIP_tiles[tp++]=0; }
			previp = sonde.ipaddr;
	}
	// draw tiles
	u8x8->drawTile(x, y, 11, myIP_tiles);
}

// len must be multiple of 2, size is fixed for u8x8 display
void U8x8Display::drawQS(uint8_t x, uint8_t y, uint8_t len, uint8_t /*size*/, uint8_t *stat, uint16_t fg, uint16_t bg) {
	for(int i=0; i<len; i+=2) {
	        uint8_t tile[8];
	        *(uint32_t *)(&tile[0]) = *(uint32_t *)(&(stattiles[stat[i]]));
	        *(uint32_t *)(&tile[4]) = *(uint32_t *)(&(stattiles[stat[i+1]]));
	        drawTile(x+i/2, y, 1, tile);
	}
}


const GFXfont *gfl[] = {
	&FreeMono9pt7b,		// 3
	&FreeMono12pt7b,	// 4
	&FreeSans9pt7b,		// 5
	&FreeSans12pt7b,	// 6
	&Picopixel,             // 7
};
struct gfxoffset_t {
	uint8_t yofs, yclear;
};
// obtained as max offset from font (last column) and maximum height (3rd column) in glyphs
// first value: offset: max offset from font glyphs (last column * (-1))   (check /, \, `, $)`
// yclear:max height: max of (height in 3rd column) + (yofs + 6th column)  (check j)
const struct gfxoffset_t gfxoffsets[]={
	{ 11, 15 },  // 13+11-9 "j"
	{ 15, 20 },  // 19+15-14
        { 13, 18 },  // 17+13-12 "j" 
        { 17, 23 }, // 23+17-17
        {  4, 6},       // 6+4-4
};
static int ngfx = sizeof(gfl)/sizeof(GFXfont *);
	
#include <pgmspace.h>
#define pgm_read_pointer(addr) ((void *)pgm_read_dword(addr))

///////////////


char Display::buf[17];
char Display::lineBuf[Display::LINEBUFLEN];

RawDisplay *Display::rdis = NULL;

//TODO: maybe merge with initFromFile later?
void Display::init() {
	Serial.printf("disptype is %d\n",sonde.config.disptype);
	rdis = new U8x8Display(sonde.config.disptype);
	Serial.println("Display created");
	rdis->begin();
	delay(100);
	Serial.println("Display initialized");
	rdis->clear();
}


Display::Display() {
	layouts = staticLayouts;
	setLayout(0);
}

#define MAXSCREENS 20
#define DISP_ACTIONS_N 12
#define DISP_TIMEOUTS_N 3

void Display::replaceLayouts(DispInfo *newlayouts, int nnew) {
	if(nnew<1) return;  // no new layouts => ignore

	// remember old layouts
	DispInfo *old = layouts;

	// assign new layouts and current layout
	Serial.printf("replaceLayouts: idx=%d n(new)=%d\n", layoutIdx, nLayouts);
	layouts = newlayouts;
	nLayouts = nnew;
	if(layoutIdx >= nLayouts) layoutIdx = 0;
	layout = layouts+layoutIdx;

	// Make it unlikely that anyone else is still using previous layouts
	delay(500);

	// and release memory not used any more
	if(old==staticLayouts) return;
	for(int i=0; i<MAXSCREENS; i++) {
		if(old[i].de) free(old[i].de);
	}
	free(old);
}

int Display::allocDispInfo(int entries, DispInfo *d, char *label)
{
	int totalsize = (entries+1)*sizeof(DispEntry) + DISP_ACTIONS_N*sizeof(uint8_t) + DISP_TIMEOUTS_N * sizeof(int16_t);
	char *mem = (char *)malloc(totalsize);
	if(!mem) return -1;
	memset (mem, 0, totalsize);

	d->de = (DispEntry *)mem;
	mem += (entries+1) * sizeof(DispEntry);

	d->actions = (uint8_t *)mem;
	mem += DISP_ACTIONS_N * sizeof(uint8_t);
	d->actions[0] = ACT_NONE;

	d->timeouts = (int16_t *)mem;

	d->label = label;
	Serial.printf("allocated %d bytes (%d entries) for %p (addr=%p)\n", totalsize, entries, d, d->de);
	return 0;
}

uint16_t encodeColor(uint32_t col) {
	return (col>>19) << 11 | ((col>>10)&0x3F) << 5 | ((col>>3)&0x1F);
}
uint16_t encodeColor(char *colstr) {
	uint32_t col;
	int res=sscanf(colstr, "%" SCNx32, &col);
	if(res!=1) return 0xffff;
	return encodeColor(col);
}

void Display::parseDispElement(char *text, DispEntry *de)
{
	char type = *text;
	if(type>='A'&&type<='Z') {
		type += 32;  // lc
		de->fmt = fontlar;
	} else {
		de->fmt = fontsma;
	}
	de->fg = colfg;
	de->bg = colbg;
	switch(type) {
	case 'l':
		de->func = disp.drawLat; 
		break;
	case 'o':
		de->func = disp.drawLon; 
		break;
	case 'a':
		de->func = disp.drawAlt; 
		break;
	case 'h':
		de->func = disp.drawHS; 
		de->extra = text[1]?strdup(text+1):NULL; 
		break;
	case 'v':
		de->func = disp.drawVS; 
		de->extra = text[1]?strdup(text+1):NULL; 
		break;
	case 'i':
		de->func = disp.drawID;
		de->extra = strdup(text+1);
		break;
	case 'q':
		{
		struct StatInfo *statinfo = (struct StatInfo *)malloc(sizeof(struct StatInfo));
		// maybe enable more flexible configuration?
		statinfo->size=3;
		statinfo->len=18;
		if(text[1]=='4') statinfo->size = 4;

		de->extra = (const char *)statinfo;
		de->func = disp.drawQS;
		}
		break;
	case 't':
		de->func = disp.drawType; 
		break;
	case 'c':
		de->func = disp.drawAFC; 
		break;
	case 'y':
		de->func = disp.drawVBat;
		de->extra = strdup(text+1); 
		break;
	case 'f':
		de->func = disp.drawFreq;
		de->extra = strdup(text+1);
		Serial.printf("parsing 'f' entry: extra is '%s'\n", de->extra);
		break;
	case 'n':
		// IP address / small always uses tiny font on TFT for backward compatibility
		// Large font can be used arbitrarily
		if(de->fmt==fontsma) de->fmt=0;
		de->func = disp.drawIP; break;
	case 's':
		de->func = disp.drawSite;
		de->extra = strdup(text+1);
		break;
	case 'k':
		de->func = disp.drawKilltimer;
		de->extra = strdup(text+1);
		break;
	case 'g':
		de->func = disp.drawGPS;
		de->extra = strdup(text+1);
		Serial.printf("parsing 'g' entry: extra is '%s'\n", de->extra);
		break;
	case 'r':
		de->func = disp.drawRSSI; 
		break;
	case 'x':
		de->func = disp.drawText;
		de->extra = strdup(text+1);
		break;
	case 'b':
		de->func = disp.drawBatt;
		de->extra = strdup(text+1);
		break;
		
	case 'z':
		// Duree vol
		de->func = disp.drawDurVol;
		de->extra = strdup(text+1);
		break;
	default:
		Serial.printf("Unknown element: %c\n", type);
		break;
	}
}

static uint8_t ACTION(char c) {
	switch(c) {
	case 'D': 
		return ACT_DISPLAY_DEFAULT;
	case 'F':
		return ACT_DISPLAY_SPECTRUM;
	case 'W':
		return ACT_DISPLAY_WIFI;
	case '+':
		return ACT_NEXTSONDE;
	case '#':	
		return ACT_NONE;
	case '>':
		return ACT_DISPLAY_NEXT;
	default:
		if(c>='0'&&c<='9')
			return ACT_DISPLAY(c-'0');
		// Hack, will change later to better syntax
		if(c>='a'&&c<='z')
			return ACT_ADDFREQ(c-'a'+2);
	}
	return ACT_NONE;
}


int Display::countEntries(File f) {
	int pos = f.position();
	int n = 0;
	while(1) {
		//String line = readLine(f);  //f.readStringUntil('\n');
		//line.trim();
		//const char *c=line.c_str();
		readLine(f, lineBuf, LINEBUFLEN);
		const char *c = trim(lineBuf);
		if(*c=='#') continue;
		if(*c>='0'&&*c<='9') n++;
		if(strchr(c,'=')) continue;
		break;
	}	
	f.seek(pos, SeekSet);
	Serial.printf("Counted %d entries\n", n);
	return n;
}

void Display::initFromFile() {
	File d = SPIFFS.open("/screens.txt", "r");
	if(!d) return;

	DispInfo *newlayouts = (DispInfo *)malloc(MAXSCREENS * sizeof(DispInfo));
	if(!newlayouts) {
		Serial.println("Init from file: FAILED, not updating layouts");
		return;
	}
	memset(newlayouts, 0, MAXSCREENS * sizeof(DispInfo));

	// default color
	colfg = 0xffff; // white; only used for ILI9225
	colbg = 0;  // black; only used for ILI9225
	int idx = -1;
	int what = -1;
	int entrysize;
	Serial.printf("Reading from /screens.txt. available=%d\n",d.available());
	while(d.available()) {
		//Serial.printf("Unused stack: %d\n", uxTaskGetStackHighWaterMark(0));
		const char *ptr;
		readLine(d, lineBuf, LINEBUFLEN);
		const char *s = trim(lineBuf);
		// String line = readLine(d);  
		// line.trim();
		// const char *s = line.c_str();
		Serial.printf("Line: '%s'\n", s);
		if(*s == '#') continue;  // ignore comments
		switch(what) {
		case -1:	// wait for start of screen (@)
			{
			if(*s != '@') {
				Serial.printf("Illegal start of screen: %s\n", s);
				continue;
			}
			char *label = strdup(s+1);
			entrysize = countEntries(d);
			Serial.printf("Reading entry with %d elements\n", entrysize);
			idx++;
			int res = allocDispInfo(entrysize, &newlayouts[idx], label);
			Serial.printf("allocDispInfo: idx %d: label is %p - %s\n",idx,newlayouts[idx].label, newlayouts[idx].label);
			if(res<0) {
				Serial.println("Error allocating memory for disp info");
				continue;
			}
			what = 0;
			}
			break;
		default:	// parse content... (additional data or line `what`)
			if(strncmp(s,"timer=",6)==0) {  // timer values
				char t1[10],t2[10],t3[10];
				sscanf(s+6, "%5[0-9a-zA-Z-] , %5[0-9a-zA-Z-] , %5[0-9a-zA-Z-]", t1, t2, t3);
				Serial.printf("timers are %s, %s, %s\n", t1, t2, t3);
				newlayouts[idx].timeouts[0] = (*t1=='n'||*t1=='N')?sonde.config.norx_timeout:atoi(t1);
				newlayouts[idx].timeouts[1] = (*t2=='n'||*t2=='N')?sonde.config.norx_timeout:atoi(t2);
				newlayouts[idx].timeouts[2] = (*t3=='n'||*t3=='N')?sonde.config.norx_timeout:atoi(t3);
				// Code later assumes milliseconds, but config.txt and screens.txt use values in seconds
				if(newlayouts[idx].timeouts[0]>0) newlayouts[idx].timeouts[0]*=1000;
				if(newlayouts[idx].timeouts[1]>0) newlayouts[idx].timeouts[1]*=1000;
				if(newlayouts[idx].timeouts[2]>0) newlayouts[idx].timeouts[2]*=1000;
				//sscanf(s+6, "%hd,%hd,%hd", newlayouts[idx].timeouts, newlayouts[idx].timeouts+1, newlayouts[idx].timeouts+2);
				Serial.printf("timer values: %d, %d, %d\n", newlayouts[idx].timeouts[0], newlayouts[idx].timeouts[1], newlayouts[idx].timeouts[2]);
			} else if(strncmp(s, "key1action=",11)==0) { // key 1 actions
				char c1,c2,c3,c4;
				sscanf(s+11, "%c,%c,%c,%c", &c1, &c2, &c3, &c4);
				newlayouts[idx].actions[1] = ACTION(c1);
				newlayouts[idx].actions[2] = ACTION(c2);
				newlayouts[idx].actions[3] = ACTION(c3);
				newlayouts[idx].actions[4] = ACTION(c4);
			} else if(strncmp(s, "key2action=",11)==0) { // key 2 actions
				char c1,c2,c3,c4;
				sscanf(s+11, "%c,%c,%c,%c", &c1, &c2, &c3, &c4);
				newlayouts[idx].actions[5] = ACTION(c1);
				newlayouts[idx].actions[6] = ACTION(c2);
				newlayouts[idx].actions[7] = ACTION(c3);
				newlayouts[idx].actions[8] = ACTION(c4);
				Serial.printf("parsing key2action: %c %c %c %c\n", c1, c2, c3, c4);
			} else if(strncmp(s, "timeaction=",11)==0) { // timer actions
				char c1,c2,c3;
				sscanf(s+11, "%c,%c,%c", &c1, &c2, &c3);
				newlayouts[idx].actions[9] = ACTION(c1);
				newlayouts[idx].actions[10] = ACTION(c2);
				newlayouts[idx].actions[11] = ACTION(c3);
			} else if(strncmp(s, "fonts=",6)==0) { // change font
				sscanf(s+6, "%d,%d", &fontsma, &fontlar);
			} else if(strncmp(s, "scale=",6)==0) { // change line->pixel scaling for ILI9225 display
				sscanf(s+6, "%d,%d", &yscale, &xscale);
			} else if(strncmp(s, "color=",6)==0) { //
				int res;
				uint32_t fg,bg;
				res=sscanf(s+6, "%" SCNx32 ",%" SCNx32, &fg, &bg);
				colfg = (fg>>19) << 11 | ((fg>>10)&0x3F) << 5 | ((fg>>3)&0x1F);
				if(res==2) {
					colbg = (bg>>19) << 11 | ((bg>>10)&0x3F) << 5 | ((bg>>3)&0x1F);
				}
			} else if( (ptr=strchr(s, '=')) ) {  // one line with some data...
				float x,y,w;
				int n;
				char text[61];
				n=sscanf(s, "%f,%f,%f", &y, &x, &w);
				sscanf(ptr+1, "%60[^\r\n]", text);
				if(sonde.config.disptype==1) { x*=xscale; y*=yscale; w*=xscale; }
				newlayouts[idx].de[what].x = x;
				newlayouts[idx].de[what].y = y;
				newlayouts[idx].de[what].width = n>2 ? w : WIDTH_AUTO;
				parseDispElement(text, newlayouts[idx].de+what);
				Serial.printf("entry at %d,%d width=%d font %d, color=%x,%x\n", (int)x, (int)y, newlayouts[idx].de[what].width, newlayouts[idx].de[what].fmt,
					newlayouts[idx].de[what].fg, newlayouts[idx].de[what].bg);
				if(newlayouts[idx].de[what].func == disp.drawGPS) {
					newlayouts[idx].usegps = GPSUSE_BASE|GPSUSE_DIST|GPSUSE_BEARING; // just all for now
				}
				what++;
				newlayouts[idx].de[what].func = NULL;
			} else {
				for(int i=0; i<12; i++) {
					Serial.printf("action %d: %d\n", i, (int)newlayouts[idx].actions[i]);
				}
 				what=-1;
			}
			break;
		}
	}
	replaceLayouts(newlayouts, idx+1);
}

void Display::circ(int x=1, int y=1) {
	// draw circle
		uint8_t tiles[48] = { 
		0, 0, 0, 0, 0, 0, 128, 64,
		32, 16, 8, 4, 4, 2, 2, 2,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		2, 2, 2, 4, 4, 8, 16, 32,
		64, 128, 0, 0, 0, 0, 0, 0 };
		disp.rdis->drawTile(x, y, 6, tiles);
		
		uint8_t tiles2[48] = { 
		0, 0, 0, 224, 28, 3, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 3, 28, 224, 0, 0, 0 };
		disp.rdis->drawTile(x, y+1, 6, tiles2);
		
		uint8_t tiles3[48] = { 
		0, 0, 0, 255, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 255, 0, 0, 0 };
		disp.rdis->drawTile(x, y+2, 6, tiles3);
		
		uint8_t tiles4[48] = { 
		0, 0, 0, 31, 224, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 224, 31, 0, 0, 0};
		disp.rdis->drawTile(x, y+3, 6, tiles4);	
		
		uint8_t tiles5[48] = { 
		0, 0, 0, 0, 1, 6, 8, 16,
		32, 64, 128, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 128, 64, 32,
		16, 8, 6, 1, 0, 0, 0, 0 };
		disp.rdis->drawTile(x, y+4, 6, tiles5);
			
		uint8_t tiles6[48] = { 
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 1, 1, 2, 2, 2,
		4, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 4, 4,
		2, 2, 2, 1, 1, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0 };
		disp.rdis->drawTile(x, y+5, 6, tiles6);
		
		uint8_t centres[16] = { 
		0, 0, 0, 0, 0, 0, 0, 128,
		128, 0, 0, 0, 0, 0, 0, 0
		};
		disp.rdis->drawTile(x+2, y+2, 2, centres);
		uint8_t centres2[16] = { 
		0, 0, 0, 0, 0, 0, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 0
		};
		disp.rdis->drawTile(x+2, y+3, 2, centres2);
		
		//N
		uint8_t N[16] = {0,0,0,0,0, 63, 2, 4,
		8, 16, 63, 0,0,0,0,0};
		disp.rdis->drawTile(x+2, y-1, 2, N);
		//S
		uint8_t S[16] = {0, 0, 0, 0, 0, 38, 73, 73,
		73, 73, 50, 0,0,0,0,0};
		disp.rdis->drawTile(x+2, y+6, 2, S);
		//O
		uint8_t O[8] = {0,224, 48, 16, 16, 48, 224, 0};
		disp.rdis->drawTile(x-1, y+2, 1, O);		
		uint8_t O2[8] = {0,7, 12, 8, 8, 12, 7, 0};
		disp.rdis->drawTile(x-1, y+3, 1, O2);				
		//E
		uint8_t E[8] = {0,240, 144, 144, 16, 16, 16, 0};
		disp.rdis->drawTile(x+6, y+2, 1, E);		
		uint8_t E2[8] = {0,15, 9, 9, 8, 8, 8, 0};
		disp.rdis->drawTile(x+6, y+3, 1, E2);		
		
}


void Display::setLayout(int newidx) {
	Serial.printf("setLayout: %d (max is %d)\n", newidx, nLayouts);
	if(newidx>=nLayouts) newidx = 0; 
	layout = &layouts[newidx];
	layoutIdx = newidx;
}

void Display::drawString(DispEntry *de, const char *str) {
	rdis->drawString(de->x, de->y, str, de->width, de->fg, de->bg);
}

void Display::drawLat(DispEntry *de) {
	rdis->setFont(de->fmt);
	if(!sonde.si()->validPos) {
	   drawString(de,"00.0000");
	   return;
	}
	//Format Degres, minutes, secondes ou decimal 1=degres 0=decimal
	if (sonde.config.degdec==1) 
	{	
		float decimal=sonde.si()->lat;
		int degres=int(decimal);
		float minutes=(decimal - degres)*60;
		float secondes=(minutes-int(minutes))*60;
		snprintf(buf, 16, "%2d %2.0f'%2.0f", degres, minutes, secondes);
		drawString(de,buf);
		rdis->drawTile(2, 2, 1, deg_tile);
	} else 
	{		
	snprintf(buf, 16, "%2.5f", sonde.si()->lat);	
	drawString(de,buf);
	}
		
}
void Display::drawLon(DispEntry *de) {
	rdis->setFont(de->fmt);
	if(!sonde.si()->validPos) {
	   drawString(de,"00.0000");
	   return;
	}
	//Format Degres, minutes, secondes ou decimal 1=degres 0=decimal
	if (sonde.config.degdec==1) 
	{	
		float decimal=sonde.si()->lon;
		int degres=int(decimal);
		float minutes=(decimal - degres)*60;
		float secondes=(minutes-int(minutes))*60;
		snprintf(buf, 16, "%2d %2.0f'%2.0f", degres, minutes, secondes);
		drawString(de,buf);
		rdis->drawTile(2, 4, 1, deg_tile);
	} else 
	{	
	snprintf(buf, 16, "%2.5f", sonde.si()->lon);
	drawString(de,buf);
	}
}
void Display::drawAlt(DispEntry *de) {
	rdis->setFont(de->fmt);
	if(!sonde.si()->validPos) {
	   drawString(de,"00000m");
	   return;
	}
	float alt = sonde.si()->alt;
	//testing only....   alt += 30000-454;
	snprintf(buf, 16, alt>=1000?"   %5.0fm":"   %3.1fm", alt);
	drawString(de,buf+strlen(buf)-6);
}
void Display::drawHS(DispEntry *de) {
	rdis->setFont(de->fmt);
	if(!sonde.si()->validPos) {
	   drawString(de,"00.00");
	   return;
	}
	boolean is_ms = (de->extra && de->extra[0]=='m')?true:false;  // m/s or km/h
	float hs = sonde.si()->hs;
	if(is_ms) hs = hs * 3.6;
	boolean has_extra = (de->extra && de->extra[1]!=0)? true: false;
	snprintf(buf, 16, hs>99?" %3.0f":" %2.1f", hs);
	if(has_extra) { strcat(buf, de->extra+1); }
	drawString(de,buf+strlen(buf)-4- (has_extra?strlen(de->extra+1):0) );
	if(!has_extra) rdis->drawTile(de->x+4,de->y,2,is_ms?ms_tiles:kmh_tiles);
}
void Display::drawVS(DispEntry *de) {
	rdis->setFont(de->fmt);
	if(!sonde.si()->validPos) {
	   drawString(de," 00.00");
	   return;
	}
	snprintf(buf, 16, "  %+2.1f", sonde.si()->vs);
	Serial.printf("drawVS: extra is %s width=%d\n", de->extra?de->extra:"<null>", de->width);
	if(de->extra) { strcat(buf, de->extra); }
	drawString(de, buf+strlen(buf)-5- (de->extra?strlen(de->extra):0) );
	if(!de->extra) rdis->drawTile(de->x+5,de->y,2,ms_tiles);
}
void Display::drawID(DispEntry *de) {
	rdis->setFont(de->fmt);
	if(!sonde.si()->validID) {
		drawString(de, "nnnnnnnn ");
		return;
	}
	// TODO: handle DFM6 IDs

	if(!de->extra || de->extra[0]=='s') {
		// real serial number, as printed on sonde
		drawString(de, sonde.si()->ser);
	} else if (de->extra[0]=='a') {
		// autorx sonde number ("DF9" and last 6 digits of real serial number)
		if(sonde.si()->type == STYPE_DFM09) {
			int n = strlen(sonde.si()->ser) - 6;
			if(n<0) n=0;
			memcpy(buf, "DF9", 3);
			memcpy(buf+3, sonde.si()->ser+n, 6);
			drawString(de, buf);
		} else {
			drawString(de, sonde.si()->ser);
		}
	} else {
		// dxlAPRS sonde number (DF6 (why??) and 5 last digits of serial number as hex number
		drawString(de, sonde.si()->id);
	}
}
void Display::drawRSSI(DispEntry *de) {
	rdis->setFont(de->fmt);
	Serial.printf("\ndBSmetre %d\n",sonde.config.dbsmetre);
	if (sonde.config.dbsmetre==0) {
		if(sonde.config.disptype!=1) {
			snprintf(buf, 16, "-%d   ", sonde.si()->rssi/2);
			int len=strlen(buf)-3;
			Serial.printf("drawRSSI: %d %d %d (%d)[%d]\n", de->y, de->x, sonde.si()->rssi/2, sonde.currentSonde, len);
			buf[5]=0;
			drawString(de,buf);
			rdis->drawTile(de->x+len, de->y, 1, (sonde.si()->rssi&1)?halfdb_tile1:empty_tile1);
			rdis->drawTile(de->x+len, de->y+1, 1, (sonde.si()->rssi&1)?halfdb_tile2:empty_tile2);
		} else { // special for 2" display
			snprintf(buf, 16, "-%d.%c  ", sonde.si()->rssi/2, (sonde.si()->rssi&1)?'5':'0');
			drawString(de,buf);
		}
	} else {
		int dbmetre=sonde.si()->rssi/2;
		Serial.printf("\nSmetre %d\n",sonde.si()->rssi/2);
		//Ici tableau Smetre
		//S1 <-121dB 		//S2 <-115dB 		//S3 <-109dB 		//S4 <-103dB 		//S5 <-97dB
		//S6 <-91dB		//S7 <-85dB 		//S8 <-79dB		//S9 <-73dB		//S9+10 <-63dB
		//S9+20 <-53dB		//S9+30 <-43db		//S9+40 <- 33dB	//S9+50dB <-23dB	//S9+60 <-13dB
		//S9+70 <-0dB		
		if (dbmetre>138) drawString(de,"S0");
		if ((dbmetre<137)&&(dbmetre>122)) drawString(de,"S1   ");
		if ((dbmetre<121)&&(dbmetre>116)) drawString(de,"S2   ");
		if ((dbmetre<115)&&(dbmetre>108)) drawString(de,"S3   ");
		if ((dbmetre<109)&&(dbmetre>104)) drawString(de,"S4   ");
		if ((dbmetre<103)&&(dbmetre>98)) drawString(de,"S5   ");
		if ((dbmetre<97)&&(dbmetre>92)) drawString(de,"S6   ");
		if ((dbmetre<91)&&(dbmetre>86)) drawString(de,"S7   ");
		if ((dbmetre<85)&&(dbmetre>80)) drawString(de,"S8   ");
		if ((dbmetre<79)&&(dbmetre>74)) drawString(de,"S9   ");
		if ((dbmetre<73)&&(dbmetre>64)) drawString(de,"S9+10");
		if ((dbmetre<63)&&(dbmetre>54)) drawString(de,"S9+20");
		if ((dbmetre<53)&&(dbmetre>44)) drawString(de,"S9+30");
		if ((dbmetre<43)&&(dbmetre>34)) drawString(de,"S9+40");
		if ((dbmetre<33)&&(dbmetre>24)) drawString(de,"S9+50");
		if ((dbmetre<23)&&(dbmetre>14)) drawString(de,"S9+60");
		if ((dbmetre<13)&&(dbmetre>0)) drawString(de,"S9+70");
	}
}
void Display::drawQS(DispEntry *de) {
	uint8_t *stat = sonde.si()->rxStat;
	struct StatInfo *statinfo = (struct StatInfo *)de->extra;
	rdis->drawQS(de->x, de->y, statinfo->len, statinfo->size, stat, de->fg, de->bg);
}

void Display::drawType(DispEntry *de) {
	rdis->setFont(de->fmt);
        drawString(de, sondeTypeStr[sonde.si()->type]);
}
void Display::drawFreq(DispEntry *de) {
	rdis->setFont(de->fmt);
        snprintf(buf, 16, "%3.3f%s", sonde.si()->freq, de->extra?de->extra:"");
        drawString(de, buf);
}
void Display::drawVBat(DispEntry *de) {
	rdis->setFont(de->fmt);
        snprintf(buf, 16, "%3.0f%s", sonde.si()->vbat, "%");
        drawString(de, buf);
}
void Display::drawDurVol(DispEntry *de) {
	rdis->setFont(de->fmt);
	Serial.printf("\nDurVol:%2d:%2d.%2d", sonde.si()->durvolheure,sonde.si()->durvolminute,sonde.si()->durvolseconde);
	snprintf(buf, 16, "%2d:%2d.%2d", sonde.si()->durvolheure,sonde.si()->durvolminute,sonde.si()->durvolseconde);
	drawString(de,buf);
}
void Display::drawAFC(DispEntry *de) {
 	if(!sonde.config.showafc) return;
	rdis->setFont(de->fmt);
	//if(sonde.si()->afc==0) { strcpy(buf, "        "); }
	//else
	{ snprintf(buf, 15, "     %+3.2fk", sonde.si()->afc*0.001); }
        drawString(de, buf+strlen(buf)-8);
}
void Display::drawIP(DispEntry *de) {
	rdis->setFont(de->fmt);
	rdis->drawIP(de->x, de->y, de->width, de->fg, de->bg);
}
void Display::drawSite(DispEntry *de) {
        rdis->setFont(de->fmt);
	switch(de->extra[0]) {
	case '#':
		// currentSonde is index in array starting with 0;
		// but we draw "1" for the first entrie and so on...
		snprintf(buf, 3, "%2d", sonde.currentSonde+1);
		buf[2]=0;
		break;
	case 't':
		snprintf(buf, 3, "%d", sonde.config.maxsonde);
		buf[2]=0;
		break;
	case 'a':
		{
		uint8_t active = 0;
 		for(int i=0; i<sonde.config.maxsonde; i++) {
	                if(sonde.sondeList[i].active) active++;
		}
 		snprintf(buf, 3, "%d", active);
		buf[2]=0;
		}
		break;
	case '/':
		snprintf(buf, 6, "%d/%d  ", sonde.currentSonde+1, sonde.config.maxsonde);
		buf[5]=0;
		break;
	case 0: case 'l': default: // launch site
		drawString(de, sonde.si()->launchsite);
		return;
	}
	if(de->extra[0]) strcat(buf, de->extra+1);
	drawString(de, buf);
}
void Telemetry() {
  if (sonde.config.telemetryOn==1) {
  File file = SPIFFS.open("/data.csv", "a");

  if (!file) {
    Serial.println("There was an error opening the file for writing");
    return;
  } else {
  
  		int dbmetre=sonde.si()->rssi/2;
		String smetre ="";
		if (dbmetre>138) smetre="S0";
		if ((dbmetre<137)&&(dbmetre>122)) smetre="S1";
		if ((dbmetre<121)&&(dbmetre>116)) smetre="S2";
		if ((dbmetre<115)&&(dbmetre>108)) smetre="S3";
		if ((dbmetre<109)&&(dbmetre>104)) smetre="S4";
		if ((dbmetre<103)&&(dbmetre>98)) smetre="S5";
		if ((dbmetre<97)&&(dbmetre>92)) smetre="S6";
		if ((dbmetre<91)&&(dbmetre>86)) smetre="S7";
		if ((dbmetre<85)&&(dbmetre>80)) smetre="S8";
		if ((dbmetre<79)&&(dbmetre>74)) smetre="S9";
		if ((dbmetre<73)&&(dbmetre>64)) smetre="S9+10";
		if ((dbmetre<63)&&(dbmetre>54)) smetre="S9+20";
		if ((dbmetre<53)&&(dbmetre>44)) smetre="S9+30";
		if ((dbmetre<43)&&(dbmetre>34)) smetre="S9+40";
		if ((dbmetre<33)&&(dbmetre>24)) smetre="S9+50";
		if ((dbmetre<23)&&(dbmetre>14)) smetre="S9+60";
		if ((dbmetre<13)&&(dbmetre>0)) smetre="S9+70";
		
    String telemetry = telemetry + sonde.si()->id + ";" + sonde.si()->ser + ";" + sondeTypeStr[sonde.si()->type] + ";" + sonde.si()->freq + "Mhz;" + sonde.si()->lat + "; "+ sonde.si()->lon + ";" + sonde.si()->alt + "m;" + sonde.config.gps_lat + ";" + sonde.config.gps_lon +";" + sonde.config.gps_alt + ";" + sonde.si()->vs + "m/s;" + sonde.si()->hs + "m/s;" + ";" + sonde.si()->dir + "°;" + sonde.si()->launchsite + ";" + sonde.si()->rssi/2 + ";" + smetre + ";" + *sonde.si()->rxStat + ";" + sonde.si()->afc*0.001 + ";" + sonde.si()->validPos + "\n";
    if (file.print(telemetry+"\n")) {
      Serial.println("File was written");
    } else {
      Serial.println("File write failed");
    }
  }

  file.close();
  }
}

void Display::drawKilltimer(DispEntry *de) {
	rdis->setFont(de->fmt);
	uint16_t value;
	switch(de->extra[0]) {
	case 'l': value = sonde.si()->launchKT; break;
	case 'b': value = sonde.si()->burstKT; break;
	case 'c': value = sonde.si()->countKT; break;
	}
	// format: 4=h:mm; 6=h:mm:ss; s=sssss
	uint16_t h=value/3600;
	uint16_t m=(value-h*3600)/60;
	uint16_t s=value%60;
	switch(de->extra[1]) {
	case '4':
		if(value!=0xffff) snprintf(buf, 5, "%d:%02d", h, m);
		else strcpy(buf, "    ");
		break;
	case '6':
		if(value!=0xffff) snprintf(buf, 7, "%d:%02d:%02d", h, m, s);
		else strcpy(buf, "       ");
		break;
	default:
		if(value!=0xffff) snprintf(buf, 6, "%5d", value);
		else strcpy(buf, "     ");
		break;
	}
	if(de->extra[1])
		strcat(buf, de->extra+2);
	drawString(de, buf);
}


#define EARTH_RADIUS (6371000.0F)
#define PE (111.6F) // la terre a un rayon de r = 6371km. Soit un périmètre de P = 2*PI*r = 40030.17km. Soit, pour un degré a = P/360 = 111.16km
#ifndef PI
#define  PI  (3.1415926535897932384626433832795)
#endif
// defined by Arduino.h   #define radians(x) ( (x)*180.0F/PI )


extern int lastCourse; // from RadioSonde_FSK.ino
void Display::calcGPS() {
	// base data
if (sonde.config.gpsOn==0){
	gpsValid = true;
	gpsLon = atof(sonde.config.gps_lon);
	gpsLat = atof(sonde.config.gps_lat);
	gpsAlt = sonde.config.gps_alt;
static int tmpc=0;
	tmpc = (tmpc+5)%360;
	gpsCourse = tmpc;
} else {
	gpsValid = nmea.isValid();
	gpsLon = nmea.getLongitude()*0.000001;
	gpsLat = nmea.getLatitude()*0.000001;
	long alt;
	nmea.getAltitude(alt); gpsAlt=(int)(alt/1000);
	gpsCourse = (int)(nmea.getCourse()/1000);
	gpsCourseOld = false;
	if(gpsCourse==0) {
		// either north or not new
		if(lastCourse!=0) // use old value...
		{
			gpsCourseOld = true;
			gpsCourse = lastCourse;
		}
	}
	sonde.config.gps_Lat=gpsLat;
	sonde.config.gps_Lon=gpsLon;
	sonde.config.gps_Alt=gpsAlt;

}

	// distance
	if( gpsValid ) {
        	float lat1 = gpsLat;
        	float lat2 = sonde.si()->lat;
        	float lon1 = gpsLon;
        	float lon2 = sonde.si()->lon;
        	float x = abs(lon2-lon1);
        	Serial.printf("\nx= %f",x);
        	float y = abs(lat2-lat1);
        	Serial.printf("\ny= %f",y);
        	float d = sqrt(x*x+y*y)*PE*1000;
        	Serial.printf("\ndist= %5.1fm",d);
		gpsDist = d;
	} else {
		gpsDist = -1;
	}
	// bearing
	if( gpsValid ) {
                float lat1 = (gpsLat);
                float lat2 = (sonde.si()->lat);
                float lon1 = (gpsLon);
                float lon2 = (sonde.si()->lon);
                float y = abs(sin(lon2-lon1)*cos(lat2));
                float x = abs(cos(lat1)*sin(lat2) - sin(lat1)*cos(lat2)*cos(lon2-lon1));
                float dir = atan2(y, x)/PI*180;
                if(dir<0) dir+=360;
		gpsDir = (int)dir;
		gpsBear = gpsDir - gpsCourse;
		if(gpsBear < 0) gpsBear += 360;
		if(gpsBear >= 360) gpsBear -= 360;
	} else {
		gpsDir = -1;
		gpsBear = -1;
	}
	
	Serial.printf("\nGPS data: valid%d  GPS at %f,%f (alt=%d,cog=%d);  sonde at dist=%d, dir=%d rel.bear=%d\n",gpsValid?1:0,
		gpsLat, gpsLon, gpsAlt, gpsCourse, gpsDist, gpsDir, gpsBear);
	

}
void Display::calcVbat() {
	//VBat %
	float vbat,r=0,Dmax, Dact;
	float vbatmax=atof(sonde.config.vbatmax), vbatmin=atof(sonde.config.vbatmin);

	if (axp192_found==false)
	{
	  int ADC_PIN = 35;
	  r = analogRead(ADC_PIN);
	  vbat = r * (3.30 / 4094.00); //convert the value to a true voltage.
	  Serial.printf("\nVbat= %1.2fV",vbat);
	  Serial.printf("\nVbatMAX= %1.2fV",vbatmax);
	  Serial.printf("\nVbatMIN= %1.2fV",vbatmin);
	  Dmax = vbatmax - vbatmin;
	  Dact = vbat - vbatmin;
	  vbat = (Dact / Dmax)*100;
	  Serial.printf("\nVbat:%3.0f\%\n",vbat);
	} else {
	  r = (axp.getLDO2Voltage() );
	  Serial.printf("\nr=%f",r);
	  vbat = r /1000; //convert the value to a true voltage.
	  Serial.printf("\nVbat= %1.2fV",vbat);
	  Dmax = vbatmax - vbatmin;
	  Dact = vbat - vbatmin;
	  vbat = (Dact / Dmax)*100;
	  Serial.printf("\nVbat:%3.0f %\n",vbat);
	}

/*
  Serial.printf("LDO2: %s\n", axp.isLDO2Enable() ? "ENABLE" : "DISABLE");
  Serial.printf("LDO2 Voltage: %d\n", axp.getLDO2Voltage());
  Serial.printf("LDO3: %s\n", axp.isLDO3Enable() ? "ENABLE" : "DISABLE");
  Serial.printf("LDO3 Voltage: %d\n", axp.getLDO3Voltage());
  Serial.printf("DCDC3: %s\n", axp.isDCDC3Enable() ? "ENABLE" : "DISABLE");
  Serial.printf("DCDC3 Voltage: %d\n", axp.getDCDC3Voltage());
  Serial.printf("Exten: %s\n", axp.isExtenEnable() ? "ENABLE" : "DISABLE");
  Serial.printf("Battery Voltage: %d\n", axp.getBattVoltage());
  Serial.printf("BAttery Percentage: %f\n", axp.getBattPercentage());
  Serial.printf("Battery Charge Current set: %d\n", axp.getSettingChargeCurrent());
  Serial.printf("Chip Temp: %d\n", axp.getTemp());
  Serial.printf("Chip TSTemp: %d\n", axp.getTSTemp());
*/

	sonde.si()->vbat=vbat;
}
void Display::calcDurVol() {

	float alt = sonde.si()->alt;
	float hs = sonde.si()->hs, durvol=0;
	int heure=0, minute=0, seconde=0;
	if (hs <0) 
	{
		durvol=(alt/hs)*-1;
		if (durvol>0) 
		{
		Serial.printf("\nDurVol=%2.2f",durvol);
		if ((durvol/60)>59)
		{
		heure=int(durvol/3600);
		minute=int((heure-(durvol/3600))*3600);
		}else {
		minute=int(durvol/60);
		seconde=int((minute-(durvol/60))*60);
		}

		}		
	} else {
		heure=0;
	}
	sonde.si()->durvolheure=heure;
	sonde.si()->durvolminute=minute;
	sonde.si()->durvolseconde=seconde;
	//Serial.printf("\nHeure=:%2d:%2d.%2d\n",heure,minute,seconde);
}
void Display::drawGPS(DispEntry *de) {
	rdis->setFont(de->fmt);
	switch(de->extra[0]) {
	case 'W':
		{
		// show if GPS location is valid
		uint8_t *tile = disp.gpsValid?gps_tile:nogps_tile;
		rdis->drawTile(de->x, de->y, 1, tile);
		}
		break;
	case 'O':
		// GPS long
		snprintf(buf, 16, "%2.5f", disp.gpsLon);
		drawString(de,buf);
		break;
	case 'A':
		// GPS lat
		snprintf(buf, 16, "%2.5f", disp.gpsLat);
		drawString(de,buf);
		break;
	case 'H':
		// GPS alt
		snprintf(buf, 16, "%4dm", disp.gpsAlt);
		drawString(de,buf);
		break;
	case 'C':
		// GPS Course over ground
		snprintf(buf, 4, "%3d", disp.gpsCourse);
		drawString(de, buf);
		break;
	case 'D':
		{
		rdis->setFont(de->fmt);
		// distance
		// equirectangular approximation is good enough
		if( (sonde.si()->validPos&0x03)!=0x03 ) {
			snprintf(buf, 16, "no pos ");
			if(de->extra && *de->extra=='5') buf[5]=0;
		} else if((!disp.gpsValid)&&(disp.gpsLat!=0)) {
			snprintf(buf, 16, "no gps ");
			if(de->extra && *de->extra=='5') buf[5]=0;
		} else {
			if(de->extra && *de->extra=='5') { // 5-character version: ****m / ***km / **e6m
				if(disp.gpsDist>999999) snprintf(buf, 16, "%3.0de6m  ", (disp.gpsDist/1000000));
				if(disp.gpsDist>999) snprintf(buf, 16, "%3.0dkm   ", (disp.gpsDist/1000));
				else snprintf(buf, 16, "%5.0dm    ", (disp.gpsDist));

			} else { // 6-character version: *****m / ****km)
				if(disp.gpsDist>999) snprintf(buf, 16, "%3.0dkm    ", (disp.gpsDist/1000));
				else snprintf(buf, 16, "%3.0dm     ", (disp.gpsDist));

			}
		}
		Serial.printf("\nDist= %dm",disp.gpsDist);
		drawString(de, buf);
		}
		break;
	case 'I':
		// dIrection
		rdis->setFont(de->fmt);
		if (!sonde.si()->validPos)  {
			rdis->setFont(de->fmt);
			drawString(de, "---");
			break;
		}
		snprintf(buf, 16, "%3d", disp.gpsDir);
		buf[3]=0;
		drawString(de, buf);
		//if(de->extra[1]==(char)176)
		rdis->drawTile(de->x+3, de->y, 1, deg_tile);		
		break;
	case 'B':
		// relative bearing
		rdis->setFont(de->fmt);
		if (!sonde.si()->validPos)  {
			drawString(de, "---");
			break;
		}
		snprintf(buf, 16, "%3d", disp.gpsBear);
		buf[3]=0;
		drawString(de, buf);
		if(de->extra[1]==(char)176)
		rdis->drawTile(de->x+3, de->y, 1, deg_tile);
		break;
	case '0':
		// diagram
		{
		Serial.printf("\nDiagram\n");
		int x=1, y=1;
		disp.circ(x,y);
		
		int dirdegres=disp.gpsDist;
		if (dirdegres==360) dirdegres=0;

		
		//reset aiguille
		uint8_t resetaiguilles[16] = {0, 0, 0, 0, 0, 0, 0, 128, 128, 0, 0, 0, 0, 0, 0, 0};
		disp.rdis->drawTile(x+2, y+2, 2, resetaiguilles);
		uint8_t resetaiguilles2[16] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0};
		disp.rdis->drawTile(x+2, y+3, 2, resetaiguilles2);		


		//Pour 0 à 90°
		if (dirdegres<91) {
		if (dirdegres<7) {
			uint8_t trait[8]={255,0,0,0,0,0,0,0};			//0°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>6)&&(dirdegres<13)) {
			uint8_t trait[8]={240,15,0,0,0,0,0,0};		//6°
			disp.rdis->drawTile(x+3, y+2, 1, trait);		
		}
		if ((dirdegres>12)&&(dirdegres<19)) {
			uint8_t trait[8]={224,24,7,0,0,0,0,0};		//12°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>18)&&(dirdegres<25)) {
			uint8_t trait[8]={128,112,14,1,0,0,0,0};		//18°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>24)&&(dirdegres<31)) {
			uint8_t trait[8]={128,96,24,6,1,0,0,0};		//24°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>30)&&(dirdegres<37)) {
			uint8_t trait[8]={192,48,24,12,6,3,0,0};		//30°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>36)&&(dirdegres<43)) {
			uint8_t trait[8]={128,64,32,24,12,6,1,0};		//36°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>42)&&(dirdegres<49)) {
			uint8_t trait[8]={128,64,32,16,8,4,2,1};		//42° à 49° dont 45°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>48)&&(dirdegres<55)) {
			uint8_t trait[8]={128,96,48,24,12,8,4,2};		//48°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>54)&&(dirdegres<61)) {
			uint8_t trait[8]={128,64,32,32,16,16,8,4};		//54°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>60)&&(dirdegres<67)) {
			uint8_t trait[8]={128,64,64,32,32,16,16,8};		//60°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>66)&&(dirdegres<73)) {
			uint8_t trait[8]={128,64,64,64,32,32,32,16};		//66°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>72)&&(dirdegres<79)) {
			uint8_t trait[8]={128,128,64,64,64,64,32,32};		//72°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>78)&&(dirdegres<85)) {
			uint8_t trait[8]={128,128,128,128,64,64,64,64};	//78°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		if ((dirdegres>84)&&(dirdegres<91)) {
			uint8_t trait[8]={128,128,128,128,128,128,128,128}; 	//84°
			disp.rdis->drawTile(x+3, y+2, 1, trait);
		}
		}
		
		
		//Pour 90° à 180°
		if ((dirdegres>90)&&(dirdegres<181)) {
		if (dirdegres<97) {
			uint8_t trait[8]={1,1,1,1,1,1,1,1};			//90°
			disp.rdis->drawTile(x+3, y+3, 1, trait);
		}
		if ((dirdegres>96)&&(dirdegres<103)) {
			uint8_t trait[8]={1,1,1,1,2,2,2,2};			//96°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}
		if ((dirdegres>102)&&(dirdegres<109)) {
			uint8_t trait[8]={1,2,2,2,2,2,2,4};			//102°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}		
		if ((dirdegres>108)&&(dirdegres<115)) {
			uint8_t trait[8]={1,2,2,4,4,4,8,8};			//108°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}
		if ((dirdegres>114)&&(dirdegres<121)) {
			uint8_t trait[8]={1,2,2,4,4,8,8,16};			//114°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}
		if ((dirdegres>120)&&(dirdegres<127)) {
			uint8_t trait[8]={1,2,4,12,24,32,32,64};		//120°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}		
		if ((dirdegres>126)&&(dirdegres<133)) {
			uint8_t trait[8]={1,1,2,12,48,64,128,128};		//126°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}
		if ((dirdegres>132)&&(dirdegres<139)) {
			uint8_t trait[8]={1,2,4,8,16,32,64,128};		//132° à 138° dont 135°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}		
		if ((dirdegres>138)&&(dirdegres<145)) {
			uint8_t trait[8]={1,6,12,24,48,96,128,0};		//144°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}
		if ((dirdegres>144)&&(dirdegres<151)) {
			uint8_t trait[8]={1,6,24,96,128,0,0,0};		//150°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}		
		if ((dirdegres>150)&&(dirdegres<163)) {
			uint8_t trait[8]={1,14,112,128,0,0,0,0};		//162°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}	
		if ((dirdegres>162)&&(dirdegres<169)) {
			uint8_t trait[8]={3,60,192,0,0,0,0,0};		//168°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}
		if ((dirdegres>168)&&(dirdegres<175)) {
			uint8_t trait[8]={15,240,0,0,0,0,0,0};		//174°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}		
		if ((dirdegres>174)&&(dirdegres<181)) {
			uint8_t trait[8]={255,0,0,0,0,0,0,0};			//180°
			disp.rdis->drawTile(x+3, y+3, 1, trait);		
		}		
		}
		
		//Pour 180 à 270
		if ((dirdegres>180)&&(dirdegres<271)) {
		if (dirdegres<187) {
			uint8_t trait[8]={0,0,0,0,0,0,0,255};			//180°
			disp.rdis->drawTile(x+2, y+3, 1, trait);
		}
		if ((dirdegres>186)&&(dirdegres<193)) {
			uint8_t trait[8]={0,0,0,0,0,0,240,15};		//186°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}
		if ((dirdegres>192)&&(dirdegres<199)) {
			uint8_t trait[8]={0,0,0,0,0,192,60,3};		//192°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}
		if ((dirdegres>198)&&(dirdegres<205)) {
			uint8_t trait[8]={0,0,0,0,128,112,14,1};		//198°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}		
		if ((dirdegres>204)&&(dirdegres<211)) {
			uint8_t trait[8]={0,0,0,128,96,24,6,1};		//204°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}
		if ((dirdegres>210)&&(dirdegres<217)) {
			uint8_t trait[8]={0,0,192,32,48,12,4,3};		//210°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}
		if ((dirdegres>216)&&(dirdegres<223)) {
			uint8_t trait[8]={0,128,96,48,24,12,6,1};		//216°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}				
		if ((dirdegres>222)&&(dirdegres<229)) {
			uint8_t trait[8]={128,64,32,16,8,4,2,1};		//222° à 229° dont 225°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}
		if ((dirdegres>228)&&(dirdegres<235)) {
			uint8_t trait[8]={128,128,96,48,24,12,6,1};		//228°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}		
		if ((dirdegres>234)&&(dirdegres<241)) {
			uint8_t trait[8]={64,32,32,24,12,8,2,1};		//234°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}
		if ((dirdegres>240)&&(dirdegres<247)) {
			uint8_t trait[8]={32,32,16,8,4,2,1};			//240°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}
		if ((dirdegres>246)&&(dirdegres<253)) {
			uint8_t trait[8]={16,8,8,4,4,2,2,1};			//246°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}		
		if ((dirdegres>252)&&(dirdegres<259)) {
			uint8_t trait[8]={8,4,4,4,2,2,2,1};			//252°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}		
		if ((dirdegres>258)&&(dirdegres<265)) {
			uint8_t trait[8]={4,4,2,2,2,2,1,1};			//258°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}
		if ((dirdegres>264)&&(dirdegres<271)) {
			uint8_t trait[8]={1,1,1,1,1,1,1,1};			//264°
			disp.rdis->drawTile(x+2, y+3, 1, trait);		
		}

		}
		
		// 270° à 360°
		if ((dirdegres>270)&&(dirdegres<360)) {
		if (dirdegres<277) {
			uint8_t trait[8]={128,128,128,128,128,128,128,128};	//270°
			disp.rdis->drawTile(x+2, y+2, 1, trait);
		}
		if ((dirdegres>276)&&(dirdegres<283)) {
			uint8_t trait[8]={64,64,64,64,128,128,128,128};	//276°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}
		if ((dirdegres>282)&&(dirdegres<289)) {
			uint8_t trait[8]={32,32,64,64,64,64,128,128};		//282°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}		
		if ((dirdegres>288)&&(dirdegres<295)) {
			uint8_t trait[8]={16,32,32,32,64,64,64,128};		//288°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}
		if ((dirdegres>294)&&(dirdegres<301)) {
			uint8_t trait[8]={8,16,16,32,32,64,64,128};		//294°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}		
		if ((dirdegres>300)&&(dirdegres<307)) {
			uint8_t trait[8]={4,4,8,16,32,64,64,128};		//300°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}
		if ((dirdegres>306)&&(dirdegres<313)) {
			uint8_t trait[8]={2,4,4,24,48,32,64,128};		//306°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}		
		if ((dirdegres>312)&&(dirdegres<319)) {
			uint8_t trait[8]={1,2,4,8,16,32,64,128};		//312° à 319° dont 315°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}
		if ((dirdegres>318)&&(dirdegres<325)) {
			uint8_t trait[8]={0,1,6,8,24,32,96,128};		//318°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}		
		if ((dirdegres>324)&&(dirdegres<331)) {
			uint8_t trait[8]={0,1,2,6,24,32,64,128};		//324°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}		
		if ((dirdegres>330)&&(dirdegres<337)) {
			uint8_t trait[8]={0,0,1,6,8,16,96,128};		//330°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}
		if ((dirdegres>336)&&(dirdegres<343)) {
			uint8_t trait[8]={0,0,0,1,6,24,96,128};		//336°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}
		if ((dirdegres>342)&&(dirdegres<349)) {
			uint8_t trait[8]={0,0,0,0,1,6,112,128};		//342°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}
		if ((dirdegres>348)&&(dirdegres<355)) {
			uint8_t trait[8]={0,0,0,0,0,1,126,128};		//348°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}		
		if ((dirdegres>354)&&(dirdegres<360)) {
			uint8_t trait[8]={0,0,0,0,0,0,0,255};			//354°
			disp.rdis->drawTile(x+2, y+2, 1, trait);		
		}		
		}
				
		}
		break;
	case 'E':
		// elevation
		float el,ele;
		el=acos(cos(PI/180*(disp.gpsLon-disp.gpsDir))*cos(PI/180*(abs(disp.gpsLat))));
		ele=180*atan((cos(el)-(EARTH_RADIUS/(EARTH_RADIUS+disp.gpsAlt)))/sin(el))/PI;
		ele=abs(ele);
		Serial.printf("\nElevation:%3.0f\n",ele);
		snprintf(buf, 16, "%3.0f", ele);
		buf[3]=0;
		drawString(de, buf);
		rdis->drawTile(de->x+3, de->y, 1, deg_tile);	
		break;

	}
}

void Display::drawBatt(DispEntry *de) {
	float val, pwr, Dmax, Dact, r, GPIO_PIN=4,ADC_PIN = 35;
	float vbatmax=atof(sonde.config.vbatmax), vbatmin=atof(sonde.config.vbatmin);
	char buf[30];
	//if(!axp192_found) return;

	xSemaphoreTake( axpSemaphore, portMAX_DELAY );
	switch(de->extra[0]) {
	case 'V':
		r = analogRead(ADC_PIN);
        	val = r * (3.30 / 4094.00); //convert the value to a true voltage.
		snprintf(buf, 30, "%.2f%s", val, de->extra+1);
		break;
	case 'C':
		r = analogRead(GPIO_PIN);
        	val = r * (3.30 / 4094.00); //convert the value to a true voltage.
		snprintf(buf, 30, "%.2f%s", val, de->extra+1);
		break;
	case 'P':
		r = analogRead(ADC_PIN);
        	val = r * (3.30 / 4094.00); //convert the value to a true voltage.
        	Dmax = vbatmax - vbatmin;
		Dact = val - vbatmin;
		val = (Dact / Dmax)*100;
		val = (vbatmax * val) /100;
        	pwr = (val * 0.40)*1000; 
		snprintf(buf, 30, "%4.0f%s", pwr, de->extra+1);
		break;
		
	default:
		*buf=0;

	}
	xSemaphoreGive( axpSemaphore );
        rdis->setFont(de->fmt);
	drawString(de, buf);
}

void Display::drawText(DispEntry *de) {
        rdis->setFont(de->fmt);
	drawString(de, de->extra);
}
extern void buzzerLed(int temps);
void Display::updateDisplayPos() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawLat && di->func != disp.drawLon) continue;
		di->func(di);
	}

}
void Display::updateDisplayPos2() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawAlt && di->func != disp.drawHS && di->func != disp.drawVS) continue;
		di->func(di);
	}
}
void Display::updateDisplayVBat() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawVBat) continue;
		di->func(di);
	}
}
void Display::updateDisplayVBatt() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawBatt) continue;
		di->func(di);
	}
}
void Display::updateDisplayID() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawID) continue;
		di->func(di);
	}
}
void Display::updateDisplayRSSI() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawRSSI) continue;
		di->func(di);
	}
}
void Display::updateStat() {
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		if(di->func != disp.drawQS) continue;
		di->func(di);
	}
}

void Display::updateDisplayRXConfig() {
       for(DispEntry *di=layout->de; di->func != NULL; di++) {
                if(di->func != disp.drawQS && di->func != disp.drawAFC) continue;
                di->func(di);
        }
}

void Display::updateDisplayIP() {
       for(DispEntry *di=layout->de; di->func != NULL; di++) {
                if(di->func != disp.drawIP) continue;
		Serial.printf("updateDisplayIP: %d %d\n",di->x, di->y);
                di->func(di);
        }
}

void Display::updateDisplay() {
	calcGPS();
	calcVbat();
	calcDurVol();
	Telemetry();
	for(DispEntry *di=layout->de; di->func != NULL; di++) {
		di->func(di);
	}

}

Display disp = Display();