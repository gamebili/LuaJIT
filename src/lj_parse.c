/*
** Lua parser (source code -> bytecode).
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_parse_c
#define LUA_CORE

#include <math.h>

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_func.h"
#include "lj_state.h"
#include "lj_bc.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#include "lj_strfmt.h"
#include "lj_lex.h"
#include "lj_parse.h"
#include "lj_vm.h"
#include "lj_vmevent.h"

#if LJ_54
#define LUA54_KNUM_BOX	0x80
#define LUA54_NOTAIL_GLOBAL_MAX	64
#define LUA54_NOTAIL_TABLE_FIELD_MAX	64
#define LUA54_NOTAIL_TABLE_ALIAS_MAX	256
#define LUA54_NOTAIL_GLOBAL_TABLE_ALIAS_MAX	64
#define LUA54_NOTAIL_TABLE_FIELD_ALIAS_MAX	64
#define LUA54_NOTAIL_GLOBAL_TABLE_FIELD_MAX	8
#define LUA54_NOTAIL_NESTED_TABLE_FIELD_MAX	32
#define LUA54_NOTAIL_GLOBAL_NESTED_FIELD_MAX	32
#endif

/* -- Parser structures and definitions ----------------------------------- */

/* Expression kinds. */
typedef enum {
  /* Constant expressions must be first and in this order: */
  VKNIL,
  VKFALSE,
  VKTRUE,
  VKSTR,	/* sval = string value */
  VKNUM,	/* nval = number value */
  VKLAST = VKNUM,
  VKCDATA,	/* nval = cdata value, not treated as a constant expression */
  /* Non-constant expressions follow: */
  VLOCAL,	/* info = local register, aux = vstack index */
  VUPVAL,	/* info = upvalue index, aux = vstack index */
  VGLOBAL,	/* sval = string value */
  VINDEXED,	/* info = table register, aux = index reg/byte/string const */
  VJMP,		/* info = instruction PC */
  VRELOCABLE,	/* info = instruction PC */
  VNONRELOC,	/* info = result register */
  VCALL,	/* info = instruction PC, aux = base */
  VVOID
} ExpKind;

/* Expression descriptor. */
typedef struct ExpDesc {
  union {
    struct {
      uint32_t info;	/* Primary info. */
      uint32_t aux;	/* Secondary info. */
    } s;
    TValue nval;	/* Number value. */
    GCstr *sval;	/* String value. */
  } u;
  ExpKind k;
  BCPos t;		/* True condition jump list. */
  BCPos f;		/* False condition jump list. */
} ExpDesc;

/* Macros for expressions. */
#define expr_hasjump(e)		((e)->t != (e)->f)

#define expr_isk(e)		((e)->k <= VKLAST)
#define expr_isk_nojump(e)	(expr_isk(e) && !expr_hasjump(e))
#define expr_isnumk(e)		((e)->k == VKNUM)
#define expr_isnumk_nojump(e)	(expr_isnumk(e) && !expr_hasjump(e))
#define expr_isstrk(e)		((e)->k == VKSTR)

#define expr_numtv(e)		check_exp(expr_isnumk((e)), &(e)->u.nval)
#define expr_numberV(e)		numberVnum(expr_numtv((e)))

/* Initialize expression. */
static LJ_AINLINE void expr_init(ExpDesc *e, ExpKind k, uint32_t info)
{
  e->k = k;
  e->u.s.info = info;
  e->f = e->t = NO_JMP;
}

/* Check number constant for +-0. */
static int expr_numiszero(ExpDesc *e)
{
  TValue *o = expr_numtv(e);
  return tvisint(o) ? (intV(o) == 0) :
	 tvisi64(o) ? (i64V(o) == 0) : tviszero(o);
}

#if LJ_54 && LJ_TARGET_ARM64
static int expr_lua54_i32k_nojump(ExpDesc *e, int nonzero)
{
  return expr_isnumk_nojump(e) && tvisint(expr_numtv(e)) &&
	 (!nonzero || intV(expr_numtv(e)) != 0);
}
#endif

/* Per-function linked list of scope blocks. */
typedef struct FuncScope {
  struct FuncScope *prev;	/* Link to outer scope. */
  MSize vstart;			/* Start of block-local variables. */
  uint8_t nactvar;		/* Number of active vars outside the scope. */
  uint8_t flags;		/* Scope flags. */
} FuncScope;

#define FSCOPE_LOOP		0x01	/* Scope is a (breakable) loop. */
#define FSCOPE_BREAK		0x02	/* Break used in scope. */
#define FSCOPE_GOLA		0x04	/* Goto or label used in scope. */
#define FSCOPE_UPVAL		0x08	/* Upvalue in scope. */
#define FSCOPE_NOCLOSE		0x10	/* Do not close upvalues. */

#define NAME_BREAK		((GCstr *)(uintptr_t)1)

/* Index into variable stack. */
typedef uint16_t VarIndex;
#define LJ_MAX_VSTACK		(65536 - LJ_MAX_UPVAL)

#if LJ_54
typedef struct Lua54NotailTableField {
  VarIndex table;		/* Source variable holding the table. */
  GCstr *field;			/* Field assigned from a no-tail helper, or any. */
} Lua54NotailTableField;

typedef struct Lua54TableAlias {
  VarIndex alias;		/* Variable currently aliasing a table source. */
  VarIndex source;		/* Canonical source variable for the table. */
} Lua54TableAlias;

typedef struct Lua54GlobalTableAlias {
  GCstr *name;			/* Global name currently aliasing a table. */
  VarIndex source;		/* Canonical source variable for the table. */
} Lua54GlobalTableAlias;

typedef struct Lua54TableFieldAlias {
  VarIndex table;		/* Source variable holding the outer table. */
  GCstr *field;			/* Field currently aliasing another table. */
  VarIndex source;		/* Canonical source variable for that table. */
} Lua54TableFieldAlias;

typedef struct Lua54GlobalTableField {
  GCstr *name;			/* Global holding the table. */
  GCstr *field;			/* Field assigned from a no-tail helper, or any. */
} Lua54GlobalTableField;

typedef struct Lua54NestedTableField {
  VarIndex table;		/* Source variable holding the outer table. */
  GCstr *outer;			/* Field holding an anonymous nested table. */
  GCstr *field;			/* Nested field from a no-tail helper, or any. */
} Lua54NestedTableField;

typedef struct Lua54GlobalNestedField {
  GCstr *name;			/* Global holding the outer table. */
  GCstr *outer;			/* Field holding an anonymous nested table. */
  GCstr *field;			/* Nested field from a no-tail helper, or any. */
} Lua54GlobalNestedField;
#endif

/* Variable/goto/label info. */
#define VSTACK_VAR_RW		0x01	/* R/W variable. */
#define VSTACK_GOTO		0x02	/* Pending goto. */
#define VSTACK_LABEL		0x04	/* Label. */
#define VSTACK_VAR_CONST	0x08	/* Lua 5.4 const local. */
#define VSTACK_VAR_CLOSE	0x10	/* Lua 5.4 to-be-closed local. */
#define VSTACK_GOTO_CLOSE	0x20	/* Lua 5.4 goto close helpers done. */
#define VSTACK_VAR_ITERHELPER	0x20	/* Local aliases to iterator helpers. */
#define VSTACK_VAR_NOTAILCALL	0x40	/* Local aliases whose callsite name matters. */
#define VSTACK_VAR_TABLE	0x80	/* Local known to hold a table source. */
#define VSTACK_VAR_ATTRMASK	(VSTACK_VAR_CONST|VSTACK_VAR_CLOSE|VSTACK_VAR_ITERHELPER|VSTACK_VAR_NOTAILCALL|VSTACK_VAR_TABLE)

/* Per-function state. */
typedef struct FuncState {
  GCtab *kt;			/* Hash table for constants. */
#if LJ_54 && LJ_DUALNUM
  GCtab *k64anchor;		/* Anchor table for transient int64 literals. */
#endif
  LexState *ls;			/* Lexer state. */
  lua_State *L;			/* Lua state. */
  FuncScope *bl;		/* Current scope. */
  struct FuncState *prev;	/* Enclosing function. */
  BCPos pc;			/* Next bytecode position. */
  BCPos lasttarget;		/* Bytecode position of last jump target. */
  BCPos jpc;			/* Pending jump list to next bytecode. */
  BCReg freereg;		/* First free register. */
  BCReg nactvar;		/* Number of active local variables. */
  BCReg nkn, nkgc;		/* Number of lua_Number/GCobj constants */
  BCLine linedefined;		/* First line of the function definition. */
  BCInsLine *bcbase;		/* Base of bytecode stack. */
  BCPos bclim;			/* Limit of bytecode stack. */
  MSize vbase;			/* Base of variable stack for this function. */
  uint8_t flags;		/* Prototype flags. */
  uint8_t numparams;		/* Number of parameters. */
  uint8_t framesize;		/* Fixed frame size. */
  uint8_t nuv;			/* Number of upvalues */
#if LJ_54
  uint8_t lua54env;		/* Implicit Lua 5.4 _ENV upvalue is present. */
  uint8_t uvnotail[LJ_MAX_UPVAL];	/* Upvalue aliases needing callsite names. */
  uint8_t uviterhelper[LJ_MAX_UPVAL];	/* Upvalue aliases to iterator helpers. */
  uint8_t ngnotail;		/* Number of tracked global no-tail aliases. */
  uint8_t ntfnotail;		/* Number of tracked table-field aliases. */
  uint8_t ngtalias;		/* Number of tracked global table aliases. */
  uint8_t ntfalias;		/* Number of tracked table-field table aliases. */
  uint8_t ngtfnotail;		/* Number of tracked global table fields. */
  uint8_t nntfnotail;		/* Number of tracked nested table-field aliases. */
  uint8_t ngntfnotail;		/* Number of tracked global nested fields. */
  uint16_t ntalias;		/* Number of tracked table variable aliases. */
  GCstr *gnotail[LUA54_NOTAIL_GLOBAL_MAX];	/* Global aliases needing names. */
  Lua54NotailTableField tfnotail[LUA54_NOTAIL_TABLE_FIELD_MAX];
  Lua54TableAlias talias[LUA54_NOTAIL_TABLE_ALIAS_MAX];
  Lua54GlobalTableAlias gtalias[LUA54_NOTAIL_GLOBAL_TABLE_ALIAS_MAX];
  Lua54TableFieldAlias tfalias[LUA54_NOTAIL_TABLE_FIELD_ALIAS_MAX];
  Lua54GlobalTableField gtfnotail[LUA54_NOTAIL_GLOBAL_TABLE_FIELD_MAX];
  Lua54NestedTableField ntfpathnotail[LUA54_NOTAIL_NESTED_TABLE_FIELD_MAX];
  Lua54GlobalNestedField gntfnotail[LUA54_NOTAIL_GLOBAL_NESTED_FIELD_MAX];
  VarIndex lua54envvidx;	/* Variable-stack entry for implicit _ENV name. */
#endif
  VarIndex varmap[LJ_MAX_LOCVAR];  /* Map from register to variable idx. */
  VarIndex uvmap[LJ_MAX_UPVAL];	/* Map from upvalue to variable idx. */
  VarIndex uvtmp[LJ_MAX_UPVAL];	/* Temporary upvalue map. */
} FuncState;

