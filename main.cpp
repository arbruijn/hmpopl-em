#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "loaddata.h"
#include "SDL2/SDL.h"
//#include "SDL/SDL_mixer.h"
#ifdef NUKED
extern "C" {
#include "opl3.h"
}
#else
#include "dbopl.h"
#endif
//#include <emscripten.h>
extern "C" {
#include "hmpfile.h"
#include "hmpopl.h"
}

#ifdef NUKED
typedef opl3_chip opl_t;
#else
typedef DBOPL::Handler opl_t;
#endif

struct song {
    struct hmp_file *hf;
    hmpopl *h;
    int gen;
    unsigned char msg[3];
};

#define SNDBUFSIZE 65536
//int deadbeef = 0xdeadbeef;
short sndbuf[SNDBUFSIZE];
int sndbuf_write = 0;
int sndbuf_read = 0;
int sndbuf_empty = 1;
int abrt = 0;
int my_nice_zero = 0;

static void SendMonoAudio(unsigned long count, int* samples) {
    abrt=1; abort();
}
static void SendStereoAudio(unsigned long count, int* samples) {
    int icount = (int)count * 2;
    while (icount > 0) {
        while (sndbuf_read == sndbuf_write && !sndbuf_empty) {
            printf("wait audio drain w=%d r=%d\n", sndbuf_write, sndbuf_read);
            //SDL_Delay(40);
            return;
        }
        int cur = (sndbuf_write < sndbuf_read ? sndbuf_read : SNDBUFSIZE) - sndbuf_write;
        short *buf;
        if (cur > icount)
            cur = icount;
        buf = &sndbuf[sndbuf_write];
        for (int i = 0; i < cur; i++) {
            int n = samples[i] * 2;
            buf[i] = n < -32768 ? -32768 : n > 32767 ? 32767 : (short)n;
        }
        samples += cur;
        icount -= cur;
        //if (sndbuf_write >= SNDBUFSIZE)
        //    sndbuf_write = my_nice_zero;
        sndbuf_write += cur;
        //sndbuf_write = ~sndbuf_write;
        //char *p = (char *)&sndbuf_write;
        //p[0] = ~p[0]; p[1] = ~p[1]; p[2] = ~p[2]; p[3] = ~p[3];
        if (sndbuf_write >= SNDBUFSIZE)
            sndbuf_write -= SNDBUFSIZE;
        //printf("snd write %d\n", cur);
        if(sndbuf_empty) {
            //printf("%d sndbuf_empty=%d\n", __LINE__, sndbuf_empty);
            sndbuf_empty = 0;
            //printf("%d sndbuf_empty=%d\n", __LINE__, sndbuf_empty);
        }
        if (sndbuf_write == SNDBUFSIZE) {
            char *p = (char *)&sndbuf_write;
            p[0] = p[1] = p[2] = p[3] = 0;
            //*(short *)&sndbuf_write = 0;
            //*((short *)&sndbuf_write + 1) = 0;
            printf("prez %d\n", sndbuf_write);
            sndbuf_write = ~my_nice_zero;
            printf("postz %d\n", sndbuf_write);
            sndbuf_write = ~sndbuf_write;
            printf("postz %d\n", sndbuf_write);
        }
    }
    //printf("wrote=%d w=%d r=%d\n", (int)count * 2, sndbuf_write, sndbuf_read);
}

static void writereg(void *data, int idx, int reg, int val) {
    #ifdef NUKED
    OPL3_WriteRegBuffered((opl_t *)data, (idx << 8) | reg, val);
    #else
    DBOPL::Handler *opl = (DBOPL::Handler *)data;
    opl->WriteReg((idx << 8) | reg, val);
    #endif
    //printf("%d.%02x, %02x ", idx, reg, val);
}

static int loadbankfile(void *runner, hmpopl *h, const char *filename, int isdrum) {
	int size;
	int dofree;
	void *data = (void *)loadurl_cf(runner, filename, &size, &dofree);
	if (!data)
	    goto err;
	if (hmpopl_set_bank(h, data, size, isdrum))
		goto err;
	if (dofree)
		free(data);
	return 0;
err:
	if (data && dofree)
		free(data);
	return -1;
}

