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

using namespace std;

// Converts string to uppercase characters
string upperStr(string in) {
	for (int i = 0; i < in.size(); ++i) {
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


// Array of note sequences
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

// Detects if a song has been played by the player
int detectSong(string song) {
	for (int i = 0; i < sizeof(playable)/sizeof(playable[0]); ++i) {
		int pos = song.find(playable[i]);
		if (pos != std::string::npos) return i;
	}
	return -1;
}

// Source type
typedef enum {
	TYPE_UNKNOWN = -1,
	TYPE_OGG = 0,
	TYPE_WAV = 1
} source_type;

// Source
typedef struct {
	source_type type;
	
	float rate;
	u32 channels;
	u32 encoding;
	u32 nsamples;
	u32 size;
	char* data;
	bool loop;
	int audiochannel;

	float mix[12];
	ndspInterpType interp;
} source;

bool channelList[24];

// Returns the first unused DSP channel available for use
int getOpenChannel() {

	for (int i = 0; i <= 23; i++) {
		if (!channelList[i]) {
			channelList[i] = true;
			return i;
		}
	}

	return -1;

}

// Initializes an audio source and returns any errors that may occur
const char *sourceInit(source *self, const char *filename) {
	FILE *file = fopen(filename, "rb");
	if (file) {
		bool valid = true;
		char buff[8];

		// Master chunk
		fread(buff, 4, 1, file); // ckId
		if (strncmp(buff, "RIFF", 4) != 0) valid = false;

		fseek(file, 4, SEEK_CUR); // skip ckSize

		fread(buff, 4, 1, file); // WAVEID
		if (strncmp(buff, "WAVE", 4) != 0) valid = false;

		// fmt Chunk
		fread(buff, 4, 1, file); // ckId
		if (strncmp(buff, "fmt ", 4) != 0) valid = false;

		fread(buff, 4, 1, file); // ckSize
		if (*buff != 16) valid = false; // should be 16 for PCM format

		fread(buff, 2, 1, file); // wFormatTag
		if (*buff != 0x0001) valid = false; // PCM format

		u16 channels;
		fread(&channels, 2, 1, file); // nChannels
		self->channels = channels;
		
		u32 rate;
		fread(&rate, 4, 1, file); // nSamplesPerSec
		self->rate = rate;

		fseek(file, 4, SEEK_CUR); // skip nAvgBytesPerSec

		u16 byte_per_block; // 1 block = 1*channelCount samples
		fread(&byte_per_block, 2, 1, file); // nBlockAlign

		u16 byte_per_sample;
		fread(&byte_per_sample, 2, 1, file); // wBitsPerSample
		byte_per_sample /= 8; // bits -> bytes

		// There may be some additionals chunks between fmt and data
		fread(&buff, 4, 1, file); // ckId
		while (strncmp(buff, "data", 4) != 0) {
			u32 size;
			fread(&size, 4, 1, file); // ckSize

			fseek(file, size, SEEK_CUR); // skip chunk

			int i = fread(&buff, 4, 1, file); // next chunk ckId

			if (i < 4) { // reached EOF before finding a data chunk
				valid = false;
				break;
			}
		}

		// data Chunk (ckId already read)
		u32 size;
		fread(&size, 4, 1, file); // ckSize
		self->size = size;

		self->nsamples = self->size / byte_per_block;

		if (byte_per_sample == 1) self->encoding = NDSP_ENCODING_PCM8;
		else if (byte_per_sample == 2) self->encoding = NDSP_ENCODING_PCM16;
		else return "unknown encoding, needs to be PCM8 or PCM16";

		if (!valid) {
			fclose(file);
			return "invalid PCM wav file";
		}

		self->audiochannel = getOpenChannel();
		self->loop = false;

		// Read data
		if (linearSpaceFree() < self->size) return "not enough linear memory available";
		self->data = (char*)linearAlloc(self->size);

		fread(self->data, self->size, 1, file);


		fclose(file);
	}
	else return "file not found";
	return "ok";
}

// Plays an initialized audio source on its assigned DSP channel
int sourcePlay(source *self) { // source:play()

	if (self->audiochannel == -1) {
		return -1;
	}

	ndspChnWaveBufClear(self->audiochannel);
	ndspChnReset(self->audiochannel);
	ndspChnInitParams(self->audiochannel);
	ndspChnSetMix(self->audiochannel, self->mix);
	ndspChnSetInterp(self->audiochannel, self->interp);
	ndspChnSetRate(self->audiochannel, self->rate);
	ndspChnSetFormat(self->audiochannel, NDSP_CHANNELS(self->channels) | NDSP_ENCODING(self->encoding));

	ndspWaveBuf* waveBuf = (ndspWaveBuf*)calloc(1, sizeof(ndspWaveBuf));

	waveBuf->data_vaddr = self->data;
	waveBuf->nsamples = self->nsamples;
	waveBuf->looping = self->loop;

	DSP_FlushDataCache((u32*)self->data, self->size);

	ndspChnWaveBufAdd(self->audiochannel, waveBuf);

	return self->audiochannel;
}

// Stop playback of an audio source
int sourceStop(source *self) { // source:stop()
	ndspChnWaveBufClear(self->audiochannel);
	return 0;
}

// Checks if audio source is playing on its assigned DSP channel
bool sourceIsPlaying(source *self) { // source:isPlaying()
	return ndspChnIsPlaying(self->audiochannel);
}

// Returns duration of audio source in seconds
double sourceGetDuration(source *self) { // source:getDuration()
	return (double)(self->nsamples) / self->rate;
}

// Returns playback position of audio source in seconds
double sourceTell(source *self) { // source:tell()
	if (!ndspChnIsPlaying(self->audiochannel)) {
		return 0;
	} else {
		return (double)(ndspChnGetSamplePos(self->audiochannel)) / self->rate;
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
	
	string path = "sdmc:/3ds/orchestrina/data/song"+intToStr(index)+".wav";
	string played = songnames[index]+".";
	
	sourceInit(song, path.c_str());
	sourcePlay(song);
	
	while (sourceTell(song) != 0) {
		hidScanInput();
		u32 keys = hidKeysDown();
		u32 released = hidKeysUp();
		
		if(keys & KEY_START)
			break;
		
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
	sftd_font    *font = sftd_load_font_mem(chiaro_otf, chiaro_otf_size);
	sf2d_texture *bgbot = sfil_load_PNG_buffer(bgbottom_png, SF2D_PLACE_RAM);
	sf2d_texture *oot = sfil_load_PNG_buffer(ocarina_png, SF2D_PLACE_RAM);
	sf2d_texture *itemblock = sfil_load_PNG_buffer(itemblock_png, SF2D_PLACE_RAM);
	sf2d_texture *buttons = sfil_load_PNG_buffer(buttons_png, SF2D_PLACE_RAM);
	
	// Initialize ndsp
	ndspInit();
	ndspSetOutputCount(23);
	
	// Audio sources
	source* ocarina1 = new source;
	sourceInit(ocarina1, "sdmc:/3ds/orchestrina/data/OOT_Notes_Ocarina_D_long.wav");
	source* ocarina2 = new source;
	sourceInit(ocarina2, "sdmc:/3ds/orchestrina/data/OOT_Notes_Ocarina_B_long.wav");
	source* ocarina3 = new source;
	sourceInit(ocarina3, "sdmc:/3ds/orchestrina/data/OOT_Notes_Ocarina_A_long.wav");
	source* ocarina4 = new source;
	sourceInit(ocarina4, "sdmc:/3ds/orchestrina/data/OOT_Notes_Ocarina_D2_long.wav");
	source* ocarina5 = new source;
	sourceInit(ocarina5, "sdmc:/3ds/orchestrina/data/OOT_Notes_Ocarina_F_long.wav");
	source* correct = new source;
	sourceInit(correct, "sdmc:/3ds/orchestrina/data/OOT_Song_Correct.wav");
	
	// Last 20 played notes
	string playingsong = "";
	
	// Debug result of playing audio
	int playRes = -1;
	
	// Index of song played
	int songtrigger = -1;

	// Button press bools for highlighting
	bool pressedL = false; // D 
	bool pressedX = false; // B
	bool pressedY = false; // A
	bool pressedA = false; // D2
	bool pressedR = false; // F
	
	
	
	while(aptMainLoop())
	{
		hidScanInput();
		
		u32 keys = hidKeysDown();
		u32 released = hidKeysUp();
	
		// Exit on Start
		if(keys & KEY_START)
			break;
		
		// Play notes on button presses
		if((keys & KEY_L) && !pressedL) {
			pressedL = true;
			playRes = sourcePlay(ocarina1);
			playingsong.push_back('l');
		}
		
		if((keys & KEY_X) && !pressedX) {
			pressedX = true;
			playRes = sourcePlay(ocarina2);
			playingsong.push_back('x');
		}
		
		if((keys & KEY_Y) && !pressedY) {
			pressedY = true;
			playRes = sourcePlay(ocarina3);
			playingsong.push_back('y');
		}
		
		if((keys & KEY_A) && !pressedA) {
			pressedA = true;
			playRes = sourcePlay(ocarina4);
			playingsong.push_back('a');
		}
		
		if((keys & KEY_R) && !pressedR) {
			pressedR = true;

			playRes = sourcePlay(ocarina5);
			playingsong.push_back('r');
		}
		
		// Check for releases
		if ((released & KEY_L) && pressedL) pressedL = false;
		if ((released & KEY_X) && pressedX) pressedX = false;
		if ((released & KEY_Y) && pressedY) pressedY = false;
		if ((released & KEY_A) && pressedA) pressedA = false;
		if ((released & KEY_R) && pressedR) pressedR = false;
		
		// Start top screen
		sf2d_start_frame(GFX_TOP, GFX_LEFT);

			sf2d_draw_rectangle(0, 0, 400, 240, RGBA8(0, 0, 0, 255));
			
			//Debug stuff
			sftd_draw_text(font, 5, 20, RGBA8(255, 255, 255, 255), 24, upperStr(playingsong).c_str()); // Last 20 notes played
			sftd_draw_text(font, 5, 50, RGBA8(255, 255, 255, 255), 12, ("Channel: "+intToStr(playRes)).c_str()); // Audio channel of last sound played
			string play = "L: "+boolToStr(sourceIsPlaying(ocarina1))+" X: "+boolToStr(sourceIsPlaying(ocarina2))+" Y: "+boolToStr(sourceIsPlaying(ocarina3))+\
			" A: "+boolToStr(sourceIsPlaying(ocarina4))+" R: "+boolToStr(sourceIsPlaying(ocarina5)); 
			sftd_draw_text(font, 5, 70, RGBA8(255, 255, 255, 255), 12, (play.c_str())); // Status of each button's sound

		sf2d_end_frame();
		
		// Start bottom screen
		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
			sf2d_draw_texture(bgbot, 0, 0);
			sf2d_draw_texture(itemblock, 10, 10);
			sf2d_draw_texture(itemblock, 75, 10);
			sf2d_draw_texture(oot, 15, 15);
			sf2d_draw_texture(buttons, 100, 100);
			
			// Red highlights when buttons pressed
			if (pressedL) sf2d_draw_rectangle(100, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressedR) sf2d_draw_rectangle(124, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressedX) sf2d_draw_rectangle(148, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressedY) sf2d_draw_rectangle(172, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressedA) sf2d_draw_rectangle(196, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
		sf2d_end_frame();

		// If song index is valid
		if (songtrigger != -1) {
			// Prevent button highlights from getting stuck
			pressedL = false; pressedX = false; pressedY = false; pressedA = false; pressedR = false;
			playRes = sourcePlay(correct);
			// Clear played notes
			playingsong = "";
			sf2d_swapbuffers();
			svcSleepThread(1500000000);
			// Play the song that was entered
			songSequence(songtrigger);
		}
		
		// Update song index
		songtrigger = detectSong(playingsong);
		
		// Keep note history short
		if (playingsong.size() > 20) {
			playingsong = playingsong.substr(playingsong.size()-20, 20);
		}
		
		// Swap buffers
		sf2d_swapbuffers();
	}
	
	// Free stuff
	sftd_free_font(font);
	// TODO: Free everything up correctly
	
	// Exit services
	ndspExit();
	sf2d_fini();
	sftd_fini();
	
	return 0;
}