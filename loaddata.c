#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *loadurl(void *runner, const char *url, int *psize) {
  FILE *f;
  off_t size;
  char *buf;
  
  if (!(f = fopen(url, "rb")))
    return NULL;
  fseeko(f, 0, SEEK_END);
  size = ftello(f);
  fseeko(f, 0, SEEK_SET); 
  if (!(buf = malloc(size))) {
    fclose(f);
    return NULL;
  }
  fread(buf, 1, size, f);
  fclose(f);
  *psize = (int)size;
  return buf;
}

const char *cf;
int cf_size;
struct cf_entry {
	const char *name;
	int len;
	int ofs;
} *cf_entries;
int cf_entries_size = 0;
int cf_entries_len;

const char *loadurl_cf(void *runner, const char *fn, int *psize, int *dofree) {
	int i;
	const char *data = loadurl(runner, fn, psize);
	if (data) {
		*dofree = 1;
		return data;
	}
	*dofree = 0;
	if (!cf) {
		cf = loadurl(runner, "descent.hog", &cf_size);
		if (!cf)
			return NULL;
		if (cf[0] != 'D' || cf[1] != 'H' || cf[2] != 'F')
			return NULL;
		const char *p = cf + 3;
		cf_entries_len = 0;
		while (p < cf + cf_size) {
			if (cf_entries_len >= cf_entries_size) {
				int size = cf_entries_size + (cf_entries_size >> 1) + 16;
				struct cf_entry *buf = realloc(cf_entries, size * sizeof(cf_entries[0]));
				if (!buf)
					return NULL;
				cf_entries = buf;
				cf_entries_size = size;
			}
			int flen = (unsigned char)p[13] | (((unsigned char)p[14]) << 8) |
				(((unsigned char)p[15]) << 16) | (((unsigned char)p[16]) << 24);
			cf_entries[cf_entries_len].name = p;
			cf_entries[cf_entries_len].len = flen;
			p += 17;
			cf_entries[cf_entries_len].ofs = p - cf;
			cf_entries_len++;
			p += flen;
			printf("cf: %s len=%x next=%x\n", cf_entries[cf_entries_len - 1].name, flen, (int)(p - cf));
		}
	}
	for (i = 0; i < cf_entries_len; i++)
		if (!strcmp(cf_entries[i].name, fn)) {
			printf("found %s\n", fn);
			if (psize)
				*psize = cf_entries[i].len;
			return cf + cf_entries[i].ofs;
		}
	printf("not found %s\n", fn);
	return NULL;
}

const char *cf_entry(int idx) {
	if (idx >= 0 && idx < cf_entries_len)
		return cf_entries[idx].name;
	return NULL;
}
