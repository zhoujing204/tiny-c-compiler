/*
 * TCC - Tiny C Compiler
 *
 * Parser - Recursive descent parser for C.
 */

#include "tcc.h"

/* Forward declarations */
static void expr_eq(TCCState *s);
static void expr_or(TCCState *s);
static void expr_and(TCCState *s);
static void expr_bitor(TCCState *s);
static void expr_xor(TCCState *s);
static void expr_bitand(TCCState *s);
static void expr_cmp(TCCState *s);
static void expr_shift(TCCState *s);
static void expr_add(TCCState *s);
static void expr_mult(TCCState *s);
static void expr_unary(TCCState *s);
static void expr_postfix(TCCState *s);
static void expr_primary(TCCState *s);
static void statement(TCCState *s);

/*============================================================
 * Type Parsing
 *============================================================*/

/* Parse base type (int, char, void, etc.) */
int parse_type(TCCState *s) {
  int t = 0;
  int sign = 0;
  int size_mod = 0;   /* 1=short, 2=long, 3=long long */
  int type_found = 0; /* Track if any type keyword was seen */

  /* Parse type specifiers */
  while (1) {
    switch (s->tok) {
    case TOK_VOID:
      t = VT_VOID;
      type_found = 1;
      next(s);
      break;
    case TOK_CHAR:
      t = VT_BYTE;
      type_found = 1;
      next(s);
      break;
    case TOK_SHORT:
      size_mod = 1;
      type_found = 1;
      next(s);
      break;
    case TOK_INT:
      t = VT_INT;
      type_found = 1;
      next(s);
      break;
    case TOK_LONG:
      if (size_mod == 2)
        size_mod = 3; /* long long */
      else
        size_mod = 2;
      type_found = 1;
      next(s);
      break;
    case TOK_FLOAT:
      t = VT_FLOAT;
      type_found = 1;
      next(s);
      break;
    case TOK_DOUBLE:
      t = VT_DOUBLE;
      type_found = 1;
      next(s);
      break;
    case TOK_SIGNED:
      sign = 1;
      type_found = 1;
      next(s);
      break;
    case TOK_UNSIGNED:
      sign = 2;
      type_found = 1;
      next(s);
      break;
    case TOK_CONST:
      t |= VT_CONSTANT;
      next(s);
      break;
    case TOK_STATIC:
      t |= VT_STATIC;
      next(s);
      break;
    case TOK_EXTERN:
      t |= VT_EXTERN;
      next(s);
      break;
    default:
      goto done;
    }
  }
done:

  /* If no type keyword was seen, return -1 to indicate failure */
  if (!type_found) {
    return -1;
  }

  /* Apply size modifiers */
  if ((t & VT_BTYPE) == 0) {
    /* Default to int if no base type specified */
    if (size_mod == 1) {
      t = (t & ~VT_BTYPE) | VT_SHORT;
    } else if (size_mod >= 2) {
      t = (t & ~VT_BTYPE) | VT_LLONG; /* long or long long -> 64-bit */
    } else if (sign != 0) {
      t = (t & ~VT_BTYPE) | VT_INT;
    }
  }

  /* Apply sign */
  if (sign == 2) {
    t |= VT_UNSIGNED;
  }

  return t;
}

/* Parse pointer types */
static int parse_pointer(TCCState *s, int t) {
  while (s->tok == '*') {
    next(s);
    t = VT_PTR | (t << 16); /* Store base type in upper bits */

    /* Handle const/volatile on pointer */
    while (s->tok == TOK_CONST) {
      t |= VT_CONSTANT;
      next(s);
    }
  }
  return t;
}

/*============================================================
 * Expression Parsing (Operator Precedence)
 *============================================================*/

/* Assignment expression (lowest precedence for assignment) */
static void expr_eq(TCCState *s) {
  SValue lval;
  int op;

  expr_or(s);

  if (s->tok == '=' || (s->tok >= TOK_ADD_ASSIGN && s->tok <= TOK_SHR_ASSIGN)) {
    op = s->tok;

    /* Save lvalue */
    lval = *s->vtop;

    next(s);
    expr_eq(s); /* Right associative */

    if (op == '=') {
      /* Simple assignment */
      gen_op(s, '=');
    } else {
      /* Compound assignment: a += b  ->  a = a + b */
      /* Simplified implementation */
      gen_op(s, '=');
    }
  }
}

