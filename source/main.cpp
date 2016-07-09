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

#include "yellowbg_png.h"
#include "impact_ttf.h"
#include "chiaro_otf.h"
#include "bgbottom_png.h"
#include "ocarina_png.h"
#include "itemblock_png.h"
#include "buttons_png.h"
#include "pressed_png.h"
#include "cleff_png.h"
#include "selected_png.h"

using namespace std;

// Wave buffer
ndspWaveBuf waveBuf;
u8* data = NULL;
u8* data2 = NULL;

string intToStr( int num )
{
	stringstream ss;
	ss << num;
	return ss.str();
}

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

int detectSong(string song) {
	for (int i = 0; i < sizeof(playable)/sizeof(playable[0]); ++i) {
		int pos = song.find(playable[i]);
		if (pos != std::string::npos) return i;
	}
	return -1;
}

int playWav(string path, int channel = 1, bool toloop = true) {
	u32 sampleRate;
	u32 dataSize;
	u16 channels;
	u16 bitsPerSample;
	
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspSetOutputCount(2); // Num of buffers
	
	// Reading wav file
	FILE* fp = fopen(path.c_str(), "rb");
	
	if(!fp)
	{
		printf("Could not open the example.wav file.\n");
		return -1;
	}
	
	char signature[4];
	
	fread(signature, 1, 4, fp);
	
	if( signature[0] != 'R' &&
		signature[1] != 'I' &&
		signature[2] != 'F' &&
		signature[3] != 'F')
	{
		printf("Wrong file format.\n");
		fclose(fp);
		return -1;
	}
	
	fseek(fp, 40, SEEK_SET);
	fread(&dataSize, 4, 1, fp);
	fseek(fp, 22, SEEK_SET);
	fread(&channels, 2, 1, fp);
	fseek(fp, 24, SEEK_SET);
	fread(&sampleRate, 4, 1, fp);
	fseek(fp, 34, SEEK_SET);
	fread(&bitsPerSample, 2, 1, fp);
	
	if(dataSize == 0 || (channels != 1 && channels != 2) ||
		(bitsPerSample != 8 && bitsPerSample != 16))
	{
		printf("Corrupted wav file.\n");
		fclose(fp);
		return -1;
	}
	
	int usingData = 0;
	if (!data) {
		if (data2) linearFree(data2); data2 = NULL;
		usingData = 1;
		// Allocating and reading samples
		data = static_cast<u8*>(linearAlloc(dataSize));
		fseek(fp, 44, SEEK_SET);
		fread(data, 1, dataSize, fp);
		fclose(fp);
	}
	
	else if (!data2) {
		usingData = 2;
		data2 = static_cast<u8*>(linearAlloc(dataSize));
		fseek(fp, 44, SEEK_SET);
		fread(data2, 1, dataSize, fp);
		fclose(fp);
	}
	
	else {
		linearFree(data); data = NULL;
		usingData = 1;
		data = static_cast<u8*>(linearAlloc(dataSize));
		fseek(fp, 44, SEEK_SET);
		fread(data, 1, dataSize, fp);
		fclose(fp);
	}
	
	
	fseek(fp, 44, SEEK_SET);
	fread(data, 1, dataSize, fp);
	fclose(fp);
	
	// Find the right format
	u16 ndspFormat;
	
	if(bitsPerSample == 8)
	{
		ndspFormat = (channels == 1) ?
			NDSP_FORMAT_MONO_PCM8 :
			NDSP_FORMAT_STEREO_PCM8;
	}
	else
	{
		ndspFormat = (channels == 1) ?
			NDSP_FORMAT_MONO_PCM16 :
			NDSP_FORMAT_STEREO_PCM16;
	}
	
	ndspChnReset(channel);
	ndspChnSetInterp(channel, NDSP_INTERP_NONE);
	ndspChnSetRate(channel, float(sampleRate));
	ndspChnSetFormat(channel, ndspFormat);
	
	// Create and play a wav buffer
	std::memset(&waveBuf, 0, sizeof(waveBuf));
	
	waveBuf.data_vaddr = reinterpret_cast<u32*>(data);
	waveBuf.nsamples = dataSize / (bitsPerSample >> 3);
	waveBuf.looping = false;
	waveBuf.status = NDSP_WBUF_FREE;
	
	if (usingData == 1) DSP_FlushDataCache(data, dataSize);
	else DSP_FlushDataCache(data2, dataSize);
	
	ndspChnWaveBufAdd(channel, &waveBuf);
	
	return ((dataSize / (bitsPerSample >> 3)) / sampleRate); // Return duration
	
}

void songSequence(int index) {
	sftd_font *font = sftd_load_font_mem(chiaro_otf, chiaro_otf_size);
	sf2d_texture *background = sfil_load_PNG_buffer(yellowbg_png, SF2D_PLACE_RAM);
	sf2d_texture *bgbot = sfil_load_PNG_buffer(bgbottom_png, SF2D_PLACE_RAM);
	sf2d_texture *oot = sfil_load_PNG_buffer(ocarina_png, SF2D_PLACE_RAM);
	sf2d_texture *itemblock = sfil_load_PNG_buffer(itemblock_png, SF2D_PLACE_RAM);
	sf2d_texture *buttons = sfil_load_PNG_buffer(buttons_png, SF2D_PLACE_RAM);
	
	float alpha = 0;
	bool fade = true;
	
	ndspChnWaveBufClear(7);
	string path = "sdmc:/3ds/orchestrina/data/song"+intToStr(index)+".wav";
	string played = songnames[index]+".";
	int duration = playWav(path,7,false);
	int frame = 0;
	while (frame < (duration*30)) {
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
			sf2d_draw_rectangle(100, 100, 120, 26, RGBA8(0xFF, 0x00, 0x00, (int)alpha));
		sf2d_end_frame();
		sf2d_swapbuffers();
		
		if (alpha < 125 && fade) alpha += 2;
		else fade = false;
		
		if (alpha > 0 && !fade) alpha -= 2;
		else fade = true;
		
		++frame;
	}
	
	for (int i = 1; i < 22; ++i) {
		ndspChnWaveBufClear(i);
	}
	linearFree(data);
}

