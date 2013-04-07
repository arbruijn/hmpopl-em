#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "loaddata.h"
#include "SDL/SDL.h"
#include "SDL/SDL_mixer.h"
#include "dbopl.h"
#include <emscripten.h>
extern "C" {
#include "hmpfile.h"
#include "hmpopl.h"
}

struct song {
    struct hmp_file *hf;
    hmpopl *h;
    int gen;
    unsigned char msg[3];
};

#define SNDBUFSIZE 48000
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
//printf("sendaudio %x\n", deadbeef);
if (sndbuf_write > SNDBUFSIZE) { printf("over before\n"); abrt=1; abort(); }
    while (icount > 0) {
        while (sndbuf_read == sndbuf_write && !sndbuf_empty) {
            printf("wait audio drain w=%d r=%d\n", sndbuf_write, sndbuf_read);
            //SDL_Delay(40);
//if (sndbuf_write > SNDBUFSIZE) { printf("over after\n"); abrt=1; abort(); }
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
//if (sndbuf_write > SNDBUFSIZE) { printf("over after inc w=%d c=%d\n", sndbuf_write, cur); abrt=1; abort(); }
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
//if (sndbuf_write > SNDBUFSIZE) { printf("over after z w=%d\n", sndbuf_write); abrt=1; abort(); }
        }
    }
//if (sndbuf_write > SNDBUFSIZE) { printf("over after w=%d\n", sndbuf_write); abrt=1; abort(); }
    //printf("wrote=%d w=%d r=%d\n", (int)count * 2, sndbuf_write, sndbuf_read);
}



static void writereg(void *data, int idx, int reg, int val) {
    DBOPL::Handler *opl = (DBOPL::Handler *)data;
    opl->WriteReg((idx << 8) | reg, val);
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
	if (data)
		free(data);
	return -1;
}