/* Logical OR */
static void expr_or(TCCState *s) {
  expr_and(s);
  while (s->tok == TOK_OR) {
    next(s);
    expr_and(s);
    gen_op(s, TOK_OR);
  }
}

/* Logical AND */
static void expr_and(TCCState *s) {
  expr_bitor(s);
  while (s->tok == TOK_AND) {
    next(s);
    expr_bitor(s);
    gen_op(s, TOK_AND);
  }
}

/* Bitwise OR */
static void expr_bitor(TCCState *s) {
  expr_xor(s);
  while (s->tok == '|') {
    next(s);
    expr_xor(s);
    gen_op(s, '|');
  }
}

/* Bitwise XOR */
static void expr_xor(TCCState *s) {
  expr_bitand(s);
  while (s->tok == '^') {
    next(s);
    expr_bitand(s);
    gen_op(s, '^');
  }
}

/* Bitwise AND */
static void expr_bitand(TCCState *s) {
  expr_cmp(s);
  while (s->tok == '&') {
    next(s);
    expr_cmp(s);
    gen_op(s, '&');
  }
}

/* Comparison operators */
static void expr_cmp(TCCState *s) {
  int op;

  expr_shift(s);
  while (s->tok == TOK_EQ || s->tok == TOK_NE || s->tok == '<' ||
         s->tok == '>' || s->tok == TOK_LE || s->tok == TOK_GE) {
    op = s->tok;
    next(s);
    expr_shift(s);
    gen_op(s, op);
  }
}

/* Shift operators */
static void expr_shift(TCCState *s) {
  int op;

  expr_add(s);
  while (s->tok == TOK_SHL || s->tok == TOK_SHR) {
    op = s->tok;
    next(s);
    expr_add(s);
    gen_op(s, op);
  }
}

/* Addition/Subtraction */
static void expr_add(TCCState *s) {
  int op;

  expr_mult(s);
  while (s->tok == '+' || s->tok == '-') {
    op = s->tok;
    next(s);
    expr_mult(s);
    gen_op(s, op);
  }
}

/* Multiplication/Division/Modulo */
static void expr_mult(TCCState *s) {
  int op;

  expr_unary(s);
  while (s->tok == '*' || s->tok == '/' || s->tok == '%') {
    op = s->tok;
    next(s);
    expr_unary(s);
    gen_op(s, op);
  }
}

/* Unary operators */
static void expr_unary(TCCState *s) {
  int op;

  switch (s->tok) {
  case '-':
    next(s);
    expr_unary(s);
    /* Negate: push 0, subtract */
    vset(s, VT_INT, VT_CONST, 0);
    vswap(s);
    gen_op(s, '-');
    break;

  case '+':
    next(s);
    expr_unary(s);
    break;

  case '!':
    next(s);
    expr_unary(s);
    gen_op(s, '!');
    break;

  case '~':
    next(s);
    expr_unary(s);
    gen_op(s, '~');
    break;

  case '*':
    /* Dereference */
    next(s);
    expr_unary(s);
    /* Mark as lvalue */
    s->vtop->r |= VT_LVAL;
    break;

  case '&':
    /* Address-of */
    next(s);
    expr_unary(s);
    /* TODO: implement address-of */
    break;

  case TOK_INC:
  case TOK_DEC:
    op = s->tok;
    next(s);
    expr_unary(s);
    gen_op(s, op == TOK_INC ? '+' : '-');
    break;

  case TOK_SIZEOF:
    next(s);
    if (s->tok == '(') {
      next(s);
      /* Check if type or expression */
      if (s->tok >= TOK_INT && s->tok <= TOK_DOUBLE) {
        int t = parse_type(s);
        skip(s, ')');
        /* Push size of type */
        int size = 4; /* Default */
        if ((t & VT_BTYPE) == VT_BYTE)
          size = 1;
        else if ((t & VT_BTYPE) == VT_SHORT)
          size = 2;
        else if ((t & VT_BTYPE) == VT_LLONG)
          size = 8;
        else if ((t & VT_BTYPE) == VT_PTR)
          size = 8; /* 64-bit */
        vset(s, VT_INT, VT_CONST, size);
      } else {
        expr(s);
        skip(s, ')');
        /* Get size of expression result */
        vpop(s);
        vset(s, VT_INT, VT_CONST, 4); /* Simplified */
      }
    } else {
      expr_unary(s);
      vpop(s);
      vset(s, VT_INT, VT_CONST, 4);
    }
    break;

  case '(':
    next(s);
    /* Check for cast */
    if (s->tok >= TOK_VOID && s->tok <= TOK_DOUBLE) {
      int t = parse_type(s);
      t = parse_pointer(s, t);
      skip(s, ')');
      expr_unary(s);
      gen_cast(s, t);
    } else {
      /* Parenthesized expression */
      expr(s);
      skip(s, ')');
    }
    break;

  default:
    expr_postfix(s);
    break;
  }
}

