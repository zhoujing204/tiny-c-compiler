/*
 * TCC - Tiny C Compiler
 *
 * Main header file with all type definitions and declarations.
 * Inspired by Fabrice Bellard's TCC.
 */

#ifndef TCC_H
#define TCC_H

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * Configuration
 *============================================================*/

#define TCC_VERSION "0.1.0"
#define MAX_INCLUDE_DEPTH 32
#define STRING_MAX_SIZE 1024
#define VSTACK_SIZE 256
#define SYM_HASH_SIZE 8192

/*============================================================
 * Token Types
 *============================================================*/

typedef enum {
  /* End of file */
  TOK_EOF = 0,

  /* Literals */
  TOK_NUM = 256, /* Integer or float literal */
  TOK_STR,       /* String literal */
  TOK_IDENT,     /* Identifier */

  /* Keywords */
  TOK_INT,
  TOK_CHAR,
  TOK_VOID,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_FOR,
  TOK_DO,
  TOK_RETURN,
  TOK_BREAK,
  TOK_CONTINUE,
  TOK_SWITCH,
  TOK_CASE,
  TOK_DEFAULT,
  TOK_SIZEOF,
  TOK_STRUCT,
  TOK_UNION,
  TOK_ENUM,
  TOK_TYPEDEF,
  TOK_STATIC,
  TOK_EXTERN,
  TOK_CONST,
  TOK_UNSIGNED,
  TOK_SIGNED,
  TOK_SHORT,
  TOK_LONG,
  TOK_FLOAT,
  TOK_DOUBLE,

  /* Multi-character operators */
  TOK_EQ,         /* == */
  TOK_NE,         /* != */
  TOK_LE,         /* <= */
  TOK_GE,         /* >= */
  TOK_SHL,        /* << */
  TOK_SHR,        /* >> */
  TOK_INC,        /* ++ */
  TOK_DEC,        /* -- */
  TOK_ARROW,      /* -> */
  TOK_AND,        /* && */
  TOK_OR,         /* || */
  TOK_ADD_ASSIGN, /* += */
  TOK_SUB_ASSIGN, /* -= */
  TOK_MUL_ASSIGN, /* *= */
  TOK_DIV_ASSIGN, /* /= */
  TOK_MOD_ASSIGN, /* %= */
  TOK_AND_ASSIGN, /* &= */
  TOK_OR_ASSIGN,  /* |= */
  TOK_XOR_ASSIGN, /* ^= */
  TOK_SHL_ASSIGN, /* <<= */
  TOK_SHR_ASSIGN, /* >>= */
  TOK_ELLIPSIS,   /* ... */

  /* Preprocessor */
  TOK_PP_DEFINE,
  TOK_PP_INCLUDE,
  TOK_PP_IFDEF,
  TOK_PP_IFNDEF,
  TOK_PP_ELSE,
  TOK_PP_ENDIF,
  TOK_PP_UNDEF,

  TOK_LAST
} TokenType;

/*============================================================
 * Type System
 *============================================================*/

/* Basic types (stored in lower 4 bits) */
#define VT_INT 0        /* int */
#define VT_BYTE 1       /* char */
#define VT_SHORT 2      /* short */
#define VT_VOID 3       /* void */
#define VT_PTR 4        /* pointer */
#define VT_ENUM 5       /* enum */
#define VT_FUNC 6       /* function */
#define VT_STRUCT 7     /* struct/union */
#define VT_FLOAT 8      /* float */
#define VT_DOUBLE 9     /* double */
#define VT_LDOUBLE 10   /* long double */
#define VT_BOOL 11      /* _Bool */
#define VT_LLONG 12     /* long long (64-bit) */
#define VT_LONG 13      /* long (only used during parsing) */
#define VT_BTYPE 0x000f /* mask for basic type */

/* Type modifiers */
#define VT_UNSIGNED 0x0010 /* unsigned */
#define VT_ARRAY 0x0020    /* array (with VT_PTR) */
#define VT_BITFIELD 0x0040 /* bitfield */
#define VT_CONSTANT 0x0800 /* const */
#define VT_VOLATILE 0x1000 /* volatile */
#define VT_DEFSIGN 0x2000  /* explicitly signed */

/* Storage class */
#define VT_EXTERN 0x0080  /* extern */
#define VT_STATIC 0x0100  /* static */
#define VT_TYPEDEF 0x0200 /* typedef */
#define VT_INLINE 0x0400  /* inline */

/*============================================================
 * Value Stack Constants (for SValue.r)
 *============================================================*/

