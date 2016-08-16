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

#define SAMPLERATE 22050
#define BYTESPERSAMPLE 4

#define INSTRUMENTCOUNT 8
#define SONGCOUNT 25

#define MAXNOTES 16
#define MAXSONGSPERINSTRUMENT (SONGCOUNT)

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
    u8 notes;
    u8 songs;
    u16 songlist[MAXSONGSPERINSTRUMENT];
} instrument;

typedef struct {
    string name;
    string sequence;
} song;

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

// Array of song titles
song songs[SONGCOUNT] = {
    { "NULL",                   "zzzzzz"   }, //  0
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
    { "Song of Healing",        "xyrxyr"   }, // 13
    { "Inverted Song of Time",  "rlyrly"   }, // 14
    { "Song of Double Time",    "yyllrr"   }, // 15
    { "Oath to Order",          "yrlrya"   }, // 16
    { "Song of Soaring",        "rxarxa"   }, // 17
    { "Sonata of Awakening",    "axaxlyl"  }, // 18
    { "Goron's Lullaby",        "lyxlyxyl" }, // 19
    { "New Wave Bossa Nova",    "xaxyrxy"  }, // 20
    { "Elegy of Emptiness",     "yxyryax"  }, // 21
    { "Song of Frogs",          "lrxya"    }, // 22
    { "NULL",                   "zzzzzz"   }, // 23
    { "NULL",                   "zzzzzz"   }  // 24
};

// Array of instruments
instrument instruments[INSTRUMENTCOUNT] = {
    { "NULL", 0, 0 },
    { "Ocarina", 5, 21,
        {  1,  2,  3,  4,  5,  6,  7,
           8,  9, 10, 11, 12, 13, 14,
          15, 16, 17, 18, 19, 20, 21 }
    },
    { "Pipes", 5, 2,
        { 18, 21 }
    },
    { "Drums", 5, 1,
        { 19 }
    },
    { "Guitar", 5, 1,
        { 20 }
    },
    { "Malon", 5, 1,
        {  3 }
    },
    { "Harp", 5, 6,
        {  7,  8,  9, 10, 11, 12 }
    },
    { "Frogs", 5, 1,
        { 22 }
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
int detectSong(string sequence, u8 instrumentid) {
    for (u8 i = 0; i < instruments[instrumentid].songs; i++) {
        u32 pos = sequence.find(songs[instruments[instrumentid].songlist[i]].sequence);
        if (pos != std::string::npos) return instruments[instrumentid].songlist[i];
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
        string path = "/3ds/orchestrina/data/notes/"+instruments[id].name+"/"+intToStr(i)+".pcm";
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

int main(int argc, char* argv[])
{
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
    sftd_font    *font = sftd_load_font_file("romfs:/chiaro.otf");
    sf2d_texture *bgbot = sfil_load_PNG_file("romfs:/bgbottom.png", SF2D_PLACE_RAM);
    sf2d_texture *oot = sfil_load_PNG_file("romfs:/ocarina.png", SF2D_PLACE_RAM);
    sf2d_texture *itemblock = sfil_load_PNG_file("romfs:/itemblock.png", SF2D_PLACE_RAM);
    sf2d_texture *buttons = sfil_load_PNG_file("romfs:/buttons.png", SF2D_PLACE_RAM);
    
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

    // Load sound effects
    source* correct = new source;
    sourceInit(correct, "/3ds/orchestrina/data/Correct.pcm", 1);

    // Last 20 played notes
    string playingsong = "";

    // Index of current selected instrument
    u8 currentinstrument = 1;
    
    // Load Ocarina by default
    instrumentInit(1);

    // Index of song played
    int songtrigger = -1;

    // Index of button being pressed
    int pressed = -1;

    // FOR REFERENCE:
    // KEY_L    :   D
    // KEY_X    :   B
    // KEY_Y    :   A
    // KEY_L    :   D2
    // KEY_R    :   F

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
        
        // Switch instrument (debug)
        if (keys & KEY_SELECT) {
            currentinstrument++;
            if (currentinstrument == INSTRUMENTCOUNT) currentinstrument = 1;
            instrumentInit(currentinstrument);
        }

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
            sftd_draw_text(font, 5, 205, RGBA8(255, 255, 255, 255), 12, ("Instrument: "+instruments[currentinstrument].name).c_str()); // Current instrument

        sf2d_end_frame();

        // Start bottom screen
        sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
            sf2d_draw_texture(bgbot, 0, 0);
            sf2d_draw_texture(itemblock, 10, 10);
            sf2d_draw_texture(itemblock, 75, 10);
            sf2d_draw_texture(oot, 12, 12);
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
            source* fullsong = new source;

            float alpha = 0;
            bool fade = true;

            string played = songs[songtrigger].name;
            string path = "/3ds/orchestrina/data/songs/"+played+".pcm";

            sourceInit(fullsong, path.c_str(), 1, SONGWAVBUF);
            sourcePlay(fullsong);

            while (aptMainLoop() && waveBuf[SONGWAVBUF].status != NDSP_WBUF_DONE) {
                hidScanInput();
                u32 keys = hidKeysDown();

                if (keys & KEY_B) break;

                sf2d_start_frame(GFX_TOP, GFX_LEFT);
                    sf2d_draw_rectangle(0, 0, 400, 240, RGBA8(0, 0, 0, 255));
                    sftd_draw_text(font, 5, 205, RGBA8(255, 255, 255, 255), 12, ("You played "+played+".").c_str()); // Current song playing
                sf2d_end_frame();
                sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
                    sf2d_draw_texture(bgbot, 0, 0);
                    sf2d_draw_texture(itemblock, 10, 10);
                    sf2d_draw_texture(itemblock, 75, 10);
                    sf2d_draw_texture(oot, 12, 12);
                    sf2d_draw_texture(buttons, 100, 100);
                    sf2d_draw_rectangle(100, 100, 120, 26, RGBA8(255, 255, 0, (int)alpha));
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

    instrumentFree(currentinstrument);
    sourceFree(correct);

    // Exit services
    romfsExit();
    ndspExit();
    sf2d_fini();
    sftd_fini();

    return 0;
}