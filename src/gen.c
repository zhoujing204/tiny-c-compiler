/*
 * TCC - Tiny C Compiler
 *
 * Generic code generation - Value stack management.
 */

#include "tcc.h"

/*============================================================
 * Initialization
 *============================================================*/

void gen_init(TCCState *s) {
  /* Create code and data sections */
  s->text_section = new_section(
      s, ".text", 1, 6); /* SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR */
  s->data_section =
      new_section(s, ".data", 1, 3); /* SHT_PROGBITS, SHF_ALLOC | SHF_WRITE */
  s->bss_section =
      new_section(s, ".bss", 8, 3); /* SHT_NOBITS, SHF_ALLOC | SHF_WRITE */

  /* Initialize code position */
  s->ind = 0;
}

/*============================================================
 * Value Stack Operations
 *============================================================*/

/* Push a typed constant onto the value stack */
void vsetc(TCCState *s, int t, int r, CValue *vc) {
  s->vtop++;
  if (s->vtop >= s->vstack + VSTACK_SIZE) {
    tcc_error(s, "value stack overflow");
    s->vtop--;
    return;
  }

  s->vtop->t = t;
  s->vtop->r = r;
  s->vtop->r2 = VT_CONST;
  s->vtop->c = *vc;
  s->vtop->sym = NULL;
}

/* Push a simple integer value */
void vset(TCCState *s, int t, int r, int64_t v) {
  CValue cv;
  cv.i = v;
  vsetc(s, t, r, &cv);
}

/* Duplicate the top of stack */
void vpush(TCCState *s) {
  s->vtop++;
  if (s->vtop >= s->vstack + VSTACK_SIZE) {
    tcc_error(s, "value stack overflow");
    s->vtop--;
    return;
  }
  *s->vtop = *(s->vtop - 1);
}

/* Pop the top of stack */
void vpop(TCCState *s) {
  if (s->vtop < s->vstack) {
    tcc_error(s, "value stack underflow");
    return;
  }
  s->vtop--;
}

/* Swap top two values */
void vswap(TCCState *s) {
  SValue tmp;
  if (s->vtop < s->vstack + 1) {
    tcc_error(s, "cannot swap - not enough values on stack");
    return;
  }
  tmp = s->vtop[0];
  s->vtop[0] = s->vtop[-1];
  s->vtop[-1] = tmp;
}

/*============================================================
 * Value Loading into Registers
 *============================================================*/

/* Save value in register r to stack */
void save_reg(TCCState *s, int r) {
  int i;
  for (i = 0; i < (s->vtop - s->vstack + 1); i++) {
    SValue *sv = &s->vstack[i];
    if ((sv->r & 0x00ff) == r) {
      /* This value is in register r. Spill it. */
      /* Allocate stack slot (8 bytes aligned) */
      s->loc = (s->loc - 8) & ~7;

      /* Store register r to this slot */
      SValue spill_sv;
      spill_sv.t = sv->t;
      spill_sv.r = VT_LOCAL | VT_LVAL;
      spill_sv.c.i = s->loc;

      store(s, r, &spill_sv);

      /* Update stack value to point to this slot */
      sv->r = VT_LOCAL | VT_LVAL;
      sv->c.i = s->loc;
    }
  }
}

/* Get value into register of class rc */
int gv(TCCState *s, int rc) {
  int r;

  if (s->vtop < s->vstack) {
    tcc_error(s, "nothing on value stack");
    return REG_RAX;
  }

  /* If already in a suitable register, return */
  r = s->vtop->r & 0x00ff;
  if (r < NB_REGS) {
    int match = 1;
    if ((rc & RC_RAX) && r != REG_RAX)
      match = 0;
    else if ((rc & RC_RCX) && r != REG_RCX)
      match = 0;
    else if ((rc & RC_RDX) && r != REG_RDX)
      match = 0;

    if (match)
      return r;
  }

  /* Need to load into register */
  if (rc & RC_RAX) {
    r = REG_RAX;
  } else if (rc & RC_RCX) {
    r = REG_RCX;
  } else if (rc & RC_RDX) {
    r = REG_RDX;
  } else {
    /* Default to RAX */
    r = REG_RAX;
  }

  /* Spill register if it is in use */
  save_reg(s, r);

  load(s, r, s->vtop);
  s->vtop->r = r;

  return r;
}

/* Get two values into registers (ensures different registers) */
void gv2(TCCState *s, int rc1, int rc2) {
  /* Load second operand into RCX */
  gv(s, RC_RCX);
  vswap(s);
  /* Load first operand into RAX */
  gv(s, RC_RAX);
  vswap(s);
}

/*============================================================
 * Code Generation Operations
 *============================================================*/

/* Generate operation on top two stack values */
void gen_op(TCCState *s, int op) {
  if (s->vtop < s->vstack) {
    tcc_error(s, "not enough values for operation");
    return;
  }

  switch (op) {
  case '=':
    /* Assignment */
    if (s->vtop < s->vstack + 1) {
      tcc_error(s, "assignment needs two values");
      return;
    }
    {
      /* Load source into register */
      int r = gv(s, RC_INT);
      vpop(s);

      /* Store to destination */
      store(s, r, s->vtop);

      /* Result is the stored value */
      s->vtop->r = r;
    }
    break;

  case '+':
  case '-':
  case '*':
  case '/':
  case '%':
  case '&':
  case '|':
  case '^':
  case TOK_SHL:
  case TOK_SHR:
    gen_opi(s, op);
    break;

  case TOK_EQ:
  case TOK_NE:
  case '<':
  case '>':
  case TOK_LE:
  case TOK_GE:
    gen_opi(s, op);
    break;

  case '!':
    /* Logical NOT */
    gv(s, RC_INT);
    gen_opi(s, '!');
    break;

  case '~':
    /* Bitwise NOT */
    gv(s, RC_INT);
    gen_opi(s, '~');
    break;

  default:
    tcc_warning(s, "unhandled operator %d", op);
    break;
  }
}

/* Cast value to type t */
void gen_cast(TCCState *s, int t) {
  int from_type = s->vtop->t & VT_BTYPE;
  int to_type = t & VT_BTYPE;

  if (from_type == to_type) {
    /* Same type, no cast needed */
    s->vtop->t = t;
    return;
  }

  /* Integer to float */
  if (to_type >= VT_FLOAT && from_type < VT_FLOAT) {
    gen_cvt_itof(s, t);
    return;
  }

  /* Float to integer */
  if (from_type >= VT_FLOAT && to_type < VT_FLOAT) {
    gen_cvt_ftoi(s, t);
    return;
  }

  /* Integer conversions - just change the type */
  s->vtop->t = t;
}

/* Get new anonymous label */
Sym *gind(TCCState *s) {
  Sym *sym;
  sym = tcc_malloc(sizeof(Sym));
  memset(sym, 0, sizeof(Sym));
  sym->c = -1;
  return sym;
}
