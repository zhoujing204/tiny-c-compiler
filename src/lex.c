/*
 * TCC - Tiny C Compiler
 *
 * Lexer / Tokenizer
 */

#include "tcc.h"

/* Keyword table */
typedef struct {
  const char *name;
  int tok;
} Keyword;

static const Keyword keywords[] = {{"int", TOK_INT},
                                   {"char", TOK_CHAR},
                                   {"void", TOK_VOID},
                                   {"if", TOK_IF},
                                   {"else", TOK_ELSE},
                                   {"while", TOK_WHILE},
                                   {"for", TOK_FOR},
                                   {"do", TOK_DO},
                                   {"return", TOK_RETURN},
                                   {"break", TOK_BREAK},
                                   {"continue", TOK_CONTINUE},
                                   {"switch", TOK_SWITCH},
                                   {"case", TOK_CASE},
                                   {"default", TOK_DEFAULT},
                                   {"sizeof", TOK_SIZEOF},
                                   {"struct", TOK_STRUCT},
                                   {"union", TOK_UNION},
                                   {"enum", TOK_ENUM},
                                   {"typedef", TOK_TYPEDEF},
                                   {"static", TOK_STATIC},
                                   {"extern", TOK_EXTERN},
                                   {"const", TOK_CONST},
                                   {"unsigned", TOK_UNSIGNED},
                                   {"signed", TOK_SIGNED},
                                   {"short", TOK_SHORT},
                                   {"long", TOK_LONG},
                                   {"float", TOK_FLOAT},
                                   {"double", TOK_DOUBLE},
                                   {NULL, 0}};

/* Identifier/string buffer */
static char tok_buf[STRING_MAX_SIZE];

/*============================================================
 * File I/O
 *============================================================*/

#define BUFFER_SIZE 4096

void tcc_open(TCCState *s, const char *filename) {
  BufferedFile *bf;
  FILE *f;

  f = fopen(filename, "rb");
  if (!f) {
    tcc_error(s, "cannot open file '%s'", filename);
    return;
  }

  bf = tcc_malloc(sizeof(BufferedFile));
  memset(bf, 0, sizeof(BufferedFile));

  bf->file = f;
  strncpy(bf->filename, filename, sizeof(bf->filename) - 1);
  bf->line_num = 1;

  /* Allocate buffer */
  bf->buffer = tcc_malloc(BUFFER_SIZE + 1);
  bf->buf_ptr = bf->buffer;
  bf->buf_end = bf->buffer;

  /* Link to include stack */
  bf->prev = s->file;
  s->file = bf;
  s->include_depth++;
}

void tcc_close(TCCState *s) {
  BufferedFile *bf = s->file;

  if (!bf)
    return;

  s->file = bf->prev;
  s->include_depth--;

  fclose(bf->file);
  tcc_free(bf->buffer);
  tcc_free(bf);
}

/* Get next character from input */
int tcc_inp(TCCState *s) {
  BufferedFile *bf = s->file;

  if (!bf)
    return EOF;

  if (bf->buf_ptr >= bf->buf_end) {
    /* Read more data */
    size_t len = fread(bf->buffer, 1, BUFFER_SIZE, bf->file);
    if (len == 0) {
      return EOF;
    }
    bf->buf_ptr = bf->buffer;
    bf->buf_end = bf->buffer + len;
  }

  return *bf->buf_ptr++;
}

/* Peek at next character without consuming */
static int peek_char(TCCState *s) {
  BufferedFile *bf = s->file;

  if (!bf)
    return EOF;

  if (bf->buf_ptr >= bf->buf_end) {
    size_t len = fread(bf->buffer, 1, BUFFER_SIZE, bf->file);
    if (len == 0) {
      return EOF;
    }
    bf->buf_ptr = bf->buffer;
    bf->buf_end = bf->buffer + len;
  }

  return *bf->buf_ptr;
}

/* Put back a character */
static void unget_char(TCCState *s) {
  BufferedFile *bf = s->file;
  if (bf && bf->buf_ptr > bf->buffer) {
    bf->buf_ptr--;
  }
}

/*============================================================
 * Tokenizer
 *============================================================*/

