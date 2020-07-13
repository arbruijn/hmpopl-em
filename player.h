#include "opl3.h"
#include "hmpfile.h"
#include "hmpopl.h"

struct song {
    struct hmp_file hfbuf;
    struct hmp_file *hf;
    hmpopl hbuf;
    hmpopl *h;
    int gen;
    unsigned char msg[3];
};


struct player {
	opl3_chip chip;
	int freq;
	char melobnk[32], drumbnk[32];
	struct song song;
	int loop;
	int playing;
};

const char *player_init(struct player *player, int freq);

const char *player_play_file(struct player *player, const char *filename, int loop);

int player_gen(struct player *player, short *buf, int bufsize);

int player_stop(struct player *player);

void player_done(struct player *player);