/* Binary and unary operators. ORDER OPR */
typedef enum BinOpr {
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_DIV, OPR_MOD, OPR_POW,  /* ORDER ARITH */
  OPR_IDIV,
  OPR_BAND, OPR_BOR, OPR_BXOR, OPR_SHL, OPR_SHR,
  OPR_CONCAT,
  OPR_NE, OPR_EQ,
  OPR_LT, OPR_GE, OPR_LE, OPR_GT,
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;

LJ_STATIC_ASSERT((int)BC_ISGE-(int)BC_ISLT == (int)OPR_GE-(int)OPR_LT);
LJ_STATIC_ASSERT((int)BC_ISLE-(int)BC_ISLT == (int)OPR_LE-(int)OPR_LT);
LJ_STATIC_ASSERT((int)BC_ISGT-(int)BC_ISLT == (int)OPR_GT-(int)OPR_LT);
LJ_STATIC_ASSERT((int)BC_SUBVV-(int)BC_ADDVV == (int)OPR_SUB-(int)OPR_ADD);
LJ_STATIC_ASSERT((int)BC_MULVV-(int)BC_ADDVV == (int)OPR_MUL-(int)OPR_ADD);
LJ_STATIC_ASSERT((int)BC_DIVVV-(int)BC_ADDVV == (int)OPR_DIV-(int)OPR_ADD);
LJ_STATIC_ASSERT((int)BC_MODVV-(int)BC_ADDVV == (int)OPR_MOD-(int)OPR_ADD);

#ifdef LUA_USE_ASSERT
#define lj_assertFS(c, ...)	(lj_assertG_(G(fs->L), (c), __VA_ARGS__))
#else
#define lj_assertFS(c, ...)	((void)fs)
#endif

/* -- Error handling ------------------------------------------------------ */

LJ_NORET LJ_NOINLINE static void err_syntax(LexState *ls, ErrMsg em)
{
  lj_lex_error(ls, ls->tok, em);
}

LJ_NORET LJ_NOINLINE static void err_token(LexState *ls, LexToken tok)
{
#if LJ_54
  if (tok == TK_name) {
    /* Lua 5.4's token2str leaves <name> unquoted for "expected" errors,
    ** while symbols stay quoted by their token text.
    */
    lj_lex_error(ls, ls->tok, LJ_ERR_XNAME);
  }
#endif
  lj_lex_error(ls, ls->tok, LJ_ERR_XTOKEN, lj_lex_token2str(ls, tok));
}

LJ_NORET static void err_limit(FuncState *fs, uint32_t limit, const char *what)
{
#if LJ_54
  if (fs->linedefined == 0)
    lj_lex_error(fs->ls, 0, LJ_ERR_XLIMM, what, limit);
  else
    lj_lex_error(fs->ls, 0, LJ_ERR_XLIMF, fs->linedefined, what, limit);
#else
  if (fs->linedefined == 0)
    lj_lex_error(fs->ls, 0, LJ_ERR_XLIMM, limit, what);
  else
    lj_lex_error(fs->ls, 0, LJ_ERR_XLIMF, fs->linedefined, limit, what);
#endif
}

#define checklimit(fs, v, l, m)		if ((v) >= (l)) err_limit(fs, l, m)
#define checklimitgt(fs, v, l, m)	if ((v) > (l)) err_limit(fs, l, m)
#define checkcond(ls, c, em)		{ if (!(c)) err_syntax(ls, em); }

/* -- Management of constants --------------------------------------------- */

/* Return bytecode encoding for primitive constant. */
#define const_pri(e)		check_exp((e)->k <= VKTRUE, (e)->k)

#define tvhaskslot(o)	((o)->u32.hi == 0)
#define tvkslot(o)	((o)->u32.lo)

#if LJ_54 && LJ_DUALNUM
static int const_num_exact_int64_float(cTValue *o)
{
  lua_Number n, ni;
  int64_t k;
  if (!tvisnum(o))
    return 0;
  n = numV(o);
  if (!(n >= (-9223372036854775807.0 - 1.0) &&
	n < 9223372036854775808.0))
    return 0;
  ni = lj_vm_floor(n);
  if (n != ni)
    return 0;
  k = lj_num2i64(n);
  return (lua_Number)k == n;
}
#endif

/* Add a number constant. */
static BCReg const_num(FuncState *fs, ExpDesc *e)
{
  lua_State *L = fs->L;
  TValue *o;
  lj_assertFS(expr_isnumk(e), "bad usage");
#if LJ_54 && LJ_DUALNUM
  if (tvisnum(&e->u.nval) || tvisi64(&e->u.nval)) {
    int64_t i64;
    int32_t k;
    UNUSED(k);
    if (tvisi64(&e->u.nval) || tvismzero(&e->u.nval) ||
	lj_num2int_check(numV(&e->u.nval), i64, k) ||
	const_num_exact_int64_float(&e->u.nval)) {
      GCtab *box = lj_tab_new(L, 1, 0);
      box->flags54 |= LUA54_KNUM_BOX;
      copyTV(L, arrayslot(box, 0), &e->u.nval);
      /* Lua tables intentionally unify numeric keys by value. The parser
      ** constant cache must not, otherwise Lua 5.4 float or boxed int64
      ** literals can lose their TValue subtype before bytecode is emitted.
      */
      settabV(L, L->top, box);
      incr_top(L);
      o = lj_tab_set(L, fs->kt, L->top-1);
      L->top--;
      goto gotnum;
    }
  }
#endif
  {
    o = lj_tab_set(L, fs->kt, &e->u.nval);
  }
#if LJ_54 && LJ_DUALNUM
gotnum:
#endif
  if (tvhaskslot(o))
    return tvkslot(o);
  o->u64 = fs->nkn;
  return fs->nkn++;
}

#if LJ_54 && LJ_DUALNUM
/* Keep boxed integer literals alive while parsing can still allocate before
** the expression is emitted into bytecode or folded into another literal.
*/
static void const_anchor_i64(FuncState *fs, cTValue *tv)
{
  if (tvisi64(tv)) {
    lua_State *L = fs->L;
    TValue *o;
    copyTV(L, L->top, tv);
    incr_top(L);
    o = lj_tab_set(L, fs->k64anchor, L->top-1);
    if (tvisnil(o))
      setboolV(o, 1);
    L->top--;
  }
}
#endif

/* Add a GC object constant. */
static BCReg const_gc(FuncState *fs, GCobj *gc, uint32_t itype)
{
  lua_State *L = fs->L;
  TValue key, *o;
  setgcV(L, &key, gc, itype);
  /* NOBARRIER: the key is new or kept alive. */
  o = lj_tab_set(L, fs->kt, &key);
  if (tvhaskslot(o))
    return tvkslot(o);
  o->u64 = fs->nkgc;
  return fs->nkgc++;
}

/* Add a string constant. */
static BCReg const_str(FuncState *fs, ExpDesc *e)
{
  lj_assertFS(expr_isstrk(e) || e->k == VGLOBAL, "bad usage");
  return const_gc(fs, obj2gco(e->u.sval), LJ_TSTR);
}

#if LJ_54
static BCReg const_lit(FuncState *fs, const char *str, size_t len)
{
  ExpDesc e;
  expr_init(&e, VKSTR, 0);
  e.u.sval = lj_parse_keepstr(fs->ls, str, len);
  return const_str(fs, &e);
}
#endif

/* Anchor string constant to avoid GC. */
GCstr *lj_parse_keepstr(LexState *ls, const char *str, size_t len)
{
  /* NOBARRIER: the key is new or kept alive. */
  lua_State *L = ls->L;
  GCstr *s = lj_str_new_intern(L, str, len);
  TValue *tv = lj_tab_setstr(L, ls->fs->kt, s);
  if (tvisnil(tv)) setboolV(tv, 1);
  lj_gc_check(L);
  return s;
}

#if LJ_HASFFI
/* Anchor cdata to avoid GC. */
void lj_parse_keepcdata(LexState *ls, TValue *tv, GCcdata *cd)
{
  /* NOBARRIER: the key is new or kept alive. */
  lua_State *L = ls->L;
  setcdataV(L, tv, cd);
  setboolV(lj_tab_set(L, ls->fs->kt, tv), 1);
}
#endif

/* -- Jump list handling -------------------------------------------------- */

/* Get next element in jump list. */
static BCPos jmp_next(FuncState *fs, BCPos pc)
{
  ptrdiff_t delta = bc_j(fs->bcbase[pc].ins);
  if ((BCPos)delta == NO_JMP)
    return NO_JMP;
  else
    return (BCPos)(((ptrdiff_t)pc+1)+delta);
}

/* Check if any of the instructions on the jump list produce no value. */
static int jmp_novalue(FuncState *fs, BCPos list)
{
  for (; list != NO_JMP; list = jmp_next(fs, list)) {
    BCIns p = fs->bcbase[list >= 1 ? list-1 : list].ins;
    if (!(bc_op(p) == BC_ISTC || bc_op(p) == BC_ISFC || bc_a(p) == NO_REG))
      return 1;
  }
  return 0;
}

/* Patch register of test instructions. */
static int jmp_patchtestreg(FuncState *fs, BCPos pc, BCReg reg)
{
  BCInsLine *ilp = &fs->bcbase[pc >= 1 ? pc-1 : pc];
  BCOp op = bc_op(ilp->ins);
  if (op == BC_ISTC || op == BC_ISFC) {
    if (reg != NO_REG && reg != bc_d(ilp->ins)) {
      setbc_a(&ilp->ins, reg);
    } else {  /* Nothing to store or already in the right register. */
      setbc_op(&ilp->ins, op+(BC_IST-BC_ISTC));
      setbc_a(&ilp->ins, 0);
    }
  } else if (bc_a(ilp->ins) == NO_REG) {
    if (reg == NO_REG) {
      ilp->ins = BCINS_AJ(BC_JMP, bc_a(fs->bcbase[pc].ins), 0);
    } else {
      setbc_a(&ilp->ins, reg);
      if (reg >= bc_a(ilp[1].ins))
	setbc_a(&ilp[1].ins, reg+1);
    }
  } else {
    return 0;  /* Cannot patch other instructions. */
  }
  return 1;
}

/* Drop values for all instructions on jump list. */
static void jmp_dropval(FuncState *fs, BCPos list)
{
  for (; list != NO_JMP; list = jmp_next(fs, list))
    jmp_patchtestreg(fs, list, NO_REG);
}

/* Patch jump instruction to target. */
static void jmp_patchins(FuncState *fs, BCPos pc, BCPos dest)
{
  BCIns *jmp = &fs->bcbase[pc].ins;
  BCPos offset = dest-(pc+1)+BCBIAS_J;
  lj_assertFS(dest != NO_JMP, "uninitialized jump target");
  if (offset > BCMAX_D)
    err_syntax(fs->ls, LJ_ERR_XJUMP);
  setbc_d(jmp, offset);
}

/* Append to jump list. */
static void jmp_append(FuncState *fs, BCPos *l1, BCPos l2)
{
  if (l2 == NO_JMP) {
    return;
  } else if (*l1 == NO_JMP) {
    *l1 = l2;
  } else {
    BCPos list = *l1;
    BCPos next;
    while ((next = jmp_next(fs, list)) != NO_JMP)  /* Find last element. */
      list = next;
    jmp_patchins(fs, list, l2);
  }
}

/* Patch jump list and preserve produced values. */
static void jmp_patchval(FuncState *fs, BCPos list, BCPos vtarget,
			 BCReg reg, BCPos dtarget)
{
  while (list != NO_JMP) {
    BCPos next = jmp_next(fs, list);
    if (jmp_patchtestreg(fs, list, reg))
      jmp_patchins(fs, list, vtarget);  /* Jump to target with value. */
    else
      jmp_patchins(fs, list, dtarget);  /* Jump to default target. */
    list = next;
  }
}

/* Jump to following instruction. Append to list of pending jumps. */
static void jmp_tohere(FuncState *fs, BCPos list)
{
  fs->lasttarget = fs->pc;
  jmp_append(fs, &fs->jpc, list);
}

/* Patch jump list to target. */
static void jmp_patch(FuncState *fs, BCPos list, BCPos target)
{
  if (target == fs->pc) {
    jmp_tohere(fs, list);
  } else {
    lj_assertFS(target < fs->pc, "bad jump target");
    jmp_patchval(fs, list, target, NO_REG, target);
  }
}

/* -- Bytecode register allocator ----------------------------------------- */

/* Bump frame size. */
static void bcreg_bump(FuncState *fs, BCReg n)
{
  BCReg sz = fs->freereg + n;
  if (sz > fs->framesize) {
    if (sz >= LJ_MAX_SLOTS)
      err_syntax(fs->ls, LJ_ERR_XSLOTS);
    fs->framesize = (uint8_t)sz;
  }
}

/* Reserve registers. */
static void bcreg_reserve(FuncState *fs, BCReg n)
{
  bcreg_bump(fs, n);
  fs->freereg += n;
}

/* Free register. */
static void bcreg_free(FuncState *fs, BCReg reg)
{
  if (reg >= fs->nactvar) {
    fs->freereg--;
    lj_assertFS(reg == fs->freereg, "bad regfree");
  }
}

/* Free register for expression. */
static void expr_free(FuncState *fs, ExpDesc *e)
{
  if (e->k == VNONRELOC)
    bcreg_free(fs, e->u.s.info);
}

/* -- Bytecode emitter ---------------------------------------------------- */

/* Emit bytecode instruction. */
static BCPos bcemit_INS(FuncState *fs, BCIns ins)
{
  BCPos pc = fs->pc;
  LexState *ls = fs->ls;
  jmp_patchval(fs, fs->jpc, pc, NO_REG, pc);
  fs->jpc = NO_JMP;
  if (LJ_UNLIKELY(pc >= fs->bclim)) {
    ptrdiff_t base = fs->bcbase - ls->bcstack;
    checklimit(fs, ls->sizebcstack, LJ_MAX_BCINS, "bytecode instructions");
    lj_mem_growvec(fs->L, ls->bcstack, ls->sizebcstack, LJ_MAX_BCINS,BCInsLine);
    fs->bclim = (BCPos)(ls->sizebcstack - base);
    fs->bcbase = ls->bcstack + base;
  }
  fs->bcbase[pc].ins = ins;
  fs->bcbase[pc].line = ls->lastline;
  fs->pc = pc+1;
  return pc;
}

#define bcemit_ABC(fs, o, a, b, c)	bcemit_INS(fs, BCINS_ABC(o, a, b, c))
#define bcemit_AD(fs, o, a, d)		bcemit_INS(fs, BCINS_AD(o, a, d))
#define bcemit_AJ(fs, o, a, j)		bcemit_INS(fs, BCINS_AJ(o, a, j))

#define bcptr(fs, e)			(&(fs)->bcbase[(e)->u.s.info].ins)

/* -- Bytecode emitter for expressions ------------------------------------ */

/* Discharge non-constant expression to any register. */
static void expr_discharge(FuncState *fs, ExpDesc *e)
{
  BCIns ins;
  if (e->k == VUPVAL) {
    ins = BCINS_AD(BC_UGET, 0, e->u.s.info);
  } else if (e->k == VGLOBAL) {
    ins = BCINS_AD(BC_GGET, 0, const_str(fs, e));
  } else if (e->k == VINDEXED) {
    BCReg rc = e->u.s.aux;
    if ((int32_t)rc < 0) {
      ins = BCINS_ABC(BC_TGETS, 0, e->u.s.info, ~rc);
    } else if (rc > BCMAX_C) {
      ins = BCINS_ABC(BC_TGETB, 0, e->u.s.info, rc-(BCMAX_C+1));
    } else {
      bcreg_free(fs, rc);
      ins = BCINS_ABC(BC_TGETV, 0, e->u.s.info, rc);
    }
    bcreg_free(fs, e->u.s.info);
  } else if (e->k == VCALL) {
    e->u.s.info = e->u.s.aux;
    e->k = VNONRELOC;
    return;
  } else if (e->k == VLOCAL) {
    e->k = VNONRELOC;
    return;
  } else {
    return;
  }
  e->u.s.info = bcemit_INS(fs, ins);
  e->k = VRELOCABLE;
}

/* Emit bytecode to set a range of registers to nil. */
static void bcemit_nil(FuncState *fs, BCReg from, BCReg n)
{
  if (fs->pc > fs->lasttarget) {  /* No jumps to current position? */
    BCIns *ip = &fs->bcbase[fs->pc-1].ins;
    BCReg pto, pfrom = bc_a(*ip);
    switch (bc_op(*ip)) {  /* Try to merge with the previous instruction. */
    case BC_KPRI:
      if (bc_d(*ip) != ~LJ_TNIL) break;
      if (from == pfrom) {
	if (n == 1) return;
      } else if (from == pfrom+1) {
	from = pfrom;
	n++;
      } else {
	break;
      }
      *ip = BCINS_AD(BC_KNIL, from, from+n-1);  /* Replace KPRI. */
      return;
    case BC_KNIL:
      pto = bc_d(*ip);
      if (pfrom <= from && from <= pto+1) {  /* Can we connect both ranges? */
	if (from+n-1 > pto)
	  setbc_d(ip, from+n-1);  /* Patch previous instruction range. */
	return;
      }
      break;
    default:
      break;
    }
  }
  /* Emit new instruction or replace old instruction. */
  bcemit_INS(fs, n == 1 ? BCINS_AD(BC_KPRI, from, VKNIL) :
			  BCINS_AD(BC_KNIL, from, from+n-1));
}

/* Discharge an expression to a specific register. Ignore branches. */
static void expr_toreg_nobranch(FuncState *fs, ExpDesc *e, BCReg reg)
{
  BCIns ins;
  expr_discharge(fs, e);
  if (e->k == VKSTR) {
    ins = BCINS_AD(BC_KSTR, reg, const_str(fs, e));
  } else if (e->k == VKNUM) {
#if LJ_DUALNUM
    cTValue *tv = expr_numtv(e);
    if (tvisint(tv) && checki16(intV(tv)))
      ins = BCINS_AD(BC_KSHORT, reg, (BCReg)(uint16_t)intV(tv));
    else
#else
    int64_t i64;
    int32_t k;
    if (lj_num2int_cond(expr_numberV(e), i64, k, checki16((int32_t)i64)))
      ins = BCINS_AD(BC_KSHORT, reg, (BCReg)(uint16_t)k);
    else
#endif
      ins = BCINS_AD(BC_KNUM, reg, const_num(fs, e));
#if LJ_HASFFI
  } else if (e->k == VKCDATA) {
    fs->flags |= PROTO_FFI;
    ins = BCINS_AD(BC_KCDATA, reg,
		   const_gc(fs, obj2gco(cdataV(&e->u.nval)), LJ_TCDATA));
#endif
  } else if (e->k == VRELOCABLE) {
    setbc_a(bcptr(fs, e), reg);
    goto noins;
  } else if (e->k == VNONRELOC) {
    if (reg == e->u.s.info)
      goto noins;
    ins = BCINS_AD(BC_MOV, reg, e->u.s.info);
  } else if (e->k == VKNIL) {
    bcemit_nil(fs, reg, 1);
    goto noins;
  } else if (e->k <= VKTRUE) {
    ins = BCINS_AD(BC_KPRI, reg, const_pri(e));
  } else {
    lj_assertFS(e->k == VVOID || e->k == VJMP, "bad expr type %d", e->k);
    return;
  }
  bcemit_INS(fs, ins);
noins:
  e->u.s.info = reg;
  e->k = VNONRELOC;
}

/* Forward declaration. */
static BCPos bcemit_jmp(FuncState *fs);

/* Discharge an expression to a specific register. */
static void expr_toreg(FuncState *fs, ExpDesc *e, BCReg reg)
{
  expr_toreg_nobranch(fs, e, reg);
  if (e->k == VJMP)
    jmp_append(fs, &e->t, e->u.s.info);  /* Add it to the true jump list. */
  if (expr_hasjump(e)) {  /* Discharge expression with branches. */
    BCPos jend, jfalse = NO_JMP, jtrue = NO_JMP;
    if (jmp_novalue(fs, e->t) || jmp_novalue(fs, e->f)) {
      BCPos jval = (e->k == VJMP) ? NO_JMP : bcemit_jmp(fs);
      jfalse = bcemit_AD(fs, BC_KPRI, reg, VKFALSE);
      bcemit_AJ(fs, BC_JMP, fs->freereg, 1);
      jtrue = bcemit_AD(fs, BC_KPRI, reg, VKTRUE);
      jmp_tohere(fs, jval);
    }
    jend = fs->pc;
    fs->lasttarget = jend;
    jmp_patchval(fs, e->f, jend, reg, jfalse);
    jmp_patchval(fs, e->t, jend, reg, jtrue);
  }
  e->f = e->t = NO_JMP;
  e->u.s.info = reg;
  e->k = VNONRELOC;
}

/* Discharge an expression to the next free register. */
static void expr_tonextreg(FuncState *fs, ExpDesc *e)
{
  expr_discharge(fs, e);
  expr_free(fs, e);
  bcreg_reserve(fs, 1);
  expr_toreg(fs, e, fs->freereg - 1);
}

/* Discharge an expression to any register. */
static BCReg expr_toanyreg(FuncState *fs, ExpDesc *e)
{
  expr_discharge(fs, e);
  if (e->k == VNONRELOC) {
    if (!expr_hasjump(e)) return e->u.s.info;  /* Already in a register. */
    if (e->u.s.info >= fs->nactvar) {
      expr_toreg(fs, e, e->u.s.info);  /* Discharge to temp. register. */
      return e->u.s.info;
    }
  }
  expr_tonextreg(fs, e);  /* Discharge to next register. */
  return e->u.s.info;
}

#if LJ_54
/* Lua 5.4 keeps source frames for selected tail-position library calls because
** their C-side errors should point at the user callsite. Dynamic arguments can
** insert nested calls between the callee fetch and final CALL, so use a bounded
** scan that is larger than the old adjacent-call-only window.
*/
#define LUA54_NOTAIL_SCAN_LIMIT	32

static void bcemit_lua54_jit_field(FuncState *fs, BCReg base,
				   const char *field, size_t len)
{
  BCReg idx = const_lit(fs, field, len);
  if (idx <= BCMAX_C) {
    bcemit_ABC(fs, BC_TGETS, base, base, idx);
  } else {
    BCReg key = fs->freereg;
    /* TGETS stores the string key in an 8 bit bytecode field. Large chunks can
    ** push private helper names beyond that range, so load the key explicitly.
    */
    bcreg_reserve(fs, 1);
    bcemit_AD(fs, BC_KSTR, key, idx);
    bcemit_ABC(fs, BC_TGETV, base, base, key);
    fs->freereg--;
  }
}

static GCstr *bcemit_lua54_const_str_by_slot(FuncState *fs, BCReg slot)
{
  GCtab *kt = fs->kt;
  Node *node = noderef(kt->node);
  MSize i;
  for (i = 0; i <= kt->hmask; i++) {
    Node *n = &node[i];
    if (tvhaskslot(&n->val) && tvkslot(&n->val) == slot &&
	tvisstr(&n->key))
      return strV(&n->key);
  }
  return NULL;
}

static GCtab *bcemit_lua54_const_tab_by_slot(FuncState *fs, BCReg slot)
{
  GCtab *kt = fs->kt;
  Node *node = noderef(kt->node);
  MSize i;
  for (i = 0; i <= kt->hmask; i++) {
    Node *n = &node[i];
    if (tvhaskslot(&n->val) && tvkslot(&n->val) == slot &&
	tvistab(&n->key))
      return tabV(&n->key);
  }
  return NULL;
}

static int bcemit_lua54_streq(GCstr *s, const char *lit, MSize len)
{
  return s && s->len == len && memcmp(strdata(s), lit, len) == 0;
}

static int lua54_global_notailcall(FuncState *fs, GCstr *name)
{
  FuncState *cur;
  MSize i;
  for (cur = fs; cur != NULL; cur = cur->prev)
    for (i = 0; i < cur->ngnotail; i++)
      if (cur->gnotail[i] == name)
	return 1;
  return 0;
}

static void lua54_mark_global_notailcall_one(FuncState *fs, GCstr *name, int on)
{
  MSize i;
  for (i = 0; i < fs->ngnotail; i++) {
    if (fs->gnotail[i] == name) {
      if (!on)
	fs->gnotail[i] = fs->gnotail[--fs->ngnotail];
      return;
    }
  }
  if (on && fs->ngnotail < LUA54_NOTAIL_GLOBAL_MAX)
    fs->gnotail[fs->ngnotail++] = name;
}

static void lua54_mark_global_notailcall(FuncState *fs, GCstr *name, int on)
{
  FuncState *cur;
  /* A helper alias may be assigned inside a setup function and used by a
  ** sibling function compiled later in the same chunk. Mirror the static mark
  ** to outer parser states so those later functions can keep Lua 5.4 callsite
  ** names without disabling unrelated global tail calls.
  */
  for (cur = fs; cur != NULL; cur = cur->prev)
    lua54_mark_global_notailcall_one(cur, name, on);
}

static int lua54_func_has_local_source(FuncState *fs, VarIndex table)
{
  BCReg i;
  for (i = 0; i < fs->nactvar; i++)
    if (fs->varmap[i] == table)
      return 1;
  return 0;
}

static int lua54_is_known_table_source(FuncState *fs, VarIndex table);
static int bcemit_lua54_is_env_upvalue_fetch(FuncState *fs, BCReg base,
					     BCPos pc);
static int lua54_reg_is_env_or_global_table(FuncState *fs, BCReg reg,
					    BCPos pc);
static int lua54_local_iterhelper(FuncState *fs, BCReg reg);
static int lua54_upvalue_iterhelper(FuncState *fs, BCReg uv);

static VarIndex lua54_resolve_table_alias(FuncState *fs, VarIndex table)
{
  MSize depth;
  if (table >= LJ_MAX_VSTACK)
    return table;
  for (depth = 0; depth < LUA54_NOTAIL_TABLE_ALIAS_MAX; depth++) {
    FuncState *cur;
    VarIndex source = table;
    MSize i;
    for (cur = fs; cur != NULL; cur = cur->prev) {
      for (i = 0; i < cur->ntalias; i++) {
	if (cur->talias[i].alias == table) {
	  source = cur->talias[i].source;
	  goto found;
	}
      }
    }
  found:
    if (source == table || source >= LJ_MAX_VSTACK)
      return table;
    table = source;
  }
  return table;
}

static VarIndex lua54_global_table_alias(FuncState *fs, GCstr *name)
{
  FuncState *cur;
  MSize i;
  if (name == NULL)
    return LJ_MAX_VSTACK;
  for (cur = fs; cur != NULL; cur = cur->prev)
    for (i = 0; i < cur->ngtalias; i++)
      if (cur->gtalias[i].name == name)
	return lua54_resolve_table_alias(fs, cur->gtalias[i].source);
  return LJ_MAX_VSTACK;
}

static void lua54_mark_global_table_alias_one(FuncState *fs, GCstr *name,
					      VarIndex source, int on)
{
  MSize i;
  for (i = 0; i < fs->ngtalias; i++) {
    if (fs->gtalias[i].name == name) {
      if (on) {
	fs->gtalias[i].source = source;
      } else {
	fs->gtalias[i] = fs->gtalias[--fs->ngtalias];
      }
      return;
    }
  }
  if (on && fs->ngtalias < LUA54_NOTAIL_GLOBAL_TABLE_ALIAS_MAX) {
    fs->gtalias[fs->ngtalias].name = name;
    fs->gtalias[fs->ngtalias].source = source;
    fs->ngtalias++;
  }
}

static void lua54_mark_global_table_alias(FuncState *fs, GCstr *name,
					  VarIndex source)
{
  FuncState *cur;
  int on;
  if (name == NULL)
    return;
  source = lua54_resolve_table_alias(fs, source);
  on = source < LJ_MAX_VSTACK && lua54_is_known_table_source(fs, source);
  /* Keep the alias visible to sibling functions compiled later in the same
  ** chunk, but only for globals whose table source is statically known.
  */
  for (cur = fs; cur != NULL; cur = cur->prev)
    lua54_mark_global_table_alias_one(cur, name, source, on);
}

static VarIndex lua54_table_field_alias(FuncState *fs, VarIndex table,
					GCstr *field)
{
  FuncState *cur;
  MSize i;
  if (table >= LJ_MAX_VSTACK || field == NULL)
    return LJ_MAX_VSTACK;
  table = lua54_resolve_table_alias(fs, table);
  for (cur = fs; cur != NULL; cur = cur->prev)
    for (i = 0; i < cur->ntfalias; i++)
      if (cur->tfalias[i].table == table &&
	  cur->tfalias[i].field == field)
	return lua54_resolve_table_alias(fs, cur->tfalias[i].source);
  return LJ_MAX_VSTACK;
}

static void lua54_mark_table_field_alias_one(FuncState *fs, VarIndex table,
					     GCstr *field, VarIndex source,
					     int on)
{
  MSize i;
  for (i = 0; i < fs->ntfalias; i++) {
    if (fs->tfalias[i].table == table && fs->tfalias[i].field == field) {
      if (on) {
	fs->tfalias[i].source = source;
      } else {
	fs->tfalias[i] = fs->tfalias[--fs->ntfalias];
      }
      return;
    }
  }
  if (on && fs->ntfalias < LUA54_NOTAIL_TABLE_FIELD_ALIAS_MAX) {
    fs->tfalias[fs->ntfalias].table = table;
    fs->tfalias[fs->ntfalias].field = field;
    fs->tfalias[fs->ntfalias].source = source;
    fs->ntfalias++;
  }
}

static void lua54_mark_table_field_alias(FuncState *fs, VarIndex table,
					 GCstr *field, VarIndex source)
{
  FuncState *cur;
  int owner = 0;
  int on;
  if (table >= LJ_MAX_VSTACK || field == NULL)
    return;
  table = lua54_resolve_table_alias(fs, table);
  source = lua54_resolve_table_alias(fs, source);
  on = source < LJ_MAX_VSTACK &&
       lua54_is_known_table_source(fs, table) &&
       lua54_is_known_table_source(fs, source);
  /* Track `holder.alias = t` only when both sides are statically known table
  ** sources. A later write of a non-table value to the same field clears the
  ** alias, so dynamic table field rebindings do not poison callsite metadata.
  */
  for (cur = fs; cur != NULL; cur = cur->prev) {
    if (!owner) {
      if (!lua54_func_has_local_source(cur, table))
	continue;
      owner = 1;
    }
    lua54_mark_table_field_alias_one(cur, table, field, source, on);
  }
}

static void lua54_mark_pending_table_field_alias(FuncState *fs, VarIndex table,
						 GCstr *field,
						 VarIndex source)
{
  int on;
  if (table >= LJ_MAX_VSTACK || field == NULL)
    return;
  table = lua54_resolve_table_alias(fs, table);
  source = lua54_resolve_table_alias(fs, source);
  on = source < LJ_MAX_VSTACK &&
       lua54_is_known_table_source(fs, table) &&
       lua54_is_known_table_source(fs, source);
  lua54_mark_table_field_alias_one(fs, table, field, source, on);
}

static void lua54_mark_table_alias_one(FuncState *fs, VarIndex alias,
				       VarIndex source)
{
  MSize i;
  for (i = 0; i < fs->ntalias; i++) {
    if (fs->talias[i].alias == alias) {
      fs->talias[i].source = source;
      return;
    }
  }
  if (fs->ntalias < LUA54_NOTAIL_TABLE_ALIAS_MAX) {
    fs->talias[fs->ntalias].alias = alias;
    fs->talias[fs->ntalias].source = source;
    fs->ntalias++;
  }
}

static void lua54_mark_table_alias(FuncState *fs, VarIndex alias,
				   VarIndex source)
{
  FuncState *cur;
  int owner = 0;
  source = lua54_resolve_table_alias(fs, source);
  if (alias >= LJ_MAX_VSTACK || source >= LJ_MAX_VSTACK || alias == source)
    return;
  if (!lua54_is_known_table_source(fs, source))
    return;
  for (cur = fs; cur != NULL; cur = cur->prev) {
    if (!owner) {
      if (!lua54_func_has_local_source(cur, alias))
	continue;
      owner = 1;
    }
    lua54_mark_table_alias_one(cur, alias, source);
  }
}

static void lua54_mark_pending_table_alias(FuncState *fs, VarIndex alias,
					   VarIndex source)
{
  source = lua54_resolve_table_alias(fs, source);
  if (alias >= LJ_MAX_VSTACK || source >= LJ_MAX_VSTACK || alias == source)
    return;
  if (!lua54_is_known_table_source(fs, source))
    return;
  lua54_mark_table_alias_one(fs, alias, source);
}

static void lua54_clear_table_alias_one(FuncState *fs, VarIndex table)
{
  MSize i = 0;
  while (i < fs->ntalias) {
    if (fs->talias[i].alias == table || fs->talias[i].source == table) {
      fs->talias[i] = fs->talias[--fs->ntalias];
    } else {
      i++;
    }
  }
}

static void lua54_clear_table_alias(FuncState *fs, VarIndex table)
{
  FuncState *cur;
  int owner = 0;
  if (table >= LJ_MAX_VSTACK)
    return;
  for (cur = fs; cur != NULL; cur = cur->prev) {
    if (!owner) {
      if (!lua54_func_has_local_source(cur, table))
	continue;
      owner = 1;
    }
    lua54_clear_table_alias_one(cur, table);
  }
}

static void lua54_clear_table_field_alias_one(FuncState *fs, VarIndex table)
{
  MSize i = 0;
  while (i < fs->ntfalias) {
    if (fs->tfalias[i].table == table || fs->tfalias[i].source == table) {
      fs->tfalias[i] = fs->tfalias[--fs->ntfalias];
    } else {
      i++;
    }
  }
}

static void lua54_clear_table_field_alias(FuncState *fs, VarIndex table)
{
  FuncState *cur;
  int owner = 0;
  if (table >= LJ_MAX_VSTACK)
    return;
  for (cur = fs; cur != NULL; cur = cur->prev) {
    if (!owner) {
      if (!lua54_func_has_local_source(cur, table))
	continue;
      owner = 1;
    }
    lua54_clear_table_field_alias_one(cur, table);
  }
}

static int lua54_table_notailcall(FuncState *fs, VarIndex table, GCstr *field)
{
  FuncState *cur;
  MSize i;
  if (table >= LJ_MAX_VSTACK || field == NULL)
    return 0;
  table = lua54_resolve_table_alias(fs, table);
  for (cur = fs; cur != NULL; cur = cur->prev)
    for (i = 0; i < cur->ntfnotail; i++)
      if (cur->tfnotail[i].table == table &&
	  (cur->tfnotail[i].field == NULL ||
	   cur->tfnotail[i].field == field))
	return 1;
  return 0;
}

static int lua54_table_has_notail_fields(FuncState *fs, VarIndex table)
{
  FuncState *cur;
  MSize i;
  table = lua54_resolve_table_alias(fs, table);
  if (table >= LJ_MAX_VSTACK)
    return 0;
  for (cur = fs; cur != NULL; cur = cur->prev)
    for (i = 0; i < cur->ntfnotail; i++)
      if (cur->tfnotail[i].table == table)
	return 1;
  return 0;
}

static void lua54_mark_table_source(FuncState *fs, VarIndex table)
{
  if (table < fs->ls->vtop)
    fs->ls->vstack[table].info |= VSTACK_VAR_TABLE;
}

static int lua54_is_known_table_source(FuncState *fs, VarIndex table)
{
  table = lua54_resolve_table_alias(fs, table);
  return table < fs->ls->vtop &&
	 ((fs->ls->vstack[table].info & VSTACK_VAR_TABLE) != 0 ||
	  lua54_table_has_notail_fields(fs, table));
}

static void lua54_mark_table_notailcall_one(FuncState *fs, VarIndex table,
					    GCstr *field, int on)
{
  MSize i;
  for (i = 0; i < fs->ntfnotail; i++) {
    if (fs->tfnotail[i].table == table && fs->tfnotail[i].field == field) {
      if (!on)
	fs->tfnotail[i] = fs->tfnotail[--fs->ntfnotail];
      return;
    }
  }
  if (on && fs->ntfnotail < LUA54_NOTAIL_TABLE_FIELD_MAX) {
    fs->tfnotail[fs->ntfnotail].table = table;
    fs->tfnotail[fs->ntfnotail].field = field;
    fs->ntfnotail++;
  }
}

static void lua54_mark_table_notailcall(FuncState *fs, VarIndex table,
					GCstr *field, int on)
{
  FuncState *cur;
  int owner = 0;
  if (table >= LJ_MAX_VSTACK)
    return;
  table = lua54_resolve_table_alias(fs, table);
  if (on)
    lua54_mark_table_source(fs, table);
  for (cur = fs; cur != NULL; cur = cur->prev) {
    if (!owner) {
      if (!lua54_func_has_local_source(cur, table))
	continue;
      owner = 1;
    }
    lua54_mark_table_notailcall_one(cur, table, field, on);
  }
}

static void lua54_mark_pending_table_notailcall(FuncState *fs, VarIndex table,
						GCstr *field, int on)
{
  if (table >= LJ_MAX_VSTACK)
    return;
  table = lua54_resolve_table_alias(fs, table);
  if (on)
    lua54_mark_table_source(fs, table);
  lua54_mark_table_notailcall_one(fs, table, field, on);
}

static int lua54_global_table_notailcall(FuncState *fs, GCstr *name,
					 GCstr *field)
{
  FuncState *cur;
  MSize i;
  if (name == NULL || field == NULL)
    return 0;
  for (cur = fs; cur != NULL; cur = cur->prev)
    for (i = 0; i < cur->ngtfnotail; i++)
      if (cur->gtfnotail[i].name == name &&
	  (cur->gtfnotail[i].field == NULL ||
	   cur->gtfnotail[i].field == field))
	return 1;
  return 0;
}

static void lua54_mark_global_table_notailcall_one(FuncState *fs,
						   GCstr *name,
						   GCstr *field, int on)
{
  MSize i;
  for (i = 0; i < fs->ngtfnotail; i++) {
    if (fs->gtfnotail[i].name == name && fs->gtfnotail[i].field == field) {
      if (!on)
	fs->gtfnotail[i] = fs->gtfnotail[--fs->ngtfnotail];
      return;
    }
  }
  if (on && fs->ngtfnotail < LUA54_NOTAIL_GLOBAL_TABLE_FIELD_MAX) {
    fs->gtfnotail[fs->ngtfnotail].name = name;
    fs->gtfnotail[fs->ngtfnotail].field = field;
    fs->ngtfnotail++;
  }
}

static void lua54_mark_global_table_notailcall(FuncState *fs, GCstr *name,
					       GCstr *field, int on)
{
  FuncState *cur;
  if (name == NULL)
    return;
  for (cur = fs; cur != NULL; cur = cur->prev)
    lua54_mark_global_table_notailcall_one(cur, name, field, on);
}

static void lua54_clear_global_table_notailcall_one(FuncState *fs,
						    GCstr *name,
						    GCstr *field)
{
  MSize i = 0;
  while (i < fs->ngtfnotail) {
    if (fs->gtfnotail[i].name == name &&
	(field == NULL || fs->gtfnotail[i].field == field)) {
      fs->gtfnotail[i] = fs->gtfnotail[--fs->ngtfnotail];
    } else {
      i++;
    }
  }
}

static void lua54_clear_global_table_notailcall(FuncState *fs, GCstr *name,
						GCstr *field)
{
  FuncState *cur;
  if (name == NULL)
    return;
  for (cur = fs; cur != NULL; cur = cur->prev)
    lua54_clear_global_table_notailcall_one(cur, name, field);
}

static int lua54_nested_table_notailcall(FuncState *fs, VarIndex table,
					 GCstr *outer, GCstr *field)
{
  FuncState *cur;
  MSize i;
  if (table >= LJ_MAX_VSTACK || outer == NULL || field == NULL)
    return 0;
  table = lua54_resolve_table_alias(fs, table);
  for (cur = fs; cur != NULL; cur = cur->prev)
    for (i = 0; i < cur->nntfnotail; i++)
      if (cur->ntfpathnotail[i].table == table &&
	  cur->ntfpathnotail[i].outer == outer &&
	  (cur->ntfpathnotail[i].field == NULL ||
	   cur->ntfpathnotail[i].field == field))
	return 1;
  return 0;
}

static void lua54_mark_nested_table_notailcall_one(FuncState *fs,
						   VarIndex table,
						   GCstr *outer,
						   GCstr *field, int on)
{
  MSize i;
  for (i = 0; i < fs->nntfnotail; i++) {
    if (fs->ntfpathnotail[i].table == table &&
	fs->ntfpathnotail[i].outer == outer &&
	fs->ntfpathnotail[i].field == field) {
      if (!on)
	fs->ntfpathnotail[i] = fs->ntfpathnotail[--fs->nntfnotail];
      return;
    }
  }
  if (on && fs->nntfnotail < LUA54_NOTAIL_NESTED_TABLE_FIELD_MAX) {
    fs->ntfpathnotail[fs->nntfnotail].table = table;
    fs->ntfpathnotail[fs->nntfnotail].outer = outer;
    fs->ntfpathnotail[fs->nntfnotail].field = field;
    fs->nntfnotail++;
  }
}

static void lua54_mark_nested_table_notailcall(FuncState *fs, VarIndex table,
					       GCstr *outer, GCstr *field,
					       int on)
{
  FuncState *cur;
  int owner = 0;
  if (table >= LJ_MAX_VSTACK || outer == NULL)
    return;
  table = lua54_resolve_table_alias(fs, table);
  if (on)
    lua54_mark_table_source(fs, table);
  for (cur = fs; cur != NULL; cur = cur->prev) {
    if (!owner) {
      if (!lua54_func_has_local_source(cur, table))
	continue;
      owner = 1;
    }
    lua54_mark_nested_table_notailcall_one(cur, table, outer, field, on);
  }
}

static void lua54_mark_pending_nested_table_notailcall(FuncState *fs,
						       VarIndex table,
						       GCstr *outer,
						       GCstr *field, int on)
{
  if (table >= LJ_MAX_VSTACK || outer == NULL)
    return;
  table = lua54_resolve_table_alias(fs, table);
  if (on)
    lua54_mark_table_source(fs, table);
  lua54_mark_nested_table_notailcall_one(fs, table, outer, field, on);
}

static void lua54_clear_nested_table_notailcall_one(FuncState *fs,
						    VarIndex table,
						    GCstr *outer)
{
  MSize i = 0;
  while (i < fs->nntfnotail) {
    if (fs->ntfpathnotail[i].table == table &&
	(outer == NULL || fs->ntfpathnotail[i].outer == outer)) {
      fs->ntfpathnotail[i] = fs->ntfpathnotail[--fs->nntfnotail];
    } else {
      i++;
    }
  }
}

static void lua54_clear_nested_table_notailcall(FuncState *fs, VarIndex table,
						GCstr *outer)
{
  FuncState *cur;
  int owner = 0;
  if (table >= LJ_MAX_VSTACK)
    return;
  table = lua54_resolve_table_alias(fs, table);
  for (cur = fs; cur != NULL; cur = cur->prev) {
    if (!owner) {
      if (!lua54_func_has_local_source(cur, table))
	continue;
      owner = 1;
    }
    lua54_clear_nested_table_notailcall_one(cur, table, outer);
  }
}

static void lua54_clear_pending_nested_table_notailcall(FuncState *fs,
						       VarIndex table,
						       GCstr *outer)
{
  if (table >= LJ_MAX_VSTACK)
    return;
  lua54_clear_nested_table_notailcall_one(fs, lua54_resolve_table_alias(fs,
    table), outer);
}

static int lua54_global_nested_notailcall(FuncState *fs, GCstr *name,
					  GCstr *outer, GCstr *field)
{
  FuncState *cur;
  MSize i;
  if (name == NULL || outer == NULL || field == NULL)
    return 0;
  for (cur = fs; cur != NULL; cur = cur->prev)
    for (i = 0; i < cur->ngntfnotail; i++)
      if (cur->gntfnotail[i].name == name &&
	  cur->gntfnotail[i].outer == outer &&
	  (cur->gntfnotail[i].field == NULL ||
	   cur->gntfnotail[i].field == field))
	return 1;
  return 0;
}

static void lua54_mark_global_nested_notailcall_one(FuncState *fs,
						    GCstr *name,
						    GCstr *outer,
						    GCstr *field, int on)
{
  MSize i;
  for (i = 0; i < fs->ngntfnotail; i++) {
    if (fs->gntfnotail[i].name == name &&
	fs->gntfnotail[i].outer == outer &&
	fs->gntfnotail[i].field == field) {
      if (!on)
	fs->gntfnotail[i] = fs->gntfnotail[--fs->ngntfnotail];
      return;
    }
  }
  if (on && fs->ngntfnotail < LUA54_NOTAIL_GLOBAL_NESTED_FIELD_MAX) {
    fs->gntfnotail[fs->ngntfnotail].name = name;
    fs->gntfnotail[fs->ngntfnotail].outer = outer;
    fs->gntfnotail[fs->ngntfnotail].field = field;
    fs->ngntfnotail++;
  }
}

static void lua54_mark_global_nested_notailcall(FuncState *fs, GCstr *name,
						GCstr *outer, GCstr *field,
						int on)
{
  FuncState *cur;
  if (name == NULL || outer == NULL)
    return;
  for (cur = fs; cur != NULL; cur = cur->prev)
    lua54_mark_global_nested_notailcall_one(cur, name, outer, field, on);
}

static void lua54_clear_global_nested_notailcall_one(FuncState *fs,
						     GCstr *name,
						     GCstr *outer)
{
  MSize i = 0;
  while (i < fs->ngntfnotail) {
    if (fs->gntfnotail[i].name == name &&
	(outer == NULL || fs->gntfnotail[i].outer == outer)) {
      fs->gntfnotail[i] = fs->gntfnotail[--fs->ngntfnotail];
    } else {
      i++;
    }
  }
}

static void lua54_clear_global_nested_notailcall(FuncState *fs, GCstr *name,
						 GCstr *outer)
{
  FuncState *cur;
  if (name == NULL)
    return;
  for (cur = fs; cur != NULL; cur = cur->prev)
    lua54_clear_global_nested_notailcall_one(cur, name, outer);
}

static void lua54_clear_table_notailcall_one(FuncState *fs, VarIndex table)
{
  MSize i = 0;
  while (i < fs->ntfnotail) {
    if (fs->tfnotail[i].table == table) {
      fs->tfnotail[i] = fs->tfnotail[--fs->ntfnotail];
    } else {
      i++;
    }
  }
}

static void lua54_clear_table_notailcall(FuncState *fs, VarIndex table)
{
  FuncState *cur;
  int owner = 0;
  if (table >= LJ_MAX_VSTACK)
    return;
  for (cur = fs; cur != NULL; cur = cur->prev) {
    if (!owner) {
      if (!lua54_func_has_local_source(cur, table))
	continue;
      owner = 1;
    }
    lua54_clear_table_notailcall_one(cur, table);
    lua54_clear_nested_table_notailcall_one(cur, table, NULL);
  }
}

static VarIndex lua54_reg_table_source(FuncState *fs, BCReg reg, BCPos pc)
{
  if (reg < fs->nactvar)
    return fs->varmap[reg];
  if (fs->pc == 0)
    return LJ_MAX_VSTACK;
  if (pc >= fs->pc)
    pc = fs->pc - 1;
  for (;;) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != reg)
      goto next;
    switch (op) {
    case BC_UGET:
      {
	BCReg uv = bc_d(ins);
	if (uv < fs->nuv)
	  return fs->uvmap[uv];
	break;
      }
    case BC_MOV:
      return lua54_reg_table_source(fs, bc_d(ins), pc);
    case BC_TGETS:
      {
	GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
	if (lua54_reg_is_env_or_global_table(fs, bc_b(ins), pc))
	  return lua54_global_table_alias(fs, field);
	if (pc > 0) {
	  VarIndex table = lua54_reg_table_source(fs, bc_b(ins), pc - 1);
	  return lua54_table_field_alias(fs, table, field);
	}
	break;
      }
    case BC_TGETV:
      if (pc > 0) {
	BCIns key = fs->bcbase[pc - 1].ins;
	if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins) &&
	    bc_c(ins) >= fs->nactvar &&
	    lua54_reg_is_env_or_global_table(fs, bc_b(ins), pc - 1)) {
	  GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_d(key));
	  return lua54_global_table_alias(fs, field);
	}
	if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins) &&
	    bc_c(ins) >= fs->nactvar) {
	  GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_d(key));
	  VarIndex table = lua54_reg_table_source(fs, bc_b(ins), pc - 1);
	  return lua54_table_field_alias(fs, table, field);
	}
      }
      break;
    default:
      break;
    }
    return LJ_MAX_VSTACK;
  next:
    if (pc == 0)
      break;
    pc--;
  }
  return LJ_MAX_VSTACK;
}

static GCstr *lua54_reg_global_table_name(FuncState *fs, BCReg reg, BCPos pc)
{
  if (fs->pc == 0)
    return NULL;
  if (pc >= fs->pc)
    pc = fs->pc - 1;
  for (;;) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != reg)
      goto next;
    if (op == BC_MOV)
      return lua54_reg_global_table_name(fs, bc_d(ins), pc);
    if (op == BC_TGETS) {
      GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
      if (lua54_reg_is_env_or_global_table(fs, bc_b(ins), pc))
	return field;
    } else if (op == BC_TGETV && pc > 0) {
      BCIns key = fs->bcbase[pc - 1].ins;
      if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins) &&
	  bc_c(ins) >= fs->nactvar &&
	  lua54_reg_is_env_or_global_table(fs, bc_b(ins), pc - 1))
	return bcemit_lua54_const_str_by_slot(fs, bc_d(key));
    }
    return NULL;
  next:
    if (pc == 0)
      break;
    pc--;
  }
  return NULL;
}

static int lua54_reg_table_field_path(FuncState *fs, BCReg reg, BCPos pc,
				      VarIndex *tablep, GCstr **globalp,
				      GCstr **fieldp)
{
  if (fs->pc == 0)
    return 0;
  if (pc >= fs->pc)
    pc = fs->pc - 1;
  for (;;) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != reg)
      goto next;
    if (op == BC_MOV)
      return lua54_reg_table_field_path(fs, bc_d(ins), pc, tablep, globalp,
					fieldp);
    if (op == BC_TGETS) {
      GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
      BCPos tablepc = pc == 0 ? 0 : pc - 1;
      VarIndex table = lua54_reg_table_source(fs, bc_b(ins), tablepc);
      GCstr *global = lua54_reg_global_table_name(fs, bc_b(ins), tablepc);
      if (field != NULL && (table < LJ_MAX_VSTACK || global != NULL)) {
	*tablep = table;
	*globalp = global;
	*fieldp = field;
	return 1;
      }
    } else if (op == BC_TGETV && pc > 0) {
      BCIns key = fs->bcbase[pc - 1].ins;
      if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins) &&
	  bc_c(ins) >= fs->nactvar) {
	GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_d(key));
	BCPos tablepc = pc - 1;
	VarIndex table = lua54_reg_table_source(fs, bc_b(ins), tablepc);
	GCstr *global = lua54_reg_global_table_name(fs, bc_b(ins), tablepc);
	if (field != NULL && (table < LJ_MAX_VSTACK || global != NULL)) {
	  *tablep = table;
	  *globalp = global;
	  *fieldp = field;
	  return 1;
	}
      }
    }
    return 0;
  next:
    if (pc == 0)
      break;
    pc--;
  }
  return 0;
}