int song_start(const char *mus_filename, const char *mus_melodic_file, const char *mus_drum_file, opl_t *opl, struct song *song) {
    char buf[32];
    void *runner = NULL;
    int song_size;
	void *song_data;
    struct hmp_file *hf;
    hmpopl *h;
	int freesong;
	
    memset(song, 0, sizeof(*song));
    if (!(h = hmpopl_new())) {
    	fprintf(stderr, "create hmpopl failed\n");
    	return -1;
    }

	if (loadbankfile(runner, h, mus_melodic_file, 0)) {
    	fprintf(stderr, "load bnk %s failed\n", mus_melodic_file);
    	goto err;
    }
	if (loadbankfile(runner, h, mus_drum_file, 1)) {
    	fprintf(stderr, "load bnk %s failed\n", mus_drum_file);
    	goto err;
    }

    if (strlen(mus_filename) < sizeof(buf)) {
        strcpy(buf, mus_filename);
        buf[strlen(buf) - 1] = 'q';
        song_data = (void *)loadurl_cf(runner, buf, &song_size, &freesong);
    } else
        song_data = NULL;
    if (!song_data && !(song_data = (void *)loadurl_cf(runner, mus_filename, &song_size, &freesong))) {
        fprintf(stderr, "open %s failed\n", mus_filename);
        goto err;
    }

    if (!(hf = hmp_open(song_data, song_size, 0xa009))) {
        fprintf(stderr, "read %s failed\n", mus_filename);
        goto err;
    }
	if (freesong)
	    free(song_data);

    hmpopl_set_write_callback(h, writereg, opl);
    hmpopl_start(h);
	hmp_reset_tracks(hf);
	hmpopl_reset(h);
    song->h = h;
    song->hf = hf;
    return 0;
err:
    hmpopl_done(h);
    return -1;
}

void generate_samples(opl_t *h, int samples) {
    #ifdef NUKED
    samples <<= 1;
    while (samples) {
        while (sndbuf_read == sndbuf_write && !sndbuf_empty) {
            printf("wait audio drain w=%d r=%d\n", sndbuf_write, sndbuf_read);
            SDL_Delay(40);
        }
        int n = (sndbuf_write < sndbuf_read ? sndbuf_read : SNDBUFSIZE) - sndbuf_write;
        if (n > samples)
            n = samples;
        //printf("gen stream %d\n", n);
        OPL3_GenerateStream(h, sndbuf + sndbuf_write, n >> 1);
        if ((sndbuf_write += n) == SNDBUFSIZE)
            sndbuf_write = 0;
        sndbuf_empty = 0;
        samples -= n;
    }
    #else
    while (samples > 0) {
        int n = samples > 512 ? 512 : samples;
        h->Generate(SendMonoAudio, SendStereoAudio, n);
        samples -= n;
    }
    #endif
}

int song_step(struct song *song, opl_t *h, int freq) {
	int rc;
	hmp_event ev;
	
	if (song->gen) {
	    int max = SNDBUFSIZE / 8;
	    int n = song->gen > max ? max : song->gen;
        generate_samples(h, n);
	    song->gen -= n;
	    if (song->gen)
	        return 0;
        hmpopl_play_midi(song->h, song->msg[0] >> 4, song->msg[0] & 0x0f, song->msg[1], song->msg[2]);
	}
	
	for (;;) {
	    if ((rc = hmp_get_event(song->hf, &ev)))
    	    return rc;
	    if (ev.datalen)
	        continue;
        if ((ev.msg[0] & 0xf0) == 0xb0 && ev.msg[1] == 7) {
            int vol = ev.msg[2];
            vol = (vol * 127) >> 7;
            vol = (vol * 127) >> 7;
            ev.msg[2] = vol;
        }
        if (ev.delta) {
            song->gen = freq * ev.delta / song->hf->tempo;
            song->msg[0] = ev.msg[0];
            song->msg[1] = ev.msg[1];
            song->msg[2] = ev.msg[2];
            break;
        }
        //printf("%02x %02x %02x\n", ev.msg[0], ev.msg[1], ev.msg[2]);
        hmpopl_play_midi(song->h, ev.msg[0] >> 4, ev.msg[0] & 0x0f, ev.msg[1], ev.msg[2]);
	}
	return 0;
}

void song_stop(struct song *song) {
    hmpopl_done(song->h);
    hmp_close(song->hf);
	song->h = NULL;
	song->hf = NULL;
}

