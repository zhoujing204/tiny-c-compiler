/*
 * TCC - Tiny C Compiler
 * 
 * Main entry point and compiler orchestration.
 */

#include "tcc.h"

/* Global compiler state */
TCCState *tcc_state = NULL;

/*============================================================
 * Compiler State Management
 *============================================================*/

TCCState *tcc_new(void)
{
    TCCState *s;
    
    s = tcc_malloc(sizeof(TCCState));
    memset(s, 0, sizeof(TCCState));
    
    /* Initialize symbol tables */
    sym_init(&s->define_stack);
    sym_init(&s->global_stack);
    sym_init(&s->local_stack);
    sym_init(&s->label_stack);
    
    /* Initialize value stack */
    s->vtop = s->vstack - 1;
    
    /* Default output type */
    s->output_type = TCC_OUTPUT_EXE;
    
    return s;
}

void tcc_delete(TCCState *s)
{
    if (!s) return;
    
    /* Free symbol tables */
    sym_free(&s->define_stack);
    sym_free(&s->global_stack);
    sym_free(&s->local_stack);
    sym_free(&s->label_stack);
    
    /* Free sections */
    Section *sec = s->sections;
    while (sec) {
        Section *next = sec->next;
        tcc_free(sec->data);
        tcc_free(sec);
        sec = next;
    }
    
    /* Free output filename */
    if (s->outfile) {
        tcc_free(s->outfile);
    }
    
    tcc_free(s);
}

/*============================================================
 * Compilation
 *============================================================*/

int tcc_compile(TCCState *s, const char *filename)
{
    tcc_state = s;
    
    /* Initialize code generation */
    gen_init(s);
    
    /* Open source file */
    tcc_open(s, filename);
    
    /* Read first token */
    next(s);
    
    /* Parse the file */
    parse_file(s);
    
    /* Close source file */
    tcc_close(s);
    
    return s->nb_errors ? -1 : 0;
}

int tcc_output_file(TCCState *s, const char *filename)
{
    return pe_output_file(s, filename);
}

/*============================================================
 * Command Line Interface
 *============================================================*/

static void print_usage(void)
{
    printf("Tiny C Compiler %s\n", TCC_VERSION);
    printf("Usage: tcc [options] infile...\n");
    printf("\n");
    printf("Options:\n");
    printf("  -o outfile     Set output filename\n");
    printf("  -c             Compile only, don't link\n");
    printf("  -v             Show version\n");
    printf("  -h             Show this help\n");
}

int main(int argc, char **argv)
{
    TCCState *s;
    const char *outfile = NULL;
    const char *infile = NULL;
    int i;
    int compile_only = 0;
    
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-o") == 0) {
                if (++i >= argc) {
                    fprintf(stderr, "tcc: -o requires an argument\n");
                    return 1;
                }
                outfile = argv[i];
            } else if (strcmp(argv[i], "-c") == 0) {
                compile_only = 1;
            } else if (strcmp(argv[i], "-v") == 0) {
                printf("tcc version %s\n", TCC_VERSION);
                return 0;
            } else if (strcmp(argv[i], "-h") == 0) {
                print_usage();
                return 0;
            } else {
                fprintf(stderr, "tcc: unknown option '%s'\n", argv[i]);
                return 1;
            }
        } else {
            infile = argv[i];
        }
    }
    
    if (!infile) {
        fprintf(stderr, "tcc: no input file\n");
        return 1;
    }
    
    /* Create compiler state */
    s = tcc_new();
    
    /* Set output type */
    if (compile_only) {
        s->output_type = TCC_OUTPUT_OBJ;
    }
    
    /* Compile */
    if (tcc_compile(s, infile) == -1) {
        tcc_delete(s);
        return 1;
    }
    
    /* Generate output */
    if (!outfile) {
        /* Default output name */
        static char default_outfile[256];
        const char *p = strrchr(infile, '.');
        size_t len = p ? (size_t)(p - infile) : strlen(infile);
        if (len > sizeof(default_outfile) - 5) {
            len = sizeof(default_outfile) - 5;
        }
        memcpy(default_outfile, infile, len);
        strcpy(default_outfile + len, compile_only ? ".obj" : ".exe");
        outfile = default_outfile;
    }
    
    if (tcc_output_file(s, outfile) == -1) {
        tcc_delete(s);
        return 1;
    }
    
    printf("Output: %s\n", outfile);
    
    tcc_delete(s);
    return 0;
}
