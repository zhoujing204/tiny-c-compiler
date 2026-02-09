/*
 * TCC - Tiny C Compiler
 *
 * x86-64 Code Generator
 *
 * Generates machine code for Windows x64 ABI.
 */

#include "tcc.h"

/*============================================================
 * x86-64 Instruction Encoding Helpers
 *============================================================*/

/* Emit a byte to the code section */
/* Emit a byte to the code section */
void g(TCCState *s, int c) {
  if (s->text_section) {
    uint8_t *p = section_ptr_add(s->text_section, 1);
    *p = (uint8_t)c;
    s->ind++;
  }
}

/* Emit a 32-bit little-endian value */
void gen_le32(TCCState *s, uint32_t v) {
  g(s, v & 0xff);
  g(s, (v >> 8) & 0xff);
  g(s, (v >> 16) & 0xff);
  g(s, (v >> 24) & 0xff);
}

/* Emit a 64-bit little-endian value */
void gen_le64(TCCState *s, uint64_t v) {
  gen_le32(s, (uint32_t)v);
  gen_le32(s, (uint32_t)(v >> 32));
}

/* Emit REX prefix if needed for 64-bit operation or extended register */
static void gen_rex(TCCState *s, int w, int r, int x, int b) {
  int rex = 0x40;
  if (w)
    rex |= 0x08; /* 64-bit operand size */
  if (r > 7)
    rex |= 0x04; /* ModRM reg field extension */
  if (x > 7)
    rex |= 0x02; /* SIB index field extension */
  if (b > 7)
    rex |= 0x01; /* ModRM r/m or SIB base extension */

  if (rex != 0x40) {
    g(s, rex);
  }
}

