int getbanks(const char *sng, int sng_size, const char *filename, char *melobnk, int melobnksize, char *drumbnk, int drumbnksize) {
	const char *p = sng;
	int filename_len = strlen(filename);
	while (p < sng + sng_size) {
		const char *e;
		if (!(e = (const char *)memchr(p, '\n', sng + sng_size - p)))
			e = sng + sng_size;
		if (e > p && e[-1] == '\r')
			e--;
		//printf("sng: %.*s\n", e - p, p);
		if (e - p > filename_len && !memcmp(p, filename, filename_len) && (p[filename_len] == '\t' || p[filename_len] == ' ')) {
			const char *pmelo = p + filename_len;
			while (*pmelo == ' ' || *pmelo == '\t')
				pmelo++;
			const char *emelo = pmelo;
			while (emelo < e && *emelo != ' ' && *emelo != '\t')
				emelo++;
			const char *pdrum = emelo;
			while (*pdrum == ' ' || *pdrum == '\t')
				pdrum++;
			const char *edrum = pdrum;
			while (edrum < e && *edrum != ' ' && *edrum != '\t')
				edrum++;
			if (emelo - pmelo < melobnksize && edrum - pdrum < drumbnksize) {
				memcpy(melobnk, pmelo, emelo - pmelo);
				melobnk[emelo - pmelo] = 0;
				memcpy(drumbnk, pdrum, edrum - pdrum);
				drumbnk[edrum - pdrum] = 0;
				return 0;
			}
		}
		p = e + 1;
		while (p < sng + sng_size && (*p == '\r' || *p == '\n'))
			p++;
	}
	return -1;
}
