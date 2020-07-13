#include <stdio.h>
#include "SDL2/SDL.h"
#include "player.h"
#include "loaddata.h"

struct player player;

static void mySDL_AudioCallback(void*h, Uint8* stream, int len) {
	player_gen(&player, (short *)stream, len >> 1);
}

//#include <emscripten.h>
int main(int argc, char **argv) {
    const char *filename;
    //const char *melobnk = "melodic.bnk";
    //const char *drumbnk = "drum.bnk";

    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        switch (argv[argi++][1]) {
	    /*
            case 'm':
                melobnk = argv[argi++];
                break;
            case 'd':
                drumbnk = argv[argi++];
                break;
	    */
            default:
                fprintf(stderr, "unknown option: %s\n", argv[argi]);
                return 1;
        }
    }
    filename = argi < argc ? argv[argi] : "descent.hmp";

    
	if(SDL_Init(SDL_INIT_AUDIO)<0) {
		printf("SDL_Init failed");
		return 1;
	}
	//atexit(SDL_Quit);

//printf("%d sndbuf_empty=%d\n", __LINE__, sndbuf_empty);
    SDL_AudioSpec spec;
    spec.freq     = 48000;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = 4096;
    spec.callback = mySDL_AudioCallback;
    if(SDL_OpenAudio(&spec, NULL) < 0) {
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        return 1;
    }
    //short buf[1024];
	int freq = spec.freq;

	printf("freq %d\n", freq);

	printf("start play %s\n", filename);

	player_init(&player, freq);

	if (player_play_file(&player, filename, 1)) {
		fprintf(stderr, "play failed\n");
		return 1;
	}
	
	SDL_PauseAudio(0);

#if 0
    while (1) {
        fflush(stdout);
        emscripten_sleep(10);
    //    SDL_Delay(100);
//#ifdef __linux__
	//player_gen(&player, buf, sizeof(buf) / sizeof(buf[0]));
//#endif
        //SDL_Event event;
        //if(!SDL_WaitEventTimeout(&event,0)) break;
        //if (event.type == SDL_QUIT) break;
    }
#endif    
    return 0;
}

int play(const char *filename) {
    printf("play: %s\n", filename);
    player_stop(&player);
	if (player_play_file(&player, filename, 1)) {
		fprintf(stderr, "play failed\n");
		return 1;
	}
	return 0;
}

const char *getsng(void) {
    //void *runner = NULL;
    //int sng_size;
    //return (const char *)loadurl(runner, "descent.sng", &sng_size);
	char *buf;
	const char *fn;
	int idx = 0;
	int bufsize = 64;
	int bufpos = 0;
	if (!(buf = (char *)malloc(bufsize)))
		return NULL;
	while ((fn = cf_entry(idx++))) {
		char *p = strchr(fn, '.');
		if (!p || strcmp(p, ".hmp"))
			continue;
		int l = strlen(fn);
		if (bufpos + l + 2 > bufsize) {
			int size = bufsize + (bufsize >> 1) + 16 + l;
			char *nbuf = (char *)realloc(buf, size);
			if (!nbuf) {
				free(buf);
				return NULL;
			}
			buf = nbuf;
			bufsize = size;
		}
		memcpy(buf + bufpos, fn, l);
		bufpos += l;
		buf[bufpos++] = '\n';
	}
	buf[bufpos] = 0;
	return buf;
}
