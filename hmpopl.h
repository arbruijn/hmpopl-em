#ifndef HMPOPL_H_
#define HMPOPL_H_

struct hmpopl;
typedef struct hmpopl hmpopl;
typedef void (*hmpopl_write_callback)(void *data, int index, int reg, int val);
hmpopl *hmpopl_new();
void hmpopl_done(hmpopl *h);
int hmpopl_set_bank(hmpopl *h, const void *data, int size, int isdrum);
//void hmpopl_set_song(hmpopl *h, const void *data, int size);
void hmpopl_set_write_callback(hmpopl *h, hmpopl_write_callback c, void *data);
void hmpopl_play_midi(hmpopl *h, int event, int ch, int param1, int param2);
void hmpopl_start(hmpopl *h);
void hmpopl_reset(hmpopl *h);

#endif
