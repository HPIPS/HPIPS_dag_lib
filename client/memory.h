/* memory, T13.816-T13.889 $DVS:time$ */

#ifndef XDAG_MEMORY_H
#define XDAG_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif
extern int g_use_tmpfile;

extern void dag_mem_tempfile_path(const char *tempfile_path);

extern int dag_mem_init(size_t size);

extern void *dag_malloc(size_t size);

extern void dag_free(void *mem);

extern void dag_mem_finish(void);

extern int dag_free_all(void);

extern char** dagCreateStringArray(int count, int stringLen);
extern void dagFreeStringArray(char** stringArray, int count);

#ifdef __cplusplus
};
#endif
		
#endif