int sinpos = 0;
static void mySDL_AudioCallback(void*, Uint8* stream, int len) {
if (abrt) return;
        if (sndbuf_write >= SNDBUFSIZE) // workaround arm bug
            sndbuf_write -= SNDBUFSIZE;
    int olen = len >> 1;
    len >>= 1;
    //write(1, "drain\n", 6);
    //printf("cb %d", len);
    short* target = (short*) stream;
    while (len > 0) {
        int i;
        if (sndbuf_read == sndbuf_write && sndbuf_empty) {
            printf("silence r=%d w=%d olen=%d\n", sndbuf_read, sndbuf_write, olen);
            for (i = 0; i < len; i++)
                target[i] = 0; //(short)(sin(((double)sinpos++ * 2 * M_PI) / 100.0) * 32000.0);
            break;
        }
        int n = (sndbuf_read < sndbuf_write ? sndbuf_write : SNDBUFSIZE) - sndbuf_read;
        if (n > len)
            n = len;
        for (i = 0; i < n; i++)
            target[i] = sndbuf[sndbuf_read + i];
        sndbuf_read += n;
        target += n;
        len -= n;
        if (sndbuf_read == SNDBUFSIZE)
            sndbuf_read = 0;
        if (sndbuf_read == sndbuf_write)
            sndbuf_empty = 1;
    }
    //printf("read=%d w=%d r=%d\n", olen, sndbuf_write, sndbuf_read);
}

char melobuf[32], drumbuf[32];
#include "getbanks.c"

int started = 0;
int freq;
//SDL_AudioSpec obtained;
struct song song;
#ifdef NUKED
opl3_chip opl;
#else
DBOPL::Handler opl;
#endif

void step() {
    if (abrt) return;
	int did_reset = 0;
    while (1) {
        int r = sndbuf_read, w = sndbuf_write;
        int avail = r - w + (w > r || (r == w && sndbuf_empty) ? SNDBUFSIZE : 0);
        if (avail < SNDBUFSIZE / 2) {
            //printf("wait r=%d w=%d e=%d, avail=%d\n", r, w, sndbuf_empty, avail);
            break;
        }
        int rc;
        if ((rc = song_step(&song, &opl, freq))) {
			//if (rc != 1)
	        printf("step failed: %d\n", rc);
			if (did_reset) {
				printf("huh? no data after reset\n");
				abrt = 1;
				return;
			}
        	hmp_reset_tracks(song.hf);
        	hmpopl_reset(song.h);
			did_reset = 1;
            //SDL_PauseAudio(1);
            //SDL_Quit();
        }
        if (!started && sndbuf_write >= 8192) {
            SDL_PauseAudio(0);
            printf("started\n");
            started = 1;
        }
    }
}

int main(int argc, char **argv) {
    const char *filename;
    const char *melobnk = "melodic.bnk";
    const char *drumbnk = "drum.bnk";

    sndbuf_empty = 1;
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        switch (argv[argi++][1]) {
            case 'm':
                melobnk = argv[argi++];
                break;
            case 'd':
                drumbnk = argv[argi++];
                break;
            default:
                fprintf(stderr, "unknown option: %s\n", argv[argi]);
                return 1;
        }
    }
    filename = argi < argc ? argv[argi] : "descent.hmp";

    getbanks(filename, melobuf, sizeof(melobuf), drumbuf, sizeof(drumbuf));
    melobnk = melobuf;
    drumbnk = drumbuf;
    
	if(SDL_Init(SDL_INIT_AUDIO)<0) {
		printf("SDL_Init failed");
		return 1;
	}
	//atexit(SDL_Quit);

//printf("%d sndbuf_empty=%d\n", __LINE__, sndbuf_empty);
#ifndef __linux__
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
    freq = spec.freq;

    printf("freq %d\n", freq);
    #ifdef NUKED
    OPL3_Reset(&opl, freq);
    #else
    opl.Init(freq);
    #endif
#endif

    printf("start play %s %s %s\n", filename, melobnk, drumbnk);
    if (song_start(filename, melobnk, drumbnk, &opl, &song))
        return 1;

    while (1) {
        step();
        fflush(stdout);
        SDL_Delay(1);
        //SDL_Event event;
        //if(!SDL_WaitEventTimeout(&event,0)) break;
        //if (event.type == SDL_QUIT) break;
    }
    return 0;
}

extern "C" int play(const char *filename) {
	printf("play %s\n", filename);
    const char *melobnk = "melodic.bnk";
    const char *drumbnk = "drum.bnk";
    song_stop(&song);
    getbanks(filename, melobuf, sizeof(melobuf), drumbuf, sizeof(drumbuf));
    if (song_start(filename, melobuf, drumbuf, &opl, &song))
        return 1;
    return 0;
}

extern "C" const char *getsng(void) {
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
		if (!strstr(fn, ".hmp"))
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