static int bcemit_lua54_is_helper_name(GCstr *field)
{
  if (!field)
    return 0;
  if (field->len == 5 && memcmp(strdata(field), "pcall", 5) == 0)
    return 1;
  /* require() reports loader/searcher failures at the source call site in
  ** Lua 5.4; a tail call would discard that frame before the error is raised.
  */
  if (field->len == 7 && memcmp(strdata(field), "require", 7) == 0)
    return 1;
  /* string.format() raises Lua 5.4 parser/formatter errors with luaL_error().
  ** Keeping a tail-position wrapper frame gives those errors the same source
  ** call-site prefix as official Lua.
  */
  if (field->len == 6 && memcmp(strdata(field), "format", 6) == 0)
    return 1;
  if (field->len == 6 && memcmp(strdata(field), "xpcall", 6) == 0)
    return 1;
  /* Loader entrypoints build Lua 5.4-style error strings from the source call
  ** frame. Keep that frame alive even for `return load(...)` / aliases.
  */
  if ((field->len == 4 && memcmp(strdata(field), "load", 4) == 0) ||
      (field->len == 6 && memcmp(strdata(field), "dofile", 6) == 0) ||
      (field->len == 8 && memcmp(strdata(field), "loadfile", 8) == 0))
    return 1;
  /* Keep table library calls in return position as ordinary calls. LuaJIT's
  ** BC_CALLT replaces the caller frame before C argument checks run, which
  ** loses Lua 5.4 source-level names such as table.concat -> 'concat'.
  */
  if ((field->len == 4 && memcmp(strdata(field), "move", 4) == 0) ||
      (field->len == 4 && memcmp(strdata(field), "sort", 4) == 0) ||
      (field->len == 6 && memcmp(strdata(field), "concat", 6) == 0) ||
      (field->len == 6 && memcmp(strdata(field), "insert", 6) == 0) ||
      (field->len == 6 && memcmp(strdata(field), "remove", 6) == 0) ||
      (field->len == 6 && memcmp(strdata(field), "unpack", 6) == 0))
    return 1;
  /* File methods reached from an arbitrary file handle need the source
  ** callsite to report dot-called missing self by public method name and to
  ** distinguish colon calls from explicit-self field/alias calls when
  ** reporting later arguments such as seek offset and setvbuf size.
  */
  if ((field->len == 4 && memcmp(strdata(field), "read", 4) == 0) ||
      (field->len == 4 && memcmp(strdata(field), "seek", 4) == 0) ||
      (field->len == 5 && memcmp(strdata(field), "close", 5) == 0) ||
      (field->len == 5 && memcmp(strdata(field), "flush", 5) == 0) ||
      (field->len == 5 && memcmp(strdata(field), "lines", 5) == 0) ||
      (field->len == 5 && memcmp(strdata(field), "write", 5) == 0) ||
      (field->len == 7 && memcmp(strdata(field), "setvbuf", 7) == 0))
    return 1;
  /* Keep covered math fast functions in return position as ordinary calls for
  ** the same reason: once BC_CALLT has replaced the caller frame, their C
  ** argument checks can only fall back to "math.xxx" instead of source "xxx".
  */
  if ((field->len == 3 && memcmp(strdata(field), "abs", 3) == 0) ||
      (field->len == 3 && memcmp(strdata(field), "cos", 3) == 0) ||
      (field->len == 3 && memcmp(strdata(field), "exp", 3) == 0) ||
      (field->len == 3 && memcmp(strdata(field), "log", 3) == 0) ||
      (field->len == 3 && memcmp(strdata(field), "sin", 3) == 0) ||
      (field->len == 3 && memcmp(strdata(field), "tan", 3) == 0) ||
      (field->len == 4 && memcmp(strdata(field), "acos", 4) == 0) ||
      (field->len == 4 && memcmp(strdata(field), "asin", 4) == 0) ||
      (field->len == 4 && memcmp(strdata(field), "ceil", 4) == 0) ||
      (field->len == 4 && memcmp(strdata(field), "modf", 4) == 0) ||
      (field->len == 4 && memcmp(strdata(field), "sqrt", 4) == 0) ||
      (field->len == 5 && memcmp(strdata(field), "floor", 5) == 0))
    return 1;
  return field->len > 7 && memcmp(strdata(field), "_lua54_", 7) == 0;
}

static int bcemit_lua54_is_coroutine_wrapper_name(GCstr *field)
{
  /* Lua 5.4 reports argument errors through the source field name for these
  ** coroutine wrappers, so tail-position calls must keep a visible frame.
  */
  return bcemit_lua54_streq(field, "resume", 6) ||
	 bcemit_lua54_streq(field, "close", 5) ||
	 bcemit_lua54_streq(field, "status", 6) ||
	 bcemit_lua54_streq(field, "isyieldable", 11) ||
	 bcemit_lua54_streq(field, "create", 6) ||
	 bcemit_lua54_streq(field, "wrap", 4);
}

static int bcemit_lua54_is_string_wrapper_name(GCstr *field)
{
  return bcemit_lua54_streq(field, "byte", 4) ||
	 bcemit_lua54_streq(field, "char", 4) ||
	 bcemit_lua54_streq(field, "dump", 4) ||
	 bcemit_lua54_streq(field, "find", 4) ||
	 bcemit_lua54_streq(field, "gsub", 4) ||
	 bcemit_lua54_streq(field, "len", 3) ||
	 bcemit_lua54_streq(field, "rep", 3) ||
	 bcemit_lua54_streq(field, "sub", 3) ||
	 bcemit_lua54_streq(field, "format", 6) ||
	 bcemit_lua54_streq(field, "gmatch", 6) ||
	 bcemit_lua54_streq(field, "lower", 5) ||
	 bcemit_lua54_streq(field, "match", 5) ||
	 bcemit_lua54_streq(field, "pack", 4) ||
	 bcemit_lua54_streq(field, "upper", 5) ||
	 bcemit_lua54_streq(field, "reverse", 7) ||
	 bcemit_lua54_streq(field, "unpack", 6) ||
	 bcemit_lua54_streq(field, "packsize", 8);
}

static int bcemit_lua54_is_math_entry_name(GCstr *field)
{
  return bcemit_lua54_streq(field, "abs", 3) ||
	 bcemit_lua54_streq(field, "acos", 4) ||
	 bcemit_lua54_streq(field, "asin", 4) ||
	 bcemit_lua54_streq(field, "atan", 4) ||
	 bcemit_lua54_streq(field, "ceil", 4) ||
	 bcemit_lua54_streq(field, "cos", 3) ||
	 bcemit_lua54_streq(field, "deg", 3) ||
	 bcemit_lua54_streq(field, "exp", 3) ||
	 bcemit_lua54_streq(field, "floor", 5) ||
	 bcemit_lua54_streq(field, "fmod", 4) ||
	 bcemit_lua54_streq(field, "log", 3) ||
	 bcemit_lua54_streq(field, "max", 3) ||
	 bcemit_lua54_streq(field, "min", 3) ||
	 bcemit_lua54_streq(field, "modf", 4) ||
	 bcemit_lua54_streq(field, "rad", 3) ||
	 bcemit_lua54_streq(field, "random", 6) ||
	 bcemit_lua54_streq(field, "randomseed", 10) ||
	 bcemit_lua54_streq(field, "sin", 3) ||
	 bcemit_lua54_streq(field, "sqrt", 4) ||
	 bcemit_lua54_streq(field, "tan", 3) ||
	 bcemit_lua54_streq(field, "tointeger", 9) ||
	 bcemit_lua54_streq(field, "type", 4) ||
	 bcemit_lua54_streq(field, "ult", 3);
}

static int bcemit_lua54_is_os_entry_name(GCstr *field)
{
  return bcemit_lua54_streq(field, "clock", 5) ||
	 bcemit_lua54_streq(field, "date", 4) ||
	 bcemit_lua54_streq(field, "difftime", 8) ||
	 bcemit_lua54_streq(field, "execute", 7) ||
	 bcemit_lua54_streq(field, "exit", 4) ||
	 bcemit_lua54_streq(field, "getenv", 6) ||
	 bcemit_lua54_streq(field, "remove", 6) ||
	 bcemit_lua54_streq(field, "rename", 6) ||
	 bcemit_lua54_streq(field, "setlocale", 9) ||
	 bcemit_lua54_streq(field, "time", 4) ||
	 bcemit_lua54_streq(field, "tmpname", 7);
}

static int bcemit_lua54_is_io_entry_name(GCstr *field)
{
  return bcemit_lua54_streq(field, "close", 5) ||
	 bcemit_lua54_streq(field, "flush", 5) ||
	 bcemit_lua54_streq(field, "input", 5) ||
	 bcemit_lua54_streq(field, "lines", 5) ||
	 bcemit_lua54_streq(field, "open", 4) ||
	 bcemit_lua54_streq(field, "output", 6) ||
	 bcemit_lua54_streq(field, "popen", 5) ||
	 bcemit_lua54_streq(field, "read", 4) ||
	 bcemit_lua54_streq(field, "tmpfile", 7) ||
	 bcemit_lua54_streq(field, "type", 4) ||
	 bcemit_lua54_streq(field, "write", 5);
}

static int bcemit_lua54_is_io_file_method_name(GCstr *field)
{
  return bcemit_lua54_streq(field, "close", 5) ||
	 bcemit_lua54_streq(field, "flush", 5) ||
	 bcemit_lua54_streq(field, "lines", 5) ||
	 bcemit_lua54_streq(field, "read", 4) ||
	 bcemit_lua54_streq(field, "seek", 4) ||
	 bcemit_lua54_streq(field, "setvbuf", 7) ||
	 bcemit_lua54_streq(field, "write", 5);
}

static int bcemit_lua54_is_debug_entry_name(GCstr *field)
{
  return bcemit_lua54_streq(field, "gethook", 7) ||
	 bcemit_lua54_streq(field, "getinfo", 7) ||
	 bcemit_lua54_streq(field, "getlocal", 8) ||
	 bcemit_lua54_streq(field, "getmetatable", 12) ||
	 bcemit_lua54_streq(field, "getregistry", 11) ||
	 bcemit_lua54_streq(field, "getupvalue", 10) ||
	 bcemit_lua54_streq(field, "getuservalue", 12) ||
	 bcemit_lua54_streq(field, "setcstacklimit", 14) ||
	 bcemit_lua54_streq(field, "sethook", 7) ||
	 bcemit_lua54_streq(field, "setlocal", 8) ||
	 bcemit_lua54_streq(field, "setmetatable", 12) ||
	 bcemit_lua54_streq(field, "setupvalue", 10) ||
	 bcemit_lua54_streq(field, "setuservalue", 12) ||
	 bcemit_lua54_streq(field, "traceback", 9) ||
	 bcemit_lua54_streq(field, "upvalueid", 9) ||
	 bcemit_lua54_streq(field, "upvaluejoin", 11);
}

static int bcemit_lua54_is_utf8_entry_name(GCstr *field)
{
  return bcemit_lua54_streq(field, "char", 4) ||
	 bcemit_lua54_streq(field, "codes", 5) ||
	 bcemit_lua54_streq(field, "codepoint", 9) ||
	 bcemit_lua54_streq(field, "len", 3) ||
	 bcemit_lua54_streq(field, "offset", 6);
}

static int bcemit_lua54_is_package_entry_name(GCstr *field)
{
  return bcemit_lua54_streq(field, "loadlib", 7) ||
	 bcemit_lua54_streq(field, "searchpath", 10);
}

static int bcemit_lua54_is_base_global_name(GCstr *field)
{
  return bcemit_lua54_streq(field, "assert", 6) ||
	 bcemit_lua54_streq(field, "collectgarbage", 14) ||
	 bcemit_lua54_streq(field, "dofile", 6) ||
	 bcemit_lua54_streq(field, "error", 5) ||
	 bcemit_lua54_streq(field, "getmetatable", 12) ||
	 bcemit_lua54_streq(field, "ipairs", 6) ||
	 bcemit_lua54_streq(field, "load", 4) ||
	 bcemit_lua54_streq(field, "loadfile", 8) ||
	 bcemit_lua54_streq(field, "next", 4) ||
	 bcemit_lua54_streq(field, "pairs", 5) ||
	 bcemit_lua54_streq(field, "pcall", 5) ||
	 bcemit_lua54_streq(field, "print", 5) ||
	 bcemit_lua54_streq(field, "rawequal", 8) ||
	 bcemit_lua54_streq(field, "rawget", 6) ||
	 bcemit_lua54_streq(field, "rawlen", 6) ||
	 bcemit_lua54_streq(field, "rawset", 6) ||
	 bcemit_lua54_streq(field, "select", 6) ||
	 bcemit_lua54_streq(field, "setmetatable", 12) ||
	 bcemit_lua54_streq(field, "tonumber", 8) ||
	 bcemit_lua54_streq(field, "tostring", 8) ||
	 bcemit_lua54_streq(field, "type", 4) ||
	 bcemit_lua54_streq(field, "warn", 4) ||
	 bcemit_lua54_streq(field, "xpcall", 6);
}

static int bcemit_lua54_is_table_fetch(FuncState *fs, BCReg base, BCPos pc,
				       const char *name, MSize len)
{
  BCIns ins, key;
  BCOp op;
  if (pc >= fs->pc)
    return 0;
  ins = fs->bcbase[pc].ins;
  op = bc_op(ins);
  if (bc_a(ins) != base)
    return 0;
  if (op == BC_TGETS && bc_b(ins) == base)
    return bcemit_lua54_streq(bcemit_lua54_const_str_by_slot(fs, bc_c(ins)),
			      name, len);
  if (op == BC_TGETV && bc_b(ins) == base && pc > 0) {
    key = fs->bcbase[pc - 1].ins;
    if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins))
      return bcemit_lua54_streq(bcemit_lua54_const_str_by_slot(fs, bc_d(key)),
				name, len);
  }
  return 0;
}

static int bcemit_lua54_is_io_std_file_fetch(FuncState *fs, BCReg base,
					     BCPos tablepc)
{
  return bcemit_lua54_is_table_fetch(fs, base, tablepc, "stdin", 5) ||
	 bcemit_lua54_is_table_fetch(fs, base, tablepc, "stdout", 6) ||
	 bcemit_lua54_is_table_fetch(fs, base, tablepc, "stderr", 6);
}

static int bcemit_lua54_is_io_std_file_method_call(FuncState *fs,
						   BCPos fieldpc,
						   GCstr *field,
						   BCPos keypc)
{
  BCIns ins;
  BCOp op;
  BCReg tablebase;
  BCPos tablepc, pos;
  if (!bcemit_lua54_is_io_file_method_name(field) || fieldpc >= fs->pc)
    return 0;
  ins = fs->bcbase[fieldpc].ins;
  op = bc_op(ins);
  if (op != BC_TGETS && op != BC_TGETV)
    return 0;
  tablebase = bc_b(ins);
  tablepc = keypc == fieldpc ? fieldpc - 1 : keypc - 1;
  if (!bcemit_lua54_is_io_std_file_fetch(fs, tablebase, tablepc))
    return 0;
  /* `return io.stdin.close()` is a tail-position file-method lookup, not an
  ** io.close() library call. Keep its caller frame so Lua 5.4 argument errors
  ** still carry the source location instead of becoming a bare message.
  */
  pos = tablepc;
  while (pos-- > 0 && tablepc - pos <= 3)
    if (bcemit_lua54_is_table_fetch(fs, tablebase, pos, "io", 2))
      return 1;
  return 0;
}

static int bcemit_lua54_is_env_upvalue_fetch(FuncState *fs, BCReg base,
					     BCPos pc)
{
  BCIns ins;
  BCReg uv;
  VarIndex vidx;
  GCstr *name;
  if (pc >= fs->pc)
    return 0;
  ins = fs->bcbase[pc].ins;
  if (bc_op(ins) != BC_UGET || bc_a(ins) != base)
    return 0;
  uv = bc_d(ins);
  if (uv >= fs->nuv)
    return 0;
  vidx = fs->uvmap[uv];
  if (vidx >= fs->ls->vtop)
    return 0;
  name = strref(fs->ls->vstack[vidx].name);
  return name->len == 4 && memcmp(strdata(name), "_ENV", 4) == 0;
}

static int bcemit_lua54_is_known_lib_wrapper_call(FuncState *fs,
						  BCPos fieldpc, GCstr *field,
						  BCPos keypc)
{
  BCIns ins;
  BCIns selfcopy;
  BCOp op;
  BCReg tablebase;
  BCPos tablepc;
  BCPos movpc;
  if (fieldpc >= fs->pc)
    return 0;
  ins = fs->bcbase[fieldpc].ins;
  op = bc_op(ins);
  if (op != BC_TGETS && op != BC_TGETV)
    return 0;
  tablebase = bc_b(ins);
  tablepc = keypc == fieldpc ? fieldpc - 1 : keypc - 1;
  /* Many library fields share short names such as len/type/close. Match the
  ** actual table fetch too, instead of committing to the first field-name hit.
  */
  if (bcemit_lua54_is_coroutine_wrapper_name(field) &&
      bcemit_lua54_is_table_fetch(fs, tablebase, tablepc, "coroutine", 9))
    return 1;
  if (bcemit_lua54_is_string_wrapper_name(field) &&
      bcemit_lua54_is_table_fetch(fs, tablebase, tablepc, "string", 6))
    return 1;
  if (bcemit_lua54_is_string_wrapper_name(field)) {
    /* Method syntax emits `MOV base+1, object; TGET* base, object, field`.
    ** If it calls the string library through the string metatable, tailcall
    ** frame replacement makes diagnostics count the hidden self as #1 and
    ** fall back to string.xxx. Keep the source method frame for Lua 5.4.
    */
    movpc = keypc == fieldpc ? fieldpc : keypc;
    if (movpc > 0) {
      selfcopy = fs->bcbase[movpc - 1].ins;
      if (bc_op(selfcopy) == BC_MOV &&
	  bc_a(selfcopy) == bc_a(ins) + 1 + fs->ls->fr2 &&
	  bc_d(selfcopy) == tablebase)
	return 1;
    }
  }
  if (bcemit_lua54_is_math_entry_name(field) &&
      bcemit_lua54_is_table_fetch(fs, tablebase, tablepc, "math", 4))
    return 1;
  if (bcemit_lua54_is_os_entry_name(field) &&
      bcemit_lua54_is_table_fetch(fs, tablebase, tablepc, "os", 2))
    return 1;
  if (bcemit_lua54_is_io_std_file_method_call(fs, fieldpc, field, keypc))
    return 1;
  if (bcemit_lua54_is_io_entry_name(field) &&
      bcemit_lua54_is_table_fetch(fs, tablebase, tablepc, "io", 2))
    return 1;
  if (bcemit_lua54_is_debug_entry_name(field) &&
      bcemit_lua54_is_table_fetch(fs, tablebase, tablepc, "debug", 5))
    return 1;
  if (bcemit_lua54_is_utf8_entry_name(field) &&
      bcemit_lua54_is_table_fetch(fs, tablebase, tablepc, "utf8", 4))
    return 1;
  if (bcemit_lua54_is_package_entry_name(field) &&
      bcemit_lua54_is_table_fetch(fs, tablebase, tablepc, "package", 7))
    return 1;
  return 0;
}

static int bcemit_lua54_is_base_global_call(FuncState *fs, BCPos fieldpc,
					    GCstr *field, BCPos keypc)
{
  BCIns ins;
  BCOp op;
  BCReg tablebase;
  BCPos tablepc;
  if (!bcemit_lua54_is_base_global_name(field) || fieldpc >= fs->pc)
    return 0;
  ins = fs->bcbase[fieldpc].ins;
  op = bc_op(ins);
  if (op != BC_TGETS && op != BC_TGETV)
    return 0;
  tablebase = bc_b(ins);
  tablepc = keypc == fieldpc ? fieldpc - 1 : keypc - 1;
  return bcemit_lua54_is_env_upvalue_fetch(fs, tablebase, tablepc);
}

static int bcemit_lua54_is_marked_global_notail_call(FuncState *fs,
						     BCPos fieldpc,
						     GCstr *field,
						     BCPos keypc)
{
  BCIns ins;
  BCOp op;
  BCReg tablebase;
  BCPos tablepc;
  if (!lua54_global_notailcall(fs, field) || fieldpc >= fs->pc)
    return 0;
  ins = fs->bcbase[fieldpc].ins;
  op = bc_op(ins);
  if (op != BC_TGETS && op != BC_TGETV)
    return 0;
  tablebase = bc_b(ins);
  tablepc = keypc == fieldpc ? fieldpc - 1 : keypc - 1;
  return bcemit_lua54_is_env_upvalue_fetch(fs, tablebase, tablepc);
}

static int bcemit_lua54_is_marked_table_notail_call(FuncState *fs,
						    BCPos fieldpc,
						    GCstr *field,
						    BCPos keypc)
{
  BCIns ins;
  BCOp op;
  BCReg tablebase;
  BCPos tablepc;
  VarIndex table;
  if (field == NULL || fieldpc >= fs->pc)
    return 0;
  ins = fs->bcbase[fieldpc].ins;
  op = bc_op(ins);
  if (op != BC_TGETS && op != BC_TGETV)
    return 0;
  tablebase = bc_b(ins);
  tablepc = keypc == fieldpc ? (fieldpc == 0 ? 0 : fieldpc - 1) :
	    (keypc == 0 ? 0 : keypc - 1);
  table = lua54_reg_table_source(fs, tablebase, tablepc);
  if (lua54_table_notailcall(fs, table, field))
    return 1;
  if (lua54_global_table_notailcall(fs,
	lua54_reg_global_table_name(fs, tablebase, tablepc), field))
    return 1;
  {
    VarIndex outertable = LJ_MAX_VSTACK;
    GCstr *global = NULL;
    GCstr *outer = NULL;
    if (lua54_reg_table_field_path(fs, tablebase, tablepc, &outertable,
				   &global, &outer)) {
      if (outertable < LJ_MAX_VSTACK &&
	  lua54_nested_table_notailcall(fs, outertable, outer, field))
	return 1;
      if (lua54_global_nested_notailcall(fs, global, outer, field))
	return 1;
    }
  }
  return 0;
}

static int bcemit_lua54_is_iterator_helper_call(FuncState *fs, BCPos fieldpc,
						GCstr *field, BCPos keypc)
{
  if (bcemit_lua54_streq(field, "pairs", 5))
    return bcemit_lua54_is_base_global_call(fs, fieldpc, field, keypc);
  return bcemit_lua54_streq(field, "lines", 5);
}

static int bcemit_lua54_is_iterator_result(FuncState *fs, BCPos callpc)
{
  BCIns call = fs->bcbase[callpc].ins;
  BCReg base = bc_a(call);
  BCPos pos = callpc;
  while (pos-- > 0 && callpc - pos <= LUA54_NOTAIL_SCAN_LIMIT) {
    BCIns ins = fs->bcbase[pos].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != base)
      continue;
    if (op == BC_MOV)
      return lua54_local_iterhelper(fs, bc_d(ins));
    if (op == BC_UGET)
      return lua54_upvalue_iterhelper(fs, bc_d(ins));
    if (op == BC_TGETS) {
      GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
      return bcemit_lua54_is_iterator_helper_call(fs, pos, field, pos);
    }
    if (op == BC_TGETV && pos >= 1) {
      BCIns key = fs->bcbase[pos - 1].ins;
      if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins) &&
	  bc_c(ins) >= fs->nactvar) {
	GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_d(key));
	return bcemit_lua54_is_iterator_helper_call(fs, pos, field, pos - 1);
      }
    }
    break;
  }
  return 0;
}

static int bcemit_lua54_is_private_helper_call(FuncState *fs, ExpDesc *e)
{
  BCPos pc = e->u.s.info;
  BCIns call = fs->bcbase[pc].ins;
  BCReg base = bc_a(call);
  if ((bc_op(call) != BC_CALL && bc_op(call) != BC_CALLM) || pc == 0)
    return 0;
  {
    BCPos pos = pc;
    /* Keep Lua 5.4 private helper and protected-call helpers as ordinary calls
    ** in return position. Their C error/unwind paths still need the caller
    ** frame; tail-call optimization would remove exactly that frame.
    */
    while (pos-- > 0 && pc - pos <= LUA54_NOTAIL_SCAN_LIMIT) {
      BCIns ins = fs->bcbase[pos].ins;
      BCOp op = bc_op(ins);
      if (bc_a(ins) != base)
	continue;
      if (op == BC_TGETS && bc_b(ins) == base) {
	GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
	return bcemit_lua54_is_helper_name(field) ||
	       bcemit_lua54_is_known_lib_wrapper_call(fs, pos, field, pos) ||
	       bcemit_lua54_is_base_global_call(fs, pos, field, pos) ||
	       bcemit_lua54_is_marked_global_notail_call(fs, pos, field,
							 pos) ||
	       bcemit_lua54_is_marked_table_notail_call(fs, pos, field,
							pos);
      }
      if (op == BC_TGETV && bc_b(ins) == base && pos >= 1) {
	BCIns key = fs->bcbase[pos - 1].ins;
	if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins) &&
	    bc_c(ins) >= fs->nactvar) {
	  GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_d(key));
	  return bcemit_lua54_is_helper_name(field) ||
		 bcemit_lua54_is_known_lib_wrapper_call(fs, pos, field,
							pos - 1) ||
		 bcemit_lua54_is_base_global_call(fs, pos, field, pos - 1) ||
		 bcemit_lua54_is_marked_global_notail_call(fs, pos, field,
							   pos - 1) ||
		 bcemit_lua54_is_marked_table_notail_call(fs, pos, field,
							  pos - 1);
	}
      }
      break;
    }
  }
  return 0;
}

static void bcemit_lua54_helper(FuncState *fs, const char *field, size_t len,
				ExpDesc *e1, ExpDesc *e2, BCReg nargs)
{
  LexState *ls = fs->ls;
  BCReg base;
  BCReg argbase;
  BCReg reusebase = NO_REG;
  /* Keep new Lua 5.4 operators out of the VM bytecode format for now: lower
  ** them to private jit helpers so default LuaJIT bytecode remains unchanged.
  */
  if (e1->k == VNONRELOC && e1->u.s.info >= fs->nactvar)
    reusebase = e1->u.s.info;
  if (e1->k != VNONRELOC && !expr_isk_nojump(e1)) {
    /* Lua 5.4 global access is lowered through _ENV table indexing. Materialize
    ** it before loading jit._lua54_* so helper setup cannot overwrite the table
    ** register still needed by a pending VINDEXED expression.
    */
    expr_tonextreg(fs, e1);
    if (e1->u.s.info >= fs->nactvar)
      reusebase = e1->u.s.info;
  }
  if (nargs == 2) {
    if (reusebase == NO_REG && e2->k == VNONRELOC &&
	e2->u.s.info >= fs->nactvar)
      reusebase = e2->u.s.info;
    if (e2->k != VNONRELOC && !expr_isk_nojump(e2)) {
      expr_tonextreg(fs, e2);
      if (reusebase == NO_REG && e2->u.s.info >= fs->nactvar)
	reusebase = e2->u.s.info;
    }
  }
  base = fs->freereg;
  if (reusebase != NO_REG) {
    BCReg need;
    base = reusebase;
    argbase = (BCReg)(base + 1 + ls->fr2);
    need = (BCReg)(argbase + nargs);
    if (fs->freereg < need)
      bcreg_reserve(fs, (BCReg)(need - fs->freereg));
    /* Reuse the earliest temporary operand as the helper call base. Local
    ** declarations make the first temporary become the first new local, so
    ** placing the helper after a materialized right operand would leak that
    ** operand into multi-assignment before the real result.
    */
    if (nargs == 2)
      expr_toreg(fs, e2, (BCReg)(argbase + 1));
    expr_toreg(fs, e1, argbase);
  }
  bcemit_AD(fs, BC_GGET, base, const_lit(fs, "jit", 3));
  if (reusebase == NO_REG) {
    bcreg_reserve(fs, 1);
    if (ls->fr2) bcreg_reserve(fs, 1);
  }
  bcemit_lua54_jit_field(fs, base, field, len);
  if (reusebase == NO_REG)
    bcreg_reserve(fs, nargs);
  argbase = (BCReg)(base + 1 + ls->fr2);
  if (reusebase == NO_REG)
    expr_toreg(fs, e1, argbase);
  if (reusebase == NO_REG && nargs == 2)
    expr_toreg(fs, e2, (BCReg)(argbase + 1));
  expr_init(e1, VCALL,
	    bcemit_ABC(fs, BC_CALL, base, 2, fs->freereg - base - ls->fr2));
  e1->u.s.aux = base;
  fs->freereg = base+1;  /* Leave one result by default, like parse_args(). */
}

static void bcemit_lua54_forstep(FuncState *fs, BCReg step)
{
  LexState *ls = fs->ls;
  BCReg base = fs->freereg;
  BCReg argbase;
  /* Keep the zero-step check out of every VM backend: call a private helper
  ** once before FORI, then write its normalized step value back to the slot.
  */
  bcemit_AD(fs, BC_GGET, base, const_lit(fs, "jit", 3));
  bcreg_reserve(fs, 1);
  if (ls->fr2) bcreg_reserve(fs, 1);
  bcemit_lua54_jit_field(fs, base, "_lua54_forstep", 14);
  bcreg_reserve(fs, 1);
  argbase = (BCReg)(base + 1 + ls->fr2);
  bcemit_AD(fs, BC_MOV, argbase, step);
  bcemit_ABC(fs, BC_CALL, base, 2, fs->freereg - base - ls->fr2);
  bcemit_AD(fs, BC_MOV, step, base);
  fs->freereg = base;
}

