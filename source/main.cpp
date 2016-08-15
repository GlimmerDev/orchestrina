#include <3ds.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>
#include <sf2d.h>
#include <sfil.h>
#include <sftd.h>
#include <queue>
#include <sstream>
#include <cctype>

#include "chiaro_otf.h"
#include "bgbottom_png.h"
#include "ocarina_png.h"
#include "itemblock_png.h"
#include "buttons_png.h"
#include "pressed_png.h"

#define SAMPLERATE 22050
#define BYTESPERSAMPLE 4

#define MAXNOTES 16

#define WAVBUFCOUNT (MAXNOTES+2)
#define SONGWAVBUF (WAVBUFCOUNT-1)

using namespace std;

// One wavebuf for each of the current instrument's notes, one for each sound effect, one for the current full song playing
ndspWaveBuf waveBuf[WAVBUFCOUNT];

bool wavbufList[WAVBUFCOUNT];

// Source type
typedef enum {
	TYPE_UNKNOWN = -1,
	TYPE_OGG = 0,
	TYPE_WAV = 1
} source_type;

// Instrument
typedef struct {
    string name;
    u16 notes;
} instrument;

// Source
typedef struct {
	// source_type type;
	
	// float rate;
	// u32 channels;
	// u32 encoding;
	u32 nsamples;
	u32 size;
	char* data;
	bool loop;
	int wbuf;
    int channel;

	// float mix[12];
	// ndspInterpType interp;
} source;

source* notes[MAXNOTES];

// Array of note sequences
// TODO: separate by instrument
string playable[25] = {
	"zzzzzz",  // NULL
	"xayxay",  // Zelda's Lullaby
	"ryxryx",  // Saria's Song
	"axyaxy",  // Epona's Song
	"yrayra",  // Sun's Song
	"ylrylr",  // Song of Time
	"lralra",  // Song of Storms
	"laxyxy",  // Minuet of Forest
	"rlrlyryr",// Bolero of Fire
	"lryyx",   // Serenade of Water
	"xyylxyr", // Nocturne of Shadow
	"lrlyrl",  // Requiem of Spirit
	"ayayxa",   // Prelude of Light
	"xyrxyr",  // Song of Healing
	"rlyrly",  // Inverted Song of Time
	"yyllrr",  // Song of Double Time
	"yrlrya",  // Oath to Order
	"rxarxa",  // Song of Soaring
	"axaxlyl", // Sonata of Awakening
	"lyxlyxyl",// Goron's Lullaby
	"xaxyrxy", // New Wave Bossa Nova
	"yxyryax",  // Elegy of Emptiness
	"zzzzzz",  // NULL
	"zzzzzz",  // NULL
	"zzzzzz"  // NULL
};

// Array of song titles
// TODO: separate by instrument
string songnames[25] = {
	"NULL",
	"Zelda's Lullaby",
	"Saria's Song",
	"Epona's Song",
	"Sun's Song",
	"Song of Time",
	"Song of Storms",
	"Minuet of Forest",
	"Bolero of Fire",
	"Serenade of Water",
	"Nocture of Shadow",
	"Requiem of Spirit",
	"Prelude of Light",
	"Song of Healing",
	"Inverted Song of Time",
	"Song of Double Time",
	"Oath to Order",
	"Song of Soaring",
	"Sonata of Awakening",
	"Goron's Lullaby",
	"New Wave Bossa Nova",
	"Elegy of Emptiness",
	"NULL",
	"NULL",
	"NULL"
};

// Array of instruments
instrument instruments[5] = {
	{"NULL", 0},
    {"Ocarina", 5},
    {"Pipes", 5},
    {"Drums", 5},
    {"Guitar", 5}
};

// Converts string to uppercase characters
string upperStr(string in) {
	for (u32 i = 0; i < in.size(); ++i) {
		in[i] = toupper(in[i]);
	}
	return in;
}

// Converts boolean value to string
string boolToStr( bool in ) {
	if (in == true) return "True";
	else return "False";
}