/* Postfix operators (calls, array indexing, member access) */
static void expr_postfix(TCCState *s) {
  expr_primary(s);

  while (1) {
    if (s->tok == '(') {
      /* Function call */
      int nb_args = 0;
      next(s);

      while (s->tok != ')') {
        expr_eq(s);
        nb_args++;
        if (s->tok == ',') {
          next(s);
        } else {
          break;
        }
      }
      skip(s, ')');

      gfunc_call(s, nb_args);
    } else if (s->tok == '[') {
      /* Array indexing */
      next(s);
      expr(s);
      skip(s, ']');
      gen_op(s, '+');
      s->vtop->r |= VT_LVAL;
    } else if (s->tok == '.') {
      /* Struct member */
      next(s);
      if (s->tok != TOK_IDENT) {
        tcc_error(s, "expected identifier");
      }
      /* TODO: implement struct member access */
      next(s);
    } else if (s->tok == TOK_ARROW) {
      /* Pointer member */
      next(s);
      if (s->tok != TOK_IDENT) {
        tcc_error(s, "expected identifier");
      }
      /* TODO: implement pointer member access */
      next(s);
    } else if (s->tok == TOK_INC || s->tok == TOK_DEC) {
      /* Post-increment/decrement */
      int op = s->tok;
      next(s);
      gen_op(s, op == TOK_INC ? '+' : '-');
    } else {
      break;
    }
  }
}

/* Primary expressions (literals, identifiers) */
static void expr_primary(TCCState *s) {
  Sym *sym;

  switch (s->tok) {
  case TOK_NUM:
    vset(s, VT_INT, VT_CONST, s->tokc.i);
    next(s);
    break;

  case TOK_STR:
    /* String literal - add to rdata section */
    {
      size_t offset;
      int len = (int)strlen(s->tokc.str) + 1;

      if (!s->rdata_section) {
        /* Create section on first use */
        s->rdata_section = new_section(s, ".rdata", 1, 0);
      }

      offset = section_add(s->rdata_section, s->tokc.str, len);
      tcc_free(s->tokc.str);

      /* Push address of string */
      CValue cv;
      cv.i = (int64_t)offset;
      vsetc(s, VT_PTR, VT_CONST | VT_SYM, &cv);
    }
    next(s);
    break;

  case TOK_IDENT:
    sym = sym_find2(s, s->tokc.str);
    if (!sym) {
      /* Implicit function declaration */
      sym = sym_push2(s, s->tokc.str, VT_FUNC | VT_INT, VT_CONST, 0);
    }

    if ((sym->t & VT_BTYPE) == VT_FUNC) {
      /* Function reference */
      CValue cv;
      cv.i = sym->c;
      vsetc(s, sym->t, VT_CONST | VT_SYM, &cv);
      s->vtop->sym = sym;
    } else {
      /* Variable reference */
      CValue cv;
      cv.i = sym->c;
      vsetc(s, sym->t, sym->r | VT_LVAL, &cv);
      s->vtop->sym = sym;
    }
    tcc_free(s->tokc.str);
    next(s);
    break;

  default:
    tcc_error(s, "unexpected token in expression");
    next(s);
    break;
  }
}