static void bcemit_lua54_checkclose(FuncState *fs, BCReg slot, GCstr *name)
{
  LexState *ls = fs->ls;
  BCReg base = fs->freereg;
  BCReg argbase, idx;
  ExpDesc e;
#if LJ_TARGET_ARM64
  BCPos skip;
#endif
  /* Keep the first slice of <close> semantics in the same compatibility
  ** helper path as the Lua 5.4-only operators, avoiding bytecode/VM churn
  ** until full scope-exit dispatch is implemented.
  */
#if LJ_TARGET_ARM64
  bcemit_AD(fs, BC_ISF, 0, slot);
  skip = bcemit_jmp(fs);
#endif
  bcemit_AD(fs, BC_GGET, base, const_lit(fs, "jit", 3));
  bcreg_reserve(fs, 1);
  if (ls->fr2) bcreg_reserve(fs, 1);
  bcemit_lua54_jit_field(fs, base, "_lua54_checkclose", 17);
  bcreg_reserve(fs, 3);
  argbase = (BCReg)(base + 1 + ls->fr2);
  bcemit_AD(fs, BC_MOV, argbase, slot);
  expr_init(&e, VKSTR, 0);
  e.u.sval = name;
  idx = const_str(fs, &e);
  bcemit_AD(fs, BC_KSTR, (BCReg)(argbase + 1), idx);
  bcemit_AD(fs, BC_KSHORT, (BCReg)(argbase + 2),
	    (BCReg)(uint16_t)((int32_t)slot - (int32_t)argbase));
  bcemit_ABC(fs, BC_CALL, base, 1, fs->freereg - base - ls->fr2);
  fs->freereg = base;
#if LJ_TARGET_ARM64
  jmp_patch(fs, skip, fs->pc);
#endif
}

static void bcemit_lua54_closevalue(FuncState *fs, BCReg slot, BCReg errval,
				    BCReg haserr)
{
  LexState *ls = fs->ls;
  BCReg base = fs->freereg;
  BCReg argbase;
  BCPos okjump;
#if LJ_TARGET_ARM64
  BCPos skipclose;
#endif
  /* The C helper only prepares the close method call and unmarks the slot.
  ** The parser emits the actual __close(value, nil) as a normal Lua call so a
  ** closing metamethod can yield and resume across this frame, matching Lua
  ** 5.4's yieldable close path.
  */
#if LJ_TARGET_ARM64
  bcemit_AD(fs, BC_ISF, 0, slot);
  skipclose = bcemit_jmp(fs);
#endif
  bcemit_AD(fs, BC_GGET, base, const_lit(fs, "jit", 3));
  bcreg_reserve(fs, 1);
  if (ls->fr2) bcreg_reserve(fs, 1);
  bcemit_lua54_jit_field(fs, base, "_lua54_closevalue", 17);
  bcreg_reserve(fs, 3);
  argbase = (BCReg)(base + 1 + ls->fr2);
  bcemit_AD(fs, BC_MOV, argbase, slot);
  bcemit_AD(fs, BC_KSHORT, (BCReg)(argbase + 1),
	    (BCReg)(uint16_t)((int32_t)slot - (int32_t)argbase));
  bcemit_AD(fs, BC_MOV, (BCReg)(argbase + 2), errval);
  bcemit_ABC(fs, BC_CALL, base, 6,
	     fs->freereg - base - ls->fr2);
  fs->freereg = (BCReg)(base + 5);
  if (ls->fr2) {
    /* C returns compact results. Move them into the same callee/gap/arg layout
    ** that a normal Lua call expression would use on FR2 targets.
    */
    bcemit_AD(fs, BC_MOV, (BCReg)(base + 5), (BCReg)(base + 4));
    bcemit_AD(fs, BC_MOV, (BCReg)(base + 4), (BCReg)(base + 3));
    bcemit_AD(fs, BC_MOV, (BCReg)(base + 3), (BCReg)(base + 2));
    bcemit_AD(fs, BC_MOV, (BCReg)(base + 2), (BCReg)(base + 1));
    fs->freereg = (BCReg)(base + 6);
  }
  /* Run __close through pcall. A close method may yield and later throw; the
  ** inner protected frame keeps that delayed error catchable, then the
  ** explicit rethrow below lets the outer close-unwind path close remaining
  ** variables with the replacement error object.
  */
  bcemit_ABC(fs, BC_CALL, base, 3, fs->freereg - base - ls->fr2);
  fs->freereg = (BCReg)(base + 2);
  argbase = fs->freereg;
  bcemit_AD(fs, BC_GGET, argbase, const_lit(fs, "jit", 3));
  bcreg_reserve(fs, 1);
  if (ls->fr2) bcreg_reserve(fs, 1);
  bcemit_lua54_jit_field(fs, argbase, "_lua54_closefinish", 18);
  bcemit_ABC(fs, BC_CALL, argbase, 1, fs->freereg - argbase - ls->fr2);
  fs->freereg = (BCReg)(base + 2);
  bcemit_AD(fs, BC_IST, 0, base);
  okjump = bcemit_AJ(fs, BC_JMP, fs->freereg, NO_JMP);
  bcemit_AD(fs, BC_MOV, errval, (BCReg)(base + 1));
  bcemit_AD(fs, BC_KPRI, haserr, 2);
  jmp_tohere(fs, okjump);
  fs->freereg = base;
#if LJ_TARGET_ARM64
  jmp_patch(fs, skipclose, fs->pc);
#endif
}

static void bcemit_lua54_throwcloseerror(FuncState *fs, BCReg errval,
					 BCReg haserr)
{
  LexState *ls = fs->ls;
  BCReg errbase;
  BCReg argbase;
  BCPos noerr;
  bcemit_AD(fs, BC_ISF, 0, haserr);
  noerr = bcemit_AJ(fs, BC_JMP, fs->freereg, NO_JMP);
  errbase = fs->freereg;
  bcemit_AD(fs, BC_GGET, errbase, const_lit(fs, "error", 5));
  bcreg_reserve(fs, 1);
  if (ls->fr2) bcreg_reserve(fs, 1);
  bcreg_reserve(fs, 2);
  argbase = (BCReg)(errbase + 1 + ls->fr2);
  bcemit_AD(fs, BC_MOV, argbase, errval);
  bcemit_AD(fs, BC_KSHORT, (BCReg)(argbase + 1), 0);
  bcemit_ABC(fs, BC_CALL, errbase, 1, fs->freereg - errbase - ls->fr2);
  jmp_tohere(fs, noerr);
}

#if !LJ_TARGET_X64
static void bcemit_lua54_returnpack_begin(FuncState *fs, BCReg base)
{
  LexState *ls = fs->ls;
  /* Close-active returns need the values to survive close handler calls.
  ** Pack them through a private helper for now; the VM unwind batch can later
  ** replace only this bridge without changing parser-visible close ordering.
  */
  bcemit_AD(fs, BC_GGET, base, const_lit(fs, "jit", 3));
  bcreg_reserve(fs, 1);
  if (ls->fr2) bcreg_reserve(fs, 1);
  bcemit_lua54_jit_field(fs, base, "_lua54_packreturn", 17);
}

static void bcemit_lua54_returnpack_end(FuncState *fs, BCReg base, ExpDesc *e)
{
  LexState *ls = fs->ls;
  if (e->k == VCALL) {
    setbc_b(bcptr(fs, e), 0);
    bcemit_ABC(fs, BC_CALLM, base, 2, e->u.s.aux - base - 1 - ls->fr2);
  } else {
    expr_tonextreg(fs, e);
    bcemit_ABC(fs, BC_CALL, base, 2, fs->freereg - base - ls->fr2);
  }
  fs->freereg = base+1;
}

static BCIns bcemit_lua54_returnunpack(FuncState *fs, BCReg base)
{
  LexState *ls = fs->ls;
  BCReg argbase;
  bcreg_reserve(fs, 1);
  if (ls->fr2) bcreg_reserve(fs, 1);
  argbase = (BCReg)(base + 1 + ls->fr2);
  bcreg_reserve(fs, 1);
  bcemit_AD(fs, BC_MOV, argbase, base);
  bcemit_AD(fs, BC_GGET, base, const_lit(fs, "jit", 3));
  bcemit_lua54_jit_field(fs, base, "_lua54_unpackreturn", 19);
  bcemit_ABC(fs, BC_CALL, base, 0, fs->freereg - base - ls->fr2);
  fs->freereg = base+1;
  return BCINS_AD(BC_RETM, base, 0);
}
#endif
#endif

/* Partially discharge expression to a value. */
static void expr_toval(FuncState *fs, ExpDesc *e)
{
  if (expr_hasjump(e))
    expr_toanyreg(fs, e);
  else
    expr_discharge(fs, e);
}

/* Emit store for LHS expression. */
#if LJ_54
static void lua54_check_const_assign_line(FuncState *fs, ExpDesc *var,
					  BCLine line)
{
  VarInfo *vi = NULL;
  if (var->k == VLOCAL || var->k == VUPVAL)
    vi = &fs->ls->vstack[var->u.s.aux];
  if (vi != NULL && (vi->info & VSTACK_VAR_CONST)) {
    /* The lexer may already have consumed a multi-line RHS or EOF. Lua 5.4
    ** reports const assignment at the left-hand-side token line instead.
    */
    fs->ls->linenumber = line;
    lj_lex_error(fs->ls, 0, LJ_ERR_XCONST, strdata(strref(vi->name)));
  }
}
#endif

static void bcemit_store(FuncState *fs, ExpDesc *var, ExpDesc *e)
{
  BCIns ins;
  if (var->k == VLOCAL) {
    VarInfo *vi = &fs->ls->vstack[var->u.s.aux];
    if (vi->info & VSTACK_VAR_CONST)
      lj_lex_error(fs->ls, 0, LJ_ERR_XCONST, strdata(strref(vi->name)));
#if LJ_54
    /* A Lua 5.4 tail-call name marker only describes the initializer source.
    ** Once a mutable local is assigned again, keeping that stale marker would
    ** disable tail calls for an unrelated value stored in the same slot.
    */
    vi->info &= ~(VSTACK_VAR_NOTAILCALL|VSTACK_VAR_TABLE);
    lua54_clear_table_notailcall(fs, var->u.s.aux);
    lua54_clear_table_alias(fs, var->u.s.aux);
    lua54_clear_table_field_alias(fs, var->u.s.aux);
#endif
    vi->info |= VSTACK_VAR_RW;
    expr_free(fs, e);
    expr_toreg(fs, e, var->u.s.info);
    return;
  } else if (var->k == VUPVAL) {
    VarInfo *vi = &fs->ls->vstack[var->u.s.aux];
    if (vi->info & VSTACK_VAR_CONST)
      lj_lex_error(fs->ls, 0, LJ_ERR_XCONST, strdata(strref(vi->name)));
#if LJ_54
    vi->info &= ~VSTACK_VAR_TABLE;
    lua54_clear_table_notailcall(fs, var->u.s.aux);
    lua54_clear_table_alias(fs, var->u.s.aux);
    lua54_clear_table_field_alias(fs, var->u.s.aux);
#endif
    vi->info |= VSTACK_VAR_RW;
    expr_toval(fs, e);
    if (e->k <= VKTRUE)
      ins = BCINS_AD(BC_USETP, var->u.s.info, const_pri(e));
    else if (e->k == VKSTR)
      ins = BCINS_AD(BC_USETS, var->u.s.info, const_str(fs, e));
    else if (e->k == VKNUM)
      ins = BCINS_AD(BC_USETN, var->u.s.info, const_num(fs, e));
    else
      ins = BCINS_AD(BC_USETV, var->u.s.info, expr_toanyreg(fs, e));
  } else if (var->k == VGLOBAL) {
    BCReg ra = expr_toanyreg(fs, e);
    ins = BCINS_AD(BC_GSET, ra, const_str(fs, var));
  } else {
    BCReg ra, rc;
    lj_assertFS(var->k == VINDEXED, "bad expr type %d", var->k);
    ra = expr_toanyreg(fs, e);
    rc = var->u.s.aux;
    if ((int32_t)rc < 0) {
      ins = BCINS_ABC(BC_TSETS, ra, var->u.s.info, ~rc);
    } else if (rc > BCMAX_C) {
      ins = BCINS_ABC(BC_TSETB, ra, var->u.s.info, rc-(BCMAX_C+1));
    } else {
#ifdef LUA_USE_ASSERT
      /* Free late alloced key reg to avoid assert on free of value reg. */
      /* This can only happen when called from expr_table(). */
      if (e->k == VNONRELOC && ra >= fs->nactvar && rc >= ra)
	bcreg_free(fs, rc);
#endif
      ins = BCINS_ABC(BC_TSETV, ra, var->u.s.info, rc);
    }
  }
  bcemit_INS(fs, ins);
  expr_free(fs, e);
}

/* Emit method lookup expression. */
static void bcemit_method(FuncState *fs, ExpDesc *e, ExpDesc *key)
{
  BCReg idx, func, fr2, obj = expr_toanyreg(fs, e);
  expr_free(fs, e);
  func = fs->freereg;
  fr2 = fs->ls->fr2;
  bcemit_AD(fs, BC_MOV, func+1+fr2, obj);  /* Copy object to 1st argument. */
  lj_assertFS(expr_isstrk(key), "bad usage");
  idx = const_str(fs, key);
  if (idx <= BCMAX_C) {
    bcreg_reserve(fs, 2+fr2);
    bcemit_ABC(fs, BC_TGETS, func, obj, idx);
  } else {
    bcreg_reserve(fs, 3+fr2);
    bcemit_AD(fs, BC_KSTR, func+2+fr2, idx);
    bcemit_ABC(fs, BC_TGETV, func, obj, func+2+fr2);
    fs->freereg--;
  }
  e->u.s.info = func;
  e->k = VNONRELOC;
}

/* -- Bytecode emitter for branches --------------------------------------- */

/* Emit unconditional branch. */
static BCPos bcemit_jmp(FuncState *fs)
{
  BCPos jpc = fs->jpc;
  BCPos j = fs->pc - 1;
  BCIns *ip = &fs->bcbase[j].ins;
  fs->jpc = NO_JMP;
  if ((int32_t)j >= (int32_t)fs->lasttarget && bc_op(*ip) == BC_UCLO) {
    setbc_j(ip, NO_JMP);
    fs->lasttarget = j+1;
  } else {
    j = bcemit_AJ(fs, BC_JMP, fs->freereg, NO_JMP);
  }
  jmp_append(fs, &j, jpc);
  return j;
}

/* Invert branch condition of bytecode instruction. */
static void invertcond(FuncState *fs, ExpDesc *e)
{
  BCIns *ip = &fs->bcbase[e->u.s.info - 1].ins;
  setbc_op(ip, bc_op(*ip)^1);
}

/* Emit conditional branch. */
static BCPos bcemit_branch(FuncState *fs, ExpDesc *e, int cond)
{
  BCPos pc;
  if (e->k == VRELOCABLE) {
    BCIns *ip = bcptr(fs, e);
    if (bc_op(*ip) == BC_NOT) {
      *ip = BCINS_AD(cond ? BC_ISF : BC_IST, 0, bc_d(*ip));
      return bcemit_jmp(fs);
    }
  }
  if (e->k != VNONRELOC) {
    bcreg_reserve(fs, 1);
    expr_toreg_nobranch(fs, e, fs->freereg-1);
  }
  bcemit_AD(fs, cond ? BC_ISTC : BC_ISFC, NO_REG, e->u.s.info);
  pc = bcemit_jmp(fs);
  expr_free(fs, e);
  return pc;
}

/* Emit branch on true condition. */
static void bcemit_branch_t(FuncState *fs, ExpDesc *e)
{
  BCPos pc;
  expr_discharge(fs, e);
  if (e->k == VKSTR || e->k == VKNUM || e->k == VKTRUE)
    pc = NO_JMP;  /* Never jump. */
  else if (e->k == VJMP)
    invertcond(fs, e), pc = e->u.s.info;
  else if (e->k == VKFALSE || e->k == VKNIL)
    expr_toreg_nobranch(fs, e, NO_REG), pc = bcemit_jmp(fs);
  else
    pc = bcemit_branch(fs, e, 0);
  jmp_append(fs, &e->f, pc);
  jmp_tohere(fs, e->t);
  e->t = NO_JMP;
}

/* Emit branch on false condition. */
static void bcemit_branch_f(FuncState *fs, ExpDesc *e)
{
  BCPos pc;
  expr_discharge(fs, e);
  if (e->k == VKNIL || e->k == VKFALSE)
    pc = NO_JMP;  /* Never jump. */
  else if (e->k == VJMP)
    pc = e->u.s.info;
  else if (e->k == VKSTR || e->k == VKNUM || e->k == VKTRUE)
    expr_toreg_nobranch(fs, e, NO_REG), pc = bcemit_jmp(fs);
  else
    pc = bcemit_branch(fs, e, 1);
  jmp_append(fs, &e->t, pc);
  jmp_tohere(fs, e->f);
  e->f = NO_JMP;
}

/* -- Bytecode emitter for operators -------------------------------------- */

#if LJ_54 && LJ_DUALNUM
static int expr_toint64k(ExpDesc *e, int64_t *ip)
{
  TValue *o = expr_numtv(e);
  if (tvisint(o)) {
    *ip = (int64_t)intV(o);
    return 1;
  } else if (tvisi64(o)) {
    *ip = i64V(o);
    return 1;
  }
  return 0;
}
#endif

/* Try constant-folding of arithmetic operators. */
static int foldarith(FuncState *fs, BinOpr opr, ExpDesc *e1, ExpDesc *e2)
{
  TValue o;
  lua_Number n;
  if (!expr_isnumk_nojump(e1) || !expr_isnumk_nojump(e2)) return 0;
#if LJ_54 && LJ_DUALNUM
  if (opr <= OPR_MUL) {
    int64_t i1, i2, r;
    lua_Unsigned u1, u2;
    if (expr_toint64k(e1, &i1) && expr_toint64k(e2, &i2)) {
      u1 = (lua_Unsigned)(lua_Integer)i1;
      u2 = (lua_Unsigned)(lua_Integer)i2;
      switch (opr) {
      case OPR_ADD: r = (int64_t)(lua_Integer)(u1 + u2); break;
      case OPR_SUB: r = (int64_t)(lua_Integer)(u1 - u2); break;
      case OPR_MUL: r = (int64_t)(lua_Integer)(u1 * u2); break;
      default: lj_assertFS(0, "bad integer fold op"); r = 0; break;
      }
      lj_obj_setint64(fs->L, &e1->u.nval, r);
      const_anchor_i64(fs, &e1->u.nval);
      return 1;
    }
  }
#endif
#if LJ_54
  /* Keep Lua 5.4 '%' unfolded: the fold helper uses the floor-based float
  ** formula and double-converted wide integers, both of which diverge from
  ** the official fmod/integer semantics the runtime implements.
  */
  if (opr == OPR_MOD) return 0;
#endif
  n = lj_vm_foldarith(expr_numberV(e1), expr_numberV(e2), (int)opr-OPR_ADD);
  setnumV(&o, n);
  if (tvisnan(&o) || tvismzero(&o)) return 0;  /* Avoid NaN and -0 as consts. */
  if (LJ_DUALNUM) {
#if LJ_54
    if (opr != OPR_DIV && opr != OPR_POW &&
	tvisint(expr_numtv(e1)) && tvisint(expr_numtv(e2))) {
#else
    {
#endif
    int64_t i64;
    int32_t k;
    if (lj_num2int_check(n, i64, k)) {
      setintV(&e1->u.nval, k);
      return 1;
    }
    }
  }
  setnumV(&e1->u.nval, n);
  return 1;
}

/* Emit arithmetic operator. */
static void bcemit_arith(FuncState *fs, BinOpr opr, ExpDesc *e1, ExpDesc *e2)
{
  BCReg rb, rc, t;
  uint32_t op;
  if (foldarith(fs, opr, e1, e2))
    return;
  if (opr == OPR_POW) {
    op = BC_POW;
    rc = expr_toanyreg(fs, e2);
    rb = expr_toanyreg(fs, e1);
  } else {
    op = opr-OPR_ADD+BC_ADDVV;
    /* Must discharge 2nd operand first since VINDEXED might free regs. */
    expr_toval(fs, e2);
    if (expr_isnumk(e2) && (rc = const_num(fs, e2)) <= BCMAX_C)
      op -= BC_ADDVV-BC_ADDVN;
    else
      rc = expr_toanyreg(fs, e2);
    /* 1st operand discharged by bcemit_binop_left, but need KNUM/KSHORT. */
    lj_assertFS(expr_isnumk(e1) || e1->k == VNONRELOC,
		"bad expr type %d", e1->k);
    expr_toval(fs, e1);
    /* Avoid two consts to satisfy bytecode constraints. */
    if (expr_isnumk(e1) && !expr_isnumk(e2) &&
	(t = const_num(fs, e1)) <= BCMAX_B) {
      rb = rc; rc = t; op -= BC_ADDVV-BC_ADDNV;
    } else {
      rb = expr_toanyreg(fs, e1);
    }
  }
  /* Using expr_free might cause asserts if the order is wrong. */
  if (e1->k == VNONRELOC && e1->u.s.info >= fs->nactvar) fs->freereg--;
  if (e2->k == VNONRELOC && e2->u.s.info >= fs->nactvar) fs->freereg--;
  e1->u.s.info = bcemit_ABC(fs, op, 0, rb, rc);
  e1->k = VRELOCABLE;
}

/* Emit comparison operator. */
static void bcemit_comp(FuncState *fs, BinOpr opr, ExpDesc *e1, ExpDesc *e2)
{
  ExpDesc *eret = e1;
  BCIns ins;
  expr_toval(fs, e1);
  if (opr == OPR_EQ || opr == OPR_NE) {
    BCOp op = opr == OPR_EQ ? BC_ISEQV : BC_ISNEV;
    BCReg ra;
    if (expr_isk(e1)) { e1 = e2; e2 = eret; }  /* Need constant in 2nd arg. */
    ra = expr_toanyreg(fs, e1);  /* First arg must be in a reg. */
    expr_toval(fs, e2);
    switch (e2->k) {
    case VKNIL: case VKFALSE: case VKTRUE:
      ins = BCINS_AD(op+(BC_ISEQP-BC_ISEQV), ra, const_pri(e2));
      break;
    case VKSTR:
#if LJ_54
      if (strislong(e2->u.sval)) {
	/* Lua 5.4 long strings are not interned. Avoid the ISEQS pointer
	** specialization so long-string constants compare by bytes via ISEQV.
	*/
	ins = BCINS_AD(op, ra, expr_toanyreg(fs, e2));
      } else
#endif
      ins = BCINS_AD(op+(BC_ISEQS-BC_ISEQV), ra, const_str(fs, e2));
      break;
    case VKNUM:
      ins = BCINS_AD(op+(BC_ISEQN-BC_ISEQV), ra, const_num(fs, e2));
      break;
    default:
      ins = BCINS_AD(op, ra, expr_toanyreg(fs, e2));
      break;
    }
  } else {
    uint32_t op = opr-OPR_LT+BC_ISLT;
    BCReg ra, rd;
    if ((op-BC_ISLT) & 1) {  /* GT -> LT, GE -> LE */
      e1 = e2; e2 = eret;  /* Swap operands. */
      op = ((op-BC_ISLT)^3)+BC_ISLT;
      expr_toval(fs, e1);
      ra = expr_toanyreg(fs, e1);
      rd = expr_toanyreg(fs, e2);
    } else {
      rd = expr_toanyreg(fs, e2);
      ra = expr_toanyreg(fs, e1);
    }
    ins = BCINS_AD(op, ra, rd);
  }
  /* Using expr_free might cause asserts if the order is wrong. */
  if (e1->k == VNONRELOC && e1->u.s.info >= fs->nactvar) fs->freereg--;
  if (e2->k == VNONRELOC && e2->u.s.info >= fs->nactvar) fs->freereg--;
  bcemit_INS(fs, ins);
  eret->u.s.info = bcemit_jmp(fs);
  eret->k = VJMP;
}

/* Fixup left side of binary operator. */
static void bcemit_binop_left(FuncState *fs, BinOpr op, ExpDesc *e)
{
  if (op == OPR_AND) {
    bcemit_branch_t(fs, e);
  } else if (op == OPR_OR) {
    bcemit_branch_f(fs, e);
  } else if (op == OPR_CONCAT) {
    expr_tonextreg(fs, e);
  } else if (op == OPR_EQ || op == OPR_NE) {
    if (!expr_isk_nojump(e)) expr_toanyreg(fs, e);
  } else {
    if (!expr_isnumk_nojump(e)) expr_toanyreg(fs, e);
  }
}

#if LJ_54
/* Emit Lua 5.4 bitwise or floor-division operator as a dedicated bytecode. */
static void bcemit_bitop(FuncState *fs, BinOpr opr, ExpDesc *e1, ExpDesc *e2)
{
  BCReg rb, rc;
  uint32_t op;
  switch (opr) {
  case OPR_BAND: op = BC_BAND; break;
  case OPR_BOR: op = BC_BOR; break;
  case OPR_BXOR: op = BC_BXOR; break;
  case OPR_SHL: op = BC_BSHL; break;
  case OPR_SHR: op = BC_BSHR; break;
  default: op = BC_IDIV; break;  /* OPR_IDIV */
  }
  rc = expr_toanyreg(fs, e2);
  rb = expr_toanyreg(fs, e1);
  /* Using expr_free might cause asserts if the order is wrong. */
  if (e1->k == VNONRELOC && e1->u.s.info >= fs->nactvar) fs->freereg--;
  if (e2->k == VNONRELOC && e2->u.s.info >= fs->nactvar) fs->freereg--;
  e1->u.s.info = bcemit_ABC(fs, op, 0, rb, rc);
  e1->k = VRELOCABLE;
}
#endif

/* Emit binary operator. */
static void bcemit_binop(FuncState *fs, BinOpr op, ExpDesc *e1, ExpDesc *e2)
{
  if (op <= OPR_POW) {
    bcemit_arith(fs, op, e1, e2);
#if LJ_54
  } else if (op == OPR_IDIV) {
    bcemit_bitop(fs, op, e1, e2);
  } else if (op >= OPR_BAND && op <= OPR_SHR) {
    bcemit_bitop(fs, op, e1, e2);
#endif
  } else if (op == OPR_AND) {
    lj_assertFS(e1->t == NO_JMP, "jump list not closed");
    expr_discharge(fs, e2);
    jmp_append(fs, &e2->f, e1->f);
    *e1 = *e2;
  } else if (op == OPR_OR) {
    lj_assertFS(e1->f == NO_JMP, "jump list not closed");
    expr_discharge(fs, e2);
    jmp_append(fs, &e2->t, e1->t);
    *e1 = *e2;
  } else if (op == OPR_CONCAT) {
    expr_toval(fs, e2);
    if (e2->k == VRELOCABLE && bc_op(*bcptr(fs, e2)) == BC_CAT) {
      lj_assertFS(e1->u.s.info == bc_b(*bcptr(fs, e2))-1,
		  "bad CAT stack layout");
      expr_free(fs, e1);
      setbc_b(bcptr(fs, e2), e1->u.s.info);
      e1->u.s.info = e2->u.s.info;
    } else {
      expr_tonextreg(fs, e2);
      expr_free(fs, e2);
      expr_free(fs, e1);
      e1->u.s.info = bcemit_ABC(fs, BC_CAT, 0, e1->u.s.info, e2->u.s.info);
    }
    e1->k = VRELOCABLE;
  } else {
    lj_assertFS(op == OPR_NE || op == OPR_EQ ||
	       op == OPR_LT || op == OPR_GE || op == OPR_LE || op == OPR_GT,
	       "bad binop %d", op);
    bcemit_comp(fs, op, e1, e2);
  }
}

/* Emit unary operator. */
static void bcemit_unop(FuncState *fs, BCOp op, ExpDesc *e)
{
  if (op == BC_NOT) {
    /* Swap true and false lists. */
    { BCPos temp = e->f; e->f = e->t; e->t = temp; }
    jmp_dropval(fs, e->f);
    jmp_dropval(fs, e->t);
    expr_discharge(fs, e);
    if (e->k == VKNIL || e->k == VKFALSE) {
      e->k = VKTRUE;
      return;
    } else if (expr_isk(e) || (LJ_HASFFI && e->k == VKCDATA)) {
      e->k = VKFALSE;
      return;
    } else if (e->k == VJMP) {
      invertcond(fs, e);
      return;
    } else if (e->k == VRELOCABLE) {
      bcreg_reserve(fs, 1);
      setbc_a(bcptr(fs, e), fs->freereg-1);
      e->u.s.info = fs->freereg-1;
      e->k = VNONRELOC;
    } else {
      lj_assertFS(e->k == VNONRELOC, "bad expr type %d", e->k);
    }
  } else {
    lj_assertFS(op == BC_UNM || op == BC_LEN, "bad unop %d", op);
    if (op == BC_UNM && !expr_hasjump(e)) {  /* Constant-fold negations. */
#if LJ_HASFFI
      if (e->k == VKCDATA) {  /* Fold in-place since cdata is not interned. */
	GCcdata *cd = cdataV(&e->u.nval);
	uint64_t *p = (uint64_t *)cdataptr(cd);
	if (cd->ctypeid == CTID_COMPLEX_DOUBLE)
	  p[1] ^= U64x(80000000,00000000);
	else
	  *p = ~*p+1u;
	return;
      } else
#endif
      if (expr_isnumk(e) && !expr_numiszero(e)) {  /* Avoid folding to -0. */
	TValue *o = expr_numtv(e);
	if (tvisint(o)) {
	  int32_t k = intV(o), negk = (int32_t)(~(uint32_t)k+1u);
#if LJ_54
	  if (k == negk) {
	    lj_obj_setint64(fs->L, o,
	      (int64_t)(lua_Integer)((lua_Unsigned)0 - (lua_Unsigned)(lua_Integer)k));
	    const_anchor_i64(fs, o);
	  } else {
	    setintV(o, negk);
	  }
#else
	  if (k == negk)
	    setnumV(o, -(lua_Number)k);
	  else
	    setintV(o, negk);
#endif
	  return;
#if LJ_54
	} else if (tvisi64(o)) {
	  lua_Integer i = (lua_Integer)i64V(o);
	  lj_obj_setint64(fs->L, o,
	    (int64_t)(lua_Integer)((lua_Unsigned)0 - (lua_Unsigned)i));
	  const_anchor_i64(fs, o);
	  return;
#endif
	} else {
	  o->u64 ^= U64x(80000000,00000000);
	  return;
	}
      }
    }
    expr_toanyreg(fs, e);
  }
  expr_free(fs, e);
  e->u.s.info = bcemit_AD(fs, op, 0, e->u.s.info);
  e->k = VRELOCABLE;
}

/* -- Lexer support ------------------------------------------------------- */

/* Check and consume optional token. */
static int lex_opt(LexState *ls, LexToken tok)
{
  if (ls->tok == tok) {
    lj_lex_next(ls);
    return 1;
  }
  return 0;
}

/* Check and consume token. */
static void lex_check(LexState *ls, LexToken tok)
{
  if (ls->tok != tok)
    err_token(ls, tok);
  lj_lex_next(ls);
}

/* Check for matching token. */
static void lex_match(LexState *ls, LexToken what, LexToken who, BCLine line)
{
  if (!lex_opt(ls, what)) {
    if (line == ls->linenumber) {
      err_token(ls, what);
    } else {
      const char *swhat = lj_lex_token2str(ls, what);
      const char *swho = lj_lex_token2str(ls, who);
      lj_lex_error(ls, ls->tok, LJ_ERR_XMATCH, swhat, swho, line);
    }
  }
}

/* Check for string token. */
static GCstr *lex_str(LexState *ls)
{
  GCstr *s;
  if (ls->tok != TK_name && (LJ_52 || ls->tok != TK_goto))
    err_token(ls, TK_name);
  s = strV(&ls->tokval);
  lj_lex_next(ls);
  return s;
}

/* -- Variable handling --------------------------------------------------- */

#define var_get(ls, fs, i)	((ls)->vstack[(fs)->varmap[(i)]])

#if LJ_54
static int lua54_local_notailcall(FuncState *fs, BCReg reg)
{
  return reg < fs->nactvar &&
	 (var_get(fs->ls, fs, reg).info & VSTACK_VAR_NOTAILCALL) != 0;
}

static int lua54_upvalue_notailcall(FuncState *fs, BCReg uv)
{
  VarIndex vidx, uvsrc;
  if (uv >= fs->nuv)
    return 0;
  if (fs->uvnotail[uv])
    return 1;
  vidx = fs->uvmap[uv];
  if (vidx < fs->ls->vtop &&
      (fs->ls->vstack[vidx].info & VSTACK_VAR_NOTAILCALL) != 0)
    return 1;
  uvsrc = fs->uvtmp[uv];
  return uvsrc < LJ_MAX_VSTACK && uvsrc < fs->ls->vtop &&
	 (fs->ls->vstack[uvsrc].info & VSTACK_VAR_NOTAILCALL) != 0;
}

static int lua54_local_iterhelper(FuncState *fs, BCReg reg)
{
  return reg < fs->nactvar &&
	 (var_get(fs->ls, fs, reg).info & VSTACK_VAR_ITERHELPER) != 0;
}

static int lua54_upvalue_iterhelper(FuncState *fs, BCReg uv)
{
  VarIndex vidx, uvsrc;
  if (uv >= fs->nuv)
    return 0;
  if (fs->uviterhelper[uv])
    return 1;
  vidx = fs->uvmap[uv];
  if (vidx < fs->ls->vtop &&
      (fs->ls->vstack[vidx].info & VSTACK_VAR_ITERHELPER) != 0)
    return 1;
  uvsrc = fs->uvtmp[uv];
  return uvsrc < LJ_MAX_VSTACK && uvsrc < fs->ls->vtop &&
	 (fs->ls->vstack[uvsrc].info & VSTACK_VAR_ITERHELPER) != 0;
}

static int lua54_callbase_notailcall(FuncState *fs, ExpDesc *e)
{
  BCPos pc = e->u.s.info;
  BCIns call = fs->bcbase[pc].ins;
  BCReg base = bc_a(call);
  if (lua54_local_notailcall(fs, base))
    return 1;
  while (pc-- > 0 && e->u.s.info - pc <= LUA54_NOTAIL_SCAN_LIMIT) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != base)
      continue;
    /* Calls move locals to a temporary base register before arguments are
    ** loaded. Follow that MOV so a local alias like `f = math.abs` can keep
    ** its source-level name when a tail-position C argument check fails.
    */
    if (op == BC_MOV)
      return lua54_local_notailcall(fs, bc_d(ins));
    if (op == BC_UGET)
      return lua54_upvalue_notailcall(fs, bc_d(ins));
    if (op == BC_TGETS) {
      GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
      return bcemit_lua54_is_helper_name(field) ||
	     bcemit_lua54_is_known_lib_wrapper_call(fs, pc, field, pc) ||
	     bcemit_lua54_is_base_global_call(fs, pc, field, pc) ||
	     bcemit_lua54_is_marked_global_notail_call(fs, pc, field, pc) ||
	     bcemit_lua54_is_marked_table_notail_call(fs, pc, field, pc);
    }
    if (op == BC_TGETV && pc > 0) {
      BCIns key = fs->bcbase[pc - 1].ins;
      if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins) &&
	  bc_c(ins) >= fs->nactvar) {
	GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_d(key));
	return bcemit_lua54_is_helper_name(field) ||
	       bcemit_lua54_is_known_lib_wrapper_call(fs, pc, field, pc - 1) ||
	       bcemit_lua54_is_base_global_call(fs, pc, field, pc - 1) ||
	       bcemit_lua54_is_marked_global_notail_call(fs, pc, field,
							 pc - 1) ||
	       bcemit_lua54_is_marked_table_notail_call(fs, pc, field,
							pc - 1);
      }
      return 1;
    }
    break;
  }
  return 0;
}