/* Special storage locations */
#define VT_CONST 0x00f0  /* constant value */
#define VT_LLOCAL 0x00f1 /* lvalue on stack */
#define VT_LOCAL 0x00f2  /* local variable */
#define VT_CMP 0x00f3    /* value is in CPU flags */
#define VT_JMP                                                                 \
  0x00f4               /* value is result of conditional jump (true if taken)  \
                        */
#define VT_JMPI 0x00f5 /* inverted VT_JMP */

/* Value flags */
#define VT_LVAL 0x0100     /* lvalue */
#define VT_SYM 0x0200      /* symbol reference */
#define VT_MUSTCAST 0x0400 /* must cast */

/*============================================================
 * x86-64 Registers
 *============================================================*/

#define REG_RAX 0
#define REG_RCX 1
#define REG_RDX 2
#define REG_RBX 3
#define REG_RSP 4
#define REG_RBP 5
#define REG_RSI 6
#define REG_RDI 7
#define REG_R8 8
#define REG_R9 9
#define REG_R10 10
#define REG_R11 11
#define REG_R12 12
#define REG_R13 13
#define REG_R14 14
#define REG_R15 15

/* Register classes */
#define RC_INT 0x0001   /* integer register */
#define RC_FLOAT 0x0002 /* float register (XMM) */
#define RC_RAX 0x0004   /* specifically RAX */
#define RC_RCX 0x0008   /* specifically RCX */
#define RC_RDX 0x0010   /* specifically RDX */

/* Number of available temp registers */
#define NB_REGS 6

/*============================================================
 * Data Structures
 *============================================================*/

/* Forward declarations */
typedef struct Sym Sym;
typedef struct Section Section;
typedef struct TCCState TCCState;
typedef struct BufferedFile BufferedFile;

/* Source file buffer */
struct BufferedFile {
  char *buf_ptr;      /* current position in buffer */
  char *buf_end;      /* end of buffer */
  char *buffer;       /* allocated buffer */
  int line_num;       /* current line number */
  char filename[256]; /* filename */
  FILE *file;         /* file handle */
  BufferedFile *prev; /* previous file in include stack */
};

/* Token value union */
typedef union {
  int64_t i; /* integer value */
  double d;  /* floating point value */
  char *str; /* string/identifier */
} CValue;

/* Symbol structure */
struct Sym {
  int v;           /* symbol token (identifier) */
  char *name;      /* symbol name string */
  int t;           /* type */
  int r;           /* register or storage info */
  int64_t c;       /* associated constant/address */
  Sym *next;       /* next symbol in hash bucket */
  Sym *prev;       /* previous in scope stack */
  Sym *prev_tok;   /* previous definition of this token */
  Section *sec;    /* section for this symbol */
  char *asm_label; /* assembly label if any */
};

/* Value on the value stack */
typedef struct {
  int t;    /* type */
  int r;    /* register/storage info */
  int r2;   /* second register (for 64-bit on 32-bit) */
  CValue c; /* constant value */
  Sym *sym; /* associated symbol */
} SValue;

/* Section for code/data */
struct Section {
  char name[64];     /* section name */
  uint8_t *data;     /* section data */
  size_t data_size;  /* current size */
  size_t data_alloc; /* allocated size */
  int sh_type;       /* section type */
  int sh_flags;      /* section flags */
  int sh_entsize;    /* entry size for tables */
  Section *next;     /* linked list */
  int sh_num;        /* section number */
  uint32_t sh_addr;  /* virtual address */
};

/* Symbol table */
typedef struct {
  Sym **hash_table; /* hash table */
  Sym *top;         /* top of scope stack */
} SymStack;

/* Compiler state */
struct TCCState {
  /* Input */
  BufferedFile *file; /* current file */
  int include_depth;  /* include nesting depth */

  /* Current token */
  int tok;     /* current token type */
  CValue tokc; /* current token value */

  /* Symbol tables */
  SymStack define_stack; /* macros */
  SymStack global_stack; /* global symbols */
  SymStack local_stack;  /* local symbols */
  SymStack label_stack;  /* labels */
  int local_scope;       /* local scope depth */

  /* Value stack for code generation */
  SValue vstack[VSTACK_SIZE];
  SValue *vtop; /* top of value stack */

  /* Sections */
  Section *sections;      /* linked list of sections */
  Section *text_section;  /* code section */
  Section *data_section;  /* initialized data */
  Section *bss_section;   /* uninitialized data */
  Section *rdata_section; /* read-only data (strings) */