/* Main expression entry point */
void expr(TCCState *s) { expr_eq(s); }

/*============================================================
 * Statement Parsing
 *============================================================*/

static void statement(TCCState *s) {
  Sym *saved;

  switch (s->tok) {
  case '{':
    /* Block */
    next(s);
    s->local_scope++;
    saved = s->local_stack.top;

    while (s->tok != '}' && s->tok != TOK_EOF) {
      /* Check for declaration - only type specifier keywords */
      if (s->tok == TOK_INT || s->tok == TOK_CHAR || s->tok == TOK_VOID ||
          s->tok == TOK_SHORT || s->tok == TOK_LONG || s->tok == TOK_FLOAT ||
          s->tok == TOK_DOUBLE || s->tok == TOK_STATIC ||
          s->tok == TOK_EXTERN || s->tok == TOK_CONST ||
          s->tok == TOK_UNSIGNED || s->tok == TOK_SIGNED) {
        decl(s, 0); /* 0 for local declarations */
      } else {
        statement(s);
      }
    }

    /* Pop local symbols */
    sym_pop(&s->local_stack, saved);
    s->local_scope--;
    skip(s, '}');
    break;

  case TOK_IF: {
    Sym *l1, *l2;
    next(s);
    skip(s, '(');
    expr(s);
    skip(s, ')');

    l1 = gind(s);
    gtst(s, 1, l1); /* Jump if false */

    statement(s);

    if (s->tok == TOK_ELSE) {
      l2 = gind(s);
      gjmp(s, l2); /* Jump over else */
      glabel(s, l1);
      next(s);
      statement(s);
      glabel(s, l2);
    } else {
      glabel(s, l1);
    }
  } break;

  case TOK_WHILE: {
    Sym *l1, *l2;
    l1 = gind(s);
    l2 = gind(s);

    glabel(s, l1); /* Start */

    next(s);
    skip(s, '(');
    expr(s);
    skip(s, ')');

    gtst(s, 1, l2); /* Jump to end if false */

    statement(s);

    gjmp(s, l1);   /* Loop */
    glabel(s, l2); /* End */
  } break;

  case TOK_FOR: {
    Sym *l_cond = gind(s);
    Sym *l_end = gind(s);
    Sym *l_update = gind(s);
    Sym *l_body = gind(s);

    next(s);
    skip(s, '(');
    if (s->tok != ';') {
      expr(s);
      vpop(s);
    }
    skip(s, ';');

    glabel(s, l_cond);
    if (s->tok != ';') {
      expr(s);
      gtst(s, 1, l_end);
    }
    skip(s, ';');

    gjmp(s, l_body); /* Jump to body */

    glabel(s, l_update);
    if (s->tok != ')') {
      expr(s);
      vpop(s);
    }
    gjmp(s, l_cond);
    skip(s, ')');

    glabel(s, l_body);
    statement(s);
    gjmp(s, l_update);

    glabel(s, l_end);
  } break;

  case TOK_DO:
    next(s);
    statement(s);
    skip(s, TOK_WHILE);
    skip(s, '(');
    expr(s);
    gv(s, RC_INT);
    vpop(s);
    skip(s, ')');
    skip(s, ';');
    break;

  case TOK_RETURN:
    next(s);
    if (s->tok != ';') {
      expr(s);
      /* Move result to return register */
      gv(s, RC_RAX);
      vpop(s);
    }
    skip(s, ';');

    /* Generate function epilogue */
    gfunc_epilog(s);
    break;

  case TOK_BREAK:
    next(s);
    skip(s, ';');
    /* TODO: jump to end of loop */
    break;

  case TOK_CONTINUE:
    next(s);
    skip(s, ';');
    /* TODO: jump to loop condition */
    break;

  case ';':
    /* Empty statement */
    next(s);
    break;

  default:
    /* Expression statement */
    expr(s);
    vpop(s);
    skip(s, ';');
    break;
  }
}

/*============================================================
 * Declaration Parsing
 *============================================================*/

