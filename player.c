//#include <stdlib.h>
#include <string.h>
#include "player.h"
#ifndef NOFILE
#include "loaddata.h"
#endif

#define opl_write(h, idx, reg, val) OPL3_WriteRegBuffered((opl3_chip *)h->write_data, (idx << 8) | reg, val)
#define NOALLOC
#include "opl3.c"
#include "hmpopl.c"
#include "hmpfile.c"

#ifndef opl_write
static void writereg(void *data, int idx, int reg, int val) {
    OPL3_WriteRegBuffered((opl3_chip *)data, (idx << 8) | reg, val);
}
#endif

#ifndef NOFILE
static int loadbankfile(void *runner, hmpopl *h, const char *filename, int isdrum) {
	int size;
	int dofree;
	void *data = (void *)loadurl_cf(runner, filename, &size, &dofree);
	if (!data)
		goto err;
	if (hmpopl_set_bank(h, data, size, isdrum))
		goto err;
	#ifndef NOALLOC
	if (dofree)
		free(data);
	#endif
	return 0;
err:
	#ifndef NOALLOC
	if (data && dofree)
		free(data);
	#endif
	return -1;
}
#endif

static const char *song_init(struct song *song, opl3_chip *chip) {
	memset(song, 0, sizeof(*song));
	#ifndef NOALLOC
	if (!(song->h = hmpopl_new()))
		return "create hmpopl failed";
	#endif
	song->h = &song->hbuf;
	hmpopl_set_write_callback(song->h, NULL, chip);
	hmpopl_start(song->h);
	return NULL;
}

#ifndef NOFILE
// returns errormsg if any
static const char *song_start_file(struct song *song, const char *mus_filename, const char *mus_melodic_file, const char *mus_drum_file) {
	char buf[32];
	void *runner = NULL;
	int song_size;
	void *song_data;
	int freesong;

	/*
	if (song->hf) {
		hmp_close(song->hf);
		song->hf = NULL;
	}
	*/

	if (loadbankfile(runner, song->h, mus_melodic_file, 0))
		return "load melo bnk failed";
	if (loadbankfile(runner, song->h, mus_drum_file, 1))
		return "load drum bnk failed";

	if (strlen(mus_filename) < sizeof(buf)) {
		strcpy(buf, mus_filename);
		buf[strlen(buf) - 1] = 'q';
		song_data = (void *)loadurl_cf(runner, buf, &song_size, &freesong);
	} else
		song_data = NULL;
	if (!song_data && !(song_data = (void *)loadurl_cf(runner, mus_filename, &song_size, &freesong)))
		return "open song failed";

	song->hf = &song->hfbuf;
	if (hmp_init(song->hf, song_data, song_size, 0xa009))
		return "read song failed";
	#ifndef NOALLOC
	if (freesong)
		free(song_data);
	#endif

	hmp_reset_tracks(song->hf);
	hmpopl_reset(song->h);
	return NULL;
}
#endif

#ifndef WEB
static void song_done(struct song *song) {
	#ifndef NOALLOC
	hmpopl_done(song->h);
	#endif
}
#endif

static int song_step(struct song *song, int freq) {
	int rc;
	hmp_event ev;
	
	if (song->msg[0]) {
		hmpopl_play_midi(song->h, song->msg[0] >> 4, song->msg[0] & 0x0f, song->msg[1], song->msg[2]);
		song->msg[0] = 0;
	}
	for (;;) {
		if ((rc = hmp_get_event(song->hf, &ev)))
			return rc;
		if (ev.datalen)
			continue;
		if ((ev.msg[0] & 0xf0) == 0xb0 && ev.msg[1] == 7) { // adjust volume
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

#ifndef WEB
static void song_stop(struct song *song) {
	hmpopl_done(song->h);
	hmp_close(song->hf);
	song->h = NULL;
	song->hf = NULL;
}
#endif

static void song_reset(struct song *song) {
	song->msg[0] = 0;
}

const char *player_init(struct player *player, int freq) {
	OPL3_Reset(&player->chip, freq);
	player->freq = freq;
	player->playing = 0;
	return song_init(&player->song, &player->chip);
}

#ifndef NOFILE
#include "getbanks.c"

// returns error message if any
const char *player_play_file(struct player *player, const char *filename, int loop) {
	char melobnk[32], drumbnk[32];
	int ret, sng_size, freesng;
	const char *sng = (const char *)loadurl_cf(NULL, "descent.sng", &sng_size, &freesng);
	if (!sng)
		return "open descent.sng failed";
	ret = getbanks(sng, sng_size, filename, melobnk, sizeof(melobnk), drumbnk, sizeof(drumbnk));
	#ifndef NOALLOC
	if (freesng)
		free(sng);
	#endif
	if (ret)
		return "getbanks failed";
	player->loop = loop;
	player->playing = 1;
	return song_start_file(&player->song, filename, melobnk, drumbnk);
}
#endif

int player_gen(struct player *player, short *buf, int bufsize) {
	int samples = bufsize >> 1; // stereo
	while (player->playing) {
		int n = player->song.gen;
		if (n > 0) {
			if (n > samples)
				n = samples;
	        OPL3_GenerateStream(&player->chip, buf, n);
	        //printf("gen samples %d\n", n);
			samples -= n;
			player->song.gen -= n;
			buf += n << 1;
			if (!samples)
				break;
		}
		if (song_step(&player->song, player->freq)) {
			if (player->loop) {
		        	hmp_reset_tracks(player->song.hf);
	        		hmpopl_reset(player->song.h);
			} else
				player->playing = 0;
		}
	}
	if (samples)
		memset(buf, 0, sizeof(buf[0]) * 2 * samples);
	return 0;
}

#ifndef WEB
int player_stop(struct player *player) {
	//Bit16s smp[2];
	player->playing = 0;
	hmpopl_reset(player->song.h);
	player->song.gen = 0;
	player->song.msg[0] = 0;
	//hmpopl_reset(player->song.h);
	memset(&player->chip, 0, sizeof(player->chip));
	OPL3_Reset(&player->chip, player->freq);
	//hmpopl_start(player->song.h);
	//printf("reset done\n");
	//song_stop(&player->song);
	//for (int i = 0; i < 100; i++)
	//	OPL3_GenerateResampled(&player->chip, smp);
	//OPL3_GenerateResampled(&player->chip, smp);
	//hmpopl_reset(player->song.h);
	return 0;
}

void player_done(struct player *player) {
	song_done(&player->song);
}
#endif

#ifndef NOALLOC
struct player *player_alloc() {
	return calloc(1, sizeof(struct player));
}

void player_free(struct player *player) {
	free(player);
}
#endif
