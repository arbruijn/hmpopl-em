#ifdef __cplusplus
extern "C" {
#endif
const char *loadurl(void *runner, const char *url, int *size);
const char *loadurl_cf(void *runner, const char *url, int *size, int *dofree);
const char *cf_entry(int idx);
#ifdef __cplusplus
}
#endif
