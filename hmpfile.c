#include <stdint.h>
//#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "hmpfile.h"

static int get_le32(const uint8_t *p) {
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

hmp_file *hmp_open(const void *data, int size, int dev) {
	int i;
	hmp_file *hmp;
	int num_tracks;
	int tempo;
	//typedef uint32_t trkdev_t[5];
	//const trkdev_t *trkdev;
	int trkdev[5];
	const uint8_t *datap = data;
	const uint8_t *hdrp = data;
	const uint8_t *dataend = datap + size;
	int num_trkok;

	hmp = malloc(sizeof(hmp_file));
	if (!hmp)
		return NULL;

	memset(hmp, 0, sizeof(*hmp));

	if (size < 0x308 || memcmp(datap, "HMIMIDIP", 8))
		goto err;

    num_tracks = get_le32(datap + 0x30);
	tempo = get_le32(datap + 0x38);

	if ((num_tracks < 1) || (num_tracks > HMP_TRACKS))
		goto err;

	hmp->num_trks = num_tracks;
    hmp->tempo = tempo;

    //trkdev = (const trkdev_t *)(datap + 0x80);
	//printf("trkdev: %02x %02x %02x %02x\n", datap[0x80], datap[0x81], datap[0x82], datap[0x83]);

    datap += 0x308;
	num_trkok = 0;
    for (i = 0; i < num_tracks; i++) {
        uint32_t tracksize;
		int j;
        
        if (datap + 12 > dataend)
            goto err;
        
        datap += 4;
        tracksize = get_le32(datap);
        datap += 4;
        datap += 4;

		tracksize -= 12;

		//printf("track %d size %d\n", i, tracksize);
		
		if (datap + tracksize > dataend)
		    goto err;

		hmp->trks[i].len = tracksize;

		if (!(hmp->trks[i].data = malloc(tracksize)))
			goto err;


        for (j = 0; j < 5; j++)
			trkdev[j] = get_le32(hdrp + 0x80 + i * 5 * 4 + j * 4);
        if (trkdev[0]) {
            int trkok = 0;
            for (j = 0; j < 5; j++)
                if (trkdev[j] == dev ||
                    (trkdev[j] == 0xa002 && dev == 0xa009) ||
                    (trkdev[j] == 0xa000 && (dev == 0xa001 || dev == 0xa008)))
                    trkok = 1;
            if (!trkok)
                hmp->trks[i].len = 0;
        }
        //printf("trkpos %d=%x dev=%x,%x,%x,%x,%x ok=%d\n", i, (unsigned int)(datap - (const uint8_t *)data), trkdev[0], trkdev[1], trkdev[2], trkdev[3], trkdev[4], !!hmp->trks[i].len);
		if (hmp->trks[i].len)
			num_trkok++;
        memcpy(hmp->trks[i].data, datap, tracksize);
        datap += tracksize;
   }
   if (!num_trkok) {
	 //printf("no ok tracks\n");
     goto err;
   }

   return hmp;

err:
   hmp_close(hmp);
   return NULL;
}

void hmp_close(hmp_file *hmp) {
	int i;

	for (i = 0; i < hmp->num_trks; i++)
		if (hmp->trks[i].data)
			free(hmp->trks[i].data);
	free(hmp);
}

/*
 * read a HMI type variabele length number
 */
static int get_var_num_hmi(unsigned char *data, int datalen, unsigned int *value) {
	unsigned char *p;
	unsigned int v = 0;
	int shift = 0;

	p = data;
	while ((datalen > 0) && !(*p & 0x80)) {
		v += *(p++) << shift;
		shift += 7;
		datalen --;
    }
	if (!datalen)
		return 0;
    v += (*(p++) & 0x7f) << shift;
	if (value) *value = v;
    return p - data;
}

/*
 * read a MIDI type variabele length number
 */
static int get_var_num(unsigned char *data, int datalen, unsigned int *value) {
	unsigned char *orgdata = data;
	unsigned int v = 0;

	while ((datalen > 0) && (*data & 0x80))
		v = (v << 7) + (*(data++) & 0x7f);
	if (!datalen)
		return 0;
    v = (v << 7) + *(data++);
    if (value) *value = v;
    return data - orgdata;
}

int hmp_get_event(hmp_file *hmp, hmp_event *ev) {
    static int cmdlen[7]={3,3,3,3,2,2,3};
	int got;
	unsigned int mindelta, delta;
	int i, ev_num;
	hmp_track *trk, *fndtrk;

	mindelta = INT_MAX;
	fndtrk = NULL;
	for (trk = hmp->trks, i = hmp->num_trks; (i--) > 0; trk++) {
		if (!trk->left)
			continue;
		if (!(got = get_var_num_hmi(trk->cur, trk->left, &delta)))
			return HMP_INVALID_FILE;
		if (trk->left > got + 2 && *(trk->cur + got) == 0xff
			&& *(trk->cur + got + 1) == 0x2f) {/* end of track */
			trk->left = 0;
			continue;
		}
        delta += trk->cur_time - hmp->cur_time;
		if (delta < mindelta) {
			mindelta = delta;
			fndtrk = trk;
		}
	}
	if (!(trk = fndtrk))
			return HMP_EOF;

	got = get_var_num_hmi(trk->cur, trk->left, &delta);

	trk->cur_time += delta;
	ev->delta = trk->cur_time - hmp->cur_time;
	hmp->cur_time = trk->cur_time;

	if ((trk->left -= got) < 3)
			return HMP_INVALID_FILE;
	trk->cur += got;
	/*memset(ev, 0, sizeof(*ev));*/ev->datalen = 0;
	ev->msg[0] = ev_num = *(trk->cur++);
	ev->trk = trk - hmp->trks;
	trk->left--;
	if (ev_num < 0x80)
	    return HMP_INVALID_FILE; /* invalid command */
	if (ev_num < 0xf0) {
		ev->msg[1] = *(trk->cur++);
		trk->left--;
		if (cmdlen[((ev_num) >> 4) - 8] == 3) {
			ev->msg[2] = *(trk->cur++);
			trk->left--;
		}
	} else if (ev_num == 0xff) {
		ev->msg[1] = *(trk->cur++);
		trk->left--;
		if (!(got = get_var_num(ev->data = trk->cur, 
			trk->left, &ev->datalen)))
			return HMP_INVALID_FILE;
	    trk->cur += ev->datalen;
		if (trk->left <= ev->datalen)
			return HMP_INVALID_FILE;
		trk->left -= ev->datalen;
	} else /* sysex -> error */
	    return HMP_INVALID_FILE;
	return 0;
}

void hmp_reset_tracks(struct hmp_file *hmp) {
	int i;

	for (i = 0; i < hmp->num_trks; i++) {
		hmp->trks[i].cur = hmp->trks[i].data;
		hmp->trks[i].left = hmp->trks[i].len;
		hmp->trks[i].cur_time = 0;
	}
	hmp->cur_time = 0;
}