static int lua54_slot_helper_init_range(FuncState *fs, BCReg slot,
					BCPos startpc, BCPos stoppc)
{
  BCPos pc = stoppc;
  while (pc-- > startpc) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != slot)
      continue;
    if (op == BC_MOV) {
      /* Propagate Lua 5.4 callsite-name preservation through simple aliases:
      ** local it2 = it; return it2()
      */
      return lua54_local_notailcall(fs, bc_d(ins));
    } else if (op == BC_UGET) {
      return lua54_upvalue_notailcall(fs, bc_d(ins));
    } else if (op == BC_CALL || op == BC_CALLM) {
      /* `local it = io.lines(...); return it()` and `local it = pairs(...);
      ** return it(...)` must keep the wrapper frame for Lua 5.4 iterator
      ** argument errors such as "bad argument #2 to 'it'".
      */
      return bcemit_lua54_is_iterator_result(fs, pc);
    } else if (op == BC_TGETS) {
      GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
      return bcemit_lua54_is_helper_name(field) ||
	     bcemit_lua54_is_known_lib_wrapper_call(fs, pc, field, pc) ||
	     bcemit_lua54_is_base_global_call(fs, pc, field, pc) ||
	     bcemit_lua54_is_marked_global_notail_call(fs, pc, field, pc) ||
	     bcemit_lua54_is_marked_table_notail_call(fs, pc, field, pc);
    } else if (op == BC_TGETV && pc > startpc) {
      BCIns key = fs->bcbase[pc - 1].ins;
      if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins) &&
	  bc_c(ins) >= fs->nactvar) {
	GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_d(key));
	return bcemit_lua54_is_helper_name(field) ||
	       bcemit_lua54_is_known_lib_wrapper_call(fs, pc, field, pc - 1) ||
	       bcemit_lua54_is_base_global_call(fs, pc, field, pc - 1) ||
	       bcemit_lua54_is_marked_global_notail_call(fs, pc, field,
							 pc - 1) ||
	       bcemit_lua54_is_marked_table_notail_call(fs, pc, field,
							pc - 1);
      }
      return 1;
    }
    return 0;
  }
  return 0;
}

static int lua54_slot_helper_init(FuncState *fs, BCReg slot, BCPos startpc)
{
  return lua54_slot_helper_init_range(fs, slot, startpc, fs->pc);
}

static int lua54_slot_iterhelper_init_range(FuncState *fs, BCReg slot,
					    BCPos startpc, BCPos stoppc)
{
  BCPos pc = stoppc;
  while (pc-- > startpc) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != slot)
      continue;
    if (op == BC_MOV)
      return lua54_local_iterhelper(fs, bc_d(ins));
    if (op == BC_UGET)
      return lua54_upvalue_iterhelper(fs, bc_d(ins));
    if (op == BC_TGETS) {
      GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
      return bcemit_lua54_is_iterator_helper_call(fs, pc, field, pc);
    } else if (op == BC_TGETV && pc > startpc) {
      BCIns key = fs->bcbase[pc - 1].ins;
      if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins) &&
	  bc_c(ins) >= fs->nactvar) {
	GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_d(key));
	return bcemit_lua54_is_iterator_helper_call(fs, pc, field, pc - 1);
      }
      return 0;
    }
    return 0;
  }
  return 0;
}

static int lua54_slot_iterhelper_init(FuncState *fs, BCReg slot, BCPos startpc)
{
  return lua54_slot_iterhelper_init_range(fs, slot, startpc, fs->pc);
}

static VarIndex lua54_slot_table_source_init_range(FuncState *fs, BCReg slot,
						   BCPos startpc,
						   BCPos stoppc)
{
  BCPos pc = stoppc;
  while (pc-- > startpc) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != slot)
      continue;
    if (op == BC_MOV)
      return lua54_resolve_table_alias(fs,
	lua54_reg_table_source(fs, bc_d(ins), pc));
    if (op == BC_UGET) {
      BCReg uv = bc_d(ins);
      if (uv < fs->nuv)
	return lua54_resolve_table_alias(fs, fs->uvmap[uv]);
    }
    return LJ_MAX_VSTACK;
  }
  return LJ_MAX_VSTACK;
}

static VarIndex lua54_slot_table_source_init(FuncState *fs, BCReg slot,
					     BCPos startpc)
{
  return lua54_slot_table_source_init_range(fs, slot, startpc, fs->pc);
}

static int lua54_slot_table_constructor_init_range(FuncState *fs, BCReg slot,
						   BCPos startpc,
						   BCPos stoppc)
{
  BCPos pc = stoppc;
  while (pc-- > startpc) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != slot)
      continue;
    return op == BC_TNEW || op == BC_TDUP;
  }
  return 0;
}

static int lua54_slot_table_constructor_init(FuncState *fs, BCReg slot,
					     BCPos startpc)
{
  return lua54_slot_table_constructor_init_range(fs, slot, startpc, fs->pc);
}

static VarIndex lua54_store_value_table_source(FuncState *fs, BCReg reg,
					       BCPos storepc);

static int lua54_const_tab_invalidated(BCIns ins, BCReg reg)
{
  BCOp op = bc_op(ins);
  return (op == BC_TSETS || op == BC_TSETV || op == BC_TSETB) &&
	 bc_b(ins) == reg;
}

static GCtab *lua54_reg_const_tab(FuncState *fs, BCReg reg, BCPos pc)
{
  if (fs->pc == 0 || pc <= fs->lasttarget)
    return NULL;
  if (pc >= fs->pc)
    pc = fs->pc - 1;
  while (pc-- > fs->lasttarget) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (lua54_const_tab_invalidated(ins, reg))
      return NULL;
    if (bc_a(ins) != reg)
      continue;
    if (op == BC_TDUP)
      return bcemit_lua54_const_tab_by_slot(fs, bc_d(ins));
    if (op == BC_MOV)
      return lua54_reg_const_tab(fs, bc_d(ins), pc);
    return NULL;
  }
  return NULL;
}

static GCstr *lua54_reg_const_str(FuncState *fs, BCReg reg, BCPos pc);

static GCstr *lua54_reg_const_cat_str(FuncState *fs, BCIns ins, BCPos pc)
{
  GCstr *part[BCMAX_C+1];
  BCReg first = bc_b(ins);
  BCReg last = bc_c(ins);
  BCReg r;
  MSize i = 0, total = 0;
  char *p;
  if (first > last)
    return NULL;
  for (r = first; r <= last; r++) {
    GCstr *s = lua54_reg_const_str(fs, r, pc);
    if (s == NULL)
      return NULL;
    if (total + s->len < total)
      return NULL;
    total += s->len;
    part[i++] = s;
  }
  if (total == 0)
    return lj_parse_keepstr(fs->ls, "", 0);
  lj_buf_reset(&fs->ls->sb);
  p = lj_buf_more(&fs->ls->sb, total);
  for (r = 0; r < i; r++)
    p = lj_buf_wmem(p, strdata(part[r]), part[r]->len);
  fs->ls->sb.w = p;
  return lj_parse_keepstr(fs->ls, fs->ls->sb.b, total);
}

static GCstr *lua54_const_tab_str_field(GCtab *tab, GCstr *field)
{
  cTValue *tv;
  if (tab == NULL || field == NULL)
    return NULL;
  tv = lj_tab_getstr(tab, field);
  return tv != NULL && tvisstr(tv) ? strV(tv) : NULL;
}

static GCstr *lua54_reg_const_str(FuncState *fs, BCReg reg, BCPos pc)
{
  if (fs->pc == 0 || pc <= fs->lasttarget)
    return NULL;
  if (pc >= fs->pc)
    pc = fs->pc - 1;
  while (pc-- > fs->lasttarget) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != reg)
      continue;
    if (op == BC_KSTR)
      return bcemit_lua54_const_str_by_slot(fs, bc_d(ins));
    if (op == BC_MOV)
      return lua54_reg_const_str(fs, bc_d(ins), pc);
    if (op == BC_CAT)
      return lua54_reg_const_cat_str(fs, ins, pc);
    if (op == BC_TGETS) {
      GCtab *tab = lua54_reg_const_tab(fs, bc_b(ins), pc);
      GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
      return lua54_const_tab_str_field(tab, field);
    }
    if (op == BC_TGETV && pc > 0) {
      GCtab *tab = lua54_reg_const_tab(fs, bc_b(ins), pc);
      GCstr *field = NULL;
      BCIns key = fs->bcbase[pc - 1].ins;
      if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins) &&
	  bc_c(ins) >= fs->nactvar)
	field = bcemit_lua54_const_str_by_slot(fs, bc_d(key));
      else
	field = lua54_reg_const_str(fs, bc_c(ins), pc);
      return lua54_const_tab_str_field(tab, field);
    }
    return NULL;
  }
  return NULL;
}

static GCstr *lua54_constructor_store_field(FuncState *fs, BCPos pc,
					    BCReg slot)
{
  BCIns ins = fs->bcbase[pc].ins;
  BCOp op = bc_op(ins);
  if (bc_b(ins) != slot)
    return NULL;
  if (op == BC_TSETS)
    return bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
  if (op == BC_TSETV && pc > 0) {
    BCIns key = fs->bcbase[pc - 1].ins;
    if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins))
      return bcemit_lua54_const_str_by_slot(fs, bc_d(key));
    return lua54_reg_const_str(fs, bc_c(ins), pc);
  }
  return NULL;
}

static void lua54_mark_constructor_nested_field_notailcalls(FuncState *fs,
							    VarIndex table,
							    GCstr *outer,
							    BCReg slot,
							    BCPos startpc,
							    BCPos stoppc,
							    int pending)
{
  BCPos pc;
  if (!lua54_slot_table_constructor_init_range(fs, slot, startpc, stoppc))
    return;
  for (pc = startpc; pc < stoppc; pc++) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    GCstr *field;
    int on;
    if (op != BC_TSETS && op != BC_TSETV)
      continue;
    field = lua54_constructor_store_field(fs, pc, slot);
    on = lua54_local_notailcall(fs, bc_a(ins)) ||
	 lua54_slot_helper_init_range(fs, bc_a(ins), startpc, pc);
    if (on) {
      if (pending)
	lua54_mark_pending_nested_table_notailcall(fs, table, outer, field, 1);
      else
	lua54_mark_nested_table_notailcall(fs, table, outer, field, 1);
    }
  }
}

static void lua54_mark_global_nested_field_notailcalls(FuncState *fs,
						       GCstr *name,
						       GCstr *outer,
						       BCReg slot,
						       BCPos startpc,
						       BCPos stoppc)
{
  BCPos pc;
  if (!lua54_slot_table_constructor_init_range(fs, slot, startpc, stoppc))
    return;
  for (pc = startpc; pc < stoppc; pc++) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    GCstr *field;
    int on;
    if (op != BC_TSETS && op != BC_TSETV)
      continue;
    field = lua54_constructor_store_field(fs, pc, slot);
    on = lua54_local_notailcall(fs, bc_a(ins)) ||
	 lua54_slot_helper_init_range(fs, bc_a(ins), startpc, pc);
    if (on)
      lua54_mark_global_nested_notailcall(fs, name, outer, field, 1);
  }
}

static void lua54_mark_global_constructor_nested_field_notailcalls(
  FuncState *fs, GCstr *name, BCReg slot, BCPos startpc, BCPos stoppc)
{
  BCPos pc;
  if (!lua54_slot_table_constructor_init_range(fs, slot, startpc, stoppc))
    return;
  for (pc = startpc; pc < stoppc; pc++) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    GCstr *outer;
    if (op != BC_TSETS && op != BC_TSETV)
      continue;
    outer = lua54_constructor_store_field(fs, pc, slot);
    if (outer == NULL)
      continue;
    lua54_clear_global_nested_notailcall(fs, name, outer);
    lua54_mark_global_nested_field_notailcalls(fs, name, outer, bc_a(ins),
					       startpc, pc);
  }
}

static void lua54_mark_constructor_field_aliases(FuncState *fs,
						 VarIndex table, BCReg slot,
						 BCPos startpc, BCPos stoppc,
						 int pending)
{
  BCPos pc;
  for (pc = startpc; pc < stoppc; pc++) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    GCstr *field;
    VarIndex source;
    int on;
    if (op != BC_TSETS && op != BC_TSETV)
      continue;
    field = lua54_constructor_store_field(fs, pc, slot);
    on = lua54_local_notailcall(fs, bc_a(ins)) ||
	 lua54_slot_helper_init_range(fs, bc_a(ins), startpc, pc);
    if (field == NULL) {
      if (on) {
	if (pending)
	  lua54_mark_pending_table_notailcall(fs, table, NULL, 1);
	else
	  lua54_mark_table_notailcall(fs, table, NULL, 1);
      }
      continue;
    }
    /* Table constructors emit field stores before the new local is active:
    ** `local holder = { inner = t }`. Scan those stores after the target
    ** local is marked as a table source so the static `inner -> t` relation
    ** can feed later `holder.inner.f = helper` callsite-name propagation.
    */
    if (pending)
      lua54_clear_pending_nested_table_notailcall(fs, table, field);
    else
      lua54_clear_nested_table_notailcall(fs, table, field);
    lua54_mark_constructor_nested_field_notailcalls(fs, table, field,
						    bc_a(ins), startpc, pc,
						    pending);
    source = lua54_store_value_table_source(fs, bc_a(ins), pc);
    if (pending)
      lua54_mark_pending_table_field_alias(fs, table, field, source);
    else
      lua54_mark_table_field_alias(fs, table, field, source);
    if (pending)
      lua54_mark_pending_table_notailcall(fs, table, field, on);
    else
      lua54_mark_table_notailcall(fs, table, field, on);
  }
}

static VarIndex lua54_expr_table_source(FuncState *fs, ExpDesc *e)
{
  VarIndex table = LJ_MAX_VSTACK;
  if (e->k == VLOCAL)
    table = lua54_resolve_table_alias(fs, (VarIndex)e->u.s.aux);
  else if (e->k == VUPVAL && e->u.s.info < fs->nuv)
    table = lua54_resolve_table_alias(fs, fs->uvmap[e->u.s.info]);
  return lua54_is_known_table_source(fs, table) ? table : LJ_MAX_VSTACK;
}

static GCstr *lua54_indexed_const_field(FuncState *fs, ExpDesc *var,
					BCPos lhspc, BCPos rhspc)
{
  if (var->k == VGLOBAL)
    return var->u.sval;
  if (var->k == VINDEXED) {
    int32_t aux = (int32_t)var->u.s.aux;
    if (aux < 0)
      return bcemit_lua54_const_str_by_slot(fs, (BCReg)(~aux));
    if (var->u.s.aux <= BCMAX_C) {
      BCReg key = (BCReg)var->u.s.aux;
      BCPos pc = rhspc;
      if (key < fs->nactvar)
	return lua54_reg_const_str(fs, key, lhspc);
      if (rhspc <= lhspc)
	return NULL;
      /* Fields whose string constant index no longer fits TGETS/TSETS are
      ** emitted as `KSTR key; TGETV/TSETV ... key`. Treat only that static
      ** KSTR shape as a constant field; real dynamic keys must stay dynamic.
      */
      while (pc > lhspc) {
	BCIns ins;
	BCOp op;
	pc--;
	ins = fs->bcbase[pc].ins;
	op = bc_op(ins);
	if (bc_a(ins) != key)
	  continue;
	if (op == BC_KSTR)
	  return bcemit_lua54_const_str_by_slot(fs, bc_d(ins));
	return NULL;
      }
    }
  }
  return NULL;
}

static int lua54_reg_is_env_or_global_table(FuncState *fs, BCReg reg, BCPos pc)
{
  while (pc-- > 0) {
    BCIns ins = fs->bcbase[pc].ins;
    BCOp op = bc_op(ins);
    if (bc_a(ins) != reg)
      continue;
    if (op == BC_UGET)
      return bcemit_lua54_is_env_upvalue_fetch(fs, reg, pc);
    if (op == BC_MOV)
      return lua54_reg_is_env_or_global_table(fs, bc_d(ins), pc);
    if (op == BC_TGETS) {
      GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_c(ins));
      if (bcemit_lua54_streq(field, "_G", 2))
	return lua54_reg_is_env_or_global_table(fs, bc_b(ins), pc);
    } else if (op == BC_TGETV && pc >= 1) {
      BCIns key = fs->bcbase[pc - 1].ins;
      if (bc_op(key) == BC_KSTR && bc_a(key) == bc_c(ins)) {
	GCstr *field = bcemit_lua54_const_str_by_slot(fs, bc_d(key));
	if (bcemit_lua54_streq(field, "_G", 2))
	  return lua54_reg_is_env_or_global_table(fs, bc_b(ins), pc);
      }
    }
    return 0;
  }
  return 0;
}

static int lua54_indexed_global_alias_store(FuncState *fs, ExpDesc *var)
{
  if (var->k == VGLOBAL)
    return 1;
  return var->k == VINDEXED &&
	 lua54_reg_is_env_or_global_table(fs, var->u.s.info, fs->pc - 1);
}

static VarIndex lua54_indexed_table_source(FuncState *fs, ExpDesc *var)
{
  if (var->k != VINDEXED)
    return LJ_MAX_VSTACK;
  return lua54_reg_table_source(fs, var->u.s.info, fs->pc - 1);
}

static BCReg lua54_last_store_value(FuncState *fs)
{
  BCIns ins;
  BCOp op;
  if (fs->pc == 0)
    return NO_REG;
  ins = fs->bcbase[fs->pc - 1].ins;
  op = bc_op(ins);
  if (op == BC_GSET || op == BC_TSETS || op == BC_TSETV || op == BC_TSETB)
    return bc_a(ins);
  return NO_REG;
}

static VarIndex lua54_store_value_table_source(FuncState *fs, BCReg reg,
					       BCPos storepc)
{
  if (reg == NO_REG)
    return LJ_MAX_VSTACK;
  if (reg < fs->nactvar)
    return lua54_reg_table_source(fs, reg, storepc);
  /* Non-local RHS values are often materialized immediately before the store,
  ** for example `holder.alias = base.inner`. Start before the final TSET/GSET
  ** so source tracing sees the TGETS/TGETV that produced the temporary value.
  */
  if (storepc > 0)
    return lua54_reg_table_source(fs, reg, storepc - 1);
  return LJ_MAX_VSTACK;
}

static void lua54_mark_notailcall_locals(LexState *ls, BCReg nvars,
					 BCPos startpc)
{
  FuncState *fs = ls->fs;
  BCReg i;
  for (i = 0; i < nvars; i++) {
    BCReg slot = (BCReg)(fs->nactvar + i);
    VarIndex table = lua54_slot_table_source_init(fs, slot, startpc);
    if (lua54_slot_table_constructor_init(fs, slot, startpc)) {
      ls->vstack[ls->vtop - nvars + i].info |= VSTACK_VAR_TABLE;
      lua54_mark_constructor_field_aliases(fs, fs->varmap[slot], slot,
					   startpc, fs->pc, 1);
    }
    if (lua54_slot_iterhelper_init(fs, slot, startpc))
      ls->vstack[ls->vtop - nvars + i].info |= VSTACK_VAR_ITERHELPER;
    if (lua54_slot_helper_init(fs, slot, startpc))
      ls->vstack[ls->vtop - nvars + i].info |= VSTACK_VAR_NOTAILCALL;
    if (table < LJ_MAX_VSTACK)
      lua54_mark_pending_table_alias(fs, fs->varmap[slot], table);
  }
}

static void lua54_mark_notailcall_store(LexState *ls, ExpDesc *var,
					BCPos lhspc, BCPos startpc)
{
  FuncState *fs = ls->fs;
  if (var->k == VLOCAL) {
    VarIndex table = lua54_slot_table_source_init(fs, var->u.s.info, startpc);
    if (lua54_slot_table_constructor_init(fs, var->u.s.info, startpc)) {
      ls->vstack[var->u.s.aux].info |= VSTACK_VAR_TABLE;
      lua54_mark_constructor_field_aliases(fs, var->u.s.aux,
					   var->u.s.info, startpc, fs->pc, 0);
    }
    if (lua54_slot_iterhelper_init(fs, var->u.s.info, startpc))
      ls->vstack[var->u.s.aux].info |= VSTACK_VAR_ITERHELPER;
    if (lua54_slot_helper_init(fs, var->u.s.info, startpc)) {
      /* Mutable locals can be rebound to coroutine.resume/close after their
      ** declaration. Re-mark the slot here so later tail-position alias calls
      ** keep the source frame and report Lua 5.4 callsite names such as 'f'.
      */
      ls->vstack[var->u.s.aux].info |= VSTACK_VAR_NOTAILCALL;
    }
    if (table < LJ_MAX_VSTACK)
      lua54_mark_table_alias(fs, var->u.s.aux, table);
  } else if (lua54_indexed_global_alias_store(fs, var)) {
    GCstr *field = lua54_indexed_const_field(fs, var, lhspc, startpc);
    BCReg ra = lua54_last_store_value(fs);
    lua54_clear_global_table_notailcall(fs, field, NULL);
    lua54_clear_global_nested_notailcall(fs, field, NULL);
    if (field != NULL && ra != NO_REG) {
      VarIndex table = lua54_store_value_table_source(fs, ra, fs->pc - 1);
      /* Track only globals that this parser pass has just assigned from a
      ** known no-tail helper result. This covers `global_it = io.lines(...)`
      ** without disabling tail calls for arbitrary global Lua functions.
      */
      lua54_mark_global_notailcall(fs, field,
	lua54_local_notailcall(fs, ra) ||
	lua54_slot_helper_init_range(fs, ra, startpc, fs->pc - 1));
      /* A global can also be a static table alias in the same chunk:
      ** `g = t; g.f = math.abs; return t.f(true)` should report field `f`
      ** like Lua 5.4, but unrelated runtime global rebindings remain dynamic
      ** and are not inferred here.
      */
      lua54_mark_global_table_alias(fs, field, table);
      lua54_mark_global_constructor_nested_field_notailcalls(fs, field, ra,
							     lhspc,
							     fs->pc - 1);
    }
  } else if (var->k == VINDEXED) {
    GCstr *field = lua54_indexed_const_field(fs, var, lhspc, startpc);
    VarIndex table = lua54_indexed_table_source(fs, var);
    GCstr *global = lua54_reg_global_table_name(fs, var->u.s.info,
						fs->pc - 1);
    VarIndex pathtable = LJ_MAX_VSTACK;
    GCstr *pathglobal = NULL;
    GCstr *pathfield = NULL;
    int havepath = lua54_reg_table_field_path(fs, var->u.s.info,
					      fs->pc - 1, &pathtable,
					      &pathglobal, &pathfield);
    BCReg ra = lua54_last_store_value(fs);
    int on = ra != NO_REG &&
      (lua54_local_notailcall(fs, ra) ||
       lua54_slot_helper_init_range(fs, ra, startpc, fs->pc - 1));
    if (field != NULL && table < LJ_MAX_VSTACK) {
      VarIndex source = ra != NO_REG ?
	lua54_store_value_table_source(fs, ra, fs->pc - 1) : LJ_MAX_VSTACK;
      lua54_clear_nested_table_notailcall(fs, table, field);
      lua54_mark_table_field_alias(fs, table, field, source);
    }
    if (field != NULL && table < LJ_MAX_VSTACK && ra != NO_REG) {
      /* A field such as `aliases.f = math.abs` needs the same source-callsite
      ** preservation as a local/global alias when it is later tail-called as
      ** `return aliases.f(...)`. Keep this static and table-scoped so an
      ** unrelated table field named `f` still uses normal tail-call codegen.
      */
      lua54_mark_table_notailcall(fs, table, field, on);
      lua54_mark_constructor_nested_field_notailcalls(fs, table, field, ra,
						      lhspc, fs->pc - 1,
						      0);
    } else if (field == NULL && table < LJ_MAX_VSTACK && on) {
      /* A runtime-computed key such as `t[key()] = math.abs` cannot name the
      ** exact field statically. Mark the known table source conservatively so
      ** a later tail-position `return t.somefield(...)` still keeps its source
      ** frame instead of falling back to the callee's library name.
      */
      lua54_mark_table_notailcall(fs, table, NULL, 1);
    }
    if (field != NULL && global != NULL) {
      lua54_mark_global_table_notailcall(fs, global, field, on);
      lua54_clear_global_nested_notailcall(fs, global, field);
      if (ra != NO_REG)
	lua54_mark_global_nested_field_notailcalls(fs, global, field, ra,
						   lhspc, fs->pc - 1);
    } else if (field == NULL && global != NULL && on) {
      lua54_mark_global_table_notailcall(fs, global, NULL, 1);
    }
    if (field != NULL && havepath) {
      if (pathtable < LJ_MAX_VSTACK)
	lua54_mark_nested_table_notailcall(fs, pathtable, pathfield, field,
					   on);
      if (pathglobal != NULL)
	lua54_mark_global_nested_notailcall(fs, pathglobal, pathfield, field,
					    on);
    } else if (field == NULL && havepath && on) {
      if (pathtable < LJ_MAX_VSTACK)
	lua54_mark_nested_table_notailcall(fs, pathtable, pathfield, NULL, 1);
      if (pathglobal != NULL)
	lua54_mark_global_nested_notailcall(fs, pathglobal, pathfield, NULL,
					    1);
    }
  }
}
#endif

/* Define a new local variable. */
static void var_new(LexState *ls, BCReg n, GCstr *name)
{
  FuncState *fs = ls->fs;
  MSize vtop = ls->vtop;
  checklimit(fs, fs->nactvar+n, LJ_MAX_LOCVAR, "local variables");
  if (LJ_UNLIKELY(vtop >= ls->sizevstack)) {
    if (ls->sizevstack >= LJ_MAX_VSTACK)
      lj_lex_error(ls, 0, LJ_ERR_XLIMC, LJ_MAX_VSTACK);
    lj_mem_growvec(ls->L, ls->vstack, ls->sizevstack, LJ_MAX_VSTACK, VarInfo);
  }
  lj_assertFS((uintptr_t)name < VARNAME__MAX ||
	      lj_tab_getstr(fs->kt, name) != NULL,
	      "unanchored variable name");
  /* NOBARRIER: name is anchored in fs->kt and ls->vstack is not a GCobj. */
  setgcref(ls->vstack[vtop].name, obj2gco(name));
  ls->vstack[vtop].info = 0;
  fs->varmap[fs->nactvar+n] = (uint16_t)vtop;
  ls->vtop = vtop+1;
}

#define var_new_lit(ls, n, v) \
  var_new(ls, (n), lj_parse_keepstr(ls, "" v, sizeof(v)-1))

#define var_new_fixed(ls, n, vn) \
  var_new(ls, (n), (GCstr *)(uintptr_t)(vn))

/* Add local variables. */
static void var_add(LexState *ls, BCReg nvars)
{
  FuncState *fs = ls->fs;
  BCReg nactvar = fs->nactvar;
  while (nvars--) {
    VarInfo *v = &var_get(ls, fs, nactvar);
    v->startpc = fs->pc;
    v->slot = nactvar++;
    v->info &= VSTACK_VAR_ATTRMASK;
  }
  fs->nactvar = nactvar;
}

