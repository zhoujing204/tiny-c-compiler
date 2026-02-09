/*
 * TCC - Tiny C Compiler
 *
 * Symbol table management.
 */

#include "tcc.h"

/*============================================================
 * Symbol Stack Operations
 *============================================================*/

void sym_init(SymStack *st) {
  st->hash_table = tcc_malloc(SYM_HASH_SIZE * sizeof(Sym *));
  memset(st->hash_table, 0, SYM_HASH_SIZE * sizeof(Sym *));
  st->top = NULL;
}

void sym_free(SymStack *st) {
  Sym *sym = st->top;
  while (sym) {
    Sym *next = sym->prev;
    if (sym->name) {
      tcc_free(sym->name);
    }
    if (sym->asm_label) {
      tcc_free(sym->asm_label);
    }
    tcc_free(sym);
    sym = next;
  }
  tcc_free(st->hash_table);
  st->hash_table = NULL;
  st->top = NULL;
}

/* String hash function */
static unsigned int str_hash(const char *s) {
  unsigned int h = 0;
  while (*s) {
    h = h * 31 + (unsigned char)*s++;
  }
  return h & (SYM_HASH_SIZE - 1);
}

/* Push a new symbol onto the symbol table (by name) */
Sym *sym_push2(TCCState *s, const char *name, int t, int r, int64_t c) {
  Sym *sym;
  SymStack *st;
  unsigned int h;

  sym = tcc_malloc(sizeof(Sym));
  memset(sym, 0, sizeof(Sym));

  sym->name = name ? tcc_strdup(name) : NULL;
  sym->v = name ? (int)str_hash(name) : 0;
  sym->t = t;
  sym->r = r;
  sym->c = c;

  /* Choose which stack to push to */
  if (s->local_scope > 0) {
    st = &s->local_stack;
  } else {
    st = &s->global_stack;
  }

  /* Add to hash table if has a name */
  if (name) {
    h = str_hash(name);
    sym->prev_tok = st->hash_table[h];
    st->hash_table[h] = sym;
  }

  /* Add to scope stack */
  sym->prev = st->top;
  st->top = sym;

  return sym;
}

/* Backward compatible wrapper - treats v as string pointer */
Sym *sym_push(TCCState *s, int v, int t, int r, int64_t c) {
  /* v is typically a cast pointer to string name */
  /* On 64-bit, we need to recover the full pointer */
  if (v != 0) {
    /* v is actually a truncated pointer - but we get full ptr from caller */
    /* For now, we rely on the string being valid at this address */
    const char *name = (const char *)(intptr_t)v;
    return sym_push2(s, name, t, r, c);
  }
  return sym_push2(s, NULL, t, r, c);
}

/* Pop symbols until reaching 'b' */
void sym_pop(SymStack *st, Sym *b) {
  Sym *sym;
  unsigned int h;

  while (st->top != b) {
    sym = st->top;
    st->top = sym->prev;

    /* Remove from hash table */
    if (sym->name) {
      h = str_hash(sym->name);
      st->hash_table[h] = sym->prev_tok;
      tcc_free(sym->name);
    }

    if (sym->asm_label) {
      tcc_free(sym->asm_label);
    }
    tcc_free(sym);
  }
}

/* Find symbol by name in local then global scope */
Sym *sym_find2(TCCState *s, const char *name) {
  Sym *sym;
  unsigned int h = str_hash(name);

  /* Search local scope first */
  sym = s->local_stack.hash_table[h];
  while (sym) {
    if (sym->name && strcmp(sym->name, name) == 0) {
      return sym;
    }
    sym = sym->prev_tok;
  }

  /* Then search global scope */
  sym = s->global_stack.hash_table[h];
  while (sym) {
    if (sym->name && strcmp(sym->name, name) == 0) {
      return sym;
    }
    sym = sym->prev_tok;
  }

  return NULL;
}

/* Backward compatible wrapper */
Sym *sym_find(TCCState *s, int v) {
  /* v is typically a cast pointer to string name */
  if (v != 0) {
    const char *name = (const char *)(intptr_t)v;
    return sym_find2(s, name);
  }
  return NULL;
}

/* Find symbol only in global scope */
Sym *global_sym_find(TCCState *s, int v) {
  if (v != 0) {
    const char *name = (const char *)(intptr_t)v;
    Sym *sym;
    unsigned int h = str_hash(name);

    sym = s->global_stack.hash_table[h];
    while (sym) {
      if (sym->name && strcmp(sym->name, name) == 0) {
        return sym;
      }
      sym = sym->prev_tok;
    }
  }
  return NULL;
}