/* Emit ModRM byte */
static void gen_modrm(TCCState *s, int mod, int reg, int rm) {
  g(s, (mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

/* Emit ModRM with displacement for local variable */
static void gen_modrm_local(TCCState *s, int reg, int offset) {
  /* Use RBP-relative addressing */
  if (offset >= -128 && offset <= 127) {
    gen_modrm(s, 1, reg, REG_RBP); /* [RBP + disp8] */
    g(s, offset & 0xff);
  } else {
    gen_modrm(s, 2, reg, REG_RBP); /* [RBP + disp32] */
    gen_le32(s, (uint32_t)offset);
  }
}

/*============================================================
 * Load Value into Register
 *============================================================*/

void load(TCCState *s, int r, SValue *sv) {
  int t = sv->t & VT_BTYPE;
  int fr = sv->r;

  /* Constant value */
  if ((fr & 0x00ff) == VT_CONST) {
    if (sv->c.i == 0) {
      /* xor r, r */
      gen_rex(s, 1, r, 0, r);
      g(s, 0x31);
      gen_modrm(s, 3, r, r);
    } else if (sv->c.i >= -0x80000000LL && sv->c.i <= 0x7fffffffLL) {
      /* mov r, imm32 (sign-extended) */
      gen_rex(s, 1, 0, 0, r);
      g(s, 0xc7);
      gen_modrm(s, 3, 0, r);
      gen_le32(s, (uint32_t)sv->c.i);
    } else {
      /* mov r, imm64 */
      gen_rex(s, 1, 0, 0, r);
      g(s, 0xb8 + (r & 7));
      gen_le64(s, sv->c.i);
    }
    return;
  }

  /* Local variable - check if r field indicates VT_LOCAL (0xf2) */
  if ((fr & 0x00ff) == VT_LOCAL || (fr & 0x00ff) == (VT_LOCAL | VT_LVAL)) {
    int size = 8; /* Default 64-bit */

    if (t == VT_BYTE)
      size = 1;
    else if (t == VT_SHORT)
      size = 2;
    else if (t == VT_INT)
      size = 4;

    if (fr & VT_LVAL) {
      /* Load from memory */
      if (size == 1) {
        /* movzx r, byte ptr [rbp + offset] */
        gen_rex(s, 0, r, 0, REG_RBP);
        g(s, 0x0f);
        g(s, (sv->t & VT_UNSIGNED) ? 0xb6 : 0xbe);
      } else if (size == 2) {
        /* movzx/movsx r, word ptr [rbp + offset] */
        gen_rex(s, 0, r, 0, REG_RBP);
        g(s, 0x0f);
        g(s, (sv->t & VT_UNSIGNED) ? 0xb7 : 0xbf);
      } else if (size == 4) {
        /* mov r32, [rbp + offset] or movsxd for signed */
        if (sv->t & VT_UNSIGNED) {
          gen_rex(s, 0, r, 0, REG_RBP);
          g(s, 0x8b);
        } else {
          gen_rex(s, 1, r, 0, REG_RBP);
          g(s, 0x63); /* movsxd */
        }
      } else {
        /* mov r64, [rbp + offset] */
        gen_rex(s, 1, r, 0, REG_RBP);
        g(s, 0x8b);
      }
      gen_modrm_local(s, r, (int)sv->c.i);
    } else {
      /* LEA - load address */
      gen_rex(s, 1, r, 0, REG_RBP);
      g(s, 0x8d);
      gen_modrm_local(s, r, (int)sv->c.i);
    }
    return;
  }

  /* Value already in a register */
  if ((fr & 0x00ff) < NB_REGS) {
    int fr_reg = fr & 0x00ff;
    if (fr_reg != r) {
      /* mov r, fr */
      gen_rex(s, 1, fr_reg, 0, r);
      g(s, 0x89);
      gen_modrm(s, 3, fr_reg, r);
    }
  }
}

/*============================================================
 * Store Register to Memory
 *============================================================*/

void store(TCCState *s, int r, SValue *sv) {
  int t = sv->t & VT_BTYPE;
  int fr = sv->r;

  /* Local variable - check if r field indicates VT_LOCAL (0xf2) */
  if ((fr & 0x00ff) == VT_LOCAL || (fr & 0x00ff) == (VT_LOCAL & 0xff)) {
    int size = 8;

    if (t == VT_BYTE)
      size = 1;
    else if (t == VT_SHORT)
      size = 2;
    else if (t == VT_INT)
      size = 4;

    if (size == 1) {
      /* mov byte ptr [rbp + offset], r8 */
      gen_rex(s, 0, r, 0, REG_RBP);
      g(s, 0x88);
    } else if (size == 2) {
      /* mov word ptr [rbp + offset], r16 */
      g(s, 0x66); /* Operand size prefix */
      gen_rex(s, 0, r, 0, REG_RBP);
      g(s, 0x89);
    } else if (size == 4) {
      /* mov dword ptr [rbp + offset], r32 */
      gen_rex(s, 0, r, 0, REG_RBP);
      g(s, 0x89);
    } else {
      /* mov qword ptr [rbp + offset], r64 */
      gen_rex(s, 1, r, 0, REG_RBP);
      g(s, 0x89);
    }
    gen_modrm_local(s, r, (int)sv->c.i);
  }
}

/*============================================================
 * Integer Operations
 *============================================================*/

void gen_opi(TCCState *s, int op) {
  int r, fr;

  if (s->vtop < s->vstack + 1 && op != '!' && op != '~') {
    tcc_error(s, "not enough operands for operator");
    return;
  }

  switch (op) {
  case '+':
    gv2(s, RC_INT, RC_INT);
    r = s->vtop[-1].r & 0xff;
    fr = s->vtop[0].r & 0xff;

    /* add r, fr */
    gen_rex(s, 1, r, 0, fr);
    g(s, 0x01);
    gen_modrm(s, 3, fr, r);

    vpop(s);
    break;

  case '-':
    gv2(s, RC_INT, RC_INT);
    r = s->vtop[-1].r & 0xff;
    fr = s->vtop[0].r & 0xff;

    /* sub r, fr */
    gen_rex(s, 1, r, 0, fr);
    g(s, 0x29);
    gen_modrm(s, 3, fr, r);

    vpop(s);
    break;

  case '*':
    gv2(s, RC_RAX, RC_INT);
    fr = s->vtop[0].r & 0xff;

    /* imul rax, fr */
    gen_rex(s, 1, REG_RAX, 0, fr);
    g(s, 0x0f);
    g(s, 0xaf);
    gen_modrm(s, 3, REG_RAX, fr);

    vpop(s);
    s->vtop->r = REG_RAX;
    break;

  case '/':
  case '%':
    gv2(s, RC_RAX, RC_INT);
    fr = s->vtop[0].r & 0xff;

    /* Need to use RCX to avoid clobbering RDX */
    if (fr == REG_RDX) {
      /* mov rcx, rdx */
      gen_rex(s, 1, REG_RCX, 0, REG_RDX);
      g(s, 0x89);
      gen_modrm(s, 3, REG_RDX, REG_RCX);
      fr = REG_RCX;
    }

    /* cqo - sign extend rax to rdx:rax */
    gen_rex(s, 1, 0, 0, 0);
    g(s, 0x99);

    /* idiv fr */
    gen_rex(s, 1, 0, 0, fr);
    g(s, 0xf7);
    gen_modrm(s, 3, 7, fr);

    vpop(s);
    if (op == '%') {
      s->vtop->r = REG_RDX; /* Remainder in RDX */
    } else {
      s->vtop->r = REG_RAX; /* Quotient in RAX */
    }
    break;

  case '&':
    gv2(s, RC_INT, RC_INT);
    r = s->vtop[-1].r & 0xff;
    fr = s->vtop[0].r & 0xff;

    /* and r, fr */
    gen_rex(s, 1, r, 0, fr);
    g(s, 0x21);
    gen_modrm(s, 3, fr, r);

    vpop(s);
    break;

  case '|':
    gv2(s, RC_INT, RC_INT);
    r = s->vtop[-1].r & 0xff;
    fr = s->vtop[0].r & 0xff;

    /* or r, fr */
    gen_rex(s, 1, r, 0, fr);
    g(s, 0x09);
    gen_modrm(s, 3, fr, r);

    vpop(s);
    break;

  case '^':
    gv2(s, RC_INT, RC_INT);
    r = s->vtop[-1].r & 0xff;
    fr = s->vtop[0].r & 0xff;

    /* xor r, fr */
    gen_rex(s, 1, r, 0, fr);
    g(s, 0x31);
    gen_modrm(s, 3, fr, r);

    vpop(s);
    break;

  case TOK_SHL:
    gv2(s, RC_INT, RC_RCX);
    r = s->vtop[-1].r & 0xff;

    /* shl r, cl */
    gen_rex(s, 1, 0, 0, r);
    g(s, 0xd3);
    gen_modrm(s, 3, 4, r);

    vpop(s);
    break;

  case TOK_SHR:
    gv2(s, RC_INT, RC_RCX);
    r = s->vtop[-1].r & 0xff;

    /* sar r, cl (or shr for unsigned) */
    gen_rex(s, 1, 0, 0, r);
    g(s, 0xd3);
    gen_modrm(s, 3, (s->vtop[-1].t & VT_UNSIGNED) ? 5 : 7, r);

    vpop(s);
    break;

  case TOK_EQ:
  case TOK_NE:
  case '<':
  case '>':
  case TOK_LE:
  case TOK_GE:
    gv2(s, RC_INT, RC_INT);
    r = s->vtop[-1].r & 0xff;
    fr = s->vtop[0].r & 0xff;

    /* cmp r, fr */
    gen_rex(s, 1, r, 0, fr);
    g(s, 0x39);
    gen_modrm(s, 3, fr, r);

    vpop(s);

    /* setcc al */
    {
      int setcc_op;
      switch (op) {
      case TOK_EQ:
        setcc_op = 0x94;
        break; /* sete */
      case TOK_NE:
        setcc_op = 0x95;
        break; /* setne */
      case '<':
        setcc_op = (s->vtop->t & VT_UNSIGNED) ? 0x92 : 0x9c;
        break; /* setb/setl */
      case '>':
        setcc_op = (s->vtop->t & VT_UNSIGNED) ? 0x97 : 0x9f;
        break; /* seta/setg */
      case TOK_LE:
        setcc_op = (s->vtop->t & VT_UNSIGNED) ? 0x96 : 0x9e;
        break; /* setbe/setle */
      case TOK_GE:
        setcc_op = (s->vtop->t & VT_UNSIGNED) ? 0x93 : 0x9d;
        break; /* setae/setge */
      default:
        setcc_op = 0x94;
        break;
      }

      g(s, 0x0f);
      g(s, setcc_op);
      gen_modrm(s, 3, 0, REG_RAX);

      /* movzx rax, al */
      gen_rex(s, 1, REG_RAX, 0, REG_RAX);
      g(s, 0x0f);
      g(s, 0xb6);
      gen_modrm(s, 3, REG_RAX, REG_RAX);
    }

    s->vtop->r = REG_RAX;
    s->vtop->t = VT_INT;
    break;

  case '~':
    r = gv(s, RC_INT);

    /* not r */
    gen_rex(s, 1, 0, 0, r);
    g(s, 0xf7);
    gen_modrm(s, 3, 2, r);
    break;

  case '!':
    r = gv(s, RC_INT);

    /* test r, r */
    gen_rex(s, 1, r, 0, r);
    g(s, 0x85);
    gen_modrm(s, 3, r, r);

    /* sete al */
    g(s, 0x0f);
    g(s, 0x94);
    gen_modrm(s, 3, 0, REG_RAX);

    /* movzx rax, al */
    gen_rex(s, 1, REG_RAX, 0, REG_RAX);
    g(s, 0x0f);
    g(s, 0xb6);
    gen_modrm(s, 3, REG_RAX, REG_RAX);

    s->vtop->r = REG_RAX;
    break;
  }
}

/*============================================================
 * Floating Point Operations (stub)
 *============================================================*/

void gen_opf(TCCState *s, int op) {
  (void)op;
  tcc_warning(s, "floating point operations not fully implemented");
}

void gen_cvt_itof(TCCState *s, int t) {
  (void)t;
  tcc_warning(s, "integer to float conversion not implemented");
}

void gen_cvt_ftoi(TCCState *s, int t) {
  (void)t;
  tcc_warning(s, "float to integer conversion not implemented");
}

/*============================================================
 * Function Prologue and Epilogue
 *============================================================*/

void gfunc_prolog(TCCState *s, int t) {
  (void)t;

  /* Windows x64 function prologue */

  /* push rbp */
  g(s, 0x55);

  /* mov rbp, rsp */
  gen_rex(s, 1, REG_RBP, 0, REG_RSP);
  g(s, 0x89);
  gen_modrm(s, 3, REG_RSP, REG_RBP);

  /* Save the first 4 parameters by pushing them to stack */
  /* This places them at [rbp-8], [rbp-16], [rbp-24], [rbp-32] */

  /* push rcx */
  g(s, 0x51);

  /* push rdx */
  g(s, 0x52);

  /* push r8 */
  g(s, 0x41);
  g(s, 0x50);

  /* push r9 */
  g(s, 0x41);
  g(s, 0x51);

  /* sub rsp, N (allocate stack space) */
  /* Allocate 64 bytes (96 - 32) */
  gen_rex(s, 1, 0, 0, REG_RSP);
  g(s, 0x83);
  gen_modrm(s, 3, 5, REG_RSP);
  g(s, 0x40); /* 64 bytes */

  /* Initialize local variable offset */
  s->loc = -32;
}

void gfunc_epilog(TCCState *s) {
  /* mov rsp, rbp */
  gen_rex(s, 1, REG_RSP, 0, REG_RBP);
  g(s, 0x89);
  gen_modrm(s, 3, REG_RBP, REG_RSP);

  /* pop rbp */
  g(s, 0x5d);

  /* ret */
  g(s, 0xc3);
}

/*============================================================
 * Function Calls
 *============================================================*/

/* Function call */
void gfunc_call(TCCState *s, int nb_args) {
  int i;
  int stack_args = 0;

  /* Windows x64 calling convention:
   * First 4 args in RCX, RDX, R8, R9
   * Remaining args on stack (right to left)
   * 32-byte shadow space required
   * Stack must be 16-byte aligned before call
   */

  /* Calculate stack space for arguments > 4 */
  if (nb_args > 4) {
    stack_args = nb_args - 4;
    /* Align stack to 16 bytes if necessary (stacks args + shadow space) */
    if ((stack_args * 8 + 32) % 16 != 0) {
      /* This is a simplification; a real compiler tracks stack depth.
       * For now, we assume stack is aligned at function entry and only
       * pushes affect it. We need to ensure (rsp - 8) % 16 == 0 when call
       * executes. (rsp - 8 because 'call' pushes return address).
       */
      /* sub rsp, 8 */
      gen_rex(s, 1, 0, 0, REG_RSP);
      g(s, 0x83);
      gen_modrm(s, 3, 5, REG_RSP);
      g(s, 0x08);
    }
  }

  /* Push stack arguments (reverse order) */
  for (i = nb_args - 1; i >= 4; i--) {
    /* Get argument from stack to register */
    gv(s, RC_INT);
    int r = s->vtop->r & 0xff;

    /* push r */
    if (r > 7)
      g(s, 0x41);
    g(s, 0x50 + (r & 7));

    vpop(s);
  }

  /* Register arguments (RCX, RDX, R8, R9) */
  /* We need to be careful not to clobber registers needed for later arguments.
   * Simple approach: Move to correct registers in reverse order?
   * No, earlier args might be in the very registers we need.
   * A full register allocator handles this.
   * For this tiny compiler, we'll bank on the fact that expression evaluation
   * usually leaves results in RAX/RCX/R10/R11.
   * We will force load into specific registers.
   */

  for (i = (nb_args > 4 ? 3 : nb_args - 1); i >= 0; i--) {
    int r;
    if (i == 0)
      r = REG_RCX;
    else if (i == 1)
      r = REG_RDX;
    else if (i == 2)
      r = REG_R8;
    else
      r = REG_R9;

    gv(s, (i == 0) ? RC_RCX : (i == 1) ? RC_RDX : (i == 2) ? RC_R8 : RC_R9);

    /* Move if not in correct register (gv might have picked another if
     * constrained) */
    if ((s->vtop->r & 0xff) != r) {
      int src = s->vtop->r & 0xff;
      gen_rex(s, 1, src, 0, r);
      g(s, 0x89);
      gen_modrm(s, 3, src, r);
    }
    vpop(s);
  }

  /* Allocate shadow space (32 bytes) */
  /* sub rsp, 32 */
  gen_rex(s, 1, 0, 0, REG_RSP);
  g(s, 0x83);
  gen_modrm(s, 3, 5, REG_RSP);
  g(s, 0x20);

  /* Get function address and call */
  /* The function address is now at the top of the stack (pushed before args) */
  /* Wait, parsing order puts function address first? */
  /* parse.c: expr() -> parses function name -> pushes symbol ? */
  /* Actually, for 'add(a,b)', parsing stack is: [func_addr, a, b] */
  /* We popped args. func_addr should be at s->vtop */

  if (s->vtop >= s->vstack && (s->vtop->r & VT_VALMASK) == VT_CONST &&
      s->vtop->sym) {
    /* Direct call */
    /* Only if it's a direct symbol reference */
    g(s, 0xe8); /* call rel32 */

    /* Calculate relative offset */
    Sym *sym = s->vtop->sym;
    /* TODO: Only works for defined symbols in same section */
    gen_le32(s, (int)(sym->c - (s->ind + 4)));

    vpop(s);
  } else {
    /* Indirect call */
    gv(s, RC_INT);
    int r = s->vtop->r & 0xff;
    if (r > 7)
      g(s, 0x41);
    g(s, 0xff);
    gen_modrm(s, 3, 2, r & 7);
    vpop(s);
  }

  /* Clean up stack (shadow space + stack args + alignment) */
  /* add rsp, N */
  int stack_adjust = 32 + (stack_args * 8);
  /* Alignment adjustment not tracked well here, assuming aligned for now */
  /* In a real implementation we'd track 'stack_depth' to know if we added
   * padding */

  if (stack_adjust > 0) {
    gen_rex(s, 1, 0, 0, REG_RSP);
    if (stack_adjust < 128) {
      g(s, 0x83);
      gen_modrm(s, 3, 0, REG_RSP);
      g(s, stack_adjust);
    } else {
      g(s, 0x81);
      gen_modrm(s, 3, 0, REG_RSP);
      gen_le32(s, stack_adjust);
    }
  }

  /* Result in RAX */
  vset(s, VT_INT, REG_RAX, 0);
}

/* Generate unconditional jump */
void gjmp(TCCState *s, Sym *l) {
  g(s, 0xe9); /* JMP rel32 */
  if (l->r == 1) {
    gen_le32(s, (int)(l->c - (s->ind + 4)));
  } else {
    gen_le32(s, (int)l->c);
    l->c = s->ind - 4;
  }
}

/* Generate conditional jump */
/* inv=0: jump if true (NE), inv=1: jump if false (E) */
void gtst(TCCState *s, int inv, Sym *l) {
  int v = s->vtop->r & 0xff;
  if (v >= NB_REGS) {
    v = gv(s, RC_INT);
  }
  vpop(s);

  /* test reg, reg */
  gen_rex(s, 1, v, 0, v);
  g(s, 0x85);
  gen_modrm(s, 3, v, v);

  g(s, 0x0f);
  g(s, inv ? 0x84 : 0x85); /* JE/JNE */

  if (l->r == 1) {
    gen_le32(s, (int)(l->c - (s->ind + 4)));
  } else {
    gen_le32(s, (int)l->c);
    l->c = s->ind - 4;
  }
}

/* Label definition */
void glabel(TCCState *s, Sym *l) {
  int p;

  /* Patch fixups */
  p = (int)l->c;
  while (p != -1) {
    int rel = s->ind - (p + 4);
    uint32_t next = *(uint32_t *)(s->text_section->data + p);
    *(uint32_t *)(s->text_section->data + p) = rel;
    p = (int)next;
  }

  l->r = 1; /* Defined */
  l->c = s->ind;
}