#if LJ_54
static int var_attr_islit(GCstr *s, const char *lit, MSize len)
{
  return s->len == len && memcmp(strdata(s), lit, len) == 0;
}

static void var_new_lua54_envuv(LexState *ls)
{
  FuncState *fs = ls->fs;
  MSize vtop = ls->vtop;
  GCstr *name;
  if (LJ_UNLIKELY(vtop >= ls->sizevstack)) {
    if (ls->sizevstack >= LJ_MAX_VSTACK)
      lj_lex_error(ls, 0, LJ_ERR_XLIMC, LJ_MAX_VSTACK);
    lj_mem_growvec(ls->L, ls->vstack, ls->sizevstack, LJ_MAX_VSTACK, VarInfo);
  }
  name = lj_parse_keepstr(ls, "_ENV", 4);
  /* Main chunks in Lua 5.4 expose _ENV as a real first upvalue, even when
  ** their bytecode has no free-name access. Keep the debug name in vstack,
  ** but advance vbase so it is not emitted as a visible local variable.
  */
  setgcref(ls->vstack[vtop].name, obj2gco(name));
  ls->vstack[vtop].startpc = 0;
  ls->vstack[vtop].endpc = 0;
  ls->vstack[vtop].slot = 0;
  ls->vstack[vtop].info = 0;
  fs->lua54env = 1;
  fs->lua54envvidx = (VarIndex)vtop;
  fs->uvmap[0] = (VarIndex)vtop;
  fs->uvtmp[0] = 0;
  fs->uvnotail[0] = 0;
  fs->uviterhelper[0] = 0;
  fs->nuv = 1;
  ls->vtop = vtop+1;
  fs->vbase = ls->vtop;
}

static int var_attr_parse(LexState *ls, VarIndex vidx)
{
  if (lex_opt(ls, '<')) {
    GCstr *attr = lex_str(ls);
    if (var_attr_islit(attr, "const", 5)) {
      ls->vstack[vidx].info |= VSTACK_VAR_CONST;
    } else if (var_attr_islit(attr, "close", 5)) {
      /* Lua 5.4 to-be-closed variables are also const after initialization;
      ** reuse the existing const assignment checks for locals and upvalues.
      */
      ls->vstack[vidx].info |= VSTACK_VAR_CLOSE | VSTACK_VAR_CONST;
      ls->fs->flags |= PROTO_NOJIT;
    } else {
      lj_lex_error(ls, 0, LJ_ERR_XATTRIB, strdata(attr));
    }
    lex_check(ls, '>');
    return (ls->vstack[vidx].info & VSTACK_VAR_CLOSE) != 0;
  }
  return 0;
}
#endif

/* Remove local variables. */
static void var_remove(LexState *ls, BCReg tolevel)
{
  FuncState *fs = ls->fs;
  while (fs->nactvar > tolevel)
    var_get(ls, fs, --fs->nactvar).endpc = fs->pc;
}

/* Lookup local variable name. */
static BCReg var_lookup_local(FuncState *fs, GCstr *n)
{
  int i;
  for (i = fs->nactvar-1; i >= 0; i--) {
    if (n == strref(var_get(fs->ls, fs, i).name))
      return (BCReg)i;
  }
  return (BCReg)-1;  /* Not found. */
}

/* Lookup or add upvalue index. */
static MSize var_lookup_uv(FuncState *fs, MSize vidx, ExpDesc *e)
{
  MSize i, n = fs->nuv;
  for (i = 0; i < n; i++)
    if (fs->uvmap[i] == vidx) {
#if LJ_54
      if (e->k == VLOCAL &&
	  (fs->ls->vstack[vidx].info & VSTACK_VAR_NOTAILCALL) != 0)
	fs->uvnotail[i] = 1;
      else if (e->k == VUPVAL && fs->prev != NULL &&
	       e->u.s.info < fs->prev->nuv && fs->prev->uvnotail[e->u.s.info])
	fs->uvnotail[i] = 1;
      if (e->k == VLOCAL &&
	  (fs->ls->vstack[vidx].info & VSTACK_VAR_ITERHELPER) != 0)
	fs->uviterhelper[i] = 1;
      else if (e->k == VUPVAL && fs->prev != NULL &&
	       e->u.s.info < fs->prev->nuv &&
	       fs->prev->uviterhelper[e->u.s.info])
	fs->uviterhelper[i] = 1;
#endif
      return i;  /* Already exists. */
    }
  /* Otherwise create a new one. */
  checklimit(fs, fs->nuv, LJ_MAX_UPVAL, "upvalues");
  lj_assertFS(e->k == VLOCAL || e->k == VUPVAL, "bad expr type %d", e->k);
  fs->uvmap[n] = (uint16_t)vidx;
  fs->uvtmp[n] = (uint16_t)(e->k == VLOCAL ? vidx : LJ_MAX_VSTACK+e->u.s.info);
#if LJ_54
  fs->uvnotail[n] = e->k == VLOCAL ?
    ((fs->ls->vstack[vidx].info & VSTACK_VAR_NOTAILCALL) != 0) :
    (fs->prev != NULL && e->u.s.info < fs->prev->nuv &&
     fs->prev->uvnotail[e->u.s.info]);
  fs->uviterhelper[n] = e->k == VLOCAL ?
    ((fs->ls->vstack[vidx].info & VSTACK_VAR_ITERHELPER) != 0) :
    (fs->prev != NULL && e->u.s.info < fs->prev->nuv &&
     fs->prev->uviterhelper[e->u.s.info]);
#endif
  fs->nuv = n+1;
  return n;
}

/* Forward declaration. */
static void fscope_uvmark(FuncState *fs, BCReg level);

/* Recursively lookup variables in enclosing functions. */
static MSize var_lookup_(FuncState *fs, GCstr *name, ExpDesc *e, int first)
{
  if (fs) {
    BCReg reg = var_lookup_local(fs, name);
    if ((int32_t)reg >= 0) {  /* Local in this function? */
      expr_init(e, VLOCAL, reg);
      if (!first)
	fscope_uvmark(fs, reg);  /* Scope now has an upvalue. */
      return (MSize)(e->u.s.aux = (uint32_t)fs->varmap[reg]);
#if LJ_54
    } else if (fs->lua54env && var_attr_islit(name, "_ENV", 4)) {
      /* The main chunk's implicit _ENV has no local register. Expose it as a
      ** normal upvalue so debug.setupvalue/upvaluejoin can replace it with
      ** any Lua value, matching Lua 5.4 instead of LuaJIT's table env.
      */
      expr_init(e, VUPVAL, 0);
      return (MSize)(e->u.s.aux = fs->lua54envvidx);
#endif
    } else {
      MSize vidx = var_lookup_(fs->prev, name, e, 0);  /* Var in outer func? */
      if ((int32_t)vidx >= 0) {  /* Yes, make it an upvalue here. */
	e->u.s.info = (uint8_t)var_lookup_uv(fs, vidx, e);
	e->k = VUPVAL;
	return vidx;
      }
    }
  } else {  /* Not found in any function, must be a global. */
    expr_init(e, VGLOBAL, 0);
    e->u.sval = name;
  }
  return (MSize)-1;  /* Global. */
}

/* Lookup variable name. */
static void var_lookup(LexState *ls, ExpDesc *e)
{
  GCstr *name = lex_str(ls);
  var_lookup_(ls->fs, name, e, 1);
#if LJ_54
  if (e->k == VGLOBAL && !var_attr_islit(name, "_ENV", 4)) {
    FuncState *fs = ls->fs;
    ExpDesc env, key;
    GCstr *envname = lj_parse_keepstr(ls, "_ENV", 4);
    if ((int32_t)var_lookup_(fs, envname, &env, 1) >= 0) {
      /* Lua 5.4 resolves free names through a lexical _ENV when one exists.
      ** The default LuaJIT VGLOBAL path remains used when no local/upvalue
      ** _ENV is in scope, preserving the existing function environment model.
      */
      expr_toanyreg(fs, &env);
      *e = env;
      expr_init(&key, VKSTR, 0);
      key.u.sval = name;
      e->k = VINDEXED;
      {
	BCReg idx = const_str(fs, &key);
	if (idx <= BCMAX_C) {
	  e->u.s.aux = ~idx;
	} else {
	  expr_tonextreg(fs, &key);
	  e->u.s.aux = key.u.s.info;
	}
      }
    }
  }
#endif
}

/* -- Goto an label handling ---------------------------------------------- */

/* Add a new goto or label. */
static MSize gola_new(LexState *ls, GCstr *name, uint8_t info, BCPos pc)
{
  FuncState *fs = ls->fs;
  MSize vtop = ls->vtop;
  if (LJ_UNLIKELY(vtop >= ls->sizevstack)) {
    if (ls->sizevstack >= LJ_MAX_VSTACK)
      lj_lex_error(ls, 0, LJ_ERR_XLIMC, LJ_MAX_VSTACK);
    lj_mem_growvec(ls->L, ls->vstack, ls->sizevstack, LJ_MAX_VSTACK, VarInfo);
  }
  lj_assertFS(name == NAME_BREAK || lj_tab_getstr(fs->kt, name) != NULL,
	      "unanchored label name");
  /* NOBARRIER: name is anchored in fs->kt and ls->vstack is not a GCobj. */
  setgcref(ls->vstack[vtop].name, obj2gco(name));
  ls->vstack[vtop].startpc = pc;
  ls->vstack[vtop].endpc = ls->linenumber;
  ls->vstack[vtop].slot = (uint8_t)fs->nactvar;
  ls->vstack[vtop].info = info;
  ls->vtop = vtop+1;
  return vtop;
}

#define gola_isgoto(v)		((v)->info & VSTACK_GOTO)
#define gola_islabel(v)		((v)->info & VSTACK_LABEL)
#define gola_isgotolabel(v)	((v)->info & (VSTACK_GOTO|VSTACK_LABEL))

/* Patch goto to jump to label. */
static void gola_patch(LexState *ls, VarInfo *vg, VarInfo *vl)
{
  FuncState *fs = ls->fs;
  BCPos pc = vg->startpc;
  setgcrefnull(vg->name);  /* Invalidate pending goto. */
  setbc_a(&fs->bcbase[pc].ins, vl->slot);
  jmp_patch(fs, pc, vl->startpc);
}

/* Patch goto to close upvalues. */
static void gola_close(LexState *ls, VarInfo *vg)
{
  FuncState *fs = ls->fs;
  BCPos pc = vg->startpc;
  BCIns *ip = &fs->bcbase[pc].ins;
  lj_assertFS(gola_isgoto(vg), "expected goto");
  lj_assertFS(bc_op(*ip) == BC_JMP || bc_op(*ip) == BC_UCLO,
	      "bad bytecode op %d", bc_op(*ip));
  setbc_a(ip, vg->slot);
  if (bc_op(*ip) == BC_JMP) {
    BCPos next = jmp_next(fs, pc);
    if (next != NO_JMP) jmp_patch(fs, next, pc);  /* Jump to UCLO. */
    setbc_op(ip, BC_UCLO);  /* Turn into UCLO. */
    setbc_j(ip, NO_JMP);
  }
}

/* Resolve pending forward gotos for label. */
static void gola_resolve(LexState *ls, FuncScope *bl, MSize idx)
{
  VarInfo *vg = ls->vstack + bl->vstart;
  VarInfo *vl = ls->vstack + idx;
  for (; vg < vl; vg++)
    if (gcrefeq(vg->name, vl->name) && gola_isgoto(vg)) {
      if (vg->slot < vl->slot) {
	GCstr *name = strref(var_get(ls, ls->fs, vg->slot).name);
	lj_assertLS((uintptr_t)name >= VARNAME__MAX, "expected goto name");
	ls->linenumber = ls->fs->bcbase[vg->startpc].line;
	lj_assertLS(strref(vg->name) != NAME_BREAK, "unexpected break");
	lj_lex_error(ls, 0, LJ_ERR_XGSCOPE,
		     strdata(strref(vg->name)), strdata(name));
      }
      gola_patch(ls, vg, vl);
    }
}

/* Fixup remaining gotos and labels for scope. */
static void gola_fixup(LexState *ls, FuncScope *bl)
{
  VarInfo *v = ls->vstack + bl->vstart;
  VarInfo *ve = ls->vstack + ls->vtop;
  for (; v < ve; v++) {
    GCstr *name = strref(v->name);
    if (name != NULL) {  /* Only consider remaining valid gotos/labels. */
      if (gola_islabel(v)) {
	VarInfo *vg;
	setgcrefnull(v->name);  /* Invalidate label that goes out of scope. */
	for (vg = v+1; vg < ve; vg++)  /* Resolve pending backward gotos. */
	  if (strref(vg->name) == name && gola_isgoto(vg)) {
	    if ((bl->flags&FSCOPE_UPVAL) && vg->slot > v->slot)
	      gola_close(ls, vg);
	    gola_patch(ls, vg, v);
	  }
      } else if (gola_isgoto(v)) {
	if (bl->prev) {  /* Propagate goto or break to outer scope. */
	  bl->prev->flags |= name == NAME_BREAK ? FSCOPE_BREAK : FSCOPE_GOLA;
	  v->slot = bl->nactvar;
	  if ((bl->flags & FSCOPE_UPVAL))
	    gola_close(ls, v);
	} else {  /* No outer scope: undefined goto label or no loop. */
	  ls->linenumber = ls->fs->bcbase[v->startpc].line;
	  if (name == NAME_BREAK)
#if LJ_54
	    lj_lex_error(ls, 0, LJ_ERR_XBREAK, ls->linenumber);
#else
	    lj_lex_error(ls, 0, LJ_ERR_XBREAK);
#endif
	  else
#if LJ_54
	    lj_lex_error(ls, 0, LJ_ERR_XLUNDEF, strdata(name), ls->linenumber);
#else
	    lj_lex_error(ls, 0, LJ_ERR_XLUNDEF, strdata(name));
#endif
	}
      }
    }
  }
}

#if !LJ_54
/* Find existing label. */
static VarInfo *gola_findlabel(LexState *ls, GCstr *name)
{
  VarInfo *v = ls->vstack + ls->fs->bl->vstart;
  VarInfo *ve = ls->vstack + ls->vtop;
  for (; v < ve; v++)
    if (strref(v->name) == name && gola_islabel(v))
      return v;
  return NULL;
}
#endif

#if LJ_54
/* Find an already-seen label that is visible from nested blocks. */
static VarInfo *gola_findactivelabel(LexState *ls, GCstr *name)
{
  VarInfo *v = ls->vstack + ls->vtop;
  VarInfo *vb = ls->vstack + ls->fs->vbase;
  while (v > vb) {
    GCstr *vname = strref((--v)->name);
    if (vname == name && gola_islabel(v))
      return v;
  }
  return NULL;
}
#endif

/* -- Scope handling ------------------------------------------------------ */

/* Begin a scope. */
static void fscope_begin(FuncState *fs, FuncScope *bl, int flags)
{
  bl->nactvar = (uint8_t)fs->nactvar;
  bl->flags = flags;
  bl->vstart = fs->ls->vtop;
  bl->prev = fs->bl;
  fs->bl = bl;
  lj_assertFS(fs->freereg == fs->nactvar, "bad regalloc");
}

/* End a scope. */
#if LJ_54
static int fscope_hascloseactive_range(FuncState *fs, BCReg fromlevel,
				       BCReg tolevel)
{
  LexState *ls = fs->ls;
  BCReg closevar;
  if (fromlevel > fs->nactvar)
    fromlevel = fs->nactvar;
  for (closevar = fromlevel; closevar > tolevel; ) {
    VarInfo *v = &var_get(ls, fs, --closevar);
    if (v->info & VSTACK_VAR_CLOSE)
      return 1;
  }
  return 0;
}

static int fscope_hascloseactive(FuncState *fs, BCReg tolevel)
{
  return fscope_hascloseactive_range(fs, fs->nactvar, tolevel);
}

static void fscope_closeactive_range(FuncState *fs, BCReg fromlevel,
				     BCReg tolevel)
{
  LexState *ls = fs->ls;
  BCReg closevar;
  BCReg errval, haserr;
  if (fromlevel > fs->nactvar)
    fromlevel = fs->nactvar;
  if (!fscope_hascloseactive_range(fs, fromlevel, tolevel))
    return;
  if (fs->freereg < fs->nactvar)
    fs->freereg = fs->nactvar;
  errval = fs->freereg;
  haserr = (BCReg)(errval + 1);
  bcreg_reserve(fs, 2);
  bcemit_AD(fs, BC_KPRI, errval, 0);
  bcemit_AD(fs, BC_KPRI, haserr, 1);
  for (closevar = fromlevel; closevar > tolevel; ) {
    VarInfo *v = &var_get(ls, fs, --closevar);
    if (v->info & VSTACK_VAR_CLOSE)
      bcemit_lua54_closevalue(fs, closevar, errval, haserr);
  }
  bcemit_lua54_throwcloseerror(fs, errval, haserr);
  fs->freereg = errval;
}

static void fscope_closeactive(FuncState *fs, BCReg tolevel)
{
  fscope_closeactive_range(fs, fs->nactvar, tolevel);
}

static BCReg fscope_breaklevel(FuncState *fs)
{
  FuncScope *bl;
  for (bl = fs->bl; bl; bl = bl->prev)
    if (bl->flags & FSCOPE_LOOP)
      return bl->nactvar;
  return 0;
}

static void gola_closependinggotos(LexState *ls, FuncScope *bl)
{
  FuncState *fs = ls->fs;
  VarInfo *v = ls->vstack + bl->vstart;
  VarInfo *ve = ls->vstack + ls->vtop;
  BCPos skip = NO_JMP;
  for (; v < ve; v++) {
    GCstr *name = strref(v->name);
    if (name != NULL && gola_isgoto(v) &&
	!(v->info & VSTACK_GOTO_CLOSE) &&
	fscope_hascloseactive_range(fs, v->slot, bl->nactvar)) {
      BCPos closepc;
      if (skip == NO_JMP)
	skip = bcemit_jmp(fs);
      closepc = fs->pc;
      jmp_patch(fs, v->startpc, closepc);
      /* A forward goto may be written before later locals in the same block.
      ** Only close variables that were already active at the goto site.
      */
      fscope_closeactive_range(fs, v->slot, bl->nactvar);
      v->startpc = bcemit_jmp(fs);
    }
  }
  if (skip != NO_JMP)
    jmp_tohere(fs, skip);
}
#endif

static void fscope_end(FuncState *fs)
{
  FuncScope *bl = fs->bl;
  LexState *ls = fs->ls;
  fs->bl = bl->prev;
#if LJ_54
  fs->freereg = fs->nactvar;
  if (!(bl->flags & FSCOPE_NOCLOSE))
    fscope_closeactive(fs, bl->nactvar);
  gola_closependinggotos(ls, bl);
#endif
  var_remove(ls, bl->nactvar);
  fs->freereg = fs->nactvar;
  lj_assertFS(bl->nactvar == fs->nactvar, "bad regalloc");
  if ((bl->flags & (FSCOPE_UPVAL|FSCOPE_NOCLOSE)) == FSCOPE_UPVAL)
    bcemit_AJ(fs, BC_UCLO, bl->nactvar, 0);
  if ((bl->flags & FSCOPE_BREAK)) {
    if ((bl->flags & FSCOPE_LOOP)) {
      MSize idx = gola_new(ls, NAME_BREAK, VSTACK_LABEL, fs->pc);
      ls->vtop = idx;  /* Drop break label immediately. */
      gola_resolve(ls, bl, idx);
    } else {  /* Need the fixup step to propagate the breaks. */
      gola_fixup(ls, bl);
      return;
    }
  }
  if ((bl->flags & FSCOPE_GOLA)) {
    gola_fixup(ls, bl);
  }
}

/* Mark scope as having an upvalue. */
static void fscope_uvmark(FuncState *fs, BCReg level)
{
  FuncScope *bl;
  for (bl = fs->bl; bl && bl->nactvar > level; bl = bl->prev)
    ;
  if (bl)
    bl->flags |= FSCOPE_UPVAL;
}

/* -- Function state management ------------------------------------------- */

/* Fixup bytecode for prototype. */
static void fs_fixup_bc(FuncState *fs, GCproto *pt, BCIns *bc, MSize n)
{
  BCInsLine *base = fs->bcbase;
  MSize i;
  BCIns op;
  pt->sizebc = n;
  if (fs->ls->fr2 != LJ_FR2) op = BC_NOT;  /* Mark non-native prototype. */
  else if ((fs->flags & PROTO_VARARG)) op = BC_FUNCV;
  else op = BC_FUNCF;
  bc[0] = BCINS_AD(op, fs->framesize, 0);
  for (i = 1; i < n; i++)
    bc[i] = base[i].ins;
}

/* Fixup upvalues for child prototype, step #2. */
static void fs_fixup_uv2(FuncState *fs, GCproto *pt)
{
  VarInfo *vstack = fs->ls->vstack;
  uint16_t *uv = proto_uv(pt);
  MSize i, n = pt->sizeuv;
  for (i = 0; i < n; i++) {
    VarIndex vidx = uv[i];
    if (vidx >= LJ_MAX_VSTACK)
      uv[i] = vidx - LJ_MAX_VSTACK;
    else if ((vstack[vidx].info & VSTACK_VAR_RW))
      uv[i] = vstack[vidx].slot | PROTO_UV_LOCAL;
    else
      uv[i] = vstack[vidx].slot | PROTO_UV_LOCAL | PROTO_UV_IMMUTABLE;
  }
}

/* Fixup constants for prototype. */
static void fs_fixup_k(FuncState *fs, GCproto *pt, void *kptr)
{
  GCtab *kt;
  TValue *array;
  Node *node;
  MSize i, hmask;
  checklimitgt(fs, fs->nkn, BCMAX_D+1, "constants");
  checklimitgt(fs, fs->nkgc, BCMAX_D+1, "constants");
  setmref(pt->k, kptr);
  pt->sizekn = fs->nkn;
  pt->sizekgc = fs->nkgc;
  kt = fs->kt;
  array = tvref(kt->array);
  for (i = 0; i < kt->asize; i++)
    if (tvhaskslot(&array[i])) {
      TValue *tv = &((TValue *)kptr)[tvkslot(&array[i])];
      if (LJ_DUALNUM)
	setintV(tv, (int32_t)i);
      else
	setnumV(tv, (lua_Number)i);
    }
  node = noderef(kt->node);
  hmask = kt->hmask;
  for (i = 0; i <= hmask; i++) {
    Node *n = &node[i];
    if (tvhaskslot(&n->val)) {
      ptrdiff_t kidx = (ptrdiff_t)tvkslot(&n->val);
      if (tvisint(&n->key)) {
	TValue *tv = &((TValue *)kptr)[kidx];
	setintV(tv, intV(&n->key));
      } else
#if LJ_54 && LJ_DUALNUM
      if (tvistab(&n->key) && (tabV(&n->key)->flags54 & LUA54_KNUM_BOX)) {
	GCtab *box = tabV(&n->key);
	TValue *tv = &((TValue *)kptr)[kidx];
	lj_assertFS(box->asize > 0 &&
		    (tvisnumber(arrayslot(box, 0)) ||
		     tvisi64(arrayslot(box, 0))),
		    "bad Lua 5.4 boxed number constant");
	copyTV(fs->L, tv, arrayslot(box, 0));
	if (tvisi64(tv))
	  lj_gc_objbarrier(fs->L, pt, gcV(tv));
      } else
#endif
      if (tvisnum(&n->key) || tvisi64(&n->key)) {
	TValue *tv = &((TValue *)kptr)[kidx];
	if (tvisi64(&n->key)) {
	  GCobj *o = gcV(&n->key);
	  copyTV(fs->L, tv, &n->key);
	  lj_gc_objbarrier(fs->L, pt, o);
	} else if (LJ_DUALNUM) {
	  int64_t i64;
	  int32_t k;
	  lj_assertFS(!tvismzero(&n->key), "unexpected -0 key");
	  if (lj_num2int_check(numV(&n->key), i64, k))
	    setintV(tv, k);
	  else
	    *tv = n->key;
	} else {
	  *tv = n->key;
	}
      } else {
	GCobj *o = gcV(&n->key);
	setgcref(((GCRef *)kptr)[~kidx], o);
	lj_gc_objbarrier(fs->L, pt, o);
	if (tvisproto(&n->key))
	  fs_fixup_uv2(fs, gco2pt(o));
      }
    }
  }
}

/* Fixup upvalues for prototype, step #1. */
static void fs_fixup_uv1(FuncState *fs, GCproto *pt, uint16_t *uv)
{
  setmref(pt->uv, uv);
  pt->sizeuv = fs->nuv;
  memcpy(uv, fs->uvtmp, fs->nuv*sizeof(VarIndex));
}

#ifndef LUAJIT_DISABLE_DEBUGINFO
/* Prepare lineinfo for prototype. */
static size_t fs_prep_line(FuncState *fs, BCLine numline)
{
  return (fs->pc-1) << (numline < 256 ? 0 : numline < 65536 ? 1 : 2);
}

/* Fixup lineinfo for prototype. */
static void fs_fixup_line(FuncState *fs, GCproto *pt,
			  void *lineinfo, BCLine numline)
{
  BCInsLine *base = fs->bcbase + 1;
  BCLine first = fs->linedefined;
  MSize i = 0, n = fs->pc-1;
  pt->firstline = fs->linedefined;
  pt->numline = numline;
  setmref(pt->lineinfo, lineinfo);
  if (LJ_LIKELY(numline < 256)) {
    uint8_t *li = (uint8_t *)lineinfo;
    do {
      BCLine delta = base[i].line - first;
      lj_assertFS(delta >= 0 && delta < 256, "bad line delta");
      li[i] = (uint8_t)delta;
    } while (++i < n);
  } else if (LJ_LIKELY(numline < 65536)) {
    uint16_t *li = (uint16_t *)lineinfo;
    do {
      BCLine delta = base[i].line - first;
      lj_assertFS(delta >= 0 && delta < 65536, "bad line delta");
      li[i] = (uint16_t)delta;
    } while (++i < n);
  } else {
    uint32_t *li = (uint32_t *)lineinfo;
    do {
      BCLine delta = base[i].line - first;
      lj_assertFS(delta >= 0, "bad line delta");
      li[i] = (uint32_t)delta;
    } while (++i < n);
  }
}

/* Prepare variable info for prototype. */
static size_t fs_prep_var(LexState *ls, FuncState *fs, size_t *ofsvar)
{
  VarInfo *vs =ls->vstack, *ve;
  MSize i, n;
  BCPos lastpc;
  lj_buf_reset(&ls->sb);  /* Copy to temp. string buffer. */
  /* Store upvalue names. */
  for (i = 0, n = fs->nuv; i < n; i++) {
    GCstr *s = strref(vs[fs->uvmap[i]].name);
    MSize len = s->len+1;
    char *p = lj_buf_more(&ls->sb, len);
    p = lj_buf_wmem(p, strdata(s), len);
    ls->sb.w = p;
  }
  *ofsvar = sbuflen(&ls->sb);
  lastpc = 0;
  /* Store local variable names and compressed ranges. */
  for (ve = vs + ls->vtop, vs += fs->vbase; vs < ve; vs++) {
    if (!gola_isgotolabel(vs)) {
      GCstr *s = strref(vs->name);
      BCPos startpc;
      char *p;
      if ((uintptr_t)s < VARNAME__MAX) {
	p = lj_buf_more(&ls->sb, 1 + 2*5);
	*p++ = (char)(uintptr_t)s;
      } else {
	MSize len = s->len+1;
	p = lj_buf_more(&ls->sb, len + 2*5);
	p = lj_buf_wmem(p, strdata(s), len);
      }
      startpc = vs->startpc;
      p = lj_strfmt_wuleb128(p, startpc-lastpc);
      p = lj_strfmt_wuleb128(p, vs->endpc-startpc);
      ls->sb.w = p;
      lastpc = startpc;
    }
  }
  lj_buf_putb(&ls->sb, '\0');  /* Terminator for varinfo. */
  return sbuflen(&ls->sb);
}

/* Fixup variable info for prototype. */
static void fs_fixup_var(LexState *ls, GCproto *pt, uint8_t *p, size_t ofsvar)
{
  setmref(pt->uvinfo, p);
  setmref(pt->varinfo, (char *)p + ofsvar);
  memcpy(p, ls->sb.b, sbuflen(&ls->sb));  /* Copy from temp. buffer. */
}
#else

/* Initialize with empty debug info, if disabled. */
#define fs_prep_line(fs, numline)		(UNUSED(numline), 0)
#define fs_fixup_line(fs, pt, li, numline) \
  pt->firstline = pt->numline = 0, setmref((pt)->lineinfo, NULL)
#define fs_prep_var(ls, fs, ofsvar)		(UNUSED(ofsvar), 0)
#define fs_fixup_var(ls, pt, p, ofsvar) \
  setmref((pt)->uvinfo, NULL), setmref((pt)->varinfo, NULL)

#endif

/* Fixup return instruction for prototype. */
static void fs_fixup_ret(FuncState *fs)
{
  BCPos lastpc = fs->pc;
  if (lastpc <= fs->lasttarget || !bc_isret_or_tail(bc_op(fs->bcbase[lastpc-1].ins))) {
#if LJ_54
    /* The synthetic fall-through return is still inside the function body.
    ** Close root-scope <close> variables before RET0 so pcall/xpcall protect
    ** errors from __close exactly like an explicit Lua 5.4 return.
    */
    fscope_closeactive(fs, 0);
#endif
    if ((fs->bl->flags & FSCOPE_UPVAL))
      bcemit_AJ(fs, BC_UCLO, 0, 0);
    bcemit_AD(fs, BC_RET0, 0, 1);  /* Need final return. */
  }
  fs->bl->flags |= FSCOPE_NOCLOSE;  /* Handled above. */
  fscope_end(fs);
  lj_assertFS(fs->bl == NULL, "bad scope nesting");
  /* May need to fixup returns encoded before first function was created. */
  if (fs->flags & PROTO_FIXUP_RETURN) {
    BCPos pc;
    for (pc = 1; pc < lastpc; pc++) {
      BCIns ins = fs->bcbase[pc].ins;
      BCPos offset;
      switch (bc_op(ins)) {
      case BC_CALLMT: case BC_CALLT:
      case BC_RETM: case BC_RET: case BC_RET0: case BC_RET1:
	offset = bcemit_INS(fs, ins);  /* Copy original instruction. */
	fs->bcbase[offset].line = fs->bcbase[pc].line;
	offset = offset-(pc+1)+BCBIAS_J;
	if (offset > BCMAX_D)
	  err_syntax(fs->ls, LJ_ERR_XFIXUP);
	/* Replace with UCLO plus branch. */
	fs->bcbase[pc].ins = BCINS_AD(BC_UCLO, 0, offset);
	break;
      case BC_FNEW:
	return;  /* We're done. */
      default:
	break;
      }
    }
  }
}

/* Finish a FuncState and return the new prototype. */
static GCproto *fs_finish(LexState *ls, BCLine line)
{
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  BCLine numline = line - fs->linedefined;
  size_t sizept, ofsk, ofsuv, ofsli, ofsdbg, ofsvar;
  GCproto *pt;

  /* Apply final fixups. */
  fs_fixup_ret(fs);

  /* Calculate total size of prototype including all colocated arrays. */
  sizept = sizeof(GCproto) + fs->pc*sizeof(BCIns) + fs->nkgc*sizeof(GCRef);
  sizept = (sizept + sizeof(TValue)-1) & ~(sizeof(TValue)-1);
  ofsk = sizept; sizept += fs->nkn*sizeof(TValue);
  ofsuv = sizept; sizept += ((fs->nuv+1)&~1)*2;
  ofsli = sizept; sizept += fs_prep_line(fs, numline);
  ofsdbg = sizept; sizept += fs_prep_var(ls, fs, &ofsvar);

  /* Allocate prototype and initialize its fields. */
  pt = (GCproto *)lj_mem_newgco(L, (MSize)sizept);
  pt->gct = ~LJ_TPROTO;
  pt->sizept = (MSize)sizept;
  pt->trace = 0;
  pt->flags = (uint8_t)(fs->flags & ~(PROTO_HAS_RETURN|PROTO_FIXUP_RETURN));
  pt->numparams = fs->numparams;
  pt->framesize = fs->framesize;
  setgcref(pt->chunkname, obj2gco(ls->chunkname));

  /* Close potentially uninitialized gap between bc and kgc. */
  *(uint32_t *)((char *)pt + ofsk - sizeof(GCRef)*(fs->nkgc+1)) = 0;
  fs_fixup_bc(fs, pt, (BCIns *)((char *)pt + sizeof(GCproto)), fs->pc);
  fs_fixup_k(fs, pt, (void *)((char *)pt + ofsk));
  fs_fixup_uv1(fs, pt, (uint16_t *)((char *)pt + ofsuv));
  fs_fixup_line(fs, pt, (void *)((char *)pt + ofsli), numline);
  fs_fixup_var(ls, pt, (uint8_t *)((char *)pt + ofsdbg), ofsvar);

  lj_vmevent_send(G(L), BC,
    setprotoV(V, V->top++, pt);
  );

#if LJ_54 && LJ_DUALNUM
  L->top -= 2;  /* Pop int64 anchors and table of constants. */
#else
  L->top--;  /* Pop table of constants. */
#endif
  ls->vtop = fs->vbase;  /* Reset variable stack. */
  ls->fs = fs->prev;
  lj_assertL(ls->fs != NULL || ls->tok == TK_eof, "bad parser state");
  return pt;
}

/* Initialize a new FuncState. */
static void fs_init(LexState *ls, FuncState *fs)
{
  lua_State *L = ls->L;
  fs->prev = ls->fs; ls->fs = fs;  /* Append to list. */
  fs->ls = ls;
  fs->vbase = ls->vtop;
  fs->L = L;
  fs->pc = 0;
  fs->lasttarget = 0;
  fs->jpc = NO_JMP;
  fs->freereg = 0;
  fs->nkgc = 0;
  fs->nkn = 0;
  fs->nactvar = 0;
  fs->nuv = 0;
  fs->bl = NULL;
  fs->flags = 0;
  fs->framesize = 1;  /* Minimum frame size. */
#if LJ_54
  fs->lua54env = 0;
  fs->lua54envvidx = 0;
  fs->ngnotail = 0;
  fs->ntfnotail = 0;
  fs->ngtalias = 0;
  fs->ntfalias = 0;
  fs->ngtfnotail = 0;
  fs->nntfnotail = 0;
  fs->ngntfnotail = 0;
  fs->ntalias = 0;
  memset(fs->uvnotail, 0, sizeof(fs->uvnotail));
  memset(fs->uviterhelper, 0, sizeof(fs->uviterhelper));
  memset(fs->gnotail, 0, sizeof(fs->gnotail));
  memset(fs->tfnotail, 0, sizeof(fs->tfnotail));
  memset(fs->gtalias, 0, sizeof(fs->gtalias));
  memset(fs->tfalias, 0, sizeof(fs->tfalias));
  memset(fs->gtfnotail, 0, sizeof(fs->gtfnotail));
  memset(fs->ntfpathnotail, 0, sizeof(fs->ntfpathnotail));
  memset(fs->gntfnotail, 0, sizeof(fs->gntfnotail));
  memset(fs->talias, 0, sizeof(fs->talias));
#endif
  fs->kt = lj_tab_new(L, 0, 0);
  /* Anchor table of constants in stack to avoid being collected. */
  settabV(L, L->top, fs->kt);
  incr_top(L);
#if LJ_54 && LJ_DUALNUM
  fs->k64anchor = lj_tab_new(L, 0, 0);
  settabV(L, L->top, fs->k64anchor);
  incr_top(L);
#endif
}

/* -- Expressions --------------------------------------------------------- */

/* Forward declaration. */
static void expr(LexState *ls, ExpDesc *v);

/* Return string expression. */
static void expr_str(LexState *ls, ExpDesc *e)
{
  expr_init(e, VKSTR, 0);
  e->u.sval = lex_str(ls);
}

/* Return index expression. */
static void expr_index(FuncState *fs, ExpDesc *t, ExpDesc *e)
{
  /* Already called: expr_toval(fs, e). */
  t->k = VINDEXED;
  if (expr_isnumk(e)) {
#if LJ_DUALNUM
    if (tvisint(expr_numtv(e))) {
      int32_t k = intV(expr_numtv(e));
      if (checku8(k)) {
	t->u.s.aux = BCMAX_C+1+(uint32_t)k;  /* 256..511: const byte key */
	return;
      }
    }
#else
    int64_t i64;
    int32_t k;
    if (lj_num2int_cond(expr_numberV(e), i64, k, checku8((int32_t)i64))) {
      t->u.s.aux = BCMAX_C+1+(uint32_t)k;  /* 256..511: const byte key */
      return;
    }
#endif
  } else if (expr_isstrk(e)) {
    BCReg idx = const_str(fs, e);
    if (idx <= BCMAX_C) {
      t->u.s.aux = ~idx;  /* -256..-1: const string key */
      return;
    }
  }
  t->u.s.aux = expr_toanyreg(fs, e);  /* 0..255: register */
}

/* Parse index expression with named field. */
static void expr_field(LexState *ls, ExpDesc *v)
{
  FuncState *fs = ls->fs;
  ExpDesc key;
  expr_toanyreg(fs, v);
  lj_lex_next(ls);  /* Skip dot or colon. */
  expr_str(ls, &key);
  expr_index(fs, v, &key);
}

/* Parse index expression with brackets. */
static void expr_bracket(LexState *ls, ExpDesc *v)
{
  lj_lex_next(ls);  /* Skip '['. */
  expr(ls, v);
  expr_toval(ls->fs, v);
  lex_check(ls, ']');
}

/* Get value of constant expression. */
static void expr_kvalue(FuncState *fs, TValue *v, ExpDesc *e)
{
  UNUSED(fs);
  if (e->k <= VKTRUE) {
    setpriV(v, ~(uint32_t)e->k);
  } else if (e->k == VKSTR) {
    setgcVraw(v, obj2gco(e->u.sval), LJ_TSTR);
  } else {
    lj_assertFS(tvisnumber(expr_numtv(e)), "bad number constant");
    *v = *expr_numtv(e);
  }
}

/* Parse table constructor expression. */
static void expr_table(LexState *ls, ExpDesc *e)
{
  FuncState *fs = ls->fs;
  BCLine line = ls->linenumber;
  GCtab *t = NULL;
  int vcall = 0, needarr = 0;
  uint32_t narr = 1;  /* First array index. */
  uint32_t nhash = 0;  /* Number of hash entries. */
  BCReg freg = fs->freereg;
  BCPos pc = bcemit_AD(fs, BC_TNEW, freg, 0);
  expr_init(e, VNONRELOC, freg);
  bcreg_reserve(fs, 1);
  freg++;
  lex_check(ls, '{');
  while (ls->tok != '}') {
    ExpDesc key, val;
    vcall = 0;
    if (ls->tok == '[') {
      expr_bracket(ls, &key);  /* Already calls expr_toval. */
      if (!expr_isk(&key)) expr_index(fs, e, &key);
      if (expr_isnumk(&key) && expr_numiszero(&key)) needarr = 1; else nhash++;
      lex_check(ls, '=');
    } else if ((ls->tok == TK_name || (!LJ_52 && ls->tok == TK_goto)) &&
	       lj_lex_lookahead(ls) == '=') {
      expr_str(ls, &key);
      lex_check(ls, '=');
      nhash++;
    } else {
      expr_init(&key, VKNUM, 0);
      setintV(&key.u.nval, (int)narr);
      narr++;
      needarr = vcall = 1;
    }
    expr(ls, &val);
    if (expr_isk(&key) && key.k != VKNIL &&
	(key.k == VKSTR || expr_isk_nojump(&val))) {
      TValue k, *v;
      if (!t) {  /* Create template table on demand. */
	BCReg kidx;
	t = lj_tab_new(fs->L, needarr ? narr : 0, hsize2hbits(nhash));
	kidx = const_gc(fs, obj2gco(t), LJ_TTAB);
	fs->bcbase[pc].ins = BCINS_AD(BC_TDUP, freg-1, kidx);
      }
      vcall = 0;
      expr_kvalue(fs, &k, &key);
      v = lj_tab_set(fs->L, t, &k);
      lj_gc_anybarriert(fs->L, t);
      if (expr_isk_nojump(&val)) {  /* Add const key/value to template table. */
	expr_kvalue(fs, v, &val);
	/* Mark nil value with table value itself to preserve the key. */
	if (key.k == VKSTR && tvisnil(v)) settabV(fs->L, v, t);
      } else {  /* Preserve the key for the following non-const store.  */
	settabV(fs->L, v, t);
	goto nonconst;
      }
    } else {
    nonconst:
      if (val.k != VCALL) { expr_toanyreg(fs, &val); vcall = 0; }
      if (expr_isk(&key)) expr_index(fs, e, &key);
      bcemit_store(fs, e, &val);
    }
    fs->freereg = freg;
    if (!lex_opt(ls, ',') && !lex_opt(ls, ';')) break;
  }
  lex_match(ls, '}', '{', line);
  if (vcall) {
    BCInsLine *ilp = &fs->bcbase[fs->pc-1];
    ExpDesc en;
#if LJ_54
    BCReg clear_callbase, clear_calln;
#endif
    lj_assertFS(bc_a(ilp->ins) == freg &&
		bc_op(ilp->ins) == (narr > 256 ? BC_TSETV : BC_TSETB),
		"bad CALL code generation");
    expr_init(&en, VKNUM, 0);
    en.u.nval.u32.lo = narr-1;
    en.u.nval.u32.hi = 0x43300000;  /* Biased integer to avoid denormals. */
    if (narr > 256) { fs->pc--; ilp--; }
#if LJ_54
    /* TSETM copies call results into the table, but the callee/gap/argument
    ** registers can still hold finalizable values. Clear them before the next
    ** allocation-triggered GC observes these dead temporaries.
    */
    clear_callbase = bc_a(ilp[-1].ins);
    clear_calln = bc_op(ilp[-1].ins) == BC_CALL ?
      (BCReg)(bc_c(ilp[-1].ins) + LJ_FR2) :
      (BCReg)(fs->freereg - clear_callbase);
#endif
    ilp->ins = BCINS_AD(BC_TSETM, freg, const_num(fs, &en));
    setbc_b(&ilp[-1].ins, 0);
#if LJ_54
    if (clear_calln != 0)
      bcemit_nil(fs, clear_callbase, clear_calln);
#endif
  }
  if (pc == fs->pc-1) {  /* Make expr relocable if possible. */
    e->u.s.info = pc;
    fs->freereg--;
    e->k = VRELOCABLE;
  } else {
    e->k = VNONRELOC;  /* May have been changed by expr_index. */
  }
  if (!t) {  /* Construct TNEW RD: hhhhhaaaaaaaaaaa. */
    BCIns *ip = &fs->bcbase[pc].ins;
    if (!needarr) narr = 0;
    else if (narr < 3) narr = 3;
    else if (narr > 0x7ff) narr = 0x7ff;
    setbc_d(ip, narr|(hsize2hbits(nhash)<<11));
  } else {
    if (needarr && t->asize < narr)
      lj_tab_reasize(fs->L, t, narr-1);
    lj_gc_check(fs->L);
  }
}

/* Parse function parameters. */
static BCReg parse_params(LexState *ls, int needself)
{
  FuncState *fs = ls->fs;
  BCReg nparams = 0;
  lex_check(ls, '(');
  if (needself)
    var_new_lit(ls, nparams++, "self");
  if (ls->tok != ')') {
    do {
      if (ls->tok == TK_name || (!LJ_52 && ls->tok == TK_goto)) {
	var_new(ls, nparams++, lex_str(ls));
      } else if (ls->tok == TK_dots) {
	lj_lex_next(ls);
	fs->flags |= PROTO_VARARG;
	break;
      } else {
	err_syntax(ls, LJ_ERR_XPARAM);
      }
    } while (lex_opt(ls, ','));
  }
  var_add(ls, nparams);
  lj_assertFS(fs->nactvar == nparams, "bad regalloc");
  bcreg_reserve(fs, nparams);
  lex_check(ls, ')');
  return nparams;
}

/* Forward declaration. */
static void parse_chunk(LexState *ls);

/* Parse body of a function. */
static void parse_body(LexState *ls, ExpDesc *e, int needself, BCLine line)
{
  FuncState fs, *pfs = ls->fs;
  FuncScope bl;
  GCproto *pt;
  ptrdiff_t oldbase = pfs->bcbase - ls->bcstack;
  fs_init(ls, &fs);
  fscope_begin(&fs, &bl, 0);
  fs.linedefined = line;
  fs.numparams = (uint8_t)parse_params(ls, needself);
  fs.bcbase = pfs->bcbase + pfs->pc;
  fs.bclim = pfs->bclim - pfs->pc;
  bcemit_AD(&fs, BC_FUNCF, 0, 0);  /* Placeholder. */
  parse_chunk(ls);
  if (ls->tok != TK_end) lex_match(ls, TK_end, TK_function, line);
  pt = fs_finish(ls, (ls->lastline = ls->linenumber));
  pfs->bcbase = ls->bcstack + oldbase;  /* May have been reallocated. */
  pfs->bclim = (BCPos)(ls->sizebcstack - oldbase);
  /* Store new prototype in the constant array of the parent. */
  expr_init(e, VRELOCABLE,
	    bcemit_AD(pfs, BC_FNEW, 0, const_gc(pfs, obj2gco(pt), LJ_TPROTO)));
#if LJ_HASFFI
  pfs->flags |= (fs.flags & PROTO_FFI);
#endif
  if (!(pfs->flags & PROTO_CHILD)) {
    if (pfs->flags & PROTO_HAS_RETURN)
      pfs->flags |= PROTO_FIXUP_RETURN;
    pfs->flags |= PROTO_CHILD;
  }
  lj_lex_next(ls);
}

/* Parse expression list. Last expression is left open. */
static BCReg expr_list(LexState *ls, ExpDesc *v)
{
  BCReg n = 1;
  expr(ls, v);
  while (lex_opt(ls, ',')) {
    expr_tonextreg(ls->fs, v);
    expr(ls, v);
    n++;
  }
  return n;
}

/* Parse function argument list. */
static void parse_args(LexState *ls, ExpDesc *e)
{
  FuncState *fs = ls->fs;
  ExpDesc args;
  BCIns ins;
  BCReg base;
  BCLine line = ls->linenumber;
  if (ls->tok == '(') {
#if !LJ_52
    if (line != ls->lastline)
      err_syntax(ls, LJ_ERR_XAMBIG);
#endif
    lj_lex_next(ls);
    if (ls->tok == ')') {  /* f(). */
      args.k = VVOID;
    } else {
      expr_list(ls, &args);
      if (args.k == VCALL)  /* f(a, b, g()) or f(a, b, ...). */
	setbc_b(bcptr(fs, &args), 0);  /* Pass on multiple results. */
    }
    lex_match(ls, ')', '(', line);
  } else if (ls->tok == '{') {
    expr_table(ls, &args);
  } else if (ls->tok == TK_string) {
    expr_init(&args, VKSTR, 0);
    args.u.sval = strV(&ls->tokval);
    lj_lex_next(ls);
  } else {
    err_syntax(ls, LJ_ERR_XFUNARG);
    return;  /* Silence compiler. */
  }
  lj_assertFS(e->k == VNONRELOC, "bad expr type %d", e->k);
  base = e->u.s.info;  /* Base register for call. */
  if (args.k == VCALL) {
    ins = BCINS_ABC(BC_CALLM, base, 2, args.u.s.aux - base - 1 - ls->fr2);
  } else {
    if (args.k != VVOID)
      expr_tonextreg(fs, &args);
    ins = BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - ls->fr2);
  }
  expr_init(e, VCALL, bcemit_INS(fs, ins));
  e->u.s.aux = base;
  fs->bcbase[fs->pc - 1].line = line;
  fs->freereg = base+1;  /* Leave one result by default. */
}

/* Parse primary expression. */
static void expr_primary(LexState *ls, ExpDesc *v)
{
  FuncState *fs = ls->fs;
  /* Parse prefix expression. */
  if (ls->tok == '(') {
    BCLine line = ls->linenumber;
    lj_lex_next(ls);
    expr(ls, v);
    lex_match(ls, ')', '(', line);
    expr_discharge(ls->fs, v);
  } else if (ls->tok == TK_name || (!LJ_52 && ls->tok == TK_goto)) {
    var_lookup(ls, v);
  } else {
    err_syntax(ls, LJ_ERR_XSYMBOL);
  }
  for (;;) {  /* Parse multiple expression suffixes. */
    if (ls->tok == '.') {
      expr_field(ls, v);
    } else if (ls->tok == '[') {
      ExpDesc key;
      expr_toanyreg(fs, v);
      expr_bracket(ls, &key);
      expr_index(fs, v, &key);
    } else if (ls->tok == ':') {
      ExpDesc key;
      lj_lex_next(ls);
      expr_str(ls, &key);
      bcemit_method(fs, v, &key);
      parse_args(ls, v);
    } else if (ls->tok == '(' || ls->tok == TK_string || ls->tok == '{') {
      expr_tonextreg(fs, v);
      if (ls->fr2) bcreg_reserve(fs, 1);
      parse_args(ls, v);
    } else {
      break;
    }
  }
}

/* Parse simple expression. */
static void expr_simple(LexState *ls, ExpDesc *v)
{
  switch (ls->tok) {
  case TK_number:
    expr_init(v, (LJ_HASFFI && tviscdata(&ls->tokval)) ? VKCDATA : VKNUM, 0);
    copyTV(ls->L, &v->u.nval, &ls->tokval);
#if LJ_54 && LJ_DUALNUM
    const_anchor_i64(ls->fs, &v->u.nval);
#endif
    break;
  case TK_string:
    expr_init(v, VKSTR, 0);
    v->u.sval = strV(&ls->tokval);
    break;
  case TK_nil:
    expr_init(v, VKNIL, 0);
    break;
  case TK_true:
    expr_init(v, VKTRUE, 0);
    break;
  case TK_false:
    expr_init(v, VKFALSE, 0);
    break;
  case TK_dots: {  /* Vararg. */
    FuncState *fs = ls->fs;
    BCReg base;
    checkcond(ls, fs->flags & PROTO_VARARG, LJ_ERR_XDOTS);
    bcreg_reserve(fs, 1);
    base = fs->freereg-1;
    expr_init(v, VCALL, bcemit_ABC(fs, BC_VARG, base, 2, fs->numparams));
    v->u.s.aux = base;
    break;
  }
  case '{':  /* Table constructor. */
    expr_table(ls, v);
    return;
  case TK_function:
    lj_lex_next(ls);
    parse_body(ls, v, 0, ls->linenumber);
    return;
  default:
    expr_primary(ls, v);
    return;
  }
  lj_lex_next(ls);
}

/* Manage syntactic levels to avoid blowing up the stack. */
static void synlevel_begin(LexState *ls)
{
  if (++ls->level >= LJ_MAX_XLEVEL)
    lj_lex_error(ls, 0, LJ_ERR_XLEVELS);
}

#define synlevel_end(ls)	((ls)->level--)

/* Convert token to binary operator. */
static BinOpr token2binop(LexToken tok)
{
  switch (tok) {
  case '+':	return OPR_ADD;
  case '-':	return OPR_SUB;
  case '*':	return OPR_MUL;
  case '/':	return OPR_DIV;
  case '%':	return OPR_MOD;
  case '^':	return OPR_POW;
#if LJ_54
  case TK_idiv:	return OPR_IDIV;
  case '&':	return OPR_BAND;
  case '|':	return OPR_BOR;
  case '~':	return OPR_BXOR;
  case TK_shl:	return OPR_SHL;
  case TK_shr:	return OPR_SHR;
#endif
  case TK_concat: return OPR_CONCAT;
  case TK_ne:	return OPR_NE;
  case TK_eq:	return OPR_EQ;
  case '<':	return OPR_LT;
  case TK_le:	return OPR_LE;
  case '>':	return OPR_GT;
  case TK_ge:	return OPR_GE;
  case TK_and:	return OPR_AND;
  case TK_or:	return OPR_OR;
  default:	return OPR_NOBINOPR;
  }
}

/* Priorities for each binary operator. ORDER OPR. */
static const struct {
  uint8_t left;		/* Left priority. */
  uint8_t right;	/* Right priority. */
} priority[] = {
  {9,9}, {9,9}, {10,10}, {10,10}, {10,10},	/* ADD SUB MUL DIV MOD */
  {13,12},					/* POW (right associative) */
  {10,10}, {6,6}, {4,4}, {5,5}, {7,7}, {7,7},	/* IDIV BAND BOR BXOR SHL SHR */
  {8,7},					/* CONCAT (right associative) */
  {3,3}, {3,3},				/* EQ NE */
  {3,3}, {3,3}, {3,3}, {3,3},		/* LT GE GT LE */
  {2,2}, {1,1}				/* AND OR */
};

#define UNARY_PRIORITY		11  /* Priority for unary operators. */

/* Forward declaration. */
static BinOpr expr_binop(LexState *ls, ExpDesc *v, uint32_t limit);

/* Parse unary expression. */
static void expr_unop(LexState *ls, ExpDesc *v)
{
  BCOp op;
#if LJ_54
  BCLine opline = ls->linenumber;
  int bitnot = 0;
#endif
  if (ls->tok == TK_not) {
    op = BC_NOT;
  } else if (ls->tok == '-') {
    op = BC_UNM;
  } else if (ls->tok == '#') {
    op = BC_LEN;
#if LJ_54
  } else if (ls->tok == '~') {
    op = BC_NOT;  /* Placeholder; bitnot path returns before bcemit_unop(). */
    bitnot = 1;
#endif
  } else {
    expr_simple(ls, v);
    return;
  }
  lj_lex_next(ls);
  expr_binop(ls, v, UNARY_PRIORITY);
#if LJ_54
  if (bitnot) {
    FuncState *fs = ls->fs;
    BCReg rd = expr_toanyreg(fs, v);
    if (v->k == VNONRELOC && v->u.s.info >= fs->nactvar) fs->freereg--;
    v->u.s.info = bcemit_AD(fs, BC_BNOT, 0, rd);
    v->k = VRELOCABLE;
    /* Lua 5.4 reports unary operator runtime errors at the operator line, not
    ** at a later operand line in split expressions.
    */
    if (v->u.s.info < fs->pc)
      fs->bcbase[v->u.s.info].line = opline;
    return;
  }
#endif
  bcemit_unop(ls->fs, op, v);
#if LJ_54
  if (v->k == VRELOCABLE && v->u.s.info < ls->fs->pc)
    ls->fs->bcbase[v->u.s.info].line = opline;
#endif
}

/* Parse binary expressions with priority higher than the limit. */
static BinOpr expr_binop(LexState *ls, ExpDesc *v, uint32_t limit)
{
  BinOpr op;
  synlevel_begin(ls);
  expr_unop(ls, v);
  op = token2binop(ls->tok);
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    ExpDesc v2;
    BinOpr nextop;
#if LJ_54
    BCLine opline = ls->linenumber;
#endif
    lj_lex_next(ls);
    bcemit_binop_left(ls->fs, op, v);
    /* Parse binary expression with higher priority. */
    nextop = expr_binop(ls, &v2, priority[op].right);
    bcemit_binop(ls->fs, op, v, &v2);
#if LJ_54
    if (v->k == VRELOCABLE && v->u.s.info < ls->fs->pc)
      ls->fs->bcbase[v->u.s.info].line = opline;
#endif
    op = nextop;
  }
  synlevel_end(ls);
  return op;  /* Return unconsumed binary operator (if any). */
}

/* Parse expression. */
static void expr(LexState *ls, ExpDesc *v)
{
  expr_binop(ls, v, 0);  /* Priority 0: parse whole expression. */
}

/* Parse conditional expression. */
static BCPos expr_cond(LexState *ls)
{
  ExpDesc v;
  expr(ls, &v);
  if (v.k == VKNIL) v.k = VKFALSE;
  bcemit_branch_t(ls->fs, &v);
  return v.f;
}

#if LJ_54
/* Lua 5.4 exposes a line hook and an active line for a multiline 'then'.
** LuaJIT emits the condition branch before consuming 'then', so the final
** branch pair inherits the condition line unless it is retagged here.
*/
static void bcemit_lua54_thenline(FuncState *fs, BCPos condexit, BCLine thenline)
{
  if (condexit == NO_JMP)
    return;
  fs->bcbase[condexit].line = thenline;
  if (condexit > 0) {
    BCOp op = bc_op(fs->bcbase[condexit-1].ins);
    if (op == BC_IST || op == BC_ISF || op == BC_ISTC || op == BC_ISFC)
      fs->bcbase[condexit-1].line = thenline;
  }
}
#endif

/* -- Assignments --------------------------------------------------------- */

/* List of LHS variables. */
typedef struct LHSVarList {
  ExpDesc v;			/* LHS variable. */
  BCPos startpc;		/* First bytecode emitted for this LHS. */
  BCLine line;			/* Line where the LHS variable starts. */
#if LJ_54
  BCPos rhspc;			/* First bytecode emitted for the RHS list. */
#endif
  struct LHSVarList *prev;	/* Link to previous LHS variable. */
} LHSVarList;

#if LJ_54
static void lua54_set_lhs_rhspc(LHSVarList *lh, BCPos rhspc)
{
  for (; lh != NULL; lh = lh->prev)
    lh->rhspc = rhspc;
}

static BCLine lua54_exprline(FuncState *fs, const ExpDesc *e)
{
  if ((e->k == VRELOCABLE || e->k == VCALL || e->k == VJMP) &&
      e->u.s.info < fs->pc)
    return fs->bcbase[e->u.s.info].line;
  return 0;
}

static void lua54_retag_lhs_env(FuncState *fs, BCPos start, BCPos stop,
				BCLine line)
{
  BCPos pc;
  for (pc = start; pc < stop; pc++) {
    BCOp op = bc_op(fs->bcbase[pc].ins);
    if ((op == BC_UGET || op == BC_GGET) && fs->bcbase[pc].line < line)
      fs->bcbase[pc].line = line;
  }
}
#endif

/* Eliminate write-after-read hazards for local variable assignment. */
static void assign_hazard(LexState *ls, LHSVarList *lh, const ExpDesc *v)
{
  FuncState *fs = ls->fs;
  BCReg reg = v->u.s.info;  /* Check against this variable. */
  BCReg tmp = fs->freereg;  /* Rename to this temp. register (if needed). */
  int hazard = 0;
  for (; lh; lh = lh->prev) {
    if (lh->v.k == VINDEXED) {
      if (lh->v.u.s.info == reg) {  /* t[i], t = 1, 2 */
	hazard = 1;
	lh->v.u.s.info = tmp;
      }
      if (lh->v.u.s.aux == reg) {  /* t[i], i = 1, 2 */
	hazard = 1;
	lh->v.u.s.aux = tmp;
      }
    }
  }
  if (hazard) {
    bcemit_AD(fs, BC_MOV, tmp, reg);  /* Rename conflicting variable. */
    bcreg_reserve(fs, 1);
  }
}

/* Adjust LHS/RHS of an assignment. */
static void assign_adjust(LexState *ls, BCReg nvars, BCReg nexps, ExpDesc *e)
{
  FuncState *fs = ls->fs;
  int32_t extra = (int32_t)nvars - (int32_t)nexps;
  if (e->k == VCALL) {
    extra++;  /* Compensate for the VCALL itself. */
    if (extra < 0) extra = 0;
    setbc_b(bcptr(fs, e), extra+1);  /* Fixup call results. */
    if (extra > 1) bcreg_reserve(fs, (BCReg)extra-1);
  } else {
    if (e->k != VVOID)
      expr_tonextreg(fs, e);  /* Close last expression. */
    if (extra > 0) {  /* Leftover LHS are set to nil. */
      BCReg reg = fs->freereg;
      bcreg_reserve(fs, (BCReg)extra);
      bcemit_nil(fs, reg, (BCReg)extra);
    }
  }
  if (nexps > nvars)
    ls->fs->freereg -= nexps - nvars;  /* Drop leftover regs. */
}