int main(int argc, char* argv[])
{
	srand(time(NULL));
	
	sf2d_init();
	sf2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));
	sf2d_set_3D(0);
	
	sftd_init();

	sftd_font *font = sftd_load_font_mem(impact_ttf, impact_ttf_size);
	sf2d_texture *background = sfil_load_PNG_buffer(yellowbg_png, SF2D_PLACE_RAM);
	sf2d_texture *bgbot = sfil_load_PNG_buffer(bgbottom_png, SF2D_PLACE_RAM);
	sf2d_texture *oot = sfil_load_PNG_buffer(ocarina_png, SF2D_PLACE_RAM);
	sf2d_texture *itemblock = sfil_load_PNG_buffer(itemblock_png, SF2D_PLACE_RAM);
	sf2d_texture *buttons = sfil_load_PNG_buffer(buttons_png, SF2D_PLACE_RAM);
	sf2d_texture *cleff = sfil_load_PNG_buffer(cleff_png, SF2D_PLACE_RAM);
	sf2d_texture *selected = sfil_load_PNG_buffer(selected_png, SF2D_PLACE_RAM);
	
	// Initialize ndsp
	ndspInit();
	
	string playingsong = "";
	int currnote = 0;
	
	int textWidth = 0;
	int textHeight = 0;
	
	bool pressedL = false; // D 
	bool pressedX = false; // B
	bool pressedY = false; // A
	bool pressedA = false; // D2
	bool pressedR = false; // F
	
	int songtrigger = -1;
	
	while(aptMainLoop())
	{
		hidScanInput();
		
		u32 keys = hidKeysDown();
		u32 released = hidKeysUp();
		
		if(keys & KEY_START)
			break;
		
		if((keys & KEY_L) && !pressedL) {
			pressedL = true;
			playWav("sdmc:/3ds/orchestrina/data/OOT_Notes_Ocarina_D_long.wav",2,false);
			playingsong.push_back('l');
			++currnote;
		}
		
		if((keys & KEY_X) && !pressedX) {
			pressedX = true;
			playWav("sdmc:/3ds/orchestrina/data/OOT_Notes_Ocarina_B_long.wav",3, false);
			playingsong.push_back('x');
			++currnote;
		}
		
		if((keys & KEY_Y) && !pressedY) {
			pressedY = true;
			playWav("sdmc:/3ds/orchestrina/data/OOT_Notes_Ocarina_A_long.wav",4,false);
			playingsong.push_back('y');
			++currnote;
		}
		
		if((keys & KEY_A) && !pressedA) {
			pressedA = true;
			playWav("sdmc:/3ds/orchestrina/data/OOT_Notes_Ocarina_D2_long.wav",5,false);
			playingsong.push_back('a');
			++currnote;
		}
		
		if((keys & KEY_R) && !pressedR) {
			pressedR = true;
			playWav("sdmc:/3ds/orchestrina/data/OOT_Notes_Ocarina_F_long.wav",6,false);
			playingsong.push_back('r');
			++currnote;
		}
		
		if ((released & KEY_L) && pressedL) pressedL = false;
		if ((released & KEY_X) && pressedX) pressedX = false;
		if ((released & KEY_Y) && pressedY) pressedY = false;
		if ((released & KEY_A) && pressedA) pressedA = false;
		if ((released & KEY_R) && pressedR) pressedR = false;
		
		sf2d_start_frame(GFX_TOP, GFX_LEFT);

			sf2d_draw_rectangle(0, 0, 400, 240, RGBA8(0, 0, 0, 255));
			sftd_draw_text(font, 5, 20, RGBA8(0, 0, 0, 255), 12, playingsong.c_str());

		sf2d_end_frame();
		
		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
			sf2d_draw_texture(bgbot, 0, 0);
			sf2d_draw_texture(itemblock, 10, 10);
			sf2d_draw_texture(itemblock, 75, 10);
			sf2d_draw_texture(oot, 15, 15);
			sf2d_draw_texture(buttons, 100, 100);
			if (pressedL) sf2d_draw_rectangle(100, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressedR) sf2d_draw_rectangle(124, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressedX) sf2d_draw_rectangle(148, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressedY) sf2d_draw_rectangle(172, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
			if (pressedA) sf2d_draw_rectangle(196, 100, 24, 26, RGBA8(0xFF, 0x00, 0x00, 0x7F));
		sf2d_end_frame();
		
		if (playingsong.size() > playingsong.max_size()) playingsong = "";

		if (songtrigger != -1) {
			ndspChnWaveBufClear(1);
			playWav("sdmc:/3ds/orchestrina/data/OOT_Song_Correct.wav",1,false);
			playingsong = "";
			sf2d_swapbuffers();
			svcSleepThread(1500000000);
			hidScanInput();
			songSequence(songtrigger);
			hidInit();
		}
		songtrigger = detectSong(playingsong);
		
		sf2d_swapbuffers();
	}
	
	for (int i = 1; i < 22; ++i) {
		ndspChnWaveBufClear(i);
	}
	
	linearFree(data);
	linearFree(data2);
	
	sftd_free_font(font);
	
	ndspExit();
	sf2d_fini();
	sftd_fini();
	
	return 0;
}