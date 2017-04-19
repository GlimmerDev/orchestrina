#include <3ds.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>
#include <sf2d.h>
#include <sfil.h>
#include <sftd.h>
#include <dirent.h>
#include <algorithm>
#include <sstream>
#include <cctype>

#define DEFAULTSAMPLERATE 22050
#define BYTESPERSAMPLE 4

#define INSTRUMENTCOUNT 10
#define SONGCOUNT 38

#define MAXNOTES 8
#define MAXSONGSPERINSTRUMENT 24

#define WAVEBUFCOUNT (MAXNOTES+4)
#define SONGWAVEBUF (WAVEBUFCOUNT-1)

#define INST_OCARINA 0
#define INST_PIPES 1
#define INST_DRUMS 2
#define INST_GUITAR 3
#define INST_MALON 4
#define INST_HARP 5
#define INST_FROGS 6
#define INST_MUSBOX 7
#define INST_FLUTE 8
#define INST_WAKER 9

using namespace std;

// One wavebuf for each of the current instrument's notes, one for each sound effect, one for the current full song playing
ndspWaveBuf waveBuf[WAVEBUFCOUNT];
bool wavebufList[WAVEBUFCOUNT];

// Noteset
enum {
    NOTESET_OOT,
    NOTESET_ST,
    NOTESET_WW
};

typedef struct {
    u8 notes;
    string notestr;
    u32 keys[MAXNOTES];
} Noteset;

// Instrument
typedef struct {
    string name;
    u8 nset;
    u8 songs;
    u16 songlist[MAXSONGSPERINSTRUMENT];
} Instrument;

// Song
typedef struct {
    string name;
    string sequence;
} Song;

// Source
typedef struct {
    u32 nsamples;
    u32 size;
    char* data;
    bool loop;
    int wbuf;
    int channel;
} Source;

Source* notes[MAXNOTES];
sf2d_texture* nicons[MAXNOTES];

// Array of notesets
Noteset notesets[3] = {
    { 5, "lxyar",  { KEY_L, KEY_X, KEY_Y, KEY_A, KEY_R } },
    { 6, "lxyarb", { KEY_L, KEY_X, KEY_Y, KEY_A, KEY_R, KEY_B } },
    { 5, "ruldn",  { KEY_A, KEY_X, KEY_Y, KEY_B, 0 } }
};

// Array of songs
Song songs[SONGCOUNT] = {
    { "NULL",                   "------"   }, //  0 - OOT/MM
    { "Zelda's Lullaby",        "xayxay"   }, //  1
    { "Saria's Song",           "ryxryx"   }, //  2
    { "Epona's Song",           "axyaxy"   }, //  3
    { "Sun's Song",             "yrayra"   }, //  4
    { "Song of Time",           "ylrylr"   }, //  5
    { "Song of Storms",         "lralra"   }, //  6
    { "Minuet of Forest",       "laxyxy"   }, //  7
    { "Bolero of Fire",         "rlrlyryr" }, //  8
    { "Serenade of Water",      "lryyx"    }, //  9
    { "Nocturne of Shadow",     "xyylxyr"  }, // 10
    { "Requiem of Spirit",      "lrlyrl"   }, // 11
    { "Prelude of Light",       "ayayxa"   }, // 12
    { "Song of Healing (MM)",   "xyrxyr"   }, // 13
    { "Inverted Song of Time",  "rlyrly"   }, // 14
    { "Song of Double Time",    "yyllrr"   }, // 15
    { "Oath to Order",          "yrlrya"   }, // 16
    { "Song of Soaring",        "rxarxa"   }, // 17
    { "Sonata of Awakening",    "axaxlyl"  }, // 18
    { "Goron's Lullaby",        "lyxlyxyl" }, // 19
    { "New Wave Bossa Nova",    "xaxyrxy"  }, // 20
    { "Elegy of Emptiness",     "yxyryax"  }, // 21
    { "Song of Frogs",          "lrxya"    }, // 22
    { "NULL",                   "------"   }, // 23 - ST
    { "Song of Awakening",      "ya"       }, // 24
    { "Song of Healing (ST)",   "lxl"      }, // 25
    { "Song of Discovery",      "aray"     }, // 26
    { "Song of Light",          "brayx"    }, // 27
    { "Song of Birds",          "brb"      }, // 28
    { "NULL",                   "------"   }, // 29 - CUSTOM/HIDDEN
    { "Chai Kingdom",           "yxaxy"    }, // 30
    { "NULL",                   "------"   }, // 31 - WW
    { "Wind's Requiem",         "ulr"      }, // 32
    { "Ballad of Gales",        "drlu"     }, // 33
    { "Command Melody",         "lnrn"     }, // 34
    { "Earth God's Lyric",      "ddarln"   }, // 35
    { "Wind God's Aria",        "uudrlr"   }, // 36
    { "Song of Passing",        "rld"      }  // 37
};

// Array of instruments
Instrument instruments[INSTRUMENTCOUNT] = {
    { "Ocarina", NOTESET_OOT, 22,
        {  1,  2,  3,  4,  5,  6,
           7,  8,  9, 10, 11, 12,
          13, 14, 15, 16, 17, 18,
          19, 20, 21, 30          }
    },
    { "Pipes", NOTESET_OOT, 2,
        { 18, 21 }
    },
    { "Drums", NOTESET_OOT, 1,
        { 19 }
    },
    { "Guitar", NOTESET_OOT, 1,
        { 20 }
    },
    { "Malon", NOTESET_OOT, 1,
        {  3 }
    },
    { "Harp", NOTESET_OOT, 6,
        {  7,  8,  9, 10, 11, 12 }
    },
    { "Frogs", NOTESET_OOT, 1,
        { 22 }
    },
    { "Music Box", NOTESET_OOT, 1,
        {  6 }
    },
    { "Spirit Flute", NOTESET_ST, 5,
        { 24, 25, 26, 27, 28 }
    },
    { "Wind Waker", NOTESET_WW, 6,
        { 32, 33, 34, 35, 36, 37 }
    }
};

// Converts string to uppercase characters
string upperStr(string in) {
    for (u32 i = 0; i < in.size(); ++i) {
        in[i] = toupper(in[i]);
    }
    return in;
}

// Converts boolean value to string
string boolToStr(bool in) {
    if (in == true) return "True";
    else return "False";
}

// Converts integer value to string
string intToStr(int num) {
    stringstream ss;
    ss << num;
    return ss.str();
}

// Download song from URL
Result downloadSong(u16 songid) {
    Result ret = 0;
    u32 statuscode = 0;
    u32 contentsize = 0;
    httpcContext context;
    string SongName = songs[songid].name;
    replace(SongName.begin(), SongName.end(), ' ', '-');
    string URL = "https://raw.githubusercontent.com/EBLeifEricson/orchestrina/master/data/Songs/" + SongName + ".pcm";

    ret = httpcOpenContext(&context, HTTPC_METHOD_GET, URL.c_str(), 1);
    if(R_FAILED(ret))return ret;

    ret = httpcAddRequestHeaderField(&context, "User-Agent", "Orchestrina");
    if(R_FAILED(ret)) {
        httpcCloseContext(&context);
        return ret;
    }

    ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
    if(R_FAILED(ret)) {
        httpcCloseContext(&context);
        return ret;
    }

    ret = httpcBeginRequest(&context);
    if(R_FAILED(ret)) {
        httpcCloseContext(&context);
        return ret;
    }

    ret = httpcGetResponseStatusCode(&context, &statuscode);
    if(R_FAILED(ret)) {
        httpcCloseContext(&context);
        return ret;
    }

    if (statuscode != 200) {
        if (statuscode >= 300 && statuscode < 400) {
            char newUrl[1024];
            httpcGetResponseHeader(&context, (char*)"Location", newUrl, 1024);
            httpcCloseContext(&context);
            ret = downloadSong(songid);
            return ret;
        }
        return statuscode;
    }

    ret = httpcGetDownloadSizeState(&context, NULL, &contentsize);
    if(R_FAILED(ret)) {
        httpcCloseContext(&context);
        return ret;
    }

    if(contentsize==0) {
        httpcCloseContext(&context);
        return 0xFFFFFFFE;
    }

    u8* filebuffer = (u8*)malloc(contentsize);
    if (filebuffer==NULL) return 0xFFFFFFFD;

    ret = httpcDownloadData(&context, filebuffer, contentsize, NULL);
    if(R_FAILED(ret)) {
        httpcCloseContext(&context);
        return ret;
    }

    httpcCloseContext(&context);

    FILE* songfile = fopen(("/3ds/orchestrina/data/songs/" + songs[songid].name + ".pcm").c_str(), "wb");
    if (songfile==NULL) return 0xFFFFFFFC;

    if (fwrite(filebuffer, 1, contentsize, songfile) < contentsize) {
        fclose(songfile);
        return 0xFFFFFFFB;
    }

    fclose(songfile);

    return 0;
}