  /* Code generation state */
  int ind;           /* current code position */
  int loc;           /* local variable offset */
  int func_ret_type; /* return type of current function */
  int func_vc;       /* return value location */

  /* Output */
  char *outfile;   /* output filename */
  int output_type; /* executable, dll, obj */

  /* Options */
  int verbose;  /* verbosity level */
  int warn_all; /* all warnings enabled */

  /* Error handling */
  int nb_errors;   /* number of errors */
  int nb_warnings; /* number of warnings */
};

/* Output types */
#define TCC_OUTPUT_EXE 0 /* executable */
#define TCC_OUTPUT_DLL 1 /* shared library */
#define TCC_OUTPUT_OBJ 2 /* object file */

/*============================================================
 * Global State
 *============================================================*/

extern TCCState *tcc_state;

/*============================================================
 * Function Declarations - tcc.c
 *============================================================*/

TCCState *tcc_new(void);
void tcc_delete(TCCState *s);
int tcc_compile(TCCState *s, const char *filename);
int tcc_output_file(TCCState *s, const char *filename);

/*============================================================
 * Function Declarations - lex.c
 *============================================================*/

void tcc_open(TCCState *s, const char *filename);
void tcc_close(TCCState *s);
int tcc_inp(TCCState *s);
void next(TCCState *s);
void next_nomacro(TCCState *s);
void expect(TCCState *s, int tok);
void skip(TCCState *s, int tok);

/*============================================================
 * Function Declarations - parse.c
 *============================================================*/

void parse_file(TCCState *s);
void decl(TCCState *s, int flags);
int parse_type(TCCState *s);
void expr(TCCState *s);
void block(TCCState *s);

/*============================================================
 * Function Declarations - sym.c
 *============================================================*/

void sym_init(SymStack *st);
void sym_free(SymStack *st);
Sym *sym_push(TCCState *s, int v, int t, int r, int64_t c);
Sym *sym_push2(TCCState *s, const char *name, int t, int r, int64_t c);
void sym_pop(SymStack *st, Sym *b);
Sym *sym_find(TCCState *s, int v);
Sym *sym_find2(TCCState *s, const char *name);
Sym *global_sym_find(TCCState *s, int v);

/*============================================================
 * Function Declarations - gen.c
 *============================================================*/

void gen_init(TCCState *s);
void vsetc(TCCState *s, int t, int r, CValue *vc);
void vset(TCCState *s, int t, int r, int64_t v);
void vpush(TCCState *s);
void vpop(TCCState *s);
void vswap(TCCState *s);
int gv(TCCState *s, int rc);
void gv2(TCCState *s, int rc1, int rc2);
void gen_op(TCCState *s, int op);
void gen_cast(TCCState *s, int t);
Sym *gind(TCCState *s);
void gjmp(TCCState *s, Sym *l);
void gtst(TCCState *s, int inv, Sym *l);
void glabel(TCCState *s, Sym *l);

/*============================================================
 * Function Declarations - x86_64-gen.c
 *============================================================*/

void g(TCCState *s, int c);
void gen_le32(TCCState *s, uint32_t v);
void gen_le64(TCCState *s, uint64_t v);
void load(TCCState *s, int r, SValue *sv);
void store(TCCState *s, int r, SValue *sv);
void gen_opi(TCCState *s, int op);
void gen_opf(TCCState *s, int op);
void gfunc_prolog(TCCState *s, int t);
void gfunc_epilog(TCCState *s);
void gfunc_call(TCCState *s, int nb_args);
void gen_cvt_itof(TCCState *s, int t);
void gen_cvt_ftoi(TCCState *s, int t);

/*============================================================
 * Function Declarations - section.c
 *============================================================*/

Section *new_section(TCCState *s, const char *name, int sh_type, int sh_flags);
void section_realloc(Section *sec, size_t new_size);
size_t section_add(Section *sec, const void *data, size_t size);
void *section_ptr_add(Section *sec, size_t size);

/*============================================================
 * Function Declarations - pe.c
 *============================================================*/

int pe_output_file(TCCState *s, const char *filename);

/*============================================================
 * Function Declarations - utils.c
 *============================================================*/

void *tcc_malloc(size_t size);
void *tcc_realloc(void *ptr, size_t size);
char *tcc_strdup(const char *s);
void tcc_free(void *ptr);
void tcc_error(TCCState *s, const char *fmt, ...);
void tcc_warning(TCCState *s, const char *fmt, ...);

#endif /* TCC_H */