// Converts integer value to string
string intToStr( int num )
{
	stringstream ss;
	ss << num;
	return ss.str();
}

// Detects if a song has been played by the player
int detectSong(string song) {
	for (u32 i = 0; i < sizeof(playable)/sizeof(playable[0]); ++i) {
		u32 pos = song.find(playable[i]);
		if (pos != std::string::npos) return i;
	}
	return -1;
}

// Returns the first unused wavBuf available for use
int getOpenWavbuf() {

	for (int i = 0; i <= WAVBUFCOUNT; i++) {
		if (!wavbufList[i]) {
			wavbufList[i] = true;
			return i;
		}
	}

	return -1;

}

// Initializes an audio source and returns any errors that may occur
int sourceInit(source *self, const char *filename, int channel, int wbuf = -1) {
	FILE *file = fopen(filename, "rb");
	if (file) {
        fseek(file, 0, SEEK_END);
        self->size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Set wavbuf
		if (wbuf==-1) self->wbuf = getOpenWavbuf();
        else self->wbuf = wbuf;
		self->loop = false;
        self->nsamples = (self->size / 2);

		// Read data
		if (linearSpaceFree() < self->size) return -1;
		self->data = (char*)linearAlloc(self->size);

		fread(self->data, self->size, 1, file);

		fclose(file);

        waveBuf[self->wbuf].data_vaddr = self->data;
        waveBuf[self->wbuf].nsamples = self->nsamples;
        waveBuf[self->wbuf].looping = self->loop;
        
        self->channel = channel;
	}
	else return -2;
	return 0;
}

// Frees an audio source and its wavBuf
void sourceFree(source *self) {
    if (self==NULL) return;
    wavbufList[self->wbuf] = false;
    if (waveBuf[self->wbuf].status == NDSP_WBUF_PLAYING || waveBuf[self->wbuf].status==NDSP_WBUF_QUEUED) ndspChnWaveBufClear(self->channel);
    delete self;
}

// Initializes an instrument and returns any errors that may occur
int instrumentInit(u8 id) {
    for (int i = 0; i < instruments[id].notes; i++) {
        string path = "/3ds/orchestrina/data/"+instruments[id].name+intToStr(i)+".pcm";
        sourceFree(notes[i]);
        notes[i] = new source;
        int result = sourceInit(notes[i], path.c_str(), 0);
        if (result!=0) return -1;
    }
    return 0;
}

void instrumentFree(u8 id) {
    for (int i = 0; i < instruments[id].notes; i++) sourceFree(notes[i]);
}

// Plays an initialized audio source on its assigned DSP channel
int sourcePlay(source *self) { // source:play()

	if (self==NULL || self->wbuf == -1) return -1;

    DSP_FlushDataCache((u32*)self->data, self->size);

    ndspChnWaveBufAdd(self->channel, &waveBuf[self->wbuf]);

	return self->wbuf;
}

// Returns duration of audio source in seconds
double sourceGetDuration(source *self) { // source:getDuration()
	return (double)(self->nsamples) / SAMPLERATE;
}

// Returns playback position of audio source in seconds
double sourceTell(source *self) { // source:tell()
	if (!ndspChnIsPlaying(self->channel)) {
		return 0;
	} else {
		return (double)(ndspChnGetSamplePos(self->channel)) / SAMPLERATE;
	}
}

