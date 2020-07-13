#define NOFILE 1
#define WEB 1
extern void log_int(int n);
typedef unsigned long size_t;
void *memset(void *av, int c, size_t len) {
	char *a = av;
	while (len--)
		*a++ = c;
	return av;
}
int memcmp(const void *av, const void *bv, size_t len) {
	const char *a = av, *b = bv;
	while (len--) {
		int n = (unsigned char)*a++ - (unsigned char)*b++;
		if (n)
			return n;
	}
	return 0;
}

#if 0
void *memchr(const void *pv, int c, size_t len) {
	const char *p = pv;
	while (len--)
		if (*p == c)
			return (void *)p;
		else
			p++;
	return (void *)0;
}

void *memcpy(void *av, const void *bv, size_t len) {
	char *a = av;
	const char *b = bv;
	while (len--)
		*a++ = *b++;
	return av;
}


size_t strlen(const char *s) {
	const char *p = s;
	while (*p)
		p++;
	return p - s;
}
#endif

#include "player.c"

struct player player;

char song_data[196608];
char melobnk_data[5404];
char drumbnk_data[5404];
short sndbuf[48000];

char *get_song_data() {
	return song_data;
}

char *get_melobnk_data() {
	return melobnk_data;
}

char *get_drumbnk_data() {
	return drumbnk_data;
}

short *playerweb_gen(int samps) {
	player_gen(&player, sndbuf, samps);
	return sndbuf;
}

//const char *playerweb_play(void *song, int song_len, void *melobnk, int melobnk_len, void *drumbnk, int drumbnk_len, int loop) {
const char *playerweb_play(int song_len, int melobnk_len, int drumbnk_len, int loop) {
	player.loop = loop;
	player.playing = 1;
	player.song.hf = &player.song.hfbuf;
	if (hmpopl_set_bank(player.song.h, melobnk_data, melobnk_len, 0))
		return "read melobnk failed";
	if (hmpopl_set_bank(player.song.h, drumbnk_data, drumbnk_len, 1))
		return "read drumbnk failed";
	if (hmp_init(player.song.hf, song_data, song_len, 0xa009, loop))
		return "read song failed";
	hmp_reset_tracks(player.song.hf);
	hmpopl_reset(player.song.h);
	song_reset(&player.song);
	return NULL;
}

void playerweb_init(int freq) {
	player_init(&player, freq);
}