// Detect if touch was within the specified coordinate region
bool isTouchInRegion(touchPosition coords, int min_x, int max_x, int min_y, int max_y) {
	return ((coords.px > min_x) && (coords.px <  max_x) && (coords.py > min_y) && (coords.py < max_y));
}

// Detects if a song has been played by the player
int detectSong(string sequence, u8 instrumentid) {

    for (u8 i = 0; i < instruments[instrumentid].songs; i++) {
        u32 pos = sequence.find(songs[instruments[instrumentid].songlist[i]].sequence);
        if (pos != string::npos) return instruments[instrumentid].songlist[i];
    }

    return -1;
}

// Returns the first unused wavBuf available for use
int getOpenWavbuf() {

    for (u32 i = 0; i <= WAVEBUFCOUNT; i++) {
        if (!wavebufList[i]) {
            wavebufList[i] = true;
            return i;
        }
    }

    return -1;

}

// Initializes an audio source and returns any errors that may occur
int sourceInit(Source *self, const char *filename, int channel, int wbuf = -1) {
    FILE *file = fopen(filename, "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        self->size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Set wavebuf
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

// Frees an audio source and its wavebuf
void sourceFree(Source *self) {
    if (self==NULL) return;
    wavebufList[self->wbuf] = false;
    if (waveBuf[self->wbuf].status == NDSP_WBUF_PLAYING || waveBuf[self->wbuf].status==NDSP_WBUF_QUEUED) ndspChnWaveBufClear(self->channel);
    linearFree(self->data);
    delete self;
}

// Initializes an instrument and returns any errors that may occur
int instrumentInit(u8 id) {

    for (u32 i = 0; i < notesets[instruments[id].nset].notes; i++) {
        // Free previous instrument's resources
        if (nicons[i] != NULL) sf2d_free_texture(nicons[i]);
        sourceFree(notes[i]);

        // Load new instrument's resources
        string ipath = "romfs:/notes/"+intToStr(instruments[id].nset)+"_"+intToStr(i)+".png";
        string spath = "romfs:/sound/notes/"+instruments[id].name+"/"+intToStr(i)+".pcm";

        nicons[i] = sfil_load_PNG_file(ipath.c_str(), SF2D_PLACE_RAM);
        notes[i] = new Source;

        int result = sourceInit(notes[i], spath.c_str(), 0);
        if (result!=0) return -1;
    }

    return 0;
}

// Frees an instrument and its resources
void instrumentFree(u8 id) {

    for (u32 i = 0; i < notesets[instruments[id].nset].notes; i++) {
        if (nicons[i] != NULL) sf2d_free_texture(nicons[i]);
        sourceFree(notes[i]);
    }
}

// Plays an initialized audio source on its assigned DSP channel
int sourcePlay(Source *self) { // Source:play()

    if (self==NULL || self->wbuf == -1) return -1;

    DSP_FlushDataCache((u32*)self->data, self->size);

    ndspChnWaveBufAdd(self->channel, &waveBuf[self->wbuf]);

    return self->wbuf;
}

int main(int argc, char* argv[]) {
    // Seed rand
    srand(time(NULL));

    // Init romfs
    romfsInit();

    // Init graphics
    sf2d_init();
    sftd_init();
    sf2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));
    sf2d_set_3D(0);

    // Load fonts & textures
    sftd_font    *font  = sftd_load_font_file("romfs:/fonts/chiaro.otf");
    sf2d_texture *bgbot = sfil_load_PNG_file("romfs:/bgbottom.png", SF2D_PLACE_RAM);
	sf2d_texture *bgtop = sfil_load_PNG_file("romfs:/bgtop.png", SF2D_PLACE_RAM);
    sf2d_texture *iicons[INSTRUMENTCOUNT];
    for (u32 i = 0; i < INSTRUMENTCOUNT; i++) iicons[i] = sfil_load_PNG_file(("romfs:/instruments/"+instruments[i].name+".png").c_str(), SF2D_PLACE_RAM);
    sf2d_texture *optionblock = sfil_load_PNG_file("romfs:/optionblock.png", SF2D_PLACE_RAM);
    sf2d_texture *oinsts = sfil_load_PNG_file("romfs:/o_instruments.png", SF2D_PLACE_RAM);
    sf2d_texture *osongs = sfil_load_PNG_file("romfs:/o_songs.png", SF2D_PLACE_RAM);
	sf2d_texture *inventory = sfil_load_PNG_file("romfs:/inventory.png", SF2D_PLACE_RAM);

    // Create data path
    mkdir("/3ds", 0777);
    mkdir("/3ds/orchestrina", 0777);
    mkdir("/3ds/orchestrina/data", 0777);
    mkdir("/3ds/orchestrina/data/songs", 0777);

    // Number of songs found on the SD card
    int nsongs = 0;

    // Check if all songs are present
    for (int i = 0; i < SONGCOUNT; i++) {
        FILE* f = fopen(("/3ds/orchestrina/data/songs/"+songs[i].name+".pcm").c_str(), "rb");
        if (f!=NULL || songs[i].name=="NULL") nsongs++;
        fclose(f);
    }

    // Initialize ndsp
    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_MONO);

    // Set channel settings
    // Channel 0 is for notes, channel 1 for everything else
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, DEFAULTSAMPLERATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
    ndspChnSetInterp(1, NDSP_INTERP_LINEAR);
    ndspChnSetRate(1, DEFAULTSAMPLERATE);
    ndspChnSetFormat(1, NDSP_FORMAT_MONO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0;
    mix[1] = 1.0;
    ndspChnSetMix(0, mix);
    ndspChnSetMix(1, mix);

    memset(waveBuf,0,sizeof(waveBuf));

    // Load sound effects
    Source* correct = new Source;
    Source* menuOpen = new Source;
    Source* menuSelect = new Source;
    sourceInit(correct, "romfs:/sound/Correct.pcm", 1);
    sourceInit(menuOpen, "romfs:/sound/Menu-Open.pcm", 1);
    sourceInit(menuSelect, "romfs:/sound/Menu-Select.pcm", 1);

    // Last 20 played notes
    string playingsong = "";

    // Index of current selected instrument
    u8 currentinstrument = 0;

    // Load Ocarina by default
    instrumentInit(0);

    // Index of song played
    int songtrigger = -1;

    // Index of button being pressed
    u32 pressed = 0xFF;

	// Whether free play is on or not
	bool freePlay = (nsongs<SONGCOUNT);

    bool errorflag = false;
    Result errorcode = 0;
	
	// Position of inventory graphic
	int invYPos = -240;
	
	bool invOpen = false;
	
	touchPosition touch;
    circlePosition pos;
	
	int wakertimer = 4;
	bool wakertimerdir = false;
	u32 rhythm = 3;
	int separation = 0;

    while(aptMainLoop()) {
        hidScanInput();

        u32 keys = hidKeysDown();
        u32 released = hidKeysUp();
		u32 held = hidKeysHeld();
        hidTouchRead(&touch);
        hidCircleRead(&pos);

        u32 keyset[MAXNOTES];
        memcpy(keyset, notesets[instruments[currentinstrument].nset].keys, MAXNOTES * sizeof(u32));

		// Play note on button press (or on rythym for wind waker)
		if ((currentinstrument != INST_WAKER) || (wakertimer == 0)) {
			u32 songLen = playingsong.size();
			for (u32 i = 0; i < notesets[instruments[currentinstrument].nset].notes; i++) {
				if ((currentinstrument == INST_WAKER) && (held & keyset[i])) {
					ndspChnWaveBufClear(0);
					pressed = i;
					sourcePlay(notes[i]);
					playingsong.push_back(notesets[instruments[currentinstrument].nset].notestr.at(i));
					break;
				}
				else if((currentinstrument != INST_WAKER) && (keys & keyset[i])) {
					if (pressed != i) ndspChnWaveBufClear(0);
					pressed = i;
					sourcePlay(notes[i]);
					playingsong.push_back(notesets[instruments[currentinstrument].nset].notestr.at(i));
				}
				// Check for releases
				if ((released & keyset[i]) && pressed==i) pressed = 0xFF;
				if ((released & keyset[i]) && held==i) held = 0xFF;
			}
			if ((playingsong.size() == songLen) && (currentinstrument == INST_WAKER)) {
				if ((playingsong.length() < rhythm) || freePlay) {
					ndspChnWaveBufClear(0);
					sourcePlay(notes[4]);
				}
				playingsong.push_back(notesets[instruments[currentinstrument].nset].notestr.at(4));
			}
		}
		
		if ((wakertimer > 100) || (wakertimer < -100)) wakertimerdir = !wakertimerdir;
		
		// Increment wind waker rythym timer
		if (currentinstrument == INST_WAKER) {
			if (wakertimerdir) wakertimer -= 4;
			else wakertimer += 4;
		}
		
		// Keep the waker in rythym for free play
		if ((currentinstrument == INST_WAKER) && (playingsong.size() > rhythm) && (!freePlay)) {
			playingsong = "";
		}

        // Change frequence based on circle pad's coordinates
        if (pos.dy > 20 || pos.dy < -20) ndspChnSetRate(0, DEFAULTSAMPLERATE + 74*pos.dy);
        else ndspChnSetRate(0, DEFAULTSAMPLERATE);
		
		if (!freePlay && currentinstrument == INST_WAKER) {
			if (keys & KEY_DLEFT) { 
				rhythm = 4; 
				separation = 0; 
				playingsong = ""; 
			}
			else if (keys & KEY_DRIGHT) { 
				rhythm = 6; 
				separation = 0;
				playingsong = ""; 
			}
			else if ((released & KEY_DLEFT) || ((released & KEY_DRIGHT) && (rhythm != 3))){ 
				rhythm = 3; 
				separation = 0; 
				playingsong = ""; 
			}
		}
		if (separation < 19) ++separation;

        // Download missing songs
        if ((keys & KEY_SELECT) && (nsongs < SONGCOUNT)) {
            httpcInit(0);
            for (int i = 0; i < SONGCOUNT; i++) {
                FILE* f = fopen(("/3ds/orchestrina/data/songs/"+songs[i].name+".pcm").c_str(), "rb");
                if (f==NULL && songs[i].name!="NULL") {
                    string drawstr = "Downloading " + songs[i].name + "...";
                    sf2d_start_frame(GFX_TOP, GFX_LEFT);
                        sf2d_draw_texture(bgtop, 0, 0);
                        sftd_draw_text(font, (400 - drawstr.size()*6)/2, 114, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 12, drawstr.c_str());
                    sf2d_end_frame();
                    sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
                        sf2d_draw_texture(bgbot, 0, 0);
                    sf2d_end_frame();
                    sf2d_swapbuffers();
                    errorcode = downloadSong(i);
                    if (errorcode==0) nsongs++;
                    else errorflag = true;
                } else fclose(f);
            }
            httpcExit();
        }

		// Toggle free play
		if ((keys & KEY_SELECT) && (nsongs==SONGCOUNT)) {
            freePlay ^= true;
            playingsong = "";
            songtrigger = -1;
        }

        // Show song list
        if (isTouchInRegion(touch, 248, 248+52, 40, 40+39) && !invOpen) {
            ndspChnWaveBufClear(1);
            sourcePlay(menuOpen);
			invOpen = true;
            int cursor = 0;
			while (invOpen && aptMainLoop()) {
                hidScanInput();
                u32 keys = hidKeysDown();

                if (keys & KEY_B) invOpen = false; // Close inventory
                
                if (keys & KEY_DOWN) {
                    if (cursor < SONGCOUNT - 15) cursor++;
                }
                if (keys & KEY_UP) {
                    if (cursor > 0) cursor--;
                }

                sf2d_start_frame(GFX_TOP, GFX_LEFT);
					sf2d_draw_texture(bgtop, 0, 0);
                    sf2d_draw_rectangle(0, 0, 400, 240, RGBA8(0x00, 0x00, 0x00, 0x7F));
                sf2d_end_frame();
                sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
                    sf2d_draw_texture(bgbot, 0, 0);
                    for (int i = 0; i < 15; i++) {
                        sftd_draw_text(font, 0, i*16, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 12, (intToStr(cursor+i)+". "+songs[cursor+i].name).c_str());
                        sftd_draw_text(font, 256, i*16, RGBA8(0x00, 0xC0, 0x00, 0xFF), 12, songs[cursor+i].sequence.c_str());
                    }
                sf2d_end_frame();
                sf2d_swapbuffers();
            }
        }

		// Switch instrument (touch)
		if (isTouchInRegion(touch, 20, 20+52, 40, 40+39) && !invOpen) {
            ndspChnWaveBufClear(1);
			sourcePlay(menuOpen);
			invOpen = true;
			while (invOpen && aptMainLoop()) {
                hidScanInput();
                u32 keys = hidKeysDown();

                if (keys & KEY_B) invOpen = false; // Close inventory

                sf2d_start_frame(GFX_TOP, GFX_LEFT);
					sf2d_draw_texture(bgtop, 0, 0);
                    sf2d_draw_rectangle(0, 0, 400, 240, RGBA8(0x00, 0x00, 0x00, 0x7F));
                sf2d_end_frame();
                sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
                    sf2d_draw_texture(bgbot, 0, 0);
                    sf2d_draw_texture(inventory, 36, invYPos);
					if (invYPos == 0) {
						hidTouchRead(&touch);
						for (int i = 0; i < INSTRUMENTCOUNT; ++i) {
							int xPos = 48*(i%5)+40;
							int yPos = 48*((int)(i/5)%4)+5;
							sf2d_draw_texture(iicons[i], xPos, yPos);
							if (isTouchInRegion(touch, xPos, xPos+48, yPos, yPos+48)) {
								sourcePlay(menuSelect);
								currentinstrument = i;
								instrumentInit(currentinstrument);
								playingsong = "";
								invOpen = false;
							}
						}
					}
                sf2d_end_frame();
                sf2d_swapbuffers();
				if (invYPos < 0) invYPos += 10;
            }
			invOpen = false;
			// Slide inventory sprite back up
			while (invYPos > -240) {
				sf2d_start_frame(GFX_TOP, GFX_LEFT);
					sf2d_draw_texture(bgtop, 0, 0);
                sf2d_end_frame();
                sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
				    sf2d_draw_texture(bgbot, 0, 0);
                    sf2d_draw_texture(inventory, 36, invYPos);
				sf2d_end_frame();
                sf2d_swapbuffers();
				invYPos -= 10;
			}
		}

        // Start top screen
        sf2d_start_frame(GFX_TOP, GFX_LEFT);
			sf2d_draw_texture(bgtop, 0, 0);

            //Debug stuff
            if (nsongs < SONGCOUNT) sftd_draw_text(font, 5, 5, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 12, "WARNING: Some songs are missing.\nYou will be unable to switch out of free play mode.\nPress SELECT to download all missing songs.");
            if (errorflag) sftd_draw_text(font, 5, 48, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 12, ("Error while downloading file. "+intToStr(errorcode)).c_str());
            sftd_draw_text(font, 5, 64, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 24, upperStr(playingsong).c_str()); // Note history
			if (currentinstrument == INST_WAKER && (!freePlay)) {
				for (u32 i = 0; i < rhythm; ++i) sf2d_draw_rectangle_rotate(8+i*separation, 100, 10, 10, RGBA8(0x00, 0x00, 0x00, 0xFF), 0.785398);
				for (u32 i = 0; i < playingsong.length(); ++i) sf2d_draw_rectangle_rotate(8+i*separation, 100, 10, 10, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 0.785398);
			}
            sftd_draw_text(font, 5, 205, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 12, ("Instrument: "+instruments[currentinstrument].name).c_str()); // Current instrument
			sftd_draw_text(font, 5, 220, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 12, ("Free Play (SEL): "+boolToStr(freePlay)).c_str()); // Free Play status

        sf2d_end_frame();

        // Start bottom screen
        sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);

            sf2d_draw_texture(bgbot, 0, 0);
			sftd_draw_text(font, 0, 0, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 12, "v0.5.0 by Leif Ericson and Ryuzaki_MrL.");
			sftd_draw_text(font, 0, 15, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 12, "Top screen graphic by Sliter.");

			sf2d_draw_texture(optionblock, 20, 40); // Instrument select button
			sf2d_draw_texture(oinsts, 25, 45); // Instrument select icon
			
			// Wind waker rythym bar
			if (currentinstrument == INST_WAKER) {
				for (int i = 0; i < 9; ++i) {
					sf2d_draw_fill_circle(97+(i*7), 60, 2, RGBA8(0x00, 0x00, 0x00, 0xFF));
				}
				for (int i = 0; i < 9; ++i) {
					sf2d_draw_fill_circle(167+(i*7), 60, 2, RGBA8(0x00, 0x00, 0x00, 0xFF));
				}
				sf2d_draw_fill_circle(90, 60, 3, RGBA8(0x00, 0x00, 0x00, 0xFF));
				sf2d_draw_fill_circle(160, 60, 4, RGBA8(255, 51, 51, 0xFF));
				sf2d_draw_fill_circle(230, 60, 3, RGBA8(0x00, 0x00, 0x00, 0xFF));
				
				// White circle
				sf2d_draw_fill_circle(160+(floor(wakertimer/10)*7), 60, 2, RGBA8(0xFF, 0xFF, 0xFF, 0xFF));
			}

            sf2d_draw_texture(optionblock, 248, 40); // Song select button
			sf2d_draw_texture(osongs, 253, 45); // Song select icon

            for (u32 i = 0; i < notesets[instruments[currentinstrument].nset].notes; i++) {

                int x = (160 - notesets[instruments[currentinstrument].nset].notes * 14) + (i * 28);

                // Draw noteset
                sf2d_draw_texture(nicons[i], x, 100);

                // Red highlights when buttons pressed
                if (pressed==i) sf2d_draw_rectangle(x, 100, 24, 24, RGBA8(0xFF, 0x00, 0x00, 0x7F));
            }

        sf2d_end_frame();

        // If song index is valid
        if (songtrigger != -1 && !freePlay) {
            sourcePlay(correct);
            pressed = 0xFF;
            playingsong = "";
            sf2d_swapbuffers();
            svcSleepThread(1500000000);

            // Play the song that was entered
            Source* fullsong = new Source;

            float alpha = 0;
            bool fade = true;

            string played = songs[songtrigger].name;
            string path = "/3ds/orchestrina/data/songs/"+played+".pcm";

            sourceInit(fullsong, path.c_str(), 1, SONGWAVEBUF);
            sourcePlay(fullsong);

            while (aptMainLoop() && waveBuf[SONGWAVEBUF].status != NDSP_WBUF_DONE) {
                hidScanInput();
                u32 keys = hidKeysDown();

                if (keys & KEY_B) break; // Cancel song playback

                sf2d_start_frame(GFX_TOP, GFX_LEFT);
					sf2d_draw_texture(bgtop, 0, 0);
                    sftd_draw_text(font, 5, 205, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 12, ("You played " + played + ".").c_str());
                sf2d_end_frame();
                sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
                    sf2d_draw_texture(bgbot, 0, 0);
                    int x = (160 - notesets[instruments[currentinstrument].nset].notes * 14);
                    for (u32 i = 0; i < notesets[instruments[currentinstrument].nset].notes; i++) sf2d_draw_texture(nicons[i], x + (i * 28), 100);
                    sf2d_draw_rectangle(x - 2, 100, notesets[instruments[currentinstrument].nset].notes * 28, 24, RGBA8(0xFF, 0xFF, 0x00, (int)alpha));
                sf2d_end_frame();
                sf2d_swapbuffers();

                if (alpha < 125 && fade) alpha += 2;
                else fade = false;

                if (alpha > 0 && !fade) alpha -= 2;
                else fade = true;
            }

            sourceFree(fullsong);
        }

        // Update song index
        songtrigger = detectSong(playingsong, currentinstrument);

        // Keep note history short
        if (playingsong.size() > 20) {
            playingsong = playingsong.substr(playingsong.size()-20, 20);
        }

        // Exit on Start
        if (keys & KEY_START) break;

        // Swap buffers
        sf2d_swapbuffers();
    }

    // Free stuff
    sftd_free_font(font);
    sf2d_free_texture(bgbot);
    sf2d_free_texture(bgtop);
    sf2d_free_texture(optionblock);
    sf2d_free_texture(oinsts);
    sf2d_free_texture(osongs);
    sf2d_free_texture(inventory);
    for (u32 i = 0; i < INSTRUMENTCOUNT; i++) sf2d_free_texture(iicons[i]);

    instrumentFree(currentinstrument);
    sourceFree(correct);
    sourceFree(menuOpen);
    sourceFree(menuSelect);

    // Exit services
    romfsExit();
    ndspExit();
    sf2d_fini();
    sftd_fini();

    return 0;
}