// Subroutine for when a song is correctly played
void songSequence(int index) {
	sftd_font    *font = sftd_load_font_mem(chiaro_otf, chiaro_otf_size);
	sf2d_texture *bgbot = sfil_load_PNG_buffer(bgbottom_png, SF2D_PLACE_RAM);
	sf2d_texture *oot = sfil_load_PNG_buffer(ocarina_png, SF2D_PLACE_RAM);
	sf2d_texture *itemblock = sfil_load_PNG_buffer(itemblock_png, SF2D_PLACE_RAM);
	sf2d_texture *buttons = sfil_load_PNG_buffer(buttons_png, SF2D_PLACE_RAM);
	
	source* song = new source;
	
	float alpha = 0;
	bool fade = true;
	
	string path = "/3ds/orchestrina/data/song"+intToStr(index)+".pcm";
	string played = songnames[index]+".";
	
	sourceInit(song, path.c_str(), 1, SONGWAVBUF);
	sourcePlay(song);
	
    while (aptMainLoop() && waveBuf[SONGWAVBUF].status != NDSP_WBUF_DONE) {
		hidScanInput();
		u32 keys = hidKeysDown();
		
		if(keys & KEY_START) {
            ndspChnWaveBufClear(1);
            break;
        }

		sf2d_start_frame(GFX_TOP, GFX_LEFT);
			sf2d_draw_rectangle(0, 0, 400, 240, RGBA8(0, 0, 0, 255));
			sftd_draw_text(font, 5, 205, RGBA8(255, 255, 255, 255), 12, "You played ");
			sftd_draw_text(font, 72, 205, RGBA8(0, 0, 255, 255), 12, played.c_str());
		sf2d_end_frame();
		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
			sf2d_draw_texture(bgbot, 0, 0);
			sf2d_draw_texture(itemblock, 10, 10);
			sf2d_draw_texture(itemblock, 75, 10);
			sf2d_draw_texture(oot, 15, 15);
			sf2d_draw_texture(buttons, 100, 100);
			sf2d_draw_rectangle(100, 100, 120, 26, RGBA8(255, 255, 0, (int)alpha));
		sf2d_end_frame();
		sf2d_swapbuffers();
		
		if (alpha < 125 && fade) alpha += 2;
		else fade = false;
		
		if (alpha > 0 && !fade) alpha -= 2;
		else fade = true;
	}
    
    // Free stuff
	sftd_free_font(font);
    sf2d_free_texture(bgbot);
    sf2d_free_texture(oot);
    sf2d_free_texture(itemblock);
    sf2d_free_texture(buttons);

    sourceFree(song);
}