/* Recursively parse assignment statement. */
static void parse_assignment(LexState *ls, LHSVarList *lh, BCReg nvars)
{
  ExpDesc e;
  checkcond(ls, VLOCAL <= lh->v.k && lh->v.k <= VINDEXED, LJ_ERR_XSYNTAX);
  if (lex_opt(ls, ',')) {  /* Collect LHS list and recurse upwards. */
    LHSVarList vl;
    vl.startpc = ls->fs->pc;
    vl.line = ls->linenumber;
#if LJ_54
    vl.rhspc = 0;
#endif
    vl.prev = lh;
    expr_primary(ls, &vl.v);
    if (vl.v.k == VLOCAL)
      assign_hazard(ls, lh, &vl.v);
    checklimit(ls->fs, ls->level + nvars, LJ_MAX_XLEVEL, "variable names");
    parse_assignment(ls, &vl, nvars+1);
  } else {  /* Parse RHS. */
    BCReg nexps;
#if LJ_54
    BCPos rhspc;
#endif
    lex_check(ls, '=');
#if LJ_54
    rhspc = ls->fs->pc;
    lua54_set_lhs_rhspc(lh, rhspc);
#endif
    nexps = expr_list(ls, &e);
#if LJ_54
    if (nvars == 1) {
      BCLine rhsline = lua54_exprline(ls->fs, &e);
      if (rhsline)
	lua54_retag_lhs_env(ls->fs, lh->startpc, rhspc, rhsline);
    }
#endif
    if (nexps == nvars) {
#if LJ_54
      BCReg clear_callbase = NO_REG, clear_calln = 0;
      BCReg clear_rhs = NO_REG;
      BCReg clear_tempbase = NO_REG, clear_tempn = 0;
#endif
      if (e.k == VCALL) {
	BCIns call = *bcptr(ls->fs, &e);
	if (bc_op(call) == BC_VARG) {  /* Vararg assignment. */
	  ls->fs->freereg--;
	  e.k = VRELOCABLE;
	} else {  /* Multiple call results. */
#if LJ_54
	  /* Lua 5.4 table finalizers make stale call temporaries observable.
	  ** After storing the call result into its LHS, clear the dead callee/gap
	  ** and argument slots so allocation-triggered GC does not retain them.
	  */
	  clear_callbase = e.u.s.aux;
	  clear_calln = bc_op(call) == BC_CALL ?
	    (BCReg)(bc_c(call) + LJ_FR2) :
	    (BCReg)(ls->fs->freereg - clear_callbase);
#endif
	  e.u.s.info = e.u.s.aux;  /* Base of call is not relocatable. */
	  e.k = VNONRELOC;
	}
      }
#if LJ_54
      if (e.k == VNONRELOC && e.u.s.info >= ls->fs->nactvar)
	clear_rhs = e.u.s.info;
      if (nvars == 1 && ls->fs->freereg > ls->fs->nactvar) {
	BCReg cleartop = ls->fs->freereg;
	if (clear_calln != 0 && clear_callbase + clear_calln > cleartop)
	  cleartop = (BCReg)(clear_callbase + clear_calln);
	/* Indexed assignments can leave LHS key temporaries and RHS table
	** constructor results in dead slots until the next call. Clear all
	** non-local temporaries before a following full GC can see them.
	*/
	clear_tempbase = ls->fs->nactvar;
	clear_tempn = (BCReg)(cleartop - clear_tempbase);
      }
#endif
#if LJ_54
      lua54_check_const_assign_line(ls->fs, &lh->v, lh->line);
#endif
      bcemit_store(ls->fs, &lh->v, &e);
#if LJ_54
      lua54_mark_notailcall_store(ls, &lh->v, lh->startpc, rhspc);
      if (clear_tempn != 0)
	bcemit_nil(ls->fs, clear_tempbase, clear_tempn);
      else if (clear_calln != 0 && clear_callbase >= ls->fs->nactvar)
	bcemit_nil(ls->fs, clear_callbase, clear_calln);
      else if (clear_rhs != NO_REG)
	bcemit_nil(ls->fs, clear_rhs, 1);
#endif
      return;
    }
    assign_adjust(ls, nvars, nexps, &e);
  }
  /* Assign RHS to LHS and recurse downwards. */
  expr_init(&e, VNONRELOC, ls->fs->freereg-1);
#if LJ_54
  lua54_check_const_assign_line(ls->fs, &lh->v, lh->line);
#endif
  bcemit_store(ls->fs, &lh->v, &e);
#if LJ_54
  lua54_mark_notailcall_store(ls, &lh->v, lh->startpc, lh->rhspc);
#endif
}

/* Parse call statement or assignment. */
static void parse_call_assign(LexState *ls)
{
  FuncState *fs = ls->fs;
  LHSVarList vl;
  vl.startpc = fs->pc;
  vl.line = ls->linenumber;
#if LJ_54
  vl.rhspc = 0;
#endif
  expr_primary(ls, &vl.v);
  if (vl.v.k == VCALL) {  /* Function call statement. */
    setbc_b(bcptr(fs, &vl.v), 1);  /* No results. */
#if LJ_54
    /* Lua 5.4 finalizers make discarded temporaries observable: a call
    ** statement such as setmetatable({}, {__gc=...}) must not keep the
    ** returned table or argument temporaries alive in dead call slots.
    */
    {
      BCIns call = *bcptr(fs, &vl.v);
      BCReg nclear = bc_op(call) == BC_CALL ? (BCReg)(bc_c(call) + LJ_FR2) :
		     (BCReg)(fs->freereg - vl.v.u.s.aux);
      if (nclear > 0)
	bcemit_nil(fs, vl.v.u.s.aux, nclear);
    }
#endif
  } else {  /* Start of an assignment. */
    vl.prev = NULL;
#if LJ_54
    /* Lua 5.4 diagnoses a bare prefix expression statement as a generic
    ** syntax error. Letting it fall into assignment parsing leaks LuaJIT's old
    ** "'=' expected" surface for chunks like load("a") or standalone "-e a".
    */
    if (ls->tok != '=' && ls->tok != ',')
      err_syntax(ls, LJ_ERR_XSYNTAX);
#endif
    parse_assignment(ls, &vl, 1);
  }
}

/* Parse 'local' statement. */
static void parse_local(LexState *ls)
{
  if (lex_opt(ls, TK_function)) {  /* Local function declaration. */
    ExpDesc v, b;
    FuncState *fs = ls->fs;
    var_new(ls, 0, lex_str(ls));
    expr_init(&v, VLOCAL, fs->freereg);
    v.u.s.aux = fs->varmap[fs->freereg];
    bcreg_reserve(fs, 1);
    var_add(ls, 1);
    parse_body(ls, &b, 0, ls->linenumber);
    /* bcemit_store(fs, &v, &b) without setting VSTACK_VAR_RW. */
    expr_free(fs, &b);
    expr_toreg(fs, &b, v.u.s.info);
    /* The upvalue is in scope, but the local is only valid after the store. */
    var_get(ls, fs, fs->nactvar - 1).startpc = fs->pc;
  } else {  /* Local variable declaration. */
    ExpDesc e;
    BCReg nexps, nvars = 0;
#if LJ_54
    FuncState *fs = ls->fs;
    BCPos initpc;
    VarIndex tablealias = LJ_MAX_VSTACK;
    int nclose = 0;
    BCReg closeidx = 0;
    GCstr *closename = NULL;
#endif
    do {  /* Collect LHS. */
      GCstr *varname = lex_str(ls);
      var_new(ls, nvars++, varname);
#if LJ_54
      if (var_attr_parse(ls, (VarIndex)(ls->vtop - 1))) {
	nclose++;
	closeidx = (BCReg)(nvars - 1);
	closename = varname;
      }
      if (nclose > 1)
	err_syntax(ls, LJ_ERR_XCLOSE);
#endif
    } while (lex_opt(ls, ','));
#if LJ_54
    initpc = fs->pc;
#endif
    if (lex_opt(ls, '=')) {  /* Optional RHS. */
      nexps = expr_list(ls, &e);
    } else {  /* Or implicitly set to nil. */
      e.k = VVOID;
      nexps = 0;
    }
#if LJ_54
    if (nvars == 1 && nexps == 1)
      tablealias = lua54_expr_table_source(fs, &e);
#endif
    assign_adjust(ls, nvars, nexps, &e);
#if LJ_54
    lua54_mark_notailcall_locals(ls, nvars, initpc);
    if (tablealias < LJ_MAX_VSTACK)
      lua54_mark_pending_table_alias(fs, fs->varmap[fs->nactvar],
				     tablealias);
#endif
    var_add(ls, nvars);
#if LJ_54
    if (nclose)
      bcemit_lua54_checkclose(fs, (BCReg)(fs->nactvar - nvars + closeidx),
			      closename);
#endif
  }
}

/* Parse 'function' statement. */
static void parse_func(LexState *ls, BCLine line)
{
  FuncState *fs;
  ExpDesc v, b;
  int needself = 0;
  lj_lex_next(ls);  /* Skip 'function'. */
  /* Parse function name. */
  var_lookup(ls, &v);
  while (ls->tok == '.')  /* Multiple dot-separated fields. */
    expr_field(ls, &v);
  if (ls->tok == ':') {  /* Optional colon to signify method call. */
    needself = 1;
    expr_field(ls, &v);
  }
  parse_body(ls, &b, needself, line);
  fs = ls->fs;
#if LJ_54
  lua54_check_const_assign_line(fs, &v, line);
#endif
  bcemit_store(fs, &v, &b);
  fs->bcbase[fs->pc - 1].line = line;  /* Set line for the store. */
}

/* -- Control transfer statements ----------------------------------------- */

/* Check for end of block. */
static int parse_isend(LexToken tok)
{
  switch (tok) {
  case TK_else: case TK_elseif: case TK_end: case TK_until: case TK_eof:
    return 1;
  default:
    return 0;
  }
}

/* Parse 'return' statement. */
static void parse_return(LexState *ls)
{
  BCIns ins;
  FuncState *fs = ls->fs;
#if LJ_54
  int closeactive = fscope_hascloseactive(fs, 0);
#if LJ_TARGET_X64
  int closefixed = !closeactive;
#else
  int closefixed = 1;
#endif
#endif
  lj_lex_next(ls);  /* Skip 'return'. */
  fs->flags |= PROTO_HAS_RETURN;
  if (parse_isend(ls->tok) || ls->tok == ';') {  /* Bare return. */
    ins = BCINS_AD(BC_RET0, 0, 1);
  } else {  /* Return with one or more values. */
    ExpDesc e;  /* Receives the _last_ expression in the list. */
#if LJ_54
#if !LJ_TARGET_X64
    if (closeactive) {
      BCReg base = fs->freereg;
      bcemit_lua54_returnpack_begin(fs, base);
      expr_list(ls, &e);
      bcemit_lua54_returnpack_end(fs, base, &e);
      fscope_closeactive(fs, 0);
      ins = bcemit_lua54_returnunpack(fs, base);
      closefixed = 0;
    } else
#endif
#endif
    {
      BCReg nret = expr_list(ls, &e);
      if (nret == 1) {  /* Return one result. */
	if (e.k == VCALL) {  /* Check for tail call. */
#ifdef LUAJIT_DISABLE_TAILCALL
	  goto notailcall;
#else
	  BCIns *ip = bcptr(fs, &e);
	  /* It doesn't pay off to add BC_VARGT just for 'return ...'. */
	  if (bc_op(*ip) == BC_VARG) goto notailcall;
#if LJ_54
	  if (closeactive)
	    goto notailcall;
	  if (bcemit_lua54_is_private_helper_call(fs, &e))
	    goto notailcall;
	  if (lua54_callbase_notailcall(fs, &e))
	    goto notailcall;
#endif
	  fs->pc--;
	  ins = BCINS_AD(bc_op(*ip)-BC_CALL+BC_CALLT, bc_a(*ip), bc_c(*ip));
#if LJ_54
	  closefixed = 0;
#endif
#endif
	} else {  /* Can return the result from any register. */
	  ins = BCINS_AD(BC_RET1, expr_toanyreg(fs, &e), 2);
	}
      } else {
	if (e.k == VCALL) {  /* Append all results from a call. */
	notailcall:
	  setbc_b(bcptr(fs, &e), 0);
	  ins = BCINS_AD(BC_RETM, fs->nactvar, e.u.s.aux - fs->nactvar);
#if LJ_54
	  closefixed = 0;
#endif
	} else {
	  expr_tonextreg(fs, &e);  /* Force contiguous registers. */
	  ins = BCINS_AD(BC_RET, fs->nactvar, nret+1);
	}
      }
    }
  }
#if LJ_54
  if (closefixed)
    fscope_closeactive(fs, 0);
#endif
  if (fs->flags & PROTO_CHILD)
    bcemit_AJ(fs, BC_UCLO, 0, 0);  /* May need to close upvalues first. */
  bcemit_INS(fs, ins);
}

/* Parse 'break' statement. */
static void parse_break(LexState *ls)
{
  uint8_t info = VSTACK_GOTO;
  ls->fs->bl->flags |= FSCOPE_BREAK;
#if LJ_54
  /* A break leaves the innermost loop immediately; close locals that belong
  ** to the loop body or nested blocks before the jump is emitted.
  */
  fscope_closeactive(ls->fs, fscope_breaklevel(ls->fs));
  info |= VSTACK_GOTO_CLOSE;
#endif
  gola_new(ls, NAME_BREAK, info, bcemit_jmp(ls->fs));
}

/* Parse 'goto' statement. */
static void parse_goto(LexState *ls)
{
  FuncState *fs = ls->fs;
  GCstr *name = lex_str(ls);
  VarInfo *vl;
  uint8_t info = VSTACK_GOTO;
#if LJ_54
  vl = gola_findactivelabel(ls, name);
#else
  vl = gola_findlabel(ls, name);
#endif
  if (vl) {  /* Treat backwards goto to a visible label like a loop. */
#if LJ_54
    /* The target level is known for an already-seen label. Close only locals
    ** that are alive above that label before the backward jump.
    */
    if (fscope_hascloseactive(fs, vl->slot))
      fscope_closeactive(fs, vl->slot);
    info |= VSTACK_GOTO_CLOSE;
#endif
    bcemit_AJ(fs, BC_LOOP, vl->slot, -1);  /* No BC range check. */
  }
  fs->bl->flags |= FSCOPE_GOLA;
  gola_new(ls, name, info, bcemit_jmp(fs));
}

/* Parse label. */
static void parse_label(LexState *ls)
{
  FuncState *fs = ls->fs;
  GCstr *name;
  MSize idx;
  fs->lasttarget = fs->pc;
  fs->bl->flags |= FSCOPE_GOLA;
  lj_lex_next(ls);  /* Skip '::'. */
  name = lex_str(ls);
#if LJ_54
  {
  VarInfo *vl = gola_findactivelabel(ls, name);
  if (vl)
    lj_lex_error(ls, 0, LJ_ERR_XLDUP, strdata(name), vl->endpc);
  }
#else
  if (gola_findlabel(ls, name))
    lj_lex_error(ls, 0, LJ_ERR_XLDUP, strdata(name));
#endif
  idx = gola_new(ls, name, VSTACK_LABEL, fs->pc);
  lex_check(ls, TK_label);
  /* Recursively parse trailing statements: labels and ';' (Lua 5.2 only). */
  for (;;) {
    if (ls->tok == TK_label) {
      synlevel_begin(ls);
      parse_label(ls);
      synlevel_end(ls);
    } else if (LJ_52 && ls->tok == ';') {
      lj_lex_next(ls);
    } else {
      break;
    }
  }
  /* Trailing label is considered to be outside of scope. */
  if (parse_isend(ls->tok) && ls->tok != TK_until)
    ls->vstack[idx].slot = fs->bl->nactvar;
  gola_resolve(ls, fs->bl, idx);
}

/* -- Blocks, loops and conditional statements ---------------------------- */

/* Parse a block. */
static void parse_block(LexState *ls)
{
  FuncState *fs = ls->fs;
  FuncScope bl;
  fscope_begin(fs, &bl, 0);
  parse_chunk(ls);
  fscope_end(fs);
}

/* Parse 'while' statement. */
static void parse_while(LexState *ls, BCLine line)
{
  FuncState *fs = ls->fs;
  BCPos start, loop, condexit;
  FuncScope bl;
  lj_lex_next(ls);  /* Skip 'while'. */
  start = fs->lasttarget = fs->pc;
  condexit = expr_cond(ls);
  fscope_begin(fs, &bl, FSCOPE_LOOP);
  lex_check(ls, TK_do);
  loop = bcemit_AD(fs, BC_LOOP, fs->nactvar, 0);
  parse_block(ls);
  jmp_patch(fs, bcemit_jmp(fs), start);
  lex_match(ls, TK_end, TK_while, line);
  fscope_end(fs);
  jmp_tohere(fs, condexit);
  jmp_patchins(fs, loop, fs->pc);
}

/* Parse 'repeat' statement. */
static void parse_repeat(LexState *ls, BCLine line)
{
  FuncState *fs = ls->fs;
  BCPos loop = fs->lasttarget = fs->pc;
  BCPos condexit;
  FuncScope bl1, bl2;
  fscope_begin(fs, &bl1, FSCOPE_LOOP);  /* Breakable loop scope. */
  fscope_begin(fs, &bl2, 0);  /* Inner scope. */
  lj_lex_next(ls);  /* Skip 'repeat'. */
  loop = bcemit_AD(fs, BC_LOOP, fs->nactvar, 0);
#if LJ_54
  /* Lua 5.4 line hooks enter a repeat loop at the first body statement, not
  ** at the 'repeat' keyword line carried by LuaJIT's synthetic LOOP bytecode.
  */
  fs->bcbase[loop].line = ls->linenumber;
#endif
  parse_chunk(ls);
  lex_match(ls, TK_until, TK_repeat, line);
  condexit = expr_cond(ls);  /* Parse condition (still inside inner scope). */
  if (!(bl2.flags & FSCOPE_UPVAL)) {  /* No upvalues? Just end inner scope. */
    fscope_end(fs);
  } else {  /* Otherwise generate: cond: UCLO+JMP out, !cond: UCLO+JMP loop. */
    parse_break(ls);  /* Break from loop and close upvalues. */
    jmp_tohere(fs, condexit);
    fscope_end(fs);  /* End inner scope and close upvalues. */
    condexit = bcemit_jmp(fs);
  }
  jmp_patch(fs, condexit, loop);  /* Jump backwards if !cond. */
  jmp_patchins(fs, loop, fs->pc);
  fscope_end(fs);  /* End loop scope. */
}

/* Parse numeric 'for'. */
static void parse_for_num(LexState *ls, GCstr *varname, BCLine line)
{
  FuncState *fs = ls->fs;
  BCReg base = fs->freereg;
  FuncScope bl;
  BCPos loop, loopend;
  ExpDesc start, stop, step;
#if LJ_54 && LJ_TARGET_ARM64
  int start_i32, stop_i32, skip_forstep;
#endif
  /* Hidden control variables. */
  var_new_fixed(ls, FORL_IDX, VARNAME_FOR_IDX);
  var_new_fixed(ls, FORL_STOP, VARNAME_FOR_STOP);
  var_new_fixed(ls, FORL_STEP, VARNAME_FOR_STEP);
  /* Visible copy of index variable. */
  var_new(ls, FORL_EXT, varname);
  lex_check(ls, '=');
  expr(ls, &start);
#if LJ_54 && LJ_TARGET_ARM64
  start_i32 = expr_lua54_i32k_nojump(&start, 0);
#endif
  expr_tonextreg(fs, &start);
  lex_check(ls, ',');
  expr(ls, &stop);
#if LJ_54 && LJ_TARGET_ARM64
  stop_i32 = expr_lua54_i32k_nojump(&stop, 0);
#endif
  expr_tonextreg(fs, &stop);
  if (lex_opt(ls, ',')) {
    expr(ls, &step);
#if LJ_54 && LJ_TARGET_ARM64
    skip_forstep = start_i32 && stop_i32 &&
		   expr_lua54_i32k_nojump(&step, 1);
#endif
    expr_tonextreg(fs, &step);
  } else {
#if LJ_54 && LJ_TARGET_ARM64
    skip_forstep = start_i32 && stop_i32;
#endif
    bcemit_AD(fs, BC_KSHORT, fs->freereg, 1);  /* Default step is 1. */
    bcreg_reserve(fs, 1);
  }
#if LJ_54
#if LJ_TARGET_ARM64
  if (!skip_forstep)
#endif
    bcemit_lua54_forstep(fs, (BCReg)(base + FORL_STEP));
#endif
  var_add(ls, 3);  /* Hidden control variables. */
  lex_check(ls, TK_do);
  loop = bcemit_AJ(fs, BC_FORI, base, NO_JMP);
  fscope_begin(fs, &bl, 0);  /* Scope for visible variables. */
  var_add(ls, 1);
  bcreg_reserve(fs, 1);
  parse_block(ls);
  fscope_end(fs);
  /* Perform loop inversion. Loop control instructions are at the end. */
  loopend = bcemit_AJ(fs, BC_FORL, base, NO_JMP);
  fs->bcbase[loopend].line = line;  /* Fix line for control ins. */
  jmp_patchins(fs, loopend, loop+1);
  jmp_patchins(fs, loop, fs->pc);
}

/* Try to predict whether the iterator is next() and specialize the bytecode.
** Detecting next() and pairs() by name is simplistic, but quite effective.
** The interpreter backs off if the check for the closure fails at runtime.
*/
static int predict_next(LexState *ls, FuncState *fs, BCPos pc)
{
  BCIns ins = fs->bcbase[pc].ins;
  GCstr *name;
  cTValue *o;
  switch (bc_op(ins)) {
  case BC_MOV:
    if (bc_d(ins) >= fs->nactvar) return 0;
    name = gco2str(gcref(var_get(ls, fs, bc_d(ins)).name));
    break;
  case BC_UGET:
    name = gco2str(gcref(ls->vstack[fs->uvmap[bc_d(ins)]].name));
    break;
  case BC_GGET:
    /* There's no inverse index (yet), so lookup the strings. */
    o = lj_tab_getstr(fs->kt, lj_str_newlit(ls->L, "pairs"));
    if (o && tvhaskslot(o) && tvkslot(o) == bc_d(ins))
      return 1;
    o = lj_tab_getstr(fs->kt, lj_str_newlit(ls->L, "next"));
    if (o && tvhaskslot(o) && tvkslot(o) == bc_d(ins))
      return 1;
    return 0;
  default:
    return 0;
  }
  return (name->len == 5 && !strcmp(strdata(name), "pairs")) ||
	 (name->len == 4 && !strcmp(strdata(name), "next"));
}

/* Parse 'for' iterator. */
static void parse_for_iter(LexState *ls, GCstr *indexname)
{
  FuncState *fs = ls->fs;
  ExpDesc e;
  BCReg nvars = 0;
#if LJ_54
  BCReg nctrl = 4;
#else
  BCReg nctrl = 3;
#endif
  BCLine line;
  BCReg base = fs->freereg + nctrl;
  BCPos loop, loopend, exprpc = fs->pc;
  FuncScope bl;
  int isnext;
  /* Hidden control variables. */
#if LJ_54
  var_new_fixed(ls, nvars++, VARNAME_FOR_STATE);
  ls->vstack[ls->vtop-1].info |= VSTACK_VAR_CLOSE;
#endif
  var_new_fixed(ls, nvars++, VARNAME_FOR_GEN);
  var_new_fixed(ls, nvars++, VARNAME_FOR_STATE);
  var_new_fixed(ls, nvars++, VARNAME_FOR_CTL);
  /* Visible variables returned from iterator. */
  var_new(ls, nvars++, indexname);
  while (lex_opt(ls, ','))
    var_new(ls, nvars++, lex_str(ls));
  lex_check(ls, TK_in);
  line = ls->linenumber;
  assign_adjust(ls, nctrl, expr_list(ls, &e), &e);
#if LJ_54
  {
    BCReg ctrlbase = (BCReg)(fs->freereg - nctrl);
    BCReg tmp = fs->freereg;
    /* Lua 5.4 stores the 4th generic-for expression as a closing value, but
    ** LuaJIT bytecode still expects generator/state/control right before the
    ** visible loop variables. Rotate: gen,state,ctl,close -> close,gen,state,ctl.
    */
    bcreg_reserve(fs, 1);
    bcemit_AD(fs, BC_MOV, tmp, (BCReg)(ctrlbase + 3));
    bcemit_AD(fs, BC_MOV, (BCReg)(ctrlbase + 3), (BCReg)(ctrlbase + 2));
    bcemit_AD(fs, BC_MOV, (BCReg)(ctrlbase + 2), (BCReg)(ctrlbase + 1));
    bcemit_AD(fs, BC_MOV, (BCReg)(ctrlbase + 1), ctrlbase);
    bcemit_AD(fs, BC_MOV, ctrlbase, tmp);
    fs->freereg = (BCReg)(ctrlbase + nctrl);
  }
#endif
  /* The iterator needs another 3 [4] slots (func [pc] | state ctl). */
  bcreg_bump(fs, 3+ls->fr2);
  isnext = (nvars <= nctrl + 2 && fs->pc > exprpc &&
	    predict_next(ls, fs, exprpc));
  var_add(ls, nctrl);  /* Hidden control variables. */
#if LJ_54
  bcemit_lua54_checkclose(fs, (BCReg)(fs->nactvar - nctrl),
			  lj_parse_keepstr(ls, "(for state)", 11));
#endif
  lex_check(ls, TK_do);
  loop = bcemit_AJ(fs, isnext ? BC_ISNEXT : BC_JMP, base, NO_JMP);
  fscope_begin(fs, &bl, 0);  /* Scope for visible variables. */
  var_add(ls, nvars-nctrl);
  bcreg_reserve(fs, nvars-nctrl);
  parse_block(ls);
  fscope_end(fs);
  /* Perform loop inversion. Loop control instructions are at the end. */
  jmp_patchins(fs, loop, fs->pc);
  bcemit_ABC(fs, isnext ? BC_ITERN : BC_ITERC, base, nvars-nctrl+1, 2+1);
  loopend = bcemit_AJ(fs, BC_ITERL, base, NO_JMP);
  fs->bcbase[loopend-1].line = line;  /* Fix line for control ins. */
  fs->bcbase[loopend].line = line;
  jmp_patchins(fs, loopend, loop+1);
}

/* Parse 'for' statement. */
static void parse_for(LexState *ls, BCLine line)
{
  FuncState *fs = ls->fs;
  GCstr *varname;
  FuncScope bl;
#if LJ_54
  int isiter = 0;
  BCLine closeline = 0;
#endif
  fscope_begin(fs, &bl, FSCOPE_LOOP);
  lj_lex_next(ls);  /* Skip 'for'. */
  varname = lex_str(ls);  /* Get first variable name. */
  if (ls->tok == '=')
    parse_for_num(ls, varname, line);
  else if (ls->tok == ',' || ls->tok == TK_in) {
#if LJ_54
    isiter = 1;
#endif
    parse_for_iter(ls, varname);
  } else
    err_syntax(ls, LJ_ERR_XFOR);
#if LJ_54
  closeline = ls->lastline;
#endif
  lex_match(ls, TK_end, TK_for, line);
#if LJ_54
  if (isiter) {
    BCLine endline = ls->linenumber;
    /* The hidden generic-for closing value is compiler plumbing. Tag its close
    ** helper with the last visible body/control line so line hooks do not see
    ** an extra synthetic line after 'end' (official db.lua line trace).
    */
    ls->linenumber = closeline;
    fscope_end(fs);  /* Resolve break list. */
    ls->linenumber = endline;
  } else {
    fscope_end(fs);  /* Resolve break list. */
  }
#else
  fscope_end(fs);  /* Resolve break list. */
#endif
}

/* Parse condition and 'then' block. */
static BCPos parse_then(LexState *ls)
{
  BCPos condexit;
#if LJ_54
  BCLine condline;
#endif
  lj_lex_next(ls);  /* Skip 'if' or 'elseif'. */
#if LJ_54
  condline = ls->linenumber;
#endif
  condexit = expr_cond(ls);
#if LJ_54
  if (ls->tok == TK_then && ls->linenumber > condline)
    bcemit_lua54_thenline(ls->fs, condexit, ls->linenumber);
#endif
  lex_check(ls, TK_then);
  parse_block(ls);
  return condexit;
}

/* Parse 'if' statement. */
static void parse_if(LexState *ls, BCLine line)
{
  FuncState *fs = ls->fs;
  BCPos flist;
  BCPos escapelist = NO_JMP;
  flist = parse_then(ls);
  while (ls->tok == TK_elseif) {  /* Parse multiple 'elseif' blocks. */
    jmp_append(fs, &escapelist, bcemit_jmp(fs));
    jmp_tohere(fs, flist);
    flist = parse_then(ls);
  }
  if (ls->tok == TK_else) {  /* Parse optional 'else' block. */
    jmp_append(fs, &escapelist, bcemit_jmp(fs));
    jmp_tohere(fs, flist);
    lj_lex_next(ls);  /* Skip 'else'. */
    parse_block(ls);
  } else {
    jmp_append(fs, &escapelist, flist);
  }
  jmp_tohere(fs, escapelist);
  lex_match(ls, TK_end, TK_if, line);
}

/* -- Parse statements ---------------------------------------------------- */

/* Parse a statement. Returns 1 if it must be the last one in a chunk. */
static int parse_stmt(LexState *ls)
{
  BCLine line = ls->linenumber;
  switch (ls->tok) {
  case TK_if:
    parse_if(ls, line);
    break;
  case TK_while:
    parse_while(ls, line);
    break;
  case TK_do:
    lj_lex_next(ls);
    parse_block(ls);
    lex_match(ls, TK_end, TK_do, line);
    break;
  case TK_for:
    parse_for(ls, line);
    break;
  case TK_repeat:
    parse_repeat(ls, line);
    break;
  case TK_function:
    parse_func(ls, line);
    break;
  case TK_local:
    lj_lex_next(ls);
    parse_local(ls);
    break;
  case TK_return:
    parse_return(ls);
    return 1;  /* Must be last. */
  case TK_break:
    lj_lex_next(ls);
    parse_break(ls);
    return !LJ_52;  /* Must be last in Lua 5.1. */
#if LJ_52
  case ';':
    lj_lex_next(ls);
    break;
#endif
  case TK_label:
    parse_label(ls);
    break;
  case TK_goto:
    if (LJ_52 || lj_lex_lookahead(ls) == TK_name) {
      lj_lex_next(ls);
      parse_goto(ls);
      break;
    }
    /* fallthrough */
  default:
    parse_call_assign(ls);
    break;
  }
  return 0;
}

/* A chunk is a list of statements optionally separated by semicolons. */
static void parse_chunk(LexState *ls)
{
  int islast = 0;
  synlevel_begin(ls);
  while (!islast && !parse_isend(ls->tok)) {
    islast = parse_stmt(ls);
    lex_opt(ls, ';');
    lj_assertLS(ls->fs->framesize >= ls->fs->freereg &&
		ls->fs->freereg >= ls->fs->nactvar,
		"bad regalloc");
    ls->fs->freereg = ls->fs->nactvar;  /* Free registers after each stmt. */
  }
  synlevel_end(ls);
}

/* Entry point of bytecode parser. */
GCproto *lj_parse(LexState *ls)
{
  FuncState fs;
  FuncScope bl;
  GCproto *pt;
  lua_State *L = ls->L;
#ifdef LUAJIT_DISABLE_DEBUGINFO
  ls->chunkname = lj_str_newlit(L, "=");
#else
  ls->chunkname = lj_str_newz(L, ls->chunkarg);
#endif
  setstrV(L, L->top, ls->chunkname);  /* Anchor chunkname string. */
  incr_top(L);
  ls->level = 0;
  fs_init(ls, &fs);
  fs.linedefined = 0;
  fs.numparams = 0;
  fs.bcbase = NULL;
  fs.bclim = 0;
  fs.flags |= PROTO_VARARG;  /* Main chunk is always a vararg func. */
#if LJ_54
  var_new_lua54_envuv(ls);
#endif
  fscope_begin(&fs, &bl, 0);
  bcemit_AD(&fs, BC_FUNCV, 0, 0);  /* Placeholder. */
  lj_lex_next(ls);  /* Read-ahead first token. */
  parse_chunk(ls);
  if (ls->tok != TK_eof)
    err_token(ls, TK_eof);
  pt = fs_finish(ls, ls->linenumber);
  L->top--;  /* Drop chunkname. */
  lj_assertL(fs.prev == NULL && ls->fs == NULL, "mismatched frame nesting");
#if LJ_54
  lj_assertL(pt->sizeuv == 1, "toplevel proto must have _ENV upvalue");
#else
  lj_assertL(pt->sizeuv == 0, "toplevel proto has upvalues");
#endif
  return pt;
}