int song_start(const char *mus_filename, const char *mus_melodic_file, const char *mus_drum_file, DBOPL::Handler *opl, struct song *song) {
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

void generate_samples(DBOPL::Handler *h, int samples) {
    while (samples > 0) {
        int n = samples > 512 ? 512 : samples;
        h->Generate(SendMonoAudio, SendStereoAudio, n);
        samples -= n;
    }
}

int song_step(struct song *song, DBOPL::Handler *h, int freq) {
	int rc;
	hmp_event ev;
	
	if (song->gen) {
	    int max = SNDBUFSIZE / 8;
	    int n = song->gen > max ? max : song->gen;
//if (sndbuf_write > SNDBUFSIZE) { printf("gensmp write over before\n"); abrt=1; abort(); }
        generate_samples(h, n);
//if (sndbuf_write > SNDBUFSIZE) { printf("gensmp write over after\n"); abrt=1; abort(); }
	    song->gen -= n;
	    if (song->gen)
	        return 0;
//if (sndbuf_write > SNDBUFSIZE) { printf("gen play_midi write over before\n"); abrt=1; abort(); }
        hmpopl_play_midi(song->h, song->msg[0] >> 4, song->msg[0] & 0x0f, song->msg[1], song->msg[2]);
//if (sndbuf_write > SNDBUFSIZE) { printf("gen play_midi write over after\n"); abrt=1; abort(); }
	}
	
	for (;;) {
//if (sndbuf_write > SNDBUFSIZE) { printf("get_ev write over before\n"); abrt=1; abort(); }
	    if ((rc = hmp_get_event(song->hf, &ev)))
    	    return rc;
//if (sndbuf_w	rite > SNDBUFSIZE) { printf("get_ev write over after\n"); abrt=1; abort(); }
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
            //generate_samples(h, freq * ev.delta / 120);
        //printf("%02x %02x %02x\n", ev.msg[0], ev.msg[1], ev.msg[2]);
//if (sndbuf_write > SNDBUFSIZE) { printf("play_midi write over before\n"); abrt=1; abort(); }
        hmpopl_play_midi(song->h, ev.msg[0] >> 4, ev.msg[0] & 0x0f, ev.msg[1], ev.msg[2]);
//if (sndbuf_write > SNDBUFSIZE) { printf("play_midi write over after\n"); abrt=1; abort(); }
        //if (ev.delta)
        //    break;
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
    //int olen = len >> 1;
    len >>= 1;
    //write(1, "drain\n", 6);
    //printf("cb %d", len);
if (sndbuf_write > SNDBUFSIZE) { printf("cb write over before %d\n", sndbuf_write); abrt=1; abort(); }
    short* target = (short*) stream;
    while (len > 0) {
        int i;
        if (sndbuf_read == sndbuf_write && sndbuf_empty) {
            //printf("silence r=%d w=%d\n", sndbuf_read, sndbuf_write);
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
if (sndbuf_write > SNDBUFSIZE) { printf("cb write over after\n"); abrt=1; abort(); }
}

char melobuf[32], drumbuf[32];
int getbanks(const char *filename, const char **melobnk, const char **drumbnk) {
    void *runner = NULL;
    int sng_size;
	int freesng;
    const char *sng = (const char *)loadurl_cf(runner, "descent.sng", &sng_size, &freesng);
	if (!sng)
		return 0;
        
    const char *p = sng;
    int filename_len = strlen(filename);
    while (p < sng + sng_size) {
        const char *e;
        if (!(e = (const char *)memchr(p, '\n', sng + sng_size - p)))
            e = sng + sng_size;
        if (e > p && e[-1] == '\r')
            e--;
        //printf("sng: %.*s\n", e - p, p);
        if (e - p > filename_len && !memcmp(p, filename, filename_len) && p[filename_len] == '\t') {
            const char *pmelo = p + filename_len + 1;
            const char *emelo = (const char *)memchr(pmelo, '\t', e - pmelo);
            if (!emelo)
                emelo = e;
            const char *pdrum = emelo + 1;
            const char *edrum = (const char *)memchr(pdrum, '\t', e - pdrum);
            if (!edrum)
                edrum = e;
            if (emelo - pmelo < (int)sizeof(melobuf) && edrum - pdrum < (int)sizeof(drumbuf)) {
                memcpy(melobuf, pmelo, emelo - pmelo);
                melobuf[emelo - pmelo] = 0;
                *melobnk = melobuf;
                memcpy(drumbuf, pdrum, edrum - pdrum);
                drumbuf[edrum - pdrum] = 0;
                *drumbnk = drumbuf;
            }
        }
        p = e + 1;
        while (p < sng + sng_size && (*p == '\r' || *p == '\n'))
            p++;
    }
	if (freesng)
		free((void *)sng);
    return 0;
}

int started = 0;
int freq;
//SDL_AudioSpec obtained;
struct song song;
DBOPL::Handler opl;
extern "C" {
extern int myOpenAudio(SDL_AudioSpec *a, SDL_AudioSpec *b);
extern void myPauseAudio(int n);
}

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
            //myPauseAudio(1);
            //SDL_Quit();
        }
        if (!started && sndbuf_write >= 4800) {
            myPauseAudio(0);
            printf("started\n");
            started = 1;
        }
    }
}

int main(int argc, char **argv) {
    const char *filename;
    const char *melobnk = "melodic.bnk";
    const char *drumbnk = "drum.bnk";


    printf("main called\n");
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

    getbanks(filename, &melobnk, &drumbnk);
    

//printf("%d sndbuf_empty=%d\n", __LINE__, sndbuf_empty);

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
    spec.samples  = 48000 / 2;
    spec.callback = mySDL_AudioCallback;
    if(myOpenAudio(&spec, NULL) < 0) {
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        return 1;
    }
    freq = spec.freq;

    printf("freq %d\n", freq);
//printf("%d sndbuf_empty=%d\n", __LINE__, sndbuf_empty);
    opl.Init(freq);
//printf("%d sndbuf_empty=%d\n", __LINE__, sndbuf_empty);
    

printf("start play %s %s %s\n", filename, melobnk, drumbnk);
    if (song_start(filename, melobnk, drumbnk, &opl, &song))
        return 1;

//printf("%d sndbuf_empty=%d\n", __LINE__, sndbuf_empty);
    emscripten_set_main_loop(step, 20, 0);
//printf("%d sndbuf_empty=%d\n", __LINE__, sndbuf_empty);

    return 0;
}

extern "C" int play(const char *filename) {
    const char *melobnk = "melodic.bnk";
    const char *drumbnk = "drum.bnk";
    song_stop(&song);
    getbanks(filename, &melobnk, &drumbnk);
    if (song_start(filename, melobnk, drumbnk, &opl, &song))
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
	printf("sng=%s\n", buf);
	return buf;
}