int main(int argc, char* argv[])
{
	// Seed rand
	srand(time(NULL));
	
	// Init graphics
	sf2d_init();
	sf2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));
	sf2d_set_3D(0);
	// Init font
	sftd_init();
	
	// Load fonts & textures
    // TODO: load from romfs instead
	sftd_font    *font = sftd_load_font_mem(chiaro_otf, chiaro_otf_size);
	sf2d_texture *bgbot = sfil_load_PNG_buffer(bgbottom_png, SF2D_PLACE_RAM);
	sf2d_texture *oot = sfil_load_PNG_buffer(ocarina_png, SF2D_PLACE_RAM);
	sf2d_texture *itemblock = sfil_load_PNG_buffer(itemblock_png, SF2D_PLACE_RAM);
	sf2d_texture *buttons = sfil_load_PNG_buffer(buttons_png, SF2D_PLACE_RAM);
	
	// Initialize ndsp
	ndspInit();

    ndspSetOutputMode(NDSP_OUTPUT_MONO);
    // ndspSetOutputCount(23);
    
    // Set channel settings
    // Channel 0 is for notes, channel 1 for everything else
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, SAMPLERATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
    ndspChnSetInterp(1, NDSP_INTERP_LINEAR);
    ndspChnSetRate(1, SAMPLERATE);
    ndspChnSetFormat(1, NDSP_FORMAT_MONO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0;
    mix[1] = 1.0;
    ndspChnSetMix(0, mix);
    ndspChnSetMix(1, mix);

    memset(waveBuf,0,sizeof(waveBuf));

	/* Audio sources */
    // Load Ocarina by default
    instrumentInit(1);
    // Load sound effects
    source* correct = new source;
	sourceInit(correct, "/3ds/orchestrina/data/Correct.pcm", 1);

	// Last 20 played notes
	string playingsong = "";

	// Index of song played
	int songtrigger = -1;

	// Index of button being pressed
    int pressed = -1;

    // FOR REFERENCE:
	// bool pressedL = false; // D (0)
	// bool pressedX = false; // B (1)
	// bool pressedY = false; // A (2)
	// bool pressedA = false; // D2 (3)
	// bool pressedR = false; // F (4)

	while(aptMainLoop())
	{
		hidScanInput();
		
		u32 keys = hidKeysDown();
		u32 released = hidKeysUp();

		// Play notes on button presses
		if((keys & KEY_L)) {
            if (pressed != KEY_L) ndspChnWaveBufClear(0);
            pressed = KEY_L;
			sourcePlay(notes[0]);
			playingsong.push_back('l');
		}
		
		if((keys & KEY_X)) {
            if (pressed != KEY_X) ndspChnWaveBufClear(0);
            pressed = KEY_X;
			sourcePlay(notes[1]);
			playingsong.push_back('x');
		}
		
		if((keys & KEY_Y)) {
            if (pressed != KEY_Y) ndspChnWaveBufClear(0);
            pressed = KEY_Y;
			sourcePlay(notes[2]);
			playingsong.push_back('y');
		}
		
		if((keys & KEY_A)) {
            if (pressed != KEY_A) ndspChnWaveBufClear(0);
            pressed = KEY_A;
			sourcePlay(notes[3]);
			playingsong.push_back('a');
		}
		
		if((keys & KEY_R)) {
            if (pressed != KEY_R) ndspChnWaveBufClear(0);
            pressed = KEY_R;
			sourcePlay(notes[4]);
			playingsong.push_back('r');
		}
        
        // Clear played notes
        if (keys & KEY_B) playingsong = "";

		// Check for releases
		if ((released & KEY_L) && pressed==KEY_L) pressed = -1;
		if ((released & KEY_X) && pressed==KEY_X) pressed = -1;
		if ((released & KEY_Y) && pressed==KEY_Y) pressed = -1;
		if ((released & KEY_A) && pressed==KEY_A) pressed = -1;
		if ((released & KEY_R) && pressed==KEY_R) pressed = -1;
		
		// Start top screen
		sf2d_start_frame(GFX_TOP, GFX_LEFT);

			sf2d_draw_rectangle(0, 0, 400, 240, RGBA8(0, 0, 0, 255));
			
			//Debug stuff
			sftd_draw_text(font, 5, 20, RGBA8(255, 255, 255, 255), 24, upperStr(playingsong).c_str()); // Last 20 notes played

		sf2d_end_frame();
		
		// Start bottom screen
		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
			sf2d_draw_texture(bgbot, 0, 0);
			sf2d_draw_texture(itemblock, 10, 10);
			sf2d_draw_texture(itemblock, 75, 10);
			sf2d_draw_texture(oot, 15, 15);
			sf2d_draw_texture(buttons, 100, 100);
			
			// Red highlights when buttons pressed
			if (pressed==KEY_L) sf2d_draw_rectangle(100, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressed==KEY_R) sf2d_draw_rectangle(124, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressed==KEY_X) sf2d_draw_rectangle(148, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressed==KEY_Y) sf2d_draw_rectangle(172, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressed==KEY_A) sf2d_draw_rectangle(196, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
		sf2d_end_frame();

		// If song index is valid
		if (songtrigger != -1) {
			sourcePlay(correct);
            pressed = -1;
			playingsong = "";
            sf2d_swapbuffers();
			svcSleepThread(1500000000);
			// Play the song that was entered
			songSequence(songtrigger);
		}

		// Update song index
        // TODO: check if song is compatible with current instrument
		songtrigger = detectSong(playingsong);

		// Keep note history short
		if (playingsong.size() > 20) {
			playingsong = playingsong.substr(playingsong.size()-20, 20);
		}

        // Exit on Start
		if(keys & KEY_START) break;

		// Swap buffers
		sf2d_swapbuffers();
	}
	
	// Free stuff
	sftd_free_font(font);
    sf2d_free_texture(bgbot);
    sf2d_free_texture(oot);
    sf2d_free_texture(itemblock);
    sf2d_free_texture(buttons);
    
    instrumentFree(1);
    sourceFree(correct);
	
	// Exit services
	ndspExit();
	sf2d_fini();
	sftd_fini();
	
	return 0;
}