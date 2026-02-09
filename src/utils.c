/*
 * TCC - Tiny C Compiler
 * 
 * Utility functions: memory management, error handling.
 */

#include "tcc.h"

/*============================================================
 * Memory Management
 *============================================================*/

void *tcc_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr && size) {
        fprintf(stderr, "tcc: out of memory\n");
        exit(1);
    }
    return ptr;
}

void *tcc_realloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size) {
        fprintf(stderr, "tcc: out of memory\n");
        exit(1);
    }
    return new_ptr;
}

char *tcc_strdup(const char *s)
{
    char *ptr;
    size_t len = strlen(s) + 1;
    ptr = tcc_malloc(len);
    memcpy(ptr, s, len);
    return ptr;
}

void tcc_free(void *ptr)
{
    free(ptr);
}

/*============================================================
 * Error Handling
 *============================================================*/

void tcc_error(TCCState *s, const char *fmt, ...)
{
    va_list ap;
    
    if (s && s->file) {
        fprintf(stderr, "%s:%d: error: ", s->file->filename, s->file->line_num);
    } else {
        fprintf(stderr, "tcc: error: ");
    }
    
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    
    fprintf(stderr, "\n");
    
    if (s) {
        s->nb_errors++;
    }
}

void tcc_warning(TCCState *s, const char *fmt, ...)
{
    va_list ap;
    
    if (s && s->file) {
        fprintf(stderr, "%s:%d: warning: ", s->file->filename, s->file->line_num);
    } else {
        fprintf(stderr, "tcc: warning: ");
    }
    
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    
    fprintf(stderr, "\n");
    
    if (s) {
        s->nb_warnings++;
    }
}