void decl(TCCState *s, int flags) {
  int t, pt;
  char *name;
  Sym *sym;

  /* Parse type */
  t = parse_type(s);
  if (t < 0) {
    tcc_error(s, "expected type");
    next(s); /* Skip the unknown token to prevent infinite loop */
    return;
  }

  /* Parse declarators */
  while (1) {
    /* Parse pointer */
    pt = parse_pointer(s, t);

    /* Get identifier */
    if (s->tok != TOK_IDENT) {
      tcc_error(s, "expected identifier");
      return;
    }
    name = s->tokc.str;
    next(s);

    /* Check for function */
    if (s->tok == '(') {
      /* Function declaration/definition */
      next(s);

      /* Create function symbol */
      sym = sym_push2(s, name, pt | VT_FUNC, VT_CONST,
                      s->text_section ? s->text_section->data_size : 0);
      sym->sec = s->text_section;

      /* Parse parameters */
      s->local_scope++;
      int param_count = 0;
      int param_offset = 16; /* After return address and saved RBP */

      while (s->tok != ')') {
        int param_t = parse_type(s);
        param_t = parse_pointer(s, param_t);

        char *param_name = NULL;
        if (s->tok == TOK_IDENT) {
          param_name = s->tokc.str;
          next(s);
        }

        if (param_name) {
          /* First 4 params in registers on Windows x64 */
          int reg_or_stack;
          if (param_count < 4) {
            reg_or_stack = VT_LOCAL; /* Will be moved to stack in prolog */
          } else {
            reg_or_stack = VT_LOCAL;
          }
          sym_push2(s, param_name, param_t, reg_or_stack, param_offset);
          param_offset += 8;
        }

        param_count++;
        if (s->tok == ',') {
          next(s);
        } else {
          break;
        }
      }
      skip(s, ')');

      /* Check for definition vs declaration */
      if (s->tok == '{') {
        /* Function definition */
        s->func_ret_type = pt;

        /* Generate prologue */
        gfunc_prolog(s, pt);

        /* Parse body */
        statement(s);

        s->local_scope--;
      } else {
        /* Just a declaration */
        s->local_scope--;
        skip(s, ';');
      }
      return;
    } else if (s->tok == '[') {
      /* Array declaration */
      next(s);
      int array_size = 0;
      if (s->tok == TOK_NUM) {
        array_size = (int)s->tokc.i;
        next(s);
      }
      skip(s, ']');

      /* Create array type */
      pt |= VT_ARRAY;

      /* Allocate local variable */
      s->loc -= array_size * 8; /* Simplified: assume 8 bytes per element */
      sym = sym_push2(s, name, pt, VT_LOCAL, s->loc);
    } else {
      /* Variable declaration */
      if (s->local_scope == 0) {
        /* Global variable */
        sym = sym_push2(s, name, pt, VT_SYM, 0);
        if (s->data_section) {
          sym->c = s->data_section->data_size;
          sym->sec = s->data_section;
          section_ptr_add(s->data_section, 8); /* 8 bytes for 64-bit */
        }
      } else {
        /* Local variable */
        int size = 8; /* Default to 8 bytes for 64-bit */
        if ((pt & VT_BTYPE) == VT_BYTE)
          size = 1;
        else if ((pt & VT_BTYPE) == VT_SHORT)
          size = 2;
        else if ((pt & VT_BTYPE) == VT_INT)
          size = 4;

        s->loc -= (size + 7) & ~7; /* Align to 8 bytes */
        sym = sym_push2(s, name, pt, VT_LOCAL, s->loc);
      }

      /* Handle initializer */
      if (s->tok == '=') {
        next(s);
        expr(s);

        /* Store to variable */
        CValue cv;
        cv.i = sym->c;
        vsetc(s, sym->t, sym->r | VT_LVAL, &cv);
        s->vtop->sym = sym;
        vswap(s);
        gen_op(s, '=');
        vpop(s);
      }
    }

    if (s->tok == ',') {
      next(s);
    } else {
      break;
    }
  }

  if (s->tok == ';') {
    next(s);
  }
}

/*============================================================
 * File Parsing
 *============================================================*/

void parse_file(TCCState *s) {
  while (s->tok != TOK_EOF) {
    decl(s, 0);
  }
}

/* Block parsing (used for function bodies) */
void block(TCCState *s) { statement(s); }