static int is_ident_start(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_char(int c) {
  return is_ident_start(c) || (c >= '0' && c <= '9');
}

static int is_digit(int c) { return c >= '0' && c <= '9'; }

static int is_hex_digit(int c) {
  return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int hex_value(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return 0;
}

/* Look up keyword */
static int lookup_keyword(const char *name) {
  const Keyword *kw = keywords;
  while (kw->name) {
    if (strcmp(kw->name, name) == 0) {
      return kw->tok;
    }
    kw++;
  }
  return 0;
}

/* Parse a number */
static void parse_number(TCCState *s, int c) {
  char *p = tok_buf;
  int64_t value = 0;
  int base = 10;
  int is_float = 0;

  /* Check for hex or octal */
  if (c == '0') {
    int nc = peek_char(s);
    if (nc == 'x' || nc == 'X') {
      /* Hex */
      tcc_inp(s);
      base = 16;
      c = tcc_inp(s);
    } else if (is_digit(nc)) {
      /* Octal */
      base = 8;
      c = tcc_inp(s);
    }
  }

  /* Parse integer part */
  if (base == 16) {
    while (is_hex_digit(c)) {
      value = value * 16 + hex_value(c);
      c = tcc_inp(s);
    }
  } else if (base == 8) {
    while (c >= '0' && c <= '7') {
      value = value * 8 + (c - '0');
      c = tcc_inp(s);
    }
  } else {
    while (is_digit(c)) {
      value = value * 10 + (c - '0');
      c = tcc_inp(s);
    }
  }

  /* Check for float */
  if (c == '.') {
    is_float = 1;
    *p++ = '.';
    c = tcc_inp(s);
    while (is_digit(c)) {
      *p++ = (char)c;
      c = tcc_inp(s);
    }
  }

  /* Check for exponent */
  if (c == 'e' || c == 'E') {
    is_float = 1;
    *p++ = (char)c;
    c = tcc_inp(s);
    if (c == '+' || c == '-') {
      *p++ = (char)c;
      c = tcc_inp(s);
    }
    while (is_digit(c)) {
      *p++ = (char)c;
      c = tcc_inp(s);
    }
  }

  /* Put back last char (the one that's not a digit/hex) */
  if (c != EOF)
    unget_char(s);

  /* Note: We've already put back the non-digit character above.
   * Don't need to handle suffixes specially - they'll be parsed as identifiers
   * which will cause a syntax error if used incorrectly. For proper suffix
   * handling, we'd need to consume them here without ungetting. */

  s->tok = TOK_NUM;
  if (is_float) {
    *p = '\0';
    s->tokc.d = strtod(tok_buf, NULL);
  } else {
    s->tokc.i = value;
  }
}

/* Parse escape sequence */
static int parse_escape(TCCState *s) {
  int c = tcc_inp(s);
  switch (c) {
  case 'n':
    return '\n';
  case 't':
    return '\t';
  case 'r':
    return '\r';
  case '0':
    return '\0';
  case '\\':
    return '\\';
  case '\'':
    return '\'';
  case '"':
    return '"';
  case 'x': {
    /* Hex escape */
    int v = 0;
    c = tcc_inp(s);
    if (is_hex_digit(c)) {
      v = hex_value(c);
      c = tcc_inp(s);
      if (is_hex_digit(c)) {
        v = v * 16 + hex_value(c);
      } else {
        unget_char(s);
      }
    } else {
      unget_char(s);
    }
    return v;
  }
  default:
    return c;
  }
}

/* Parse string literal */
static void parse_string(TCCState *s, int quote) {
  char *p = tok_buf;
  int c;

  while (1) {
    c = tcc_inp(s);
    if (c == quote)
      break;
    if (c == EOF || c == '\n') {
      tcc_error(s, "unterminated string");
      break;
    }
    if (c == '\\') {
      c = parse_escape(s);
    }
    if (p - tok_buf < STRING_MAX_SIZE - 1) {
      *p++ = (char)c;
    }
  }
  *p = '\0';

  if (quote == '"') {
    s->tok = TOK_STR;
    s->tokc.str = tcc_strdup(tok_buf);
  } else {
    /* Character constant */
    s->tok = TOK_NUM;
    s->tokc.i = tok_buf[0];
  }
}

/* Skip whitespace and comments */
static void skip_whitespace(TCCState *s) {
  int c;

  while (1) {
    c = tcc_inp(s);

    if (c == ' ' || c == '\t' || c == '\r') {
      continue;
    }

    if (c == '\n') {
      s->file->line_num++;
      continue;
    }

    /* Comments */
    if (c == '/') {
      int nc = peek_char(s);
      if (nc == '/') {
        /* Line comment */
        tcc_inp(s);
        while ((c = tcc_inp(s)) != '\n' && c != EOF)
          ;
        if (c == '\n')
          s->file->line_num++;
        continue;
      } else if (nc == '*') {
        /* Block comment */
        tcc_inp(s);
        while (1) {
          c = tcc_inp(s);
          if (c == EOF)
            break;
          if (c == '\n')
            s->file->line_num++;
          if (c == '*' && peek_char(s) == '/') {
            tcc_inp(s);
            break;
          }
        }
        continue;
      }
    }

    break;
  }

  if (c != EOF)
    unget_char(s);
}

/* Main tokenizer function (without macro expansion) */
void next_nomacro(TCCState *s) {
  int c;

  skip_whitespace(s);

  c = tcc_inp(s);

  if (c == EOF) {
    s->tok = TOK_EOF;
    return;
  }

  /* Identifier or keyword */
  if (is_ident_start(c)) {
    char *p = tok_buf;
    int kw;

    while (is_ident_char(c)) {
      if (p - tok_buf < STRING_MAX_SIZE - 1) {
        *p++ = (char)c;
      }
      c = tcc_inp(s);
    }
    *p = '\0';
    if (c != EOF)
      unget_char(s);

    /* Check if keyword */
    kw = lookup_keyword(tok_buf);
    if (kw) {
      s->tok = kw;
    } else {
      s->tok = TOK_IDENT;
      s->tokc.str = tcc_strdup(tok_buf);
    }
    return;
  }

  /* Number */
  if (is_digit(c)) {
    parse_number(s, c);
    return;
  }

  /* String or character literal */
  if (c == '"' || c == '\'') {
    parse_string(s, c);
    return;
  }

  /* Operators */
  switch (c) {
  case '+':
    if (peek_char(s) == '+') {
      tcc_inp(s);
      s->tok = TOK_INC;
    } else if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_ADD_ASSIGN;
    } else {
      s->tok = '+';
    }
    break;

  case '-':
    if (peek_char(s) == '-') {
      tcc_inp(s);
      s->tok = TOK_DEC;
    } else if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_SUB_ASSIGN;
    } else if (peek_char(s) == '>') {
      tcc_inp(s);
      s->tok = TOK_ARROW;
    } else {
      s->tok = '-';
    }
    break;

  case '*':
    if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_MUL_ASSIGN;
    } else {
      s->tok = '*';
    }
    break;

  case '/':
    if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_DIV_ASSIGN;
    } else {
      s->tok = '/';
    }
    break;

  case '%':
    if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_MOD_ASSIGN;
    } else {
      s->tok = '%';
    }
    break;

  case '=':
    if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_EQ;
    } else {
      s->tok = '=';
    }
    break;

  case '!':
    if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_NE;
    } else {
      s->tok = '!';
    }
    break;

  case '<':
    if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_LE;
    } else if (peek_char(s) == '<') {
      tcc_inp(s);
      if (peek_char(s) == '=') {
        tcc_inp(s);
        s->tok = TOK_SHL_ASSIGN;
      } else {
        s->tok = TOK_SHL;
      }
    } else {
      s->tok = '<';
    }
    break;

  case '>':
    if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_GE;
    } else if (peek_char(s) == '>') {
      tcc_inp(s);
      if (peek_char(s) == '=') {
        tcc_inp(s);
        s->tok = TOK_SHR_ASSIGN;
      } else {
        s->tok = TOK_SHR;
      }
    } else {
      s->tok = '>';
    }
    break;

  case '&':
    if (peek_char(s) == '&') {
      tcc_inp(s);
      s->tok = TOK_AND;
    } else if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_AND_ASSIGN;
    } else {
      s->tok = '&';
    }
    break;

  case '|':
    if (peek_char(s) == '|') {
      tcc_inp(s);
      s->tok = TOK_OR;
    } else if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_OR_ASSIGN;
    } else {
      s->tok = '|';
    }
    break;

  case '^':
    if (peek_char(s) == '=') {
      tcc_inp(s);
      s->tok = TOK_XOR_ASSIGN;
    } else {
      s->tok = '^';
    }
    break;

  case '.':
    if (peek_char(s) == '.') {
      tcc_inp(s);
      if (peek_char(s) == '.') {
        tcc_inp(s);
        s->tok = TOK_ELLIPSIS;
      } else {
        unget_char(s);
        s->tok = '.';
      }
    } else {
      s->tok = '.';
    }
    break;

  case '#':
    /* Preprocessor - simplified handling */
    s->tok = '#';
    break;

  default:
    /* Single character token */
    s->tok = c;
    break;
  }
}

/* Main tokenizer with macro expansion (simplified) */
void next(TCCState *s) { next_nomacro(s); }

/* Expect a specific token */
void expect(TCCState *s, int tok) {
  if (s->tok != tok) {
    if (tok < 256) {
      tcc_error(s, "expected '%c'", tok);
    } else {
      tcc_error(s, "expected token %d", tok);
    }
  }
}

/* Skip a token, error if not present */
void skip(TCCState *s, int tok) {
  expect(s, tok);
  next(s);
}
