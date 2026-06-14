/*
** Trace recorder (bytecode -> SSA IR).
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_record_c
#define LUA_CORE

#include <math.h>
#include <string.h>

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_err.h"
#include "lj_str.h"
#include "lj_strscan.h"
#include "lj_tab.h"
#include "lj_gc.h"
#include "lj_meta.h"
#include "lj_frame.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#include "lj_bc.h"
#include "lj_ff.h"
#if LJ_HASPROFILE
#include "lj_debug.h"
#endif
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_ircall.h"
#include "lj_iropt.h"
#include "lj_trace.h"
#include "lj_record.h"
#include "lj_ffrecord.h"
#include "lj_snap.h"
#include "lj_dispatch.h"
#include "lj_vm.h"
#include "lj_prng.h"

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)			(&J->cur.ir[(ref)])

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

/* Emit raw IR without passing through optimizations. */
#define emitir_raw(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_ir_emit(J))

#if LJ_54
static int rec_is_longstr(cTValue *o)
{
  return tvisstr(o) && strV(o)->len > LJ_STR_MAXSHORT;
}

static int rec_lua54_strcmp_locale(GCstr *a, GCstr *b, IROp op)
{
  int32_t res = lj_str_cmp_locale(a, b);
  switch (op) {
  case IR_LT: return (res < 0);
  case IR_GE: return (res >= 0);
  case IR_LE: return (res <= 0);
  case IR_GT: return (res > 0);
  default: lj_assertX(0, "bad IR op %d", op); return 0;
  }
}

#if LJ_DUALNUM
#define LJ_LUA54_I32_MAX	((int64_t)2147483647)
#define LJ_LUA54_I32_MIN	((int64_t)(-LJ_LUA54_I32_MAX - 1))
#define IRCONV_I64_INT_SEXT	((IRT_I64<<IRCONV_DSH)|IRT_INT|IRCONV_SEXT)
#define IRCONV_INT_I64_NARROW	((IRT_INT<<IRCONV_DSH)|IRT_I64)
#define IRCONV_I64_NUM		((IRT_I64<<IRCONV_DSH)|IRT_NUM)
#define IRCONV_NUM_I64_SIGNED	((IRT_NUM<<IRCONV_DSH)|IRT_I64)

static int rec_lua54_tref_isi64(TRef tr)
{
  return tref_type(tr) == IRT_INT64 || tref_type(tr) == IRT_I64;
}

static int rec_lua54_tref_isinteger(TRef tr)
{
  return tref_isinteger(tr) || rec_lua54_tref_isi64(tr);
}

static int rec_lua54_tref_isnumeric(TRef tr)
{
  return tref_isnumber(tr) || rec_lua54_tref_isi64(tr);
}

static int rec_lua54_tv_isinteger(cTValue *tv)
{
  return tvisint(tv) || tvisi64(tv);
}

static int64_t rec_lua54_tv_i64(cTValue *tv)
{
  return tvisint(tv) ? (int64_t)intV(tv) : (int64_t)i64V(tv);
}

static int rec_lua54_tv_toint64(cTValue *tv, int64_t *ip)
{
  if (rec_lua54_tv_isinteger(tv)) {
    *ip = rec_lua54_tv_i64(tv);
    return 1;
  } else if (tvisstr(tv)) {
    if (!lj_strscan_toint64ok54(strV(tv)))
      return 0;
    *ip = lj_strscan_toint6454(strV(tv));
    return 1;
  }
  return 0;
}

static int rec_lua54_numtoint64_exact(lua_Number n, int64_t *ip)
{
  lua_Number ni;
  int64_t k;
  if (!(n >= (-9223372036854775807.0 - 1.0) &&
	n < 9223372036854775808.0))
    return 0;
  ni = lj_vm_floor(n);
  if (n != ni)
    return 0;
  k = lj_num2i64(n);
  if ((lua_Number)k != n)
    return 0;
  *ip = k;
  return 1;
}

static int rec_lua54_i64cmp(cTValue *a, cTValue *b, IROp op)
{
  int64_t ia = rec_lua54_tv_i64(a);
  int64_t ib = rec_lua54_tv_i64(b);
  switch (op) {
  case IR_LT: return ia < ib;
  case IR_GE: return ia >= ib;
  case IR_LE: return ia <= ib;
  case IR_GT: return ia > ib;
  default: lj_assertX(0, "bad IR op %d", op); return 0;
  }
}

static int rec_lua54_cmpmode(IROp op)
{
  return (int)op - (int)IR_LT;
}

static int rec_lua54_i64numcmp(cTValue *a, cTValue *b, IROp op)
{
  int mode = rec_lua54_cmpmode(op);
  if (rec_lua54_tv_isinteger(a))
    return lj_obj_i64cmpnum(rec_lua54_tv_i64(a), numV(b), mode);
  else
    return lj_obj_numcmpi64(numV(a), rec_lua54_tv_i64(b), mode);
}

static TRef rec_lua54_i64ref(jit_State *J, TRef tr)
{
  if (tref_isinteger(tr))
    return emitir(IRT(IR_CONV, IRT_I64), tr, IRCONV_I64_INT_SEXT);
  if (tref_type(tr) == IRT_I64)
    return tr;
  lj_assertJ(rec_lua54_tref_isi64(tr), "bad int64 TValue ref");
#if LJ_TARGET_ARM64
  {
    TRef addr = emitir(IRT(IR_ADD, IRT_PGC), tr,
		       lj_ir_kintpgc(J, offsetof(GCint64, i)));
    return emitir(IRT(IR_XLOAD, IRT_I64), addr, IRXLOAD_VOLATILE);
  }
#else
  return emitir(IRT(IR_FLOAD, IRT_I64), tr, IRFL_INT64_VALUE);
#endif
}

#if LJ_54
static TRef rec_lua54_table_i64key(jit_State *J, TRef key, cTValue *keyv)
{
  if (tvisi64(keyv)) {
    return rec_lua54_i64ref(J, key);
  } else if (tvisnum(keyv)) {
    int64_t i64v;
    TRef i64, back;
    if (!rec_lua54_numtoint64_exact(numV(keyv), &i64v) || checki32(i64v))
      return 0;
    if (!tref_isnum(key))
      return 0;
    if (tref_isk(key))
      return lj_ir_kint64(J, (uint64_t)i64v);
    i64 = emitir(IRT(IR_CONV, IRT_I64), key, IRCONV_I64_NUM);
    back = emitir(IRTN(IR_CONV), i64, IRCONV_NUM_I64_SIGNED);
    emitir(IRTG(IR_EQ, IRT_NUM), back, key);
    return i64;
  }
  return 0;
}
#endif

static TRef rec_lua54_numref(jit_State *J, TRef tr)
{
  if (tref_isnum(tr))
    return tr;
  if (tref_isinteger(tr))
    return emitir(IRTN(IR_CONV), tr, IRCONV_NUM_INT);
  return emitir(IRTN(IR_CONV), rec_lua54_i64ref(J, tr), IRCONV_NUM_I64_SIGNED);
}

static TRef rec_lua54_tonumref(jit_State *J, TRef tr, cTValue *tv)
{
  if (rec_lua54_tref_isnumeric(tr))
    return rec_lua54_numref(J, tr);
  if (tvisstr(tv) && tref_isstr(tr)) {
    TValue tmp;
    TRef fmt;
    if (!lj_strscan_number54(J->L, strV(tv), &tmp))
      return 0;
    fmt = lj_ir_call(J, IRCALL_lj_strscan_numtype54s, tr);
    emitir(IRTGI(IR_EQ), fmt,
	   lj_ir_kint(J, rec_lua54_tv_isinteger(&tmp) ? 1 : 2));
    return lj_ir_call(J, IRCALL_lj_strscan_tonum54s, tr);
  }
  return 0;
}

static TRef rec_lua54_i64result(jit_State *J, TRef tr, int64_t rv)
{
  if (rv >= LJ_LUA54_I32_MIN && rv <= LJ_LUA54_I32_MAX) {
    emitir(IRTG(IR_GE, IRT_I64), tr,
	   lj_ir_kint64(J, (uint64_t)LJ_LUA54_I32_MIN));
    emitir(IRTG(IR_LE, IRT_I64), tr,
	   lj_ir_kint64(J, (uint64_t)LJ_LUA54_I32_MAX));
    return emitir(IRTI(IR_CONV), tr, IRCONV_INT_I64_NARROW);
  } else {
    emitir(IRTG(rv < LJ_LUA54_I32_MIN ? IR_LT : IR_GT, IRT_I64), tr,
	   lj_ir_kint64(J, (uint64_t)(rv < LJ_LUA54_I32_MIN ?
				      LJ_LUA54_I32_MIN : LJ_LUA54_I32_MAX)));
    return lj_ir_call(J, IRCALL_lj_obj_newint64, tr);
  }
}

#if LJ_TARGET_ARM64
static TRef rec_lua54_i64result_raw(jit_State *J, TRef tr, int64_t rv)
{
  if (rv >= LJ_LUA54_I32_MIN && rv <= LJ_LUA54_I32_MAX) {
    emitir(IRTG(IR_GE, IRT_I64), tr,
	   lj_ir_kint64(J, (uint64_t)LJ_LUA54_I32_MIN));
    emitir(IRTG(IR_LE, IRT_I64), tr,
	   lj_ir_kint64(J, (uint64_t)LJ_LUA54_I32_MAX));
    return emitir(IRTI(IR_CONV), tr, IRCONV_INT_I64_NARROW);
  }
  return tr;
}

static TRef rec_lua54_boxraw_i64(jit_State *J, TRef tr)
{
  if (tref_type(tr) == IRT_I64)
    return lj_ir_call(J, IRCALL_lj_obj_newint64, tr);
  return tr;
}

static TRef rec_lua54_store_owned_i64(jit_State *J, TRef obj, TRef tr,
				      BCReg ra)
{
  TRef owner = emitir(IRT(IR_FLOAD, IRT_PGC), obj, IRFL_INT64_OWNER);
  TRef slot = emitir(IRT(IR_ADD, IRT_PGC), REF_BASE,
		     lj_ir_kintpgc(J, (int32_t)ra * 8));
  TRef fref;
  emitir(IRTG(IR_EQ, IRT_PGC), owner, slot);
  fref = emitir(IRT(IR_FREF, IRT_PGC), obj, IRFL_INT64_VALUE);
  emitir(IRT(IR_FSTORE, IRT_I64), fref, tr);
  J->needsnap = 1;
  return obj;
}

static TRef rec_lua54_xload_i64value(jit_State *J, TRef obj)
{
  TRef addr = emitir(IRT(IR_ADD, IRT_PGC), obj,
		     lj_ir_kintpgc(J, offsetof(GCint64, i)));
  return emitir(IRT(IR_XLOAD, IRT_I64), addr, IRXLOAD_VOLATILE);
}

static int rec_lua54_box_loopcarried_i64(jit_State *J, BCReg ra,
					 cTValue *lbase)
{
  TRef old = J->base[ra];
  return tvisi64(&lbase[ra]) && old && tref_type(old) == IRT_INT64;
}

static int rec_lua54_loopback_op(BCOp op)
{
  return op == BC_FORL || op == BC_IFORL || op == BC_JFORL;
}

/* Recognized loop-carried owned int64 arith/bitop writers (mirror the owned
** store dispatch). These read and rewrite the accumulator slot in place. */
static int rec_lua54_owned_arith_op(BCOp op)
{
  switch (op) {
  case BC_ADDVN: case BC_SUBVN: case BC_MULVN:
  case BC_ADDNV: case BC_SUBNV: case BC_MULNV:
  case BC_ADDVV: case BC_SUBVV: case BC_MULVV:
  case BC_BAND: case BC_BOR: case BC_BXOR:
  case BC_BSHL: case BC_BSHR: case BC_BNOT:
    return 1;
  default:
    return 0;
  }
}

/* Read-only comparison/test bytecodes: reading the accumulator value here is
** safe (no reference is copied out). ISTC/ISFC are excluded -- they copy the
** tested operand into another slot. */
static int rec_lua54_readonly_test_op(BCOp op)
{
  switch (op) {
  case BC_ISLT: case BC_ISGE: case BC_ISLE: case BC_ISGT:
  case BC_ISEQV: case BC_ISNEV: case BC_ISEQS: case BC_ISNES:
  case BC_ISEQN: case BC_ISNEN: case BC_ISEQP: case BC_ISNEP:
  case BC_IST: case BC_ISF:
    return 1;
  default:
    return 0;
  }
}

/* Calls/closures/varargs/iterators/returns can leak the accumulator box -- as
** a call argument, a returned value, or, if the slot is captured, through an
** inlined callee reading it via an upvalue (which exposes no operand here). The
** owned in-place int64 store is never safe in their presence. */
static int rec_lua54_leaky_op(BCOp op)
{
  switch (op) {
  case BC_CALLM: case BC_CALL: case BC_CALLMT: case BC_CALLT:
  case BC_ITERC: case BC_ITERN: case BC_ITERL: case BC_VARG:
  case BC_FNEW: case BC_UCLO: case BC_TSETM:
  case BC_RETM: case BC_RET: case BC_RET0: case BC_RET1:
    return 1;
  default:
    return 0;
  }
}

/* Does this bytecode reference stack slot ra as a register operand, or within a
** base range? Conservative (may over-report a touch). */
static int rec_lua54_op_touches_slot(BCIns ins, BCReg ra)
{
  BCOp op = bc_op(ins);
  BCMode ma = bcmode_a(op);
  if (ma == BCMdst || ma == BCMvar) {
    if (bc_a(ins) == ra) return 1;
  } else if (ma == BCMbase || ma == BCMrbase) {
    if (bc_a(ins) <= ra) return 1;  /* Base range [A, top) may include ra. */
  }
  if (bcmode_b(op) == BCMvar && bc_b(ins) == ra) return 1;
  if (bcmode_b(op) == BCMnone) {  /* AD-format op: operand D. */
    if (bcmode_d(op) == BCMvar && bc_d(ins) == ra) return 1;
  } else {  /* ABC-format op: operand C. */
    if (bcmode_c(op) == BCMvar && bc_c(ins) == ra) return 1;
  }
  return 0;
}

/* Scan the whole loop body [header, loopback) and decide whether the owned
** int64 accumulator in slot ra can be observed through another live reference.
** The owned in-place store is sound only when it cannot: ra may appear solely
** as an operand of the recognized arith/bitop writers and read-only tests. Any
** other op that touches ra (a copy/store reading it, an overwrite) or any
** call/closure/vararg/return means the box could be aliased -> not safe. This
** is the fix for the owned-box aliasing corruption: the interpreter stays
** correct because copyTV() calls clearint64owner() on every value copy, but the
** JIT's SSA form has no runtime copy to hang an owner-clear on, so a forward
** scan (the previous gate) missed aliases that precede the arith or escape
** through an inlined callee's upvalue. Conservative: anything unrecognized
** returns 0 (box fresh, always correct). */
static int rec_lua54_body_owned_safe(const BCIns *header,
				     const BCIns *loopback, BCReg ra)
{
  const BCIns *pc;
  for (pc = header; pc < loopback; pc++) {
    BCIns ins = *pc;
    BCOp op = bc_op(ins);
    if (rec_lua54_owned_arith_op(op) || rec_lua54_readonly_test_op(op))
      continue;
    if (rec_lua54_leaky_op(op))
      return 0;
    if (rec_lua54_op_touches_slot(ins, ra))
      return 0;
  }
  return 1;
}

static int rec_lua54_next_loopback_keeps_slot(jit_State *J, BCReg ra)
{
  const BCIns *start = proto_bc(J->pt);
  const BCIns *end = start + J->pt->sizebc;
  const BCIns *loopback = NULL, *header = NULL, *pc;
  int depth;
  /* Find this iteration's loop-back forward from the recording pc. A backward
  ** JMP before it means a non-FOR loop (while/repeat) -- bail conservatively. */
  for (pc = J->pc + 1; pc < end; pc++) {
    BCIns ins = *pc;
    BCOp op = bc_op(ins);
    if (rec_lua54_loopback_op(op)) { loopback = pc; break; }
    if (op == BC_JMP && pc + bc_j(ins) + 1 <= pc)
      return 0;
  }
  /* Require a FORL-style loop-back with the accumulator below the loop-control
  ** slots. (A hot loop-back may be IFORL/JFORL whose operand D is a trace
  ** literal, not a jump, so the body start is found by matching FORI, below.) */
  if (!loopback || !(ra < bc_a(*loopback) + FORL_EXT))
    return 0;
  /* Find the matching loop header by scanning back for the FORI that opens this
  ** loop, balancing any inner FOR loops that lie before the recording pc. */
  depth = 0;
  for (pc = J->pc; pc > start; pc--) {
    BCOp op = bc_op(pc[-1]);
    if (rec_lua54_loopback_op(op)) {
      depth++;
    } else if (op == BC_FORI || op == BC_JFORI) {
      if (depth == 0) { header = pc; break; }
      depth--;
    }
  }
  if (!header || header > J->pc)
    return 0;
  /* Reject loops nested inside another loop: an outer loop can alias the
  ** accumulator box in body code outside [header, loopback) that we do not
  ** scan, so the in-place store would be unsound there. An enclosing loop opens
  ** at a FORI seen (scanning back from our header) before its own loop-back. */
  depth = 0;
  for (pc = header - 1; pc > start; pc--) {
    BCOp op = bc_op(pc[-1]);
    if (rec_lua54_loopback_op(op))
      depth++;
    else if (op == BC_FORI || op == BC_JFORI) {
      if (depth == 0)
	return 0;
      depth--;
    }
  }
  return rec_lua54_body_owned_safe(header, loopback, ra);
}
#endif

static TRef rec_lua54_toint64ref(jit_State *J, TRef tr, cTValue *tv)
{
  if (rec_lua54_tv_isinteger(tv)) {
    if (!rec_lua54_tref_isinteger(tr))
      return 0;
    return rec_lua54_i64ref(J, tr);
  } else if (tvisstr(tv)) {
    TRef ok;
    if (!tref_isstr(tr))
      return 0;
    ok = lj_ir_call(J, IRCALL_lj_strscan_toint64ok54, tr);
    emitir(IRTG(IR_NE, IRT_INT), ok, lj_ir_kint(J, 0));
    return lj_ir_call(J, IRCALL_lj_strscan_toint6454, tr);
  }
  return 0;
}

static TRef rec_lua54_unm_intref(jit_State *J, TRef irc, int64_t ic,
				 int rawok)
{
  int64_t rv = (int64_t)(lua_Integer)((lua_Unsigned)0 - (lua_Unsigned)ic);
  TRef tr = emitir(IRT(IR_NEG, IRT_I64), irc, irc);
#if LJ_TARGET_ARM64
  if (rawok)
    return rec_lua54_i64result_raw(J, tr, rv);
#else
  UNUSED(rawok);
#endif
  return rec_lua54_i64result(J, tr, rv);
}

static TRef rec_lua54_unm_int(jit_State *J, TRef rc, cTValue *rcv, int rawok)
{
  return rec_lua54_unm_intref(J, rec_lua54_i64ref(J, rc),
			      rec_lua54_tv_i64(rcv), rawok);
}

static TRef rec_lua54_arith_intref(jit_State *J, TRef irb, TRef irc,
				   int64_t ib, int64_t ic, MMS mm, int rawok)
{
  lua_Unsigned ub = (lua_Unsigned)(lua_Integer)ib;
  lua_Unsigned uc = (lua_Unsigned)(lua_Integer)ic;
  IROp op = (IROp)((int)mm - (int)MM_add + (int)IR_ADD);
  int64_t rv;
  TRef tr;
  switch (mm) {
  case MM_add: rv = (int64_t)(lua_Integer)(ub + uc); break;
  case MM_sub: rv = (int64_t)(lua_Integer)(ub - uc); break;
  case MM_mul: rv = (int64_t)(lua_Integer)(ub * uc); break;
  default: lj_assertJ(0, "bad Lua 5.4 integer arithmetic op"); rv = 0; break;
  }
  tr = emitir(IRT(op, IRT_I64), irb, irc);
#if LJ_TARGET_ARM64
  if (rawok)
    return rec_lua54_i64result_raw(J, tr, rv);
#else
  UNUSED(rawok);
#endif
  return rec_lua54_i64result(J, tr, rv);
}

#if LJ_TARGET_ARM64
static TRef rec_lua54_arith_intref_owned(jit_State *J, TRef irb, TRef irc,
					 int64_t ib, int64_t ic, MMS mm,
					 TRef obj, BCReg ra)
{
  IROp op = (IROp)((int)mm - (int)MM_add + (int)IR_ADD);
  TRef tr;
  switch (mm) {
  case MM_add: break;
  case MM_sub: break;
  case MM_mul: break;
  default: lj_assertJ(0, "bad Lua 5.4 integer arithmetic op"); break;
  }
  UNUSED(ib); UNUSED(ic);
  tr = emitir(IRT(op, IRT_I64), irb, irc);
  return rec_lua54_store_owned_i64(J, obj, tr, ra);
}

static TRef rec_lua54_arith_i32(jit_State *J, TRef rb, TRef rc,
				cTValue *rbv, cTValue *rcv, MMS mm)
{
  lua_Unsigned ub, uc;
  int64_t rv;
  IROp op;
  if (!tref_isinteger(rb) || !tref_isinteger(rc) ||
      !tvisint(rbv) || !tvisint(rcv))
    return 0;
  ub = (lua_Unsigned)(lua_Integer)intV(rbv);
  uc = (lua_Unsigned)(lua_Integer)intV(rcv);
  switch (mm) {
  case MM_add:
    rv = (int64_t)(lua_Integer)(ub + uc);
    op = IR_ADDOV;
    break;
  case MM_sub:
    rv = (int64_t)(lua_Integer)(ub - uc);
    op = IR_SUBOV;
    break;
  case MM_mul:
    rv = (int64_t)(lua_Integer)(ub * uc);
    op = IR_MULOV;
    break;
  default:
    return 0;
  }
  if (rv < LJ_LUA54_I32_MIN || rv > LJ_LUA54_I32_MAX)
    return 0;
  return emitir(IRTGI(op), rb, rc);
}

static int rec_lua54_next_tgetv_key(jit_State *J, BCReg ra)
{
  BCIns next = J->pc[1];
  if (bc_op(next) == BC_TGETV || bc_op(next) == BC_TSETV)
    return bc_c(next) == ra;
  if (bc_op(next) == BC_UGET && bc_a(next) != ra) {
    next = J->pc[2];
    return (bc_op(next) == BC_TGETV || bc_op(next) == BC_TSETV) &&
	   bc_c(next) == ra;
  }
  return 0;
}

static int rec_lua54_next_bitop_operand(jit_State *J, BCReg ra)
{
  const BCIns *pc = J->pc + 1;
  int n;
  for (n = 0; n < 3; n++, pc++) {
    BCIns next = *pc;
    BCOp op = bc_op(next);
    switch (op) {
    case BC_BAND: case BC_BOR: case BC_BXOR: case BC_BSHL: case BC_BSHR:
      return bc_b(next) == ra || bc_c(next) == ra;
    case BC_BNOT:
      return bc_c(next) == ra;
    case BC_KSHORT: case BC_KNUM:
      if (bc_a(next) == ra)
	return 0;
      break;
    default:
      return 0;
    }
  }
  return 0;
}

static int rec_lua54_next_arith_bitop_operand(jit_State *J, BCReg ra)
{
  const BCIns *pc = J->pc + 1;
  int n;
  for (n = 0; n < 5; n++, pc++) {
    BCIns next = *pc;
    BCOp op = bc_op(next);
    switch (op) {
    case BC_BAND: case BC_BOR: case BC_BXOR: case BC_BSHL: case BC_BSHR:
      return bc_b(next) == ra || bc_c(next) == ra;
    case BC_BNOT:
      return bc_c(next) == ra;
    case BC_ADDNV: case BC_SUBNV: case BC_MULNV:
    case BC_ADDVN: case BC_SUBVN: case BC_MULVN:
    case BC_ADDVV: case BC_SUBVV: case BC_MULVV:
      if (bc_a(next) != ra)
	return 0;
      break;
    case BC_KSHORT: case BC_KNUM:
      if (bc_a(next) == ra)
	return 0;
      break;
    default:
      return 0;
    }
  }
  return 0;
}

static int rec_lua54_next_unm_operand(jit_State *J, BCReg ra)
{
  BCIns next = J->pc[1];
  return bc_op(next) == BC_UNM && bc_c(next) == ra;
}

static int rec_lua54_next_mod_operand(jit_State *J, BCReg ra)
{
  BCIns next = J->pc[1];
  BCOp op = bc_op(next);
  switch (op) {
  case BC_MODVN: case BC_IDIV:
    return bc_b(next) == ra;
  case BC_MODNV:
    return bc_c(next) == ra;
  case BC_MODVV:
    return bc_b(next) == ra || bc_c(next) == ra;
  default:
    return 0;
  }
}

static int rec_lua54_next_comp_operand(jit_State *J, BCReg ra)
{
  const BCIns *pc = J->pc + 1;
  const BCIns *end = pc + 4;
  for (; pc < end; pc++) {
    BCIns ins = *pc;
    BCOp op = bc_op(ins);
    switch (op) {
    case BC_ISLT: case BC_ISGE: case BC_ISLE: case BC_ISGT:
    case BC_ISEQV: case BC_ISNEV:
      return bc_a(ins) == ra || bc_d(ins) == ra;
    case BC_ISEQN: case BC_ISNEN:
      return bc_a(ins) == ra;
    default:
      if (bcmode_d(op) == BCMjump || bcmode_a(op) == BCMbase ||
	  bc_isret_or_tail(op))
	return 0;
      if (bcmode_a(op) == BCMdst && bc_a(ins) == ra)
	return 0;
      break;
    }
  }
  return 0;
}

static int rec_lua54_loopcarried_arith_i64(jit_State *J, BCIns ins,
					   TRef rb, TRef rc, cTValue *lbase)
{
  BCReg ra = bc_a(ins);
  /* Store the result in place into the box the slot already owns whenever the
  ** slot stays a loop-carried owned int64 through to the loop-back, even when
  ** the writing op's operands are temporaries (e.g. "s = (s + i) - (i % 7)")
  ** and across an intervening comparison and conditional reset. The runtime
  ** owner guard in rec_lua54_store_owned_i64 keeps this correct; keeps_slot
  ** tolerates further owned int64 writes to the same slot. Without this such
  ** loops box per iteration and abort the trace with type instability. */
  UNUSED(rb); UNUSED(rc);
  if (!rec_lua54_next_loopback_keeps_slot(J, ra))
    return 0;
  return rec_lua54_box_loopcarried_i64(J, ra, lbase);
}

static int rec_lua54_tgets_minmax(jit_State *J, BCIns ins)
{
  GCstr *s;
  if (bc_op(ins) != BC_TGETS)
    return 0;
  s = gco2str(proto_kgc(J->pt, ~(ptrdiff_t)bc_c(ins)));
  return (s->len == 3 && memcmp(strdata(s), "max", 3) == 0) ||
	 (s->len == 3 && memcmp(strdata(s), "min", 3) == 0);
}

static int rec_lua54_alias_has(BCReg *alias, int nalias, BCReg slot)
{
  int i;
  for (i = 0; i < nalias; i++)
    if (alias[i] == slot)
      return 1;
  return 0;
}

static int rec_lua54_next_math_minmax_arg(jit_State *J, BCReg ra)
{
  BCReg alias[8], ffslot = LJ_MAX_JSLOTS;
  int i, n, nalias = 1;
  alias[0] = ra;
  for (n = 1; n <= 16; n++) {
    BCIns ins = J->pc[n];
    BCOp op = bc_op(ins);
    if (op == BC_MOV && rec_lua54_alias_has(alias, nalias, bc_d(ins))) {
      BCReg dst = bc_a(ins);
      if (!rec_lua54_alias_has(alias, nalias, dst) &&
	  nalias < (int)(sizeof(alias)/sizeof(alias[0])))
	alias[nalias++] = dst;
      continue;
    }
    if (rec_lua54_tgets_minmax(J, ins)) {
      ffslot = bc_a(ins);
      continue;
    }
    if (op == BC_CALL && bc_a(ins) == ffslot && bc_c(ins) != 0) {
      BCReg first = bc_a(ins) + 1 + LJ_FR2;
      BCReg last = first + bc_c(ins) - 2;
      for (i = 0; i < nalias; i++)
	if (alias[i] >= first && alias[i] <= last)
	  return 1;
      ffslot = LJ_MAX_JSLOTS;
      continue;
    }
    switch (op) {
    case BC_JMP: case BC_UCLO:
    case BC_RET: case BC_RET0: case BC_RET1:
    case BC_RETM:
      return 0;
    default:
      break;
    }
    if (rec_lua54_alias_has(alias, nalias, bc_a(ins)) &&
	(op == BC_UGET || bcmode_a(op) == BCMdst || bcmode_a(op) == BCMbase))
      return 0;
  }
  return 0;
}

static int rec_lua54_next_string_format_arg(jit_State *J, BCReg ra)
{
  BCReg alias[8];
  int i, n, nalias = 1;
  alias[0] = ra;
  for (n = 1; n <= 16; n++) {
    BCIns ins = J->pc[n];
    BCOp op = bc_op(ins);
    if (op == BC_MOV) {
      BCReg src = bc_d(ins), dst = bc_a(ins);
      if (rec_lua54_alias_has(alias, nalias, src) &&
	  !rec_lua54_alias_has(alias, nalias, dst) &&
	  nalias < (int)(sizeof(alias)/sizeof(alias[0])))
	alias[nalias++] = dst;
      continue;
    }
    if (op == BC_CALL) {
      BCReg base = bc_a(ins);
      BCReg narg = bc_c(ins);
      if (narg != 0) {
	BCReg first = base + 1 + LJ_FR2;
	BCReg last = first + narg - 2;
	for (i = 0; i < nalias; i++) {
	  if (alias[i] >= first && alias[i] <= last) {
	    cTValue *fnv = &J->L->base[base];
	    if (tvisfunc(fnv) && funcV(fnv)->c.ffid == FF_string_format)
	      return 1;
	    return 0;
	  }
	}
      }
      return 0;
    }
    switch (op) {
    case BC_JMP: case BC_UCLO:
    case BC_RET: case BC_RET0: case BC_RET1:
    case BC_RETM:
      return 0;
    case BC_KSTR: case BC_KSHORT: case BC_KNUM:
      if (rec_lua54_alias_has(alias, nalias, bc_a(ins)))
	return 0;
      break;
    default:
      return 0;
    }
  }
  return 0;
}
#endif

static TRef rec_lua54_arith_int(jit_State *J, TRef rb, TRef rc,
				cTValue *rbv, cTValue *rcv, MMS mm,
				int rawok)
{
  return rec_lua54_arith_intref(J, rec_lua54_i64ref(J, rb),
				rec_lua54_i64ref(J, rc),
				rec_lua54_tv_i64(rbv), rec_lua54_tv_i64(rcv),
				mm, rawok || tref_type(rb) == IRT_I64 ||
				    tref_type(rc) == IRT_I64);
}

/* Convert a bitwise operand to a raw IRT_I64 ref plus its runtime value.
** Strings never coerce for bitwise operators; exact-integer floats are
** guarded with a numeric round-trip like the recff int64 converters.
*/
static TRef rec_lua54_bitop_toint64ref(jit_State *J, TRef tr, cTValue *tv,
				       int64_t *ip)
{
  if (tvisnum(tv)) {
    TRef i64, back;
    if (!rec_lua54_numtoint64_exact(numV(tv), ip))
      return 0;
    if (!tref_isnum(tr))
      return 0;
    i64 = emitir(IRT(IR_CONV, IRT_I64), tr, IRCONV_I64_NUM);
    back = emitir(IRTN(IR_CONV), i64, IRCONV_NUM_I64_SIGNED);
    emitir(IRTG(IR_EQ, IRT_NUM), back, tr);
    return i64;
  }
  if (tvisstr(tv) || !rec_lua54_tv_toint64(tv, ip))
    return 0;
#if LJ_TARGET_ARM64
  if (tvisi64(tv) && tref_type(tr) == IRT_INT64 && !tref_isk(tr))
    return rec_lua54_xload_i64value(J, tr);
#endif
  return rec_lua54_toint64ref(J, tr, tv);
}

/* Runtime Lua 5.4 shift semantics, mirroring lj_meta.c bitop_shift(). */
static int64_t rec_lua54_shiftint(int64_t a, int64_t sh, int left)
{
  int64_t s = sh;
  uint64_t u = (uint64_t)a;
  if (s < 0) {
    if (s <= -64)
      return 0;
    s = -s;
    left = !left;
  } else if (s >= 64) {
    return 0;
  }
  return left ? (int64_t)(u << s) : (int64_t)(u >> s);
}

/* Record a Lua 5.4 shift with count guards matching the runtime count. */
static TRef rec_lua54_shiftref(jit_State *J, TRef irv, TRef irsh,
			       int64_t sh, IROp op)
{
  if (sh >= 64 || sh <= -64) {
    emitir(IRTG(sh < 0 ? IR_LE : IR_GE, IRT_I64), irsh,
	   lj_ir_kint64(J, (uint64_t)(sh < 0 ? -64 : 64)));
    return lj_ir_kint(J, 0);
  } else {
    TRef irs;
    if (sh < 0) {
      emitir(IRTG(IR_LT, IRT_I64), irsh, lj_ir_kint64(J, 0));
      emitir(IRTG(IR_GE, IRT_I64), irsh, lj_ir_kint64(J, (uint64_t)-63));
      irsh = emitir(IRT(IR_NEG, IRT_I64), irsh, irsh);
      op = (op == IR_BSHL) ? IR_BSHR : IR_BSHL;
    } else {
      emitir(IRTG(IR_GE, IRT_I64), irsh, lj_ir_kint64(J, 0));
      emitir(IRTG(IR_LE, IRT_I64), irsh, lj_ir_kint64(J, 63));
    }
    irs = emitir(IRTI(IR_CONV), irsh, IRCONV_INT_I64_NARROW);
    return emitir(IRT(op, IRT_I64), irv, irs);
  }
}
#endif

#endif

/* -- Sanity checks ------------------------------------------------------- */

#ifdef LUA_USE_ASSERT
/* Sanity check the whole IR -- sloooow. */
static void rec_check_ir(jit_State *J)
{
  IRRef i, nins = J->cur.nins, nk = J->cur.nk;
  lj_assertJ(nk <= REF_BIAS && nins >= REF_BIAS && nins < 65536,
	     "inconsistent IR layout");
  for (i = nk; i < nins; i++) {
    IRIns *ir = IR(i);
    uint32_t mode = lj_ir_mode[ir->o];
    IRRef op1 = ir->op1;
    IRRef op2 = ir->op2;
    const char *err = NULL;
    switch (irm_op1(mode)) {
    case IRMnone:
      if (op1 != 0) err = "IRMnone op1 used";
      break;
    case IRMref:
      if (op1 < nk || (i >= REF_BIAS ? op1 >= i : op1 <= i))
	err = "IRMref op1 out of range";
      break;
    case IRMlit: break;
    case IRMcst:
      if (i >= REF_BIAS) { err = "constant in IR range"; break; }
      if (irt_is64(ir->t) && ir->o != IR_KNULL)
	i++;
      continue;
    }
    switch (irm_op2(mode)) {
    case IRMnone:
      if (op2) err = "IRMnone op2 used";
      break;
    case IRMref:
      if (op2 < nk || (i >= REF_BIAS ? op2 >= i : op2 <= i))
	err = "IRMref op2 out of range";
      break;
    case IRMlit: break;
    case IRMcst: err = "IRMcst op2"; break;
    }
    if (!err && ir->prev) {
      if (ir->prev < nk || (i >= REF_BIAS ? ir->prev >= i : ir->prev <= i))
	err = "chain out of range";
      else if (ir->o != IR_NOP && IR(ir->prev)->o != ir->o)
	err = "chain to different op";
    }
    lj_assertJ(!err, "bad IR %04d op %d(%04d,%04d): %s",
	       i-REF_BIAS,
	       ir->o,
	       irm_op1(mode) == IRMref ? op1-REF_BIAS : op1,
	       irm_op2(mode) == IRMref ? op2-REF_BIAS : op2,
	       err);
  }
}

/* Compare stack slots and frames of the recorder and the VM. */
static void rec_check_slots(jit_State *J)
{
  BCReg s, nslots = J->baseslot + J->maxslot;
  int32_t depth = 0;
  cTValue *base = J->L->base - J->baseslot;
  lj_assertJ(J->baseslot >= 1+LJ_FR2, "bad baseslot");
  lj_assertJ(J->baseslot == 1+LJ_FR2 || (J->slot[J->baseslot-1] & TREF_FRAME),
	     "baseslot does not point to frame");
  lj_assertJ(nslots <= LJ_MAX_JSLOTS, "slot overflow");
  for (s = 0; s < nslots; s++) {
    TRef tr = J->slot[s];
    if (tr) {
      cTValue *tv = &base[s];
      IRRef ref = tref_ref(tr);
      IRIns *ir = NULL;  /* Silence compiler. */
      lj_assertJ(tv < J->L->top, "slot %d above top of Lua stack", s);
      if (!LJ_FR2 || ref || !(tr & (TREF_FRAME | TREF_CONT))) {
	lj_assertJ(ref >= J->cur.nk && ref < J->cur.nins,
		   "slot %d ref %04d out of range", s, ref - REF_BIAS);
	ir = IR(ref);
	lj_assertJ(irt_t(ir->t) == tref_t(tr), "slot %d IR type mismatch", s);
      }
      if (s == 0) {
	lj_assertJ(tref_isfunc(tr), "frame slot 0 is not a function");
#if LJ_FR2
      } else if (s == 1) {
	lj_assertJ((tr & ~TREF_FRAME) == 0, "bad frame slot 1");
#endif
      } else if ((tr & TREF_FRAME)) {
	GCfunc *fn = gco2func(frame_gc(tv));
	BCReg delta = (BCReg)(tv - frame_prev(tv));
#if LJ_FR2
	lj_assertJ(!ref || ir_knum(ir)->u64 == tv->u64,
		   "frame slot %d PC mismatch", s);
	tr = J->slot[s-1];
	ir = IR(tref_ref(tr));
#endif
	lj_assertJ(tref_isfunc(tr),
		   "frame slot %d is not a function", s-LJ_FR2);
	lj_assertJ(!tref_isk(tr) || fn == ir_kfunc(ir),
		   "frame slot %d function mismatch", s-LJ_FR2);
	lj_assertJ(s > delta + LJ_FR2 ? (J->slot[s-delta] & TREF_FRAME)
				      : (s == delta + LJ_FR2),
		   "frame slot %d broken chain", s-LJ_FR2);
	depth++;
      } else if ((tr & TREF_CONT)) {
#if LJ_FR2
	lj_assertJ(!ref || ir_knum(ir)->u64 == tv->u64,
		   "cont slot %d continuation mismatch", s);
#else
	lj_assertJ(ir_kptr(ir) == gcrefp(tv->gcr, void),
		   "cont slot %d continuation mismatch", s);
#endif
	lj_assertJ((J->slot[s+1+LJ_FR2] & TREF_FRAME),
		   "cont slot %d not followed by frame", s);
	depth++;
      } else if ((tr & TREF_KEYINDEX)) {
	lj_assertJ(tref_isint(tr), "keyindex slot %d bad type %d",
				   s, tref_type(tr));
      } else {
	/* Number repr. may differ, but other types must be the same. */
	lj_assertJ(tvisnumber(tv) ? tref_isnumber(tr) :
				    itype2irt(tv) == tref_type(tr),
		   "slot %d type mismatch: stack type %d vs IR type %d",
		   s, itypemap(tv), tref_type(tr));
	if (tref_isk(tr)) {  /* Compare constants. */
	  TValue tvk;
	  lj_ir_kvalue(J->L, &tvk, ir);
	  lj_assertJ((tvisnum(&tvk) && tvisnan(&tvk)) ?
		     (tvisnum(tv) && tvisnan(tv)) :
		     lj_obj_equal(tv, &tvk),
		     "slot %d const mismatch: stack %016llx vs IR %016llx",
		     s, tv->u64, tvk.u64);
	}
      }
    }
  }
  lj_assertJ(J->framedepth == depth,
	     "frame depth mismatch %d vs %d", J->framedepth, depth);
}
#endif

/* -- Type handling and specialization ------------------------------------ */

/* Note: these functions return tagged references (TRef). */

/* Specialize a slot to a specific type. Note: slot can be negative! */
static TRef sloadt(jit_State *J, int32_t slot, IRType t, int mode)
{
  /* Caller may set IRT_GUARD in t. */
  TRef ref = emitir_raw(IRT(IR_SLOAD, t), (int32_t)J->baseslot+slot, mode);
  J->base[slot] = ref;
  return ref;
}

/* Specialize a slot to the runtime type. Note: slot can be negative! */
static TRef sload(jit_State *J, int32_t slot)
{
  IRType t = itype2irt(&J->L->base[slot]);
  TRef ref = emitir_raw(IRTG(IR_SLOAD, t), (int32_t)J->baseslot+slot,
			IRSLOAD_TYPECHECK);
  if (irtype_ispri(t)) ref = TREF_PRI(t);  /* Canonicalize primitive refs. */
  J->base[slot] = ref;
  return ref;
}

/* Get TRef from slot. Load slot and specialize if not done already. */
#define getslot(J, s)	(J->base[(s)] ? J->base[(s)] : sload(J, (int32_t)(s)))

/* Get TRef for current function. */
static TRef getcurrf(jit_State *J)
{
  if (J->base[-1-LJ_FR2])
    return J->base[-1-LJ_FR2];
  /* Non-base frame functions ought to be loaded already. */
  lj_assertJ(J->baseslot == 1+LJ_FR2, "bad baseslot");
  return sloadt(J, -1-LJ_FR2, IRT_FUNC, IRSLOAD_READONLY);
}

/* Compare for raw object equality.
** Returns 0 if the objects are the same.
** Returns 1 if they are different, but the same type.
** Returns 2 for two different types.
** Comparisons between primitives always return 1 -- no caller cares about it.
*/
int lj_record_objcmp(jit_State *J, TRef a, TRef b, cTValue *av, cTValue *bv)
{
  int diff = !lj_obj_equal(av, bv);
  if (!tref_isk2(a, b)) {  /* Shortcut, also handles primitives. */
    IRType ta = tref_isinteger(a) ? IRT_INT : tref_type(a);
    IRType tb = tref_isinteger(b) ? IRT_INT : tref_type(b);
#if LJ_54
    if ((rec_is_longstr(av) || rec_is_longstr(bv)) &&
	(ta == IRT_STR || tb == IRT_STR)) {
      TRef eq;
      if (ta != tb)
	return 2;  /* Two different types are never equal. */
      /* Runtime Lua 5.4 long strings are not interned. Compare bytes through
      ** the shared helper instead of guarding on the first pair of GC objects
      ** seen by the recorder; hot loops may rotate through many equal long
      ** strings with distinct identities.
      */
      eq = lj_ir_call(J, IRCALL_lj_str_equal, a, b);
      emitir(IRTG(diff ? IR_EQ : IR_NE, IRT_INT), eq, lj_ir_kint(J, 0));
      return diff;
    }
#endif
#if LJ_54 && LJ_DUALNUM
    if ((ta == IRT_INT64 || ta == IRT_I64 ||
	 tb == IRT_INT64 || tb == IRT_I64) &&
	(ta == IRT_INT || ta == IRT_INT64 || ta == IRT_I64) &&
	(tb == IRT_INT || tb == IRT_INT64 || tb == IRT_I64)) {
      emitir(IRTG(diff ? IR_NE : IR_EQ, IRT_I64),
	     rec_lua54_i64ref(J, a), rec_lua54_i64ref(J, b));
      return diff;
    }
    if ((ta == IRT_INT64 && tb == IRT_NUM) ||
	(ta == IRT_NUM && tb == IRT_INT64)) {
      TRef eq = ta == IRT_INT64 ?
	lj_ir_call(J, IRCALL_lj_obj_i64eqnum, rec_lua54_i64ref(J, a), b) :
	lj_ir_call(J, IRCALL_lj_obj_i64eqnum, rec_lua54_i64ref(J, b), a);
      emitir(IRTG(diff ? IR_EQ : IR_NE, IRT_INT), eq, lj_ir_kint(J, 0));
      return diff;
    }
#endif
    if (ta != tb) {
      /* Widen mixed number/int comparisons to number/number comparison. */
      if (ta == IRT_INT && tb == IRT_NUM) {
	a = emitir(IRTN(IR_CONV), a, IRCONV_NUM_INT);
	ta = IRT_NUM;
      } else if (ta == IRT_NUM && tb == IRT_INT) {
	b = emitir(IRTN(IR_CONV), b, IRCONV_NUM_INT);
      } else {
	return 2;  /* Two different types are never equal. */
      }
    }
    emitir(IRTG(diff ? IR_NE : IR_EQ, ta), a, b);
  }
  return diff;
}

/* Constify a value. Returns 0 for non-representable object types. */
TRef lj_record_constify(jit_State *J, cTValue *o)
{
  if (tvisgcv(o))
    return lj_ir_kgc(J, gcV(o), itype2irt(o));
  else if (tvisint(o))
    return lj_ir_kint(J, intV(o));
  else if (tvisnum(o))
#if LJ_54 && LJ_DUALNUM
    /* Lua 5.4 exposes integer vs. float as observable subtypes.  Keep a
    ** runtime float TValue as KNUM even when its value is integral, otherwise
    ** hot traces can turn expressions such as i+0.0 back into integers.
    */
    return lj_ir_knum(J, numV(o));
#else
    return lj_ir_knumint(J, numV(o));
#endif
  else if (tvisbool(o))
    return TREF_PRI(itype2irt(o));
  else
    return 0;  /* Can't represent lightuserdata (pointless). */
}

/* Emit a VLOAD with the correct type. */
TRef lj_record_vload(jit_State *J, TRef ref, MSize idx, IRType t)
{
  TRef tr = emitir(IRTG(IR_VLOAD, t), ref, idx);
  if (irtype_ispri(t)) tr = TREF_PRI(t);  /* Canonicalize primitives. */
  return tr;
}

/* -- Record loop ops ----------------------------------------------------- */

/* Loop event. */
typedef enum {
  LOOPEV_LEAVE,		/* Loop is left or not entered. */
  LOOPEV_ENTERLO,	/* Loop is entered with a low iteration count left. */
  LOOPEV_ENTER		/* Loop is entered. */
} LoopEvent;

/* Canonicalize slots: convert integers to numbers. */
static void canonicalize_slots(jit_State *J)
{
  BCReg s;
  if (LJ_DUALNUM) return;
  for (s = J->baseslot+J->maxslot-1; s >= 1; s--) {
    TRef tr = J->slot[s];
    if (tref_isinteger(tr) && !(tr & TREF_KEYINDEX)) {
      IRIns *ir = IR(tref_ref(tr));
      if (!(ir->o == IR_SLOAD && (ir->op2 & (IRSLOAD_READONLY))))
	J->slot[s] = emitir(IRTN(IR_CONV), tr, IRCONV_NUM_INT);
    }
  }
}

/* Stop recording. */
void lj_record_stop(jit_State *J, TraceLink linktype, TraceNo lnk)
{
#ifdef LUAJIT_ENABLE_TABLE_BUMP
  if (J->retryrec)
    lj_trace_err(J, LJ_TRERR_RETRY);
#endif
  lj_trace_end(J);
  J->cur.linktype = (uint8_t)linktype;
  J->cur.link = (uint16_t)lnk;
  /* Looping back at the same stack level? */
  if (lnk == J->cur.traceno && J->framedepth + J->retdepth == 0) {
    if ((J->flags & JIT_F_OPT_LOOP))  /* Shall we try to create a loop? */
      goto nocanon;  /* Do not canonicalize or we lose the narrowing. */
    if (J->cur.root)  /* Otherwise ensure we always link to the root trace. */
      J->cur.link = J->cur.root;
  }
  canonicalize_slots(J);
nocanon:
  /* Note: all loop ops must set J->pc to the following instruction! */
  lj_snap_add(J);  /* Add loop snapshot. */
  J->needsnap = 0;
  J->mergesnap = 1;  /* In case recording continues. */
}

/* Search bytecode backwards for a int/num constant slot initializer. */
static TRef find_kinit(jit_State *J, const BCIns *endpc, BCReg slot, IRType t)
{
  /* This algorithm is rather simplistic and assumes quite a bit about
  ** how the bytecode is generated. It works fine for FORI initializers,
  ** but it won't necessarily work in other cases (e.g. iterator arguments).
  ** It doesn't do anything fancy, either (like backpropagating MOVs).
  */
  const BCIns *pc, *startpc = proto_bc(J->pt);
  for (pc = endpc-1; pc > startpc; pc--) {
    BCIns ins = *pc;
    BCOp op = bc_op(ins);
    /* First try to find the last instruction that stores to this slot. */
    if (bcmode_a(op) == BCMbase && bc_a(ins) <= slot) {
      return 0;  /* Multiple results, e.g. from a CALL or KNIL. */
    } else if (bcmode_a(op) == BCMdst && bc_a(ins) == slot) {
      if (op == BC_KSHORT || op == BC_KNUM) {  /* Found const. initializer. */
	/* Now try to verify there's no forward jump across it. */
	const BCIns *kpc = pc;
	for (; pc > startpc; pc--)
	  if (bc_op(*pc) == BC_JMP) {
	    const BCIns *target = pc+bc_j(*pc)+1;
	    if (target > kpc && target <= endpc)
	      return 0;  /* Conditional assignment. */
	  }
	if (op == BC_KSHORT) {
	  int32_t k = (int32_t)(int16_t)bc_d(ins);
	  return t == IRT_INT ? lj_ir_kint(J, k) : lj_ir_knum(J, (lua_Number)k);
	} else {
	  cTValue *tv = proto_knumtv(J->pt, bc_d(ins));
	  if (t == IRT_INT) {
	    if (tvisint(tv)) {
	      return lj_ir_kint(J, intV(tv));
	    } else {
	      int64_t i64;
	      int32_t k;
	      if (lj_num2int_check(numV(tv), i64, k))  /* -0 is ok here. */
		return lj_ir_kint(J, k);
	    }
	    return 0;  /* Type mismatch. */
	  } else {
	    return lj_ir_knum(J, numberVnum(tv));
	  }
	}
      }
      return 0;  /* Non-constant initializer. */
    }
  }
  return 0;  /* No assignment to this slot found? */
}

/* Load and optionally convert a FORI argument from a slot. */
static TRef fori_load(jit_State *J, BCReg slot, IRType t, int mode)
{
  int conv = (tvisint(&J->L->base[slot]) != (t==IRT_INT)) ? IRSLOAD_CONVERT : 0;
  return sloadt(J, (int32_t)slot,
		t + (((mode & IRSLOAD_TYPECHECK) ||
		      (conv && t == IRT_INT && !(mode >> 16))) ?
		     IRT_GUARD : 0),
		mode + conv);
}

/* Convert FORI argument to expected target type. */
static TRef fori_conv(jit_State *J, TRef tr, IRType t)
{
  if (t == IRT_INT) {
    if (!tref_isinteger(tr))
      return emitir(IRTGI(IR_CONV), tr, IRCONV_INT_NUM|IRCONV_CHECK);
  } else {
    if (!tref_isnum(tr))
      return emitir(IRTN(IR_CONV), tr, IRCONV_NUM_INT);
  }
  return tr;
}

/* Peek before FORI to find a const initializer. Otherwise load from slot. */
static TRef fori_arg(jit_State *J, const BCIns *fori, BCReg slot,
		     IRType t, int mode)
{
  TRef tr = J->base[slot];
  if (tr) {
    tr = fori_conv(J, tr, t);
  } else {
    tr = find_kinit(J, fori, slot, t);
    if (!tr)
      tr = fori_load(J, slot, t, mode);
  }
  return tr;
}

#if LJ_54 && LJ_DUALNUM
static int rec_for_lua54_intmode(cTValue *tv)
{
  /* Lua 5.4 keeps a numeric for loop in integer mode only when both the
  ** initial value and step are tagged integers after FORI coercion. The JIT
  ** must not re-narrow integral-looking float loops, or math.type(i) changes.
  */
  return tvisint(&tv[FORL_IDX]) && tvisint(&tv[FORL_STEP]);
}

static int rec_for_allint(cTValue *tv)
{
  return tvisint(&tv[FORL_IDX]) && tvisint(&tv[FORL_STOP]) &&
	 tvisint(&tv[FORL_STEP]);
}

static int rec_for_hasi64(cTValue *tv)
{
  return tvisi64(&tv[FORL_IDX]) || tvisi64(&tv[FORL_STOP]) ||
	 tvisi64(&tv[FORL_STEP]);
}

static int rec_for_i64mode(cTValue *tv)
{
  if (!rec_lua54_tv_isinteger(&tv[FORL_IDX]) ||
      !rec_lua54_tv_isinteger(&tv[FORL_STOP]) ||
      !rec_lua54_tv_isinteger(&tv[FORL_STEP]))
    return 0;
  return rec_lua54_tv_i64(&tv[FORL_STEP]) != 0;
}

static LoopEvent rec_for_i64_iter(IROp *op, cTValue *o, int isforl)
{
  int64_t idx = rec_lua54_tv_i64(&o[FORL_IDX]);
  int64_t stop = rec_lua54_tv_i64(&o[FORL_STOP]);
  int64_t step = rec_lua54_tv_i64(&o[FORL_STEP]);
  if (isforl)
    idx = (int64_t)((lua_Unsigned)idx + (lua_Unsigned)step);
  if (step > 0) {
    if (idx <= stop) {
      *op = IR_LE;
      return LOOPEV_ENTER;
    }
    *op = IR_GT;
  } else {
    if (idx >= stop) {
      *op = IR_GE;
      return LOOPEV_ENTER;
    }
    *op = IR_LT;
  }
  return LOOPEV_LEAVE;
}

static TRef rec_for_i64_rawarg(jit_State *J, BCReg slot)
{
  return rec_lua54_i64ref(J, getslot(J, slot));
}

static void rec_for_i64_check(jit_State *J, TRef stop, TRef step, int dir)
{
  TRef zero = lj_ir_kint64(J, 0);
  if (dir) {
    TRef maxv = lj_ir_kint64(J, (uint64_t)LUA_MAXINTEGER);
    TRef maxstep;
    emitir(IRTG(IR_GT, IRT_I64), step, zero);
    maxstep = emitir(IRT(IR_SUB, IRT_I64), maxv, step);
    emitir(IRTG(IR_LE, IRT_I64), stop, maxstep);
  } else {
    TRef minv = lj_ir_kint64(J, (uint64_t)LUA_MININTEGER);
    TRef minstep;
    emitir(IRTG(IR_LT, IRT_I64), step, zero);
    minstep = emitir(IRT(IR_SUB, IRT_I64), minv, step);
    emitir(IRTG(IR_GE, IRT_I64), stop, minstep);
  }
}

static void rec_for_loop_i64(jit_State *J, const BCIns *fori, ScEvEntry *scev,
			     int init)
{
  BCReg ra = bc_a(*fori);
  cTValue *tv = &J->L->base[ra];
  TRef stop, step, idxobj, idxslot, idx;
  int64_t rv = rec_lua54_tv_i64(&tv[FORL_IDX]);
  int dir;
  if (!rec_for_i64mode(tv))
    lj_trace_err(J, LJ_TRERR_GFAIL);
  dir = rec_lua54_tv_i64(&tv[FORL_STEP]) > 0;
  stop = rec_for_i64_rawarg(J, ra+FORL_STOP);
  step = rec_for_i64_rawarg(J, ra+FORL_STEP);
  idxslot = idxobj = getslot(J, ra+FORL_IDX);
  idx = rec_lua54_i64ref(J, idxobj);
  rec_for_i64_check(J, stop, step, dir);
  if (!init) {
    int64_t istep = rec_lua54_tv_i64(&tv[FORL_STEP]);
    rv = (int64_t)((lua_Unsigned)rv + (lua_Unsigned)istep);
    idx = emitir(IRT(IR_ADD, IRT_I64), idx, step);
    idxobj = rec_lua54_i64result(J, idx, rv);
#if LJ_TARGET_ARM64
    if (tvisi64(&tv[FORL_IDX]) &&
	rv >= LJ_LUA54_I32_MIN && rv <= LJ_LUA54_I32_MAX)
      idxslot = lj_ir_call(J, IRCALL_lj_obj_newint64, idx);
    else
#endif
      idxslot = idxobj;
    J->base[ra+FORL_IDX] = idxslot;
  }
  J->base[ra+FORL_STOP] = getslot(J, ra+FORL_STOP);
  J->base[ra+FORL_STEP] = getslot(J, ra+FORL_STEP);
  J->base[ra+FORL_EXT] = idxobj;
  scev->t.irt = IRT_I64;
  scev->dir = dir;
  scev->stop = tref_ref(stop);
  scev->step = tref_ref(step);
  scev->start = 0;
  scev->idx = tref_ref(idx);
  setmref(scev->pc, fori);
  J->maxslot = ra+FORL_EXT+1;
}
#endif

/* Return the direction of the FOR loop iterator.
** It's important to exactly reproduce the semantics of the interpreter.
*/
static int rec_for_direction(cTValue *o)
{
  return (tvisint(o) ? intV(o) : (int32_t)o->u32.hi) >= 0;
}

/* Simulate the runtime behavior of the FOR loop iterator. */
static LoopEvent rec_for_iter(IROp *op, cTValue *o, int isforl)
{
  lua_Number stopv = numberVnum(&o[FORL_STOP]);
  lua_Number idxv = numberVnum(&o[FORL_IDX]);
  lua_Number stepv = numberVnum(&o[FORL_STEP]);
  if (isforl)
    idxv += stepv;
  if (rec_for_direction(&o[FORL_STEP])) {
    if (idxv <= stopv) {
      *op = IR_LE;
      return idxv + 2*stepv > stopv ? LOOPEV_ENTERLO : LOOPEV_ENTER;
    }
    *op = IR_GT; return LOOPEV_LEAVE;
  } else {
    if (stopv <= idxv) {
      *op = IR_GE;
      return idxv + 2*stepv < stopv ? LOOPEV_ENTERLO : LOOPEV_ENTER;
    }
    *op = IR_LT; return LOOPEV_LEAVE;
  }
}

/* Record checks for FOR loop overflow and step direction. */
static void rec_for_check(jit_State *J, IRType t, int dir,
			  TRef stop, TRef step, int init)
{
  if (!tref_isk(step)) {
    /* Non-constant step: need a guard for the direction. */
    TRef zero = (t == IRT_INT) ? lj_ir_kint(J, 0) : lj_ir_knum_zero(J);
    emitir(IRTG(dir ? IR_GE : IR_LT, t), step, zero);
    /* Add hoistable overflow checks for a narrowed FORL index. */
    if (init && t == IRT_INT) {
      if (tref_isk(stop)) {
	/* Constant stop: optimize check away or to a range check for step. */
	int32_t k = IR(tref_ref(stop))->i;
	if (dir) {
	  if (k > 0)
	    emitir(IRTGI(IR_LE), step, lj_ir_kint(J, (int32_t)0x7fffffff-k));
	} else {
	  if (k < 0)
	    emitir(IRTGI(IR_GE), step, lj_ir_kint(J, (int32_t)0x80000000-k));
	}
      } else {
	/* Stop+step variable: need full overflow check. */
	TRef tr = emitir(IRTGI(IR_ADDOV), step, stop);
	emitir(IRTI(IR_USE), tr, 0);  /* ADDOV is weak. Avoid dead result. */
      }
    }
  } else if (init && t == IRT_INT && !tref_isk(stop)) {
    /* Constant step: optimize overflow check to a range check for stop. */
    int32_t k = IR(tref_ref(step))->i;
    k = (int32_t)(dir ? 0x7fffffff : 0x80000000) - k;
    emitir(IRTGI(dir ? IR_LE : IR_GE), stop, lj_ir_kint(J, k));
  }
}

/* Record a FORL instruction. */
static void rec_for_loop(jit_State *J, const BCIns *fori, ScEvEntry *scev,
			 int init)
{
  BCReg ra = bc_a(*fori);
  cTValue *tv = &J->L->base[ra];
  TRef idx = J->base[ra+FORL_IDX];
#if LJ_54 && LJ_DUALNUM
  if (rec_for_hasi64(tv)) {
    rec_for_loop_i64(J, fori, scev, init);
    return;
  }
  int intmode = rec_for_lua54_intmode(tv);
  IRType t = idx ? tref_type(idx) :
	     intmode ? lj_opt_narrow_forl(J, tv) : IRT_NUM;
  if (!intmode)
    t = IRT_NUM;
#else
  IRType t = idx ? tref_type(idx) :
	     (init || LJ_DUALNUM) ? lj_opt_narrow_forl(J, tv) : IRT_NUM;
#endif
#if LJ_54 && LJ_DUALNUM
  if (t != IRT_INT && rec_for_allint(tv))
    lj_trace_err(J, LJ_TRERR_GFAIL);  /* Keep boundary int loops correct. */
#endif
  int mode = IRSLOAD_INHERIT +
    ((!LJ_DUALNUM || tvisint(tv) == (t == IRT_INT)) ? IRSLOAD_READONLY : 0);
  TRef stop = fori_arg(J, fori, ra+FORL_STOP, t, mode);
  TRef step = fori_arg(J, fori, ra+FORL_STEP, t, mode);
  int tc, dir = rec_for_direction(&tv[FORL_STEP]);
  lj_assertJ(bc_op(*fori) == BC_FORI || bc_op(*fori) == BC_JFORI,
	     "bad bytecode %d instead of FORI/JFORI", bc_op(*fori));
  scev->t.irt = t;
  scev->dir = dir;
  scev->stop = tref_ref(stop);
  scev->step = tref_ref(step);
  rec_for_check(J, t, dir, stop, step, init);
  scev->start = tref_ref(find_kinit(J, fori, ra+FORL_IDX, IRT_INT));
  tc = (LJ_DUALNUM &&
	!(scev->start && irref_isk(scev->stop) && irref_isk(scev->step) &&
	  tvisint(&tv[FORL_IDX]) == (t == IRT_INT))) ?
	IRSLOAD_TYPECHECK : 0;
  if (tc) {
    J->base[ra+FORL_STOP] = stop;
    J->base[ra+FORL_STEP] = step;
  }
  if (idx)
    idx = fori_conv(J, idx, t);
  if (!idx)
    idx = fori_load(J, ra+FORL_IDX, t,
		    IRSLOAD_INHERIT + tc + (J->scev.start << 16));
  if (!init)
    J->base[ra+FORL_IDX] = idx = emitir(IRT(IR_ADD, t), idx, step);
  J->base[ra+FORL_EXT] = idx;
  scev->idx = tref_ref(idx);
  setmref(scev->pc, fori);
  J->maxslot = ra+FORL_EXT+1;
}

/* Record FORL/JFORL or FORI/JFORI. */
static LoopEvent rec_for(jit_State *J, const BCIns *fori, int isforl)
{
  BCReg ra = bc_a(*fori);
  TValue *tv = &J->L->base[ra];
  TRef *tr = &J->base[ra];
  IROp op;
  LoopEvent ev;
  TRef stop;
  IRType t;
  /* Avoid semantic mismatches and always failing guards. */
#if LJ_54 && LJ_DUALNUM
  if (rec_for_hasi64(tv)) {
    TRef stop;
    ScEvEntry scev;
    if (!isforl)
      lj_trace_err(J, LJ_TRERR_GFAIL);
    rec_for_loop_i64(J, fori, &scev, 0);
    t = IRT_I64;
    stop = scev.stop;
    ev = rec_for_i64_iter(&op, tv, isforl);
    if (ev == LOOPEV_LEAVE) {
      J->maxslot = ra+FORL_EXT+1;
      J->pc = fori+1;
    } else {
      J->maxslot = ra;
      J->pc = fori+bc_j(*fori)+1;
    }
    lj_snap_add(J);

    emitir(IRTG(op, t), scev.idx, stop);

    if (ev == LOOPEV_LEAVE) {
      J->maxslot = ra;
      J->pc = fori+bc_j(*fori)+1;
    } else {
      J->maxslot = ra+FORL_EXT+1;
      J->pc = fori+1;
    }
    J->needsnap = 1;
    return ev;
  }
#endif
  if ((tvisnum(&tv[FORL_IDX]) && tvisnan(&tv[FORL_IDX])) ||
      (tvisnum(&tv[FORL_STOP]) && tvisnan(&tv[FORL_STOP])) ||
      (tvisnum(&tv[FORL_STEP]) && tvisnan(&tv[FORL_STEP])) ||
      tvismzero(&tv[FORL_STEP]))
    lj_trace_err(J, LJ_TRERR_GFAIL);
  if (isforl) {  /* Handle FORL/JFORL opcodes. */
    TRef idx = tr[FORL_IDX];
    if (mref(J->scev.pc, const BCIns) == fori && tref_ref(idx) == J->scev.idx) {
      t = J->scev.t.irt;
      stop = J->scev.stop;
      idx = emitir(IRT(IR_ADD, t), idx, J->scev.step);
      tr[FORL_EXT] = tr[FORL_IDX] = idx;
    } else {
      ScEvEntry scev;
      rec_for_loop(J, fori, &scev, 0);
      t = scev.t.irt;
      stop = scev.stop;
    }
  } else {  /* Handle FORI/JFORI opcodes. */
    BCReg i;
    lj_meta_for(J->L, tv);
#if LJ_54 && LJ_DUALNUM
    t = rec_for_lua54_intmode(tv) ? lj_opt_narrow_forl(J, tv) : IRT_NUM;
#else
    t = (LJ_DUALNUM || tref_isint(tr[FORL_IDX])) ? lj_opt_narrow_forl(J, tv) :
						   IRT_NUM;
#endif
#if LJ_54 && LJ_DUALNUM
    if (t != IRT_INT && rec_for_allint(tv))
      lj_trace_err(J, LJ_TRERR_GFAIL);
#endif
    for (i = FORL_IDX; i <= FORL_STEP; i++) {
      if (!tr[i]) sload(J, ra+i);
      lj_assertJ(tref_isnumber_str(tr[i]), "bad FORI argument type");
      if (tref_isstr(tr[i]))
	tr[i] = emitir(IRTG(IR_STRTO, IRT_NUM), tr[i], 0);
      tr[i] = fori_conv(J, tr[i], t);
    }
    tr[FORL_EXT] = tr[FORL_IDX];
    stop = tr[FORL_STOP];
    rec_for_check(J, t, rec_for_direction(&tv[FORL_STEP]),
		  stop, tr[FORL_STEP], 1);
  }

  ev = rec_for_iter(&op, tv, isforl);
  if (ev == LOOPEV_LEAVE) {
    J->maxslot = ra+FORL_EXT+1;
    J->pc = fori+1;
  } else {
    J->maxslot = ra;
    J->pc = fori+bc_j(*fori)+1;
  }
  lj_snap_add(J);

  emitir(IRTG(op, t), tr[FORL_IDX], stop);

  if (ev == LOOPEV_LEAVE) {
    J->maxslot = ra;
    J->pc = fori+bc_j(*fori)+1;
  } else {
    J->maxslot = ra+FORL_EXT+1;
    J->pc = fori+1;
  }
  J->needsnap = 1;
  return ev;
}

/* Record ITERL/JITERL. */
static LoopEvent rec_iterl(jit_State *J, const BCIns iterins)
{
  BCReg ra = bc_a(iterins);
#if LJ_54
  if (J->L->closelist != NULL)
    lj_trace_err_info(J, LJ_TRERR_NYIBC);
#if LJ_TARGET_ARM64
  {
    cTValue *b = &J->L->base[ra-3];
    if (tvisfunc(b) && funcV(b)->c.ffid == FF_next &&
	tvistab(b+1)) {
      GCtab *t = tabV(b+1);
      if (t->asize != 0 || tvisint(&J->L->base[ra]))
	lj_trace_err_info(J, LJ_TRERR_NYIBC);
    }
  }
#endif
#endif
  if (!tref_isnil(getslot(J, ra))) {  /* Looping back? */
    J->base[ra-1] = J->base[ra];  /* Copy result of ITERC to control var. */
    J->maxslot = ra-1+bc_b(J->pc[-1]);
    J->pc += bc_j(iterins)+1;
    return LOOPEV_ENTER;
  } else {
    J->maxslot = ra-3;
    J->pc++;
    return LOOPEV_LEAVE;
  }
}

/* Record LOOP/JLOOP. Now, that was easy. */
static LoopEvent rec_loop(jit_State *J, BCReg ra, int skip)
{
  if (ra < J->maxslot) J->maxslot = ra;
  J->pc += skip;
  return LOOPEV_ENTER;
}

/* Check if a loop repeatedly failed to trace because it didn't loop back. */
static int innerloopleft(jit_State *J, const BCIns *pc)
{
  ptrdiff_t i;
  for (i = 0; i < PENALTY_SLOTS; i++)
    if (mref(J->penalty[i].pc, const BCIns) == pc) {
      if ((J->penalty[i].reason == LJ_TRERR_LLEAVE ||
	   J->penalty[i].reason == LJ_TRERR_LINNER) &&
	  J->penalty[i].val >= 2*PENALTY_MIN)
	return 1;
      break;
    }
  return 0;
}

/* Handle the case when an interpreted loop op is hit. */
static void rec_loop_interp(jit_State *J, const BCIns *pc, LoopEvent ev)
{
  if (J->parent == 0 && J->exitno == 0) {
    if (pc == J->startpc && J->framedepth + J->retdepth == 0) {
      if (bc_op(J->cur.startins) == BC_ITERN) return;  /* See rec_itern(). */
      /* Same loop? */
      if (ev == LOOPEV_LEAVE)  /* Must loop back to form a root trace. */
	lj_trace_err(J, LJ_TRERR_LLEAVE);
      lj_record_stop(J, LJ_TRLINK_LOOP, J->cur.traceno);  /* Looping trace. */
    } else if (ev != LOOPEV_LEAVE) {  /* Entering inner loop? */
      /* It's usually better to abort here and wait until the inner loop
      ** is traced. But if the inner loop repeatedly didn't loop back,
      ** this indicates a low trip count. In this case try unrolling
      ** an inner loop even in a root trace. But it's better to be a bit
      ** more conservative here and only do it for very short loops.
      */
      if (bc_j(*pc) != -1 && !innerloopleft(J, pc))
	lj_trace_err(J, LJ_TRERR_LINNER);  /* Root trace hit an inner loop. */
      if ((ev != LOOPEV_ENTERLO &&
	   J->loopref && J->cur.nins - J->loopref > 24) || --J->loopunroll < 0)
	lj_trace_err(J, LJ_TRERR_LUNROLL);  /* Limit loop unrolling. */
      J->loopref = J->cur.nins;
    }
  } else if (ev != LOOPEV_LEAVE) {  /* Side trace enters an inner loop. */
    J->loopref = J->cur.nins;
    if (--J->loopunroll < 0)
      lj_trace_err(J, LJ_TRERR_LUNROLL);  /* Limit loop unrolling. */
  }  /* Side trace continues across a loop that's left or not entered. */
}

/* Handle the case when an already compiled loop op is hit. */
static void rec_loop_jit(jit_State *J, TraceNo lnk, LoopEvent ev)
{
  if (J->parent == 0 && J->exitno == 0) {  /* Root trace hit an inner loop. */
    /* Better let the inner loop spawn a side trace back here. */
    lj_trace_err(J, LJ_TRERR_LINNER);
  } else if (ev != LOOPEV_LEAVE) {  /* Side trace enters a compiled loop. */
    J->instunroll = 0;  /* Cannot continue across a compiled loop op. */
    if (J->pc == J->startpc && J->framedepth + J->retdepth == 0)
      lj_record_stop(J, LJ_TRLINK_LOOP, J->cur.traceno);  /* Form extra loop. */
    else
      lj_record_stop(J, LJ_TRLINK_ROOT, lnk);  /* Link to the loop. */
  }  /* Side trace continues across a loop that's left or not entered. */
}

/* Record ITERN. */
static LoopEvent rec_itern(jit_State *J, BCReg ra, BCReg rb)
{
#if LJ_BE
  /* YAGNI: Disabled on big-endian due to issues with lj_vm_next,
  ** IR_HIOP, RID_RETLO/RID_RETHI and ra_destpair.
  */
  UNUSED(ra); UNUSED(rb);
  setintV(&J->errinfo, (int32_t)BC_ITERN);
  lj_trace_err_info(J, LJ_TRERR_NYIBC);
#else
  RecordIndex ix;
  /* Since ITERN is recorded at the start, we need our own loop detection. */
  if (J->pc == J->startpc &&
      J->framedepth + J->retdepth == 0 && J->parent == 0 && J->exitno == 0) {
    IRRef ref = REF_FIRST + LJ_HASPROFILE;
#ifdef LUAJIT_ENABLE_CHECKHOOK
    ref += 3;
#endif
    if (J->cur.nins > ref ||
       (LJ_HASPROFILE && J->cur.nins == ref && J->cur.ir[ref-1].o != IR_PROF)) {
      J->instunroll = 0;  /* Cannot continue unrolling across an ITERN. */
      lj_record_stop(J, LJ_TRLINK_LOOP, J->cur.traceno);  /* Looping trace. */
      return LOOPEV_ENTER;
    }
  }
  J->maxslot = ra;
  lj_snap_add(J);  /* Required to make JLOOP the first ins in a side-trace. */
  ix.tab = getslot(J, ra-2);
  ix.key = J->base[ra-1] ? J->base[ra-1] :
	   sloadt(J, (int32_t)(ra-1), IRT_GUARD|IRT_INT,
		  IRSLOAD_TYPECHECK|IRSLOAD_KEYINDEX);
  copyTV(J->L, &ix.tabv, &J->L->base[ra-2]);
  copyTV(J->L, &ix.keyv, &J->L->base[ra-1]);
  ix.idxchain = (rb < 3);  /* Omit value type check, if unused. */
  ix.mobj = 1;  /* We need the next index, too. */
  J->maxslot = ra + lj_record_next(J, &ix);
  J->needsnap = 1;
  if (!tref_isnil(ix.key)) {  /* Looping back? */
    J->base[ra-1] = ix.mobj | TREF_KEYINDEX;  /* Control var has next index. */
    J->base[ra] = ix.key;
    J->base[ra+1] = ix.val;
    J->pc += bc_j(J->pc[1])+2;
    return LOOPEV_ENTER;
  } else {
    J->maxslot = ra-3;
    J->pc += 2;
    return LOOPEV_LEAVE;
  }
#endif
}

/* Record ISNEXT. */
static void rec_isnext(jit_State *J, BCReg ra)
{
  cTValue *b = &J->L->base[ra-3];
  if (tvisfunc(b) && funcV(b)->c.ffid == FF_next &&
      tvistab(b+1) && tvisnil(b+2)) {
    /* These checks are folded away for a compiled pairs(). */
    TRef func = getslot(J, ra-3);
    TRef trid = emitir(IRT(IR_FLOAD, IRT_U8), func, IRFL_FUNC_FFID);
    emitir(IRTGI(IR_EQ), trid, lj_ir_kint(J, FF_next));
    (void)getslot(J, ra-2); /* Type check for table. */
    (void)getslot(J, ra-1); /* Type check for nil key. */
    J->base[ra-1] = lj_ir_kint(J, 0) | TREF_KEYINDEX;
    J->maxslot = ra;
  } else {  /* Abort trace. Interpreter will despecialize bytecode. */
    lj_trace_err(J, LJ_TRERR_RECERR);
  }
}

/* -- Record profiler hook checks ----------------------------------------- */

#if LJ_HASPROFILE

/* Need to insert profiler hook check? */
static int rec_profile_need(jit_State *J, GCproto *pt, const BCIns *pc)
{
  GCproto *ppt;
  lj_assertJ(J->prof_mode == 'f' || J->prof_mode == 'l',
	     "bad profiler mode %c", J->prof_mode);
  if (!pt)
    return 0;
  ppt = J->prev_pt;
  J->prev_pt = pt;
  if (pt != ppt && ppt) {
    J->prev_line = -1;
    return 1;
  }
  if (J->prof_mode == 'l') {
    BCLine line = lj_debug_line(pt, proto_bcpos(pt, pc));
    BCLine pline = J->prev_line;
    J->prev_line = line;
    if (pline != line)
      return 1;
  }
  return 0;
}

static void rec_profile_ins(jit_State *J, const BCIns *pc)
{
  if (J->prof_mode && rec_profile_need(J, J->pt, pc)) {
    emitir(IRTG(IR_PROF, IRT_NIL), 0, 0);
    lj_snap_add(J);
  }
}

static void rec_profile_ret(jit_State *J)
{
  if (J->prof_mode == 'f') {
    emitir(IRTG(IR_PROF, IRT_NIL), 0, 0);
    J->prev_pt = NULL;
    lj_snap_add(J);
  }
}

#endif

/* -- Record calls and returns -------------------------------------------- */

/* Specialize to the runtime value of the called function or its prototype. */
static TRef rec_call_specialize(jit_State *J, GCfunc *fn, TRef tr)
{
  TRef kfunc;
  if (isluafunc(fn)) {
    GCproto *pt = funcproto(fn);
    /* Too many closures created? Probably not a monomorphic function. */
    if (pt->flags >= PROTO_CLC_POLY) {  /* Specialize to prototype instead. */
      TRef trpt = emitir(IRT(IR_FLOAD, IRT_PGC), tr, IRFL_FUNC_PC);
      emitir(IRTG(IR_EQ, IRT_PGC), trpt, lj_ir_kptr(J, proto_bc(pt)));
      (void)lj_ir_kgc(J, obj2gco(pt), IRT_PROTO);  /* Prevent GC of proto. */
      return tr;
    }
  } else {
    /* Don't specialize to non-monomorphic builtins. */
    switch (fn->c.ffid) {
    case FF_coroutine_wrap_aux:
    case FF_string_gmatch_aux:
      /* NYI: io_file_iter doesn't have an ffid, yet. */
      {  /* Specialize to the ffid. */
	TRef trid = emitir(IRT(IR_FLOAD, IRT_U8), tr, IRFL_FUNC_FFID);
	emitir(IRTGI(IR_EQ), trid, lj_ir_kint(J, fn->c.ffid));
      }
      return tr;
    default:
      /* NYI: don't specialize to non-monomorphic C functions. */
      break;
    }
  }
  /* Otherwise specialize to the function (closure) value itself. */
  kfunc = lj_ir_kfunc(J, fn);
  emitir(IRTG(IR_EQ, IRT_FUNC), tr, kfunc);
  return kfunc;
}

/* Record call setup. */
static void rec_call_setup(jit_State *J, BCReg func, ptrdiff_t nargs)
{
  RecordIndex ix;
  TValue *functv = &J->L->base[func];
  TRef kfunc, *fbase = &J->base[func];
  ptrdiff_t i;
  (void)getslot(J, func); /* Ensure func has a reference. */
  for (i = 1; i <= nargs; i++)
    (void)getslot(J, func+LJ_FR2+i);  /* Ensure all args have a reference. */
  if (!tref_isfunc(fbase[0])) {  /* Resolve __call metamethod. */
    ix.tab = fbase[0];
    copyTV(J->L, &ix.tabv, functv);
    if (!lj_record_mm_lookup(J, &ix, MM_call) || !tref_isfunc(ix.mobj))
      lj_trace_err(J, LJ_TRERR_NOMM);
    for (i = ++nargs; i > LJ_FR2; i--)  /* Shift arguments up. */
      fbase[i+LJ_FR2] = fbase[i+LJ_FR2-1];
#if LJ_FR2
    fbase[2] = fbase[0];
#endif
    fbase[0] = ix.mobj;  /* Replace function. */
    functv = &ix.mobjv;
  }
  kfunc = rec_call_specialize(J, funcV(functv), fbase[0]);
#if LJ_FR2
  fbase[0] = kfunc;
  fbase[1] = TREF_FRAME;
#else
  fbase[0] = kfunc | TREF_FRAME;
#endif
  J->maxslot = (BCReg)nargs;
}

/* Record call. */
void lj_record_call(jit_State *J, BCReg func, ptrdiff_t nargs)
{
  rec_call_setup(J, func, nargs);
  /* Bump frame. */
  J->framedepth++;
  J->base += func+1+LJ_FR2;
  J->baseslot += func+1+LJ_FR2;
  if (J->baseslot + J->maxslot >= LJ_MAX_JSLOTS)
    lj_trace_err(J, LJ_TRERR_STACKOV);
}

/* Record tail call. */
void lj_record_tailcall(jit_State *J, BCReg func, ptrdiff_t nargs)
{
  rec_call_setup(J, func, nargs);
  if (frame_isvarg(J->L->base - 1)) {
    BCReg cbase = (BCReg)frame_delta(J->L->base - 1);
    if (--J->framedepth < 0)
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    func += cbase;
  }
  /* Move func + args down. */
  if (LJ_FR2 && J->baseslot == 2)
    J->base[func+1] = TREF_FRAME;
  memmove(&J->base[-1-LJ_FR2], &J->base[func], sizeof(TRef)*(J->maxslot+1+LJ_FR2));
  /* Note: the new TREF_FRAME is now at J->base[-1] (even for slot #0). */
  /* Tailcalls can form a loop, so count towards the loop unroll limit. */
  if (++J->tailcalled > J->loopunroll)
    lj_trace_err(J, LJ_TRERR_LUNROLL);
}

/* Check unroll limits for down-recursion. */
static int check_downrec_unroll(jit_State *J, GCproto *pt)
{
  IRRef ptref;
  for (ptref = J->chain[IR_KGC]; ptref; ptref = IR(ptref)->prev)
    if (ir_kgc(IR(ptref)) == obj2gco(pt)) {
      int count = 0;
      IRRef ref;
      for (ref = J->chain[IR_RETF]; ref; ref = IR(ref)->prev)
	if (IR(ref)->op1 == ptref)
	  count++;
      if (count) {
	if (J->pc == J->startpc) {
	  if (count + J->tailcalled > J->param[JIT_P_recunroll])
	    return 1;
	} else {
	  lj_trace_err(J, LJ_TRERR_DOWNREC);
	}
      }
    }
  return 0;
}

static TRef rec_cat(jit_State *J, BCReg baseslot, BCReg topslot);

/* Record return. */
void lj_record_ret(jit_State *J, BCReg rbase, ptrdiff_t gotresults)
{
  TValue *frame = J->L->base - 1;
  ptrdiff_t i;
  BCReg baseadj = 0;
  for (i = 0; i < gotresults; i++)
    (void)getslot(J, rbase+i);  /* Ensure all results have a reference. */
  while (frame_ispcall(frame)) {  /* Immediately resolve pcall() returns. */
    BCReg cbase = (BCReg)frame_delta(frame);
    if (--J->framedepth <= 0)
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    lj_assertJ(J->baseslot > 1+LJ_FR2, "bad baseslot for return");
    gotresults++;
    baseadj += cbase;
    rbase += cbase;
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    J->base[--rbase] = TREF_TRUE;  /* Prepend true to results. */
    frame = frame_prevd(frame);
    J->needsnap = 1;  /* Stop catching on-trace errors. */
  }
  /* Return to lower frame via interpreter for unhandled cases. */
  if (J->framedepth == 0 && J->pt && bc_isret(bc_op(*J->pc)) &&
       (!frame_islua(frame) ||
	(J->parent == 0 && J->exitno == 0 &&
	 !bc_isret(bc_op(J->cur.startins))))) {
    /* NYI: specialize to frame type and return directly, not via RET*. */
    for (i = 0; i < (ptrdiff_t)rbase; i++)
      J->base[i] = 0;  /* Purge dead slots. */
#if LJ_54 && LJ_DUALNUM && LJ_TARGET_ARM64
    /* A Lua 5.4 bitop/arith result that was kept as an unboxed IRT_I64 for an
    ** in-trace consumer must be materialized before returning to the
    ** interpreter (e.g. a bitwise metamethod result crossing lj_cont_ra): the
    ** RETURN tail-link's asm_stack_restore would otherwise store the raw 64-bit
    ** value tagged as an integer TValue, truncating it to the low 32 bits.
    ** Side exits already box via snap_restoreval; this is its tail-link
    ** counterpart, mirroring the raw-i64 boxing done for table-value stores. */
    for (i = 0; i < gotresults; i++)
      if (J->base[rbase+i])
	J->base[rbase+i] = rec_lua54_boxraw_i64(J, J->base[rbase+i]);
#endif
    J->maxslot = rbase + (BCReg)gotresults;
    lj_record_stop(J, LJ_TRLINK_RETURN, 0);  /* Return to interpreter. */
    return;
  }
  if (frame_isvarg(frame)) {
    BCReg cbase = (BCReg)frame_delta(frame);
    if (--J->framedepth < 0)  /* NYI: return of vararg func to lower frame. */
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    lj_assertJ(J->baseslot > 1+LJ_FR2, "bad baseslot for return");
    baseadj += cbase;
    rbase += cbase;
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    frame = frame_prevd(frame);
  }
  if (frame_islua(frame)) {  /* Return to Lua frame. */
    BCIns callins = *(frame_pc(frame)-1);
    ptrdiff_t nresults = bc_b(callins) ? (ptrdiff_t)bc_b(callins)-1 :gotresults;
    BCReg cbase = bc_a(callins);
    GCproto *pt = funcproto(frame_func(frame - (cbase+1+LJ_FR2)));
    if ((pt->flags & PROTO_NOJIT))
      lj_trace_err(J, LJ_TRERR_CJITOFF);
    if (J->framedepth == 0 && J->pt && frame == J->L->base - 1) {
      if (!J->cur.root && check_downrec_unroll(J, pt)) {
	J->maxslot = (BCReg)(rbase + gotresults);
	lj_snap_purge(J);
	lj_record_stop(J, LJ_TRLINK_DOWNREC, J->cur.traceno);  /* Down-rec. */
	return;
      }
      lj_snap_add(J);
    }
    for (i = 0; i < nresults; i++)  /* Adjust results. */
      J->base[i-1-LJ_FR2] = i < gotresults ? J->base[rbase+i] : TREF_NIL;
    J->maxslot = cbase+(BCReg)nresults;
    if (J->framedepth > 0) {  /* Return to a frame that is part of the trace. */
      J->framedepth--;
      lj_assertJ(J->baseslot > cbase+1+LJ_FR2, "bad baseslot for return");
      J->baseslot -= cbase+1+LJ_FR2;
      J->base -= cbase+1+LJ_FR2;
    } else if (J->parent == 0 && J->exitno == 0 &&
	       !bc_isret(bc_op(J->cur.startins))) {
      /* Return to lower frame would leave the loop in a root trace. */
      lj_trace_err(J, LJ_TRERR_LLEAVE);
    } else if (J->needsnap) {  /* Tailcalled to ff with side-effects. */
      lj_trace_err(J, LJ_TRERR_NYIRETL);  /* No way to insert snapshot here. */
    } else if (1 + pt->framesize >= LJ_MAX_JSLOTS ||
	       J->baseslot + J->maxslot >= LJ_MAX_JSLOTS) {
      lj_trace_err(J, LJ_TRERR_STACKOV);
    } else {  /* Return to lower frame. Guard for the target we return to. */
      TRef trpt = lj_ir_kgc(J, obj2gco(pt), IRT_PROTO);
      TRef trpc = lj_ir_kptr(J, (void *)frame_pc(frame));
      emitir(IRTG(IR_RETF, IRT_PGC), trpt, trpc);
      J->retdepth++;
      J->needsnap = 1;
      J->scev.idx = REF_NIL;
      lj_assertJ(J->baseslot == 1+LJ_FR2, "bad baseslot for return");
      /* Shift result slots up and clear the slots of the new frame below. */
      memmove(J->base + cbase, J->base-1-LJ_FR2, sizeof(TRef)*nresults);
      memset(J->base-1-LJ_FR2, 0, sizeof(TRef)*(cbase+1+LJ_FR2));
    }
  } else if (frame_iscont(frame)) {  /* Return to continuation frame. */
    ASMFunction cont = frame_contf(frame);
    BCReg cbase = (BCReg)frame_delta(frame);
    if ((J->framedepth -= 2) < 0)
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    J->maxslot = cbase-(2<<LJ_FR2);
    if (cont == lj_cont_ra) {
      /* Copy result to destination slot. */
      BCReg dst = bc_a(*(frame_contpc(frame)-1));
      J->base[dst] = gotresults ? J->base[cbase+rbase] : TREF_NIL;
      if (dst >= J->maxslot) {
	J->maxslot = dst+1;
      }
    } else if (cont == lj_cont_nop) {
      /* Nothing to do here. */
    } else if (cont == lj_cont_cat) {
      BCReg bslot = bc_b(*(frame_contpc(frame)-1));
      TRef tr = gotresults ? J->base[cbase+rbase] : TREF_NIL;
      if (bslot != J->maxslot) {  /* Concatenate the remainder. */
	/* Simulate lower frame and result. */
	TValue *b = J->L->base - baseadj, save;
	/* Can't handle MM_concat + CALLT + fast func side-effects. */
	if (J->postproc != LJ_POST_NONE)
	  lj_trace_err(J, LJ_TRERR_NYIRETL);
	J->base[J->maxslot] = tr;
	copyTV(J->L, &save, b-(2<<LJ_FR2));
	if (gotresults)
	  copyTV(J->L, b-(2<<LJ_FR2), b+rbase);
	else
	  setnilV(b-(2<<LJ_FR2));
	J->L->base = b - cbase;
	tr = rec_cat(J, bslot, cbase-(2<<LJ_FR2));
	b = J->L->base + cbase;  /* Undo. */
	J->L->base = b + baseadj;
	copyTV(J->L, b-(2<<LJ_FR2), &save);
      }
      if (tr >= 0xffffff00) {
	lj_err_throw(J->L, -(int32_t)tr);  /* Propagate errors. */
      } else if (tr) {  /* Store final result. */
	BCReg dst = bc_a(*(frame_contpc(frame)-1));
	J->base[dst] = tr;
	if (dst >= J->maxslot) {
	  J->maxslot = dst+1;
	}
      }  /* Otherwise continue with another __concat call. */
    } else {
      /* Result type already specialized. */
      lj_assertJ(cont == lj_cont_condf || cont == lj_cont_condt,
		 "bad continuation type");
    }
  } else {
    lj_trace_err(J, LJ_TRERR_NYIRETL);  /* NYI: handle return to C frame. */
  }
  lj_assertJ(J->baseslot >= 1+LJ_FR2, "bad baseslot for return");
}

/* -- Metamethod handling ------------------------------------------------- */

/* Prepare to record call to metamethod. */
static BCReg rec_mm_prep(jit_State *J, ASMFunction cont)
{
  BCReg s, top = cont == lj_cont_cat ? J->maxslot : curr_proto(J->L)->framesize;
#if LJ_FR2
  J->base[top] = lj_ir_k64(J, IR_KNUM, u64ptr(contptr(cont)));
  J->base[top+1] = TREF_CONT;
#else
  J->base[top] = lj_ir_kptr(J, contptr(cont)) | TREF_CONT;
#endif
  J->framedepth++;
  for (s = J->maxslot; s < top; s++)
    J->base[s] = 0;  /* Clear frame gap to avoid resurrecting previous refs. */
  return top+1+LJ_FR2;
}

/* Record metamethod lookup. */
int lj_record_mm_lookup(jit_State *J, RecordIndex *ix, MMS mm)
{
  RecordIndex mix;
  GCtab *mt;
  if (tref_istab(ix->tab)) {
    mt = tabref(tabV(&ix->tabv)->metatable);
    mix.tab = emitir(IRT(IR_FLOAD, IRT_TAB), ix->tab, IRFL_TAB_META);
  } else if (tref_isudata(ix->tab)) {
    int udtype = udataV(&ix->tabv)->udtype;
    mt = tabref(udataV(&ix->tabv)->metatable);
    mix.tab = emitir(IRT(IR_FLOAD, IRT_TAB), ix->tab, IRFL_UDATA_META);
    /* The metatables of special userdata objects are treated as immutable. */
    if (udtype != UDTYPE_USERDATA) {
      cTValue *mo;
      if (LJ_HASFFI && udtype == UDTYPE_FFI_CLIB) {
	/* Specialize to the C library namespace object. */
	emitir(IRTG(IR_EQ, IRT_PGC), ix->tab, lj_ir_kptr(J, udataV(&ix->tabv)));
      } else {
	/* Specialize to the type of userdata. */
	TRef tr = emitir(IRT(IR_FLOAD, IRT_U8), ix->tab, IRFL_UDATA_UDTYPE);
	emitir(IRTGI(IR_EQ), tr, lj_ir_kint(J, udtype));
      }
  immutable_mt:
      mo = lj_tab_getstr(mt, mmname_str(J2G(J), mm));
      ix->mt = mix.tab;
      ix->mtv = mt;
      if (!mo || tvisnil(mo))
	return 0;  /* No metamethod. */
      /* Treat metamethod or index table as immutable, too. */
      if (!(tvisfunc(mo) || tvistab(mo)))
	lj_trace_err(J, LJ_TRERR_BADTYPE);
      copyTV(J->L, &ix->mobjv, mo);
      ix->mobj = lj_ir_kgc(J, gcV(mo), tvisfunc(mo) ? IRT_FUNC : IRT_TAB);
      return 1;  /* Got metamethod or index table. */
    }
  } else {
    /* Specialize to base metatable. Must flush mcode in lua_setmetatable(). */
    mt = tabref(basemt_obj(J2G(J), &ix->tabv));
    if (mt == NULL) {
      ix->mt = TREF_NIL;
      return 0;  /* No metamethod. */
    }
    /* The cdata metatable is treated as immutable. */
    if (LJ_HASFFI && tref_iscdata(ix->tab)) {
      mix.tab = TREF_NIL;
      goto immutable_mt;
    }
    ix->mt = mix.tab = lj_ir_ggfload(J, IRT_TAB,
      GG_OFS(g.gcroot[GCROOT_BASEMT+itypemap(&ix->tabv)]));
    goto nocheck;
  }
  ix->mt = mt ? mix.tab : TREF_NIL;
  emitir(IRTG(mt ? IR_NE : IR_EQ, IRT_TAB), mix.tab, lj_ir_knull(J, IRT_TAB));
nocheck:
  if (mt) {
    GCstr *mmstr = mmname_str(J2G(J), mm);
    cTValue *mo = lj_tab_getstr(mt, mmstr);
    if (mo && !tvisnil(mo))
      copyTV(J->L, &ix->mobjv, mo);
    ix->mtv = mt;
    settabV(J->L, &mix.tabv, mt);
    setstrV(J->L, &mix.keyv, mmstr);
    mix.key = lj_ir_kstr(J, mmstr);
    mix.val = 0;
    mix.idxchain = 0;
    ix->mobj = lj_record_idx(J, &mix);
    return !tref_isnil(ix->mobj);  /* 1 if metamethod found, 0 if not. */
  }
  return 0;  /* No metamethod. */
}

/* Record call to arithmetic metamethod. */
static TRef rec_mm_arith(jit_State *J, RecordIndex *ix, MMS mm)
{
  /* Set up metamethod call first to save ix->tab and ix->tabv. */
  BCReg func = rec_mm_prep(J, mm == MM_concat ? lj_cont_cat : lj_cont_ra);
  TRef *base = J->base + func;
  TValue *basev = J->L->base + func;
  base[1+LJ_FR2] = ix->tab; base[2+LJ_FR2] = ix->key;
  copyTV(J->L, basev+1+LJ_FR2, &ix->tabv);
  copyTV(J->L, basev+2+LJ_FR2, &ix->keyv);
  if (!lj_record_mm_lookup(J, ix, mm)) {  /* Lookup mm on 1st operand. */
    if (mm != MM_unm) {
      ix->tab = ix->key;
      copyTV(J->L, &ix->tabv, &ix->keyv);
      if (lj_record_mm_lookup(J, ix, mm))  /* Lookup mm on 2nd operand. */
	goto ok;
    }
    lj_trace_err(J, LJ_TRERR_NOMM);
  }
ok:
  base[0] = ix->mobj;
#if LJ_FR2
  base[1] = 0;
#endif
  copyTV(J->L, basev+0, &ix->mobjv);
  lj_record_call(J, func, 2);
  return 0;  /* No result yet. */
}

/* Record call to __len metamethod. */
static TRef rec_mm_len(jit_State *J, TRef tr, TValue *tv)
{
  RecordIndex ix;
  ix.tab = tr;
  copyTV(J->L, &ix.tabv, tv);
  if (lj_record_mm_lookup(J, &ix, MM_len)) {
    BCReg func = rec_mm_prep(J, lj_cont_ra);
    TRef *base = J->base + func;
    TValue *basev = J->L->base + func;
    base[0] = ix.mobj; copyTV(J->L, basev+0, &ix.mobjv);
    base += LJ_FR2;
    basev += LJ_FR2;
    base[1] = tr; copyTV(J->L, basev+1, tv);
#if LJ_52
    base[2] = tr; copyTV(J->L, basev+2, tv);
#else
    base[2] = TREF_NIL; setnilV(basev+2);
#endif
    lj_record_call(J, func, 2);
  } else {
    if (LJ_52 && tref_istab(tr))
      return emitir(IRTI(IR_ALEN), tr, TREF_NIL);
    lj_trace_err(J, LJ_TRERR_NOMM);
  }
  return 0;  /* No result yet. */
}

/* Call a comparison metamethod. */
static void rec_mm_callcomp(jit_State *J, RecordIndex *ix, int op)
{
  BCReg func = rec_mm_prep(J, (op&1) ? lj_cont_condf : lj_cont_condt);
  TRef *base = J->base + func + LJ_FR2;
  TValue *tv = J->L->base + func + LJ_FR2;
  base[-LJ_FR2] = ix->mobj; base[1] = ix->val; base[2] = ix->key;
  copyTV(J->L, tv-LJ_FR2, &ix->mobjv);
  copyTV(J->L, tv+1, &ix->valv);
  copyTV(J->L, tv+2, &ix->keyv);
  lj_record_call(J, func, 2);
}

/* Record call to equality comparison metamethod (for tab and udata only). */
static void rec_mm_equal(jit_State *J, RecordIndex *ix, int op)
{
  ix->tab = ix->val;
  copyTV(J->L, &ix->tabv, &ix->valv);
#if LJ_54
  if (!lj_record_mm_lookup(J, ix, MM_eq)) {  /* Try 1st operand first. */
    ix->tab = ix->key;
    copyTV(J->L, &ix->tabv, &ix->keyv);
    if (!lj_record_mm_lookup(J, ix, MM_eq))
      return;
  }
  /* Lua 5.4 accepts either side's __eq and uses the left side first, so the
  ** recorder must not require both operands to expose the same metamethod.
  */
  rec_mm_callcomp(J, ix, op);
#else
  if (lj_record_mm_lookup(J, ix, MM_eq)) {  /* Lookup mm on 1st operand. */
    cTValue *bv;
    TRef mo1 = ix->mobj;
    TValue mo1v;
    copyTV(J->L, &mo1v, &ix->mobjv);
    /* Avoid the 2nd lookup and the objcmp if the metatables are equal. */
    bv = &ix->keyv;
    if (tvistab(bv) && tabref(tabV(bv)->metatable) == ix->mtv) {
      TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_TAB_META);
      emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
    } else if (tvisudata(bv) && tabref(udataV(bv)->metatable) == ix->mtv) {
      TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_UDATA_META);
      emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
    } else {  /* Lookup metamethod on 2nd operand and compare both. */
      ix->tab = ix->key;
      copyTV(J->L, &ix->tabv, bv);
      if (!lj_record_mm_lookup(J, ix, MM_eq) ||
	  lj_record_objcmp(J, mo1, ix->mobj, &mo1v, &ix->mobjv))
	return;
    }
    rec_mm_callcomp(J, ix, op);
  }
#endif
}

/* Record call to ordered comparison metamethods (for arbitrary objects). */
static void rec_mm_comp(jit_State *J, RecordIndex *ix, int op)
{
  ix->tab = ix->val;
  copyTV(J->L, &ix->tabv, &ix->valv);
  while (1) {
    MMS mm = (op & 2) ? MM_le : MM_lt;  /* Try __le + __lt or only __lt. */
#if LJ_52
    if (!lj_record_mm_lookup(J, ix, mm)) {  /* Lookup mm on 1st operand. */
      ix->tab = ix->key;
      copyTV(J->L, &ix->tabv, &ix->keyv);
      if (!lj_record_mm_lookup(J, ix, mm))  /* Lookup mm on 2nd operand. */
	goto nomatch;
    }
    rec_mm_callcomp(J, ix, op);
    return;
#else
    if (lj_record_mm_lookup(J, ix, mm)) {  /* Lookup mm on 1st operand. */
      cTValue *bv;
      TRef mo1 = ix->mobj;
      TValue mo1v;
      copyTV(J->L, &mo1v, &ix->mobjv);
      /* Avoid the 2nd lookup and the objcmp if the metatables are equal. */
      bv = &ix->keyv;
      if (tvistab(bv) && tabref(tabV(bv)->metatable) == ix->mtv) {
	TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_TAB_META);
	emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
      } else if (tvisudata(bv) && tabref(udataV(bv)->metatable) == ix->mtv) {
	TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_UDATA_META);
	emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
      } else {  /* Lookup metamethod on 2nd operand and compare both. */
	ix->tab = ix->key;
	copyTV(J->L, &ix->tabv, bv);
	if (!lj_record_mm_lookup(J, ix, mm) ||
	    lj_record_objcmp(J, mo1, ix->mobj, &mo1v, &ix->mobjv))
	  goto nomatch;
      }
      rec_mm_callcomp(J, ix, op);
      return;
    }
#endif
  nomatch:
    /* Lookup failed. Retry with  __lt and swapped operands. */
    if (!(op & 2)) break;  /* Already at __lt. Interpreter will throw. */
#if LJ_54 && !defined(LUA_COMPAT_LT_LE)
    break;
#else
    ix->tab = ix->key; ix->key = ix->val; ix->val = ix->tab;
    copyTV(J->L, &ix->tabv, &ix->keyv);
    copyTV(J->L, &ix->keyv, &ix->valv);
    copyTV(J->L, &ix->valv, &ix->tabv);
    op ^= 3;
#endif
  }
}

#if LJ_HASFFI
/* Setup call to cdata comparison metamethod. */
static void rec_mm_comp_cdata(jit_State *J, RecordIndex *ix, int op, MMS mm)
{
  lj_snap_add(J);
  if (tref_iscdata(ix->val)) {
    ix->tab = ix->val;
    copyTV(J->L, &ix->tabv, &ix->valv);
  } else {
    lj_assertJ(tref_iscdata(ix->key), "cdata expected");
    ix->tab = ix->key;
    copyTV(J->L, &ix->tabv, &ix->keyv);
  }
  lj_record_mm_lookup(J, ix, mm);
  rec_mm_callcomp(J, ix, op);
}
#endif

/* -- Indexed access ------------------------------------------------------ */

#ifdef LUAJIT_ENABLE_TABLE_BUMP
/* Bump table allocations in bytecode when they grow during recording. */
static void rec_idx_bump(jit_State *J, RecordIndex *ix)
{
  RBCHashEntry *rbc = &J->rbchash[(ix->tab & (RBCHASH_SLOTS-1))];
  if (tref_ref(ix->tab) == rbc->ref) {
    const BCIns *pc = mref(rbc->pc, const BCIns);
    GCtab *tb = tabV(&ix->tabv);
    uint32_t nhbits;
    IRIns *ir;
    if (!tvisnil(&ix->keyv))
      (void)lj_tab_set(J->L, tb, &ix->keyv);  /* Grow table right now. */
    nhbits = tb->hmask > 0 ? lj_fls(tb->hmask)+1 : 0;
    ir = IR(tref_ref(ix->tab));
    if (ir->o == IR_TNEW) {
      uint32_t ah = bc_d(*pc);
      uint32_t asize = ah & 0x7ff, hbits = ah >> 11;
      if (nhbits > hbits) hbits = nhbits;
      if (tb->asize > asize) {
	asize = tb->asize <= 0x7ff ? tb->asize : 0x7ff;
      }
      if ((asize | (hbits<<11)) != ah) {  /* Has the size changed? */
	/* Patch bytecode, but continue recording (for more patching). */
	setbc_d(pc, (asize | (hbits<<11)));
	/* Patching TNEW operands is only safe if the trace is aborted. */
	ir->op1 = asize; ir->op2 = hbits;
	J->retryrec = 1;  /* Abort the trace at the end of recording. */
      }
    } else if (ir->o == IR_TDUP) {
      GCtab *tpl = gco2tab(proto_kgc(&gcref(rbc->pt)->pt, ~(ptrdiff_t)bc_d(*pc)));
      /* Grow template table, but preserve keys with nil values. */
      if ((tb->asize > tpl->asize && (1u << nhbits)-1 == tpl->hmask) ||
	  (tb->asize == tpl->asize && (1u << nhbits)-1 > tpl->hmask)) {
	Node *node = noderef(tpl->node);
	uint32_t i, hmask = tpl->hmask, asize;
	TValue *array;
	for (i = 0; i <= hmask; i++) {
	  if (!tvisnil(&node[i].key) && tvisnil(&node[i].val))
	    settabV(J->L, &node[i].val, tpl);
	}
	if (!tvisnil(&ix->keyv) && tref_isk(ix->key)) {
	  TValue *o = lj_tab_set(J->L, tpl, &ix->keyv);
	  if (tvisnil(o)) settabV(J->L, o, tpl);
	}
	lj_tab_resize(J->L, tpl, tb->asize, nhbits);
	node = noderef(tpl->node);
	hmask = tpl->hmask;
	for (i = 0; i <= hmask; i++) {
	  /* This is safe, since template tables only hold immutable values. */
	  if (tvistab(&node[i].val))
	    setnilV(&node[i].val);
	}
	/* The shape of the table may have changed. Clean up array part, too. */
	asize = tpl->asize;
	array = tvref(tpl->array);
	for (i = 0; i < asize; i++) {
	  if (tvistab(&array[i]))
	    setnilV(&array[i]);
	}
	J->retryrec = 1;  /* Abort the trace at the end of recording. */
      }
    }
  }
}
#endif

/* Record bounds-check. */
static void rec_idx_abc(jit_State *J, TRef asizeref, TRef ikey, uint32_t asize)
{
  /* Try to emit invariant bounds checks. */
  if ((J->flags & (JIT_F_OPT_LOOP|JIT_F_OPT_ABC)) ==
      (JIT_F_OPT_LOOP|JIT_F_OPT_ABC)) {
    IRRef ref = tref_ref(ikey);
    IRIns *ir = IR(ref);
    int32_t ofs = 0;
    IRRef ofsref = 0;
    /* Handle constant offsets. */
    if (ir->o == IR_ADD && irref_isk(ir->op2)) {
      ofsref = ir->op2;
      ofs = IR(ofsref)->i;
      ref = ir->op1;
      ir = IR(ref);
    }
    /* Got scalar evolution analysis results for this reference? */
    if (ref == J->scev.idx) {
      int32_t stop;
      lj_assertJ(irt_isint(J->scev.t) && ir->o == IR_SLOAD,
		 "only int SCEV supported");
      stop = numberVint(&(J->L->base - J->baseslot)[ir->op1 + FORL_STOP]);
      /* Runtime value for stop of loop is within bounds? */
      if ((uint64_t)stop + ofs < (uint64_t)asize) {
	/* Emit invariant bounds check for stop. */
	uint32_t abc = IRTG(IR_ABC, tref_isk(asizeref) ? IRT_U32 : IRT_P32);
	emitir(abc, asizeref, ofs == 0 ? J->scev.stop :
	       emitir(IRTI(IR_ADD), J->scev.stop, ofsref));
	/* Emit invariant bounds check for start, if not const or negative. */
	if (!(J->scev.dir && J->scev.start &&
	      (int64_t)IR(J->scev.start)->i + ofs >= 0))
	  emitir(abc, asizeref, ikey);
	return;
      }
    }
  }
  emitir(IRTGI(IR_ABC), asizeref, ikey);  /* Emit regular bounds check. */
}

/* Record indexed key lookup. */
static TRef rec_idx_key(jit_State *J, RecordIndex *ix, IRRef *rbref,
			IRType1 *rbguard)
{
  TRef key;
  GCtab *t = tabV(&ix->tabv);
  ix->oldv = lj_tab_get(J->L, t, &ix->keyv);  /* Lookup previous value. */
  /* lj_tab_getstr skips present keys whose value is nil (weak-table GC safety),
  ** but the HREFK fast path below needs the node address for a present key even
  ** with a nil value -- e.g. a constructor template field. Without it the store
  ** falls back to NEWREF, which breaks store->load forwarding and blocks
  ** allocation sinking of loop-carried constructors. Re-find the node for
  ** non-weak tables (no metatable => keys are live, so this is GC-safe). */
  if (ix->oldv == niltvg(J2G(J)) && tvisstr(&ix->keyv) &&
      gcref(t->metatable) == NULL) {
    cTValue *nv = lj_tab_getstr_node(t, strV(&ix->keyv));
    if (nv) ix->oldv = nv;
  }
  *rbref = 0;
  rbguard->irt = 0;

  /* Integer keys are looked up in the array part first. */
  key = ix->key;
  if (tref_isnumber(key)) {
    int32_t k;
    if (tvisint(&ix->keyv)) {
      k = intV(&ix->keyv);
    } else {
      int64_t i64;
      if (!lj_num2int_check(numV(&ix->keyv), i64, k)) k = LJ_MAX_ASIZE;
    }
    if ((MSize)k < LJ_MAX_ASIZE) {  /* Potential array key? */
      TRef ikey = lj_opt_narrow_index(J, key);
      TRef asizeref = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_ASIZE);
      if ((MSize)k < t->asize) {  /* Currently an array key? */
	TRef arrayref;
	rec_idx_abc(J, asizeref, ikey, t->asize);
	arrayref = emitir(IRT(IR_FLOAD, IRT_PGC), ix->tab, IRFL_TAB_ARRAY);
	return emitir(IRT(IR_AREF, IRT_PGC), arrayref, ikey);
      } else {  /* Currently not in array (may be an array extension)? */
	emitir(IRTGI(IR_ULE), asizeref, ikey);  /* Inv. bounds check. */
	if (k == 0 && tref_isk(key))
	  key = lj_ir_knum_zero(J);  /* Canonicalize 0 or +-0.0 to +0.0. */
	/* And continue with the hash lookup. */
      }
    } else if (!tref_isk(key)) {
      /* We can rule out const numbers which failed the integerness test
      ** above. But all other numbers are potential array keys.
      */
      if (t->asize == 0) {  /* True sparse tables have an empty array part. */
	/* Guard that the array part stays empty. */
	TRef tmp = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_ASIZE);
	emitir(IRTGI(IR_EQ), tmp, lj_ir_kint(J, 0));
      } else {
	lj_trace_err(J, LJ_TRERR_NYITMIX);
      }
    }
  }

  /* Otherwise the key is located in the hash part. */
  if (t->hmask == 0) {  /* Shortcut for empty hash part. */
    /* Guard that the hash part stays empty. */
    TRef tmp = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_HMASK);
    emitir(IRTGI(IR_EQ), tmp, lj_ir_kint(J, 0));
    return lj_ir_kkptr(J, niltvg(J2G(J)));
  }
  if (tref_isinteger(key))  /* Hash keys are based on numbers, not ints. */
    key = emitir(IRTN(IR_CONV), key, IRCONV_NUM_INT);
  if (tref_isk(key)) {
    /* Optimize lookup of constant hash keys. */
    GCSize hslot = (GCSize)((char *)ix->oldv-(char *)&noderef(t->node)[0].val);
    if (hslot <= t->hmask*(GCSize)sizeof(Node) &&
	hslot <= 65535*(GCSize)sizeof(Node)) {
      TRef node, kslot, hm;
      *rbref = J->cur.nins;  /* Mark possible rollback point. */
      *rbguard = J->guardemit;
      hm = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_HMASK);
      emitir(IRTGI(IR_EQ), hm, lj_ir_kint(J, (int32_t)t->hmask));
      node = emitir(IRT(IR_FLOAD, IRT_PGC), ix->tab, IRFL_TAB_NODE);
      kslot = lj_ir_kslot(J, key, (IRRef)(hslot / sizeof(Node)));
      return emitir(IRTG(IR_HREFK, IRT_PGC), node, kslot);
    }
  }
  /* Fall back to a regular hash lookup. */
  return emitir(IRT(IR_HREF, IRT_PGC), ix->tab, key);
}

/* Determine whether a key is NOT one of the fast metamethod names. */
static int nommstr(jit_State *J, TRef key)
{
  if (tref_isstr(key)) {
    if (tref_isk(key)) {
      GCstr *str = ir_kstr(IR(tref_ref(key)));
      uint32_t mm;
      for (mm = 0; mm <= MM_FAST; mm++)
	if (mmname_str(J2G(J), mm) == str)
	  return 0;  /* MUST be one the fast metamethod names. */
    } else {
      return 0;  /* Variable string key MAY be a metamethod name. */
    }
  }
  return 1;  /* CANNOT be a metamethod name. */
}

#if LJ_54
static int rec_tab_isweak(jit_State *J, GCtab *t)
{
  GCtab *mt = tabref(t->metatable);
  cTValue *mode;
  GCstr *s;
  MSize i;
  if (t->marked & LJ_GC_WEAK)
    return 1;
  if (!mt)
    return 0;
  mode = lj_meta_fastg(J2G(J), mt, MM_mode);
  if (!(mode && tvisstr(mode)))
    return 0;
  s = strV(mode);
  for (i = 0; i < s->len; i++) {
    char c = strdata(s)[i];
    if (c == 'k' || c == 'v')
      return 1;
  }
  return 0;
}
#endif

/* Record indexed load/store. */
TRef lj_record_idx(jit_State *J, RecordIndex *ix)
{
  TRef xref;
  IROp xrefop, loadop;
  IRRef rbref;
  IRType1 rbguard;
  cTValue *oldv;

  while (!tref_istab(ix->tab)) { /* Handle non-table lookup. */
    /* Never call raw lj_record_idx() on non-table. */
    lj_assertJ(ix->idxchain != 0, "bad usage");
    if (!lj_record_mm_lookup(J, ix, ix->val ? MM_newindex : MM_index))
      lj_trace_err(J, LJ_TRERR_NOMM);
  handlemm:
    if (tref_isfunc(ix->mobj)) {  /* Handle metamethod call. */
      BCReg func = rec_mm_prep(J, ix->val ? lj_cont_nop : lj_cont_ra);
      TRef *base = J->base + func + LJ_FR2;
      TValue *tv = J->L->base + func + LJ_FR2;
      base[-LJ_FR2] = ix->mobj; base[1] = ix->tab; base[2] = ix->key;
      setfuncV(J->L, tv-LJ_FR2, funcV(&ix->mobjv));
      copyTV(J->L, tv+1, &ix->tabv);
      copyTV(J->L, tv+2, &ix->keyv);
      if (ix->val) {
	base[3] = ix->val;
	copyTV(J->L, tv+3, &ix->valv);
	lj_record_call(J, func, 3);  /* mobj(tab, key, val) */
	return 0;
      } else {
	lj_record_call(J, func, 2);  /* res = mobj(tab, key) */
	return 0;  /* No result yet. */
      }
    }
#if LJ_HASBUFFER
    /* The index table of buffer objects is treated as immutable. */
    if (ix->mt == TREF_NIL && !ix->val &&
	tref_isudata(ix->tab) && udataV(&ix->tabv)->udtype == UDTYPE_BUFFER &&
	tref_istab(ix->mobj) && tref_isstr(ix->key) && tref_isk(ix->key)) {
      cTValue *val = lj_tab_getstr(tabV(&ix->mobjv), strV(&ix->keyv));
      TRef tr = lj_record_constify(J, val);
      if (tr) return tr;  /* Specialize to the value, i.e. a method. */
    }
#endif
    /* Otherwise retry lookup with metaobject. */
    ix->tab = ix->mobj;
    copyTV(J->L, &ix->tabv, &ix->mobjv);
    if (--ix->idxchain == 0)
      lj_trace_err(J, LJ_TRERR_IDXLOOP);
  }

  /* First catch nil and NaN keys for tables. */
  if (tvisnil(&ix->keyv) || (tvisnum(&ix->keyv) && tvisnan(&ix->keyv))) {
    if (ix->val)  /* Better fail early. */
      lj_trace_err(J, LJ_TRERR_STORENN);
    if (tref_isk(ix->key)) {
      if (ix->idxchain && lj_record_mm_lookup(J, ix, MM_index))
	goto handlemm;
      return TREF_NIL;
    }
  }

#if LJ_54 && LJ_DUALNUM
  {
    TRef i64ref = rec_lua54_table_i64key(J, ix->key, &ix->keyv);
    if (i64ref) {
      GCtab *tab = tabV(&ix->tabv);
      TRef trnull = lj_ir_kkptr(J, NULL);
      TRef xref;
      ix->oldv = oldv = lj_tab_get(J->L, tab, &ix->keyv);
      if (!ix->val) {
	IRType t = itype2irt(oldv);
	TRef res;
	if (rec_tab_isweak(J, tab))
	  lj_trace_err(J, LJ_TRERR_NYIWEAK);
	xref = lj_ir_call(J, IRCALL_lj_tab_geti64, ix->tab, i64ref);
	if (oldv == niltvg(J2G(J))) {
	  emitir(IRTG(IR_EQ, IRT_PGC), xref, trnull);
	  res = TREF_NIL;
	} else {
	  emitir(IRTG(IR_NE, IRT_PGC), xref, trnull);
	  res = emitir(IRTG(IR_HLOAD, t), xref, 0);
	}
	if (t == IRT_NIL && ix->idxchain &&
	    lj_record_mm_lookup(J, ix, MM_index))
	  goto handlemm;
	if (irtype_ispri(t))
	  res = TREF_PRI(t);
	return res;
      } else {
	GCtab *mt = tabref(tab->metatable);
	if (tvisnil(oldv)) {
	  int hasmm = 0;
	  if (ix->idxchain && mt) {
	    cTValue *mo = lj_tab_getstr(mt, mmname_str(J2G(J), MM_newindex));
	    hasmm = mo && !tvisnil(mo);
	  }
	  if (hasmm) {
	    xref = lj_ir_call(J, IRCALL_lj_tab_geti64, ix->tab, i64ref);
	    emitir(IRTG(IR_EQ, IRT_PGC), xref, trnull);
	  }
	  if (ix->idxchain && lj_record_mm_lookup(J, ix, MM_newindex)) {
	    lj_assertJ(hasmm, "inconsistent metamethod handling");
	    goto handlemm;
	  }
	  lj_assertJ(!hasmm, "inconsistent metamethod handling");
	  xref = lj_ir_call(J, IRCALL_lj_tab_seti64, ix->tab, i64ref);
	} else {
	  xref = lj_ir_call(J, IRCALL_lj_tab_geti64, ix->tab, i64ref);
	  emitir(IRTG(IR_NE, IRT_PGC), xref, trnull);
	}
	if (!LJ_DUALNUM && tref_isinteger(ix->val))
	  ix->val = emitir(IRTN(IR_CONV), ix->val, IRCONV_NUM_INT);
#if LJ_54 && LJ_DUALNUM && LJ_TARGET_ARM64
	ix->val = rec_lua54_boxraw_i64(J, ix->val);
#endif
	emitir(IRT(IR_HSTORE, tref_type(ix->val)), xref, ix->val);
	if (tref_isgcv(ix->val))
	  emitir(IRT(IR_TBAR, IRT_NIL), ix->tab, 0);
	J->needsnap = 1;
	return 0;
      }
    }
  }
#endif

#if LJ_54
  if (rec_is_longstr(&ix->keyv)) {
    if (!ix->val) {
      TRef xref, trnil;
      cTValue *oldv = lj_tab_get(J->L, tabV(&ix->tabv), &ix->keyv);
      if (rec_tab_isweak(J, tabV(&ix->tabv)))
	/* Weak-table slots may disappear between trace iterations. The helper
	** lookup is bytewise-correct for Lua 5.4 long strings, but it still must
	** not record weak-table lifetime assumptions into a trace.
	*/
	lj_trace_err(J, LJ_TRERR_NYIWEAK);
      xref = lj_ir_call(J, IRCALL_lj_tab_getstr, ix->tab, ix->key);
      trnil = lj_ir_kkptr(J, NULL);
      if (oldv == niltvg(J2G(J))) {
	emitir(IRTG(IR_EQ, IRT_PGC), xref, trnil);
	if (ix->idxchain && lj_record_mm_lookup(J, ix, MM_index))
	  goto handlemm;
	return TREF_NIL;
      } else {
	TRef bits, res = lj_record_constify(J, oldv);
	if (!res)
	  lj_trace_err(J, LJ_TRERR_NYILSTR);
	/* Runtime Lua 5.4 long strings are not interned, so table lookup must
	** compare bytes, not GC object identity. The side-effect call keeps the
	** lookup tied to the current table/key contents and returns the matching
	** value slot. Guard the slot's raw TValue bits and then constify the
	** observed value; if the table value changes, the trace exits before it
	** can reuse a stale specialized result.
	*/
	emitir(IRTG(IR_NE, IRT_PGC), xref, trnil);
#if LJ_32
	lj_needsplit(J);
#endif
	bits = emitir(IRT(IR_XLOAD, IRT_I64), xref, 0);
	emitir(IRTG(IR_EQ, IRT_I64), bits, lj_ir_kint64(J, oldv->u64));
	return res;
      }
    } else {
      TRef xref;
      cTValue *oldv = lj_tab_get(J->L, tabV(&ix->tabv), &ix->keyv);
      if (rec_tab_isweak(J, tabV(&ix->tabv)))
	lj_trace_err(J, LJ_TRERR_NYIWEAK);
      if (oldv == niltvg(J2G(J)) && ix->idxchain &&
	  lj_record_mm_lookup(J, ix, MM_newindex))
	goto handlemm;
      /* Runtime Lua 5.4 long strings are byte-equal table keys, not interned
      ** identities. Re-run a bytewise helper on every trace iteration and
      ** store through the regular TValue store path. Missing keys go through
      ** lj_tab_setstr() so rehash and key write-barrier semantics stay
      ** centralized in the table layer instead of duplicating NEWREF logic in
      ** the recorder. This keeps the same trace path valid for both GC64 and
      ** non-GC64 TValue layouts.
      */
      if (oldv == niltvg(J2G(J))) {
	xref = lj_ir_call(J, IRCALL_lj_tab_setstr, ix->tab, ix->key);
      } else {
	TRef trnil = lj_ir_kkptr(J, NULL);
	xref = lj_ir_call(J, IRCALL_lj_tab_getstr, ix->tab, ix->key);
	emitir(IRTG(IR_NE, IRT_PGC), xref, trnil);
      }
      if (!LJ_DUALNUM && tref_isinteger(ix->val))
	ix->val = emitir(IRTN(IR_CONV), ix->val, IRCONV_NUM_INT);
#if LJ_54 && LJ_DUALNUM && LJ_TARGET_ARM64
      ix->val = rec_lua54_boxraw_i64(J, ix->val);
#endif
      emitir(IRT(IR_HSTORE, tref_type(ix->val)), xref, ix->val);
      if (tref_isgcv(ix->val))
	emitir(IRT(IR_TBAR, IRT_NIL), ix->tab, 0);
      if (!nommstr(J, ix->key)) {
	TRef fref = emitir(IRT(IR_FREF, IRT_PGC), ix->tab, IRFL_TAB_NOMM);
	emitir(IRT(IR_FSTORE, IRT_U8), fref, lj_ir_kint(J, 0));
      }
      J->needsnap = 1;
      return 0;
    }
    /* Any long-string table access not handled above would need bytewise key
    ** lookup plus correct NEWREF/barrier semantics. Keep it in the interpreter
    ** until dedicated long-string-aware IR exists.
    */
    lj_trace_err(J, LJ_TRERR_NYILSTR);
  }

  if (!ix->val && rec_tab_isweak(J, tabV(&ix->tabv)))
    /* GC may clear weak slots between loop iterations. Do not record a table
    ** load that the optimizer can treat as stable across collector progress. */
    lj_trace_err(J, LJ_TRERR_NYIWEAK);
#endif

  /* Record the key lookup. */
  xref = rec_idx_key(J, ix, &rbref, &rbguard);
  xrefop = IR(tref_ref(xref))->o;
  loadop = xrefop == IR_AREF ? IR_ALOAD : IR_HLOAD;
  /* The lj_meta_tset() inconsistency is gone, but better play safe. */
  oldv = xrefop == IR_KKPTR ? (cTValue *)ir_kptr(IR(tref_ref(xref))) : ix->oldv;

  if (ix->val == 0) {  /* Indexed load */
    IRType t = itype2irt(oldv);
    TRef res;
    if (oldv == niltvg(J2G(J))) {
      emitir(IRTG(IR_EQ, IRT_PGC), xref, lj_ir_kkptr(J, niltvg(J2G(J))));
      res = TREF_NIL;
    } else {
      res = emitir(IRTG(loadop, t), xref, 0);
    }
    if (tref_ref(res) < rbref) {  /* HREFK + load forwarded? */
      lj_ir_rollback(J, rbref);  /* Rollback to eliminate hmask guard. */
      J->guardemit = rbguard;
    }
    if (t == IRT_NIL && ix->idxchain && lj_record_mm_lookup(J, ix, MM_index))
      goto handlemm;
    if (irtype_ispri(t)) res = TREF_PRI(t);  /* Canonicalize primitives. */
    return res;
  } else {  /* Indexed store. */
    GCtab *mt = tabref(tabV(&ix->tabv)->metatable);
    int keybarrier = tref_isgcv(ix->key) && !tref_isnil(ix->val);
    if (tref_ref(xref) < rbref) {  /* HREFK forwarded? */
      lj_ir_rollback(J, rbref);  /* Rollback to eliminate hmask guard. */
      J->guardemit = rbguard;
    }
    if (tvisnil(oldv)) {  /* Previous value was nil? */
      /* Need to duplicate the hasmm check for the early guards. */
      int hasmm = 0;
      if (ix->idxchain && mt) {
	cTValue *mo = lj_tab_getstr(mt, mmname_str(J2G(J), MM_newindex));
	hasmm = mo && !tvisnil(mo);
      }
      if (hasmm)
	emitir(IRTG(loadop, IRT_NIL), xref, 0);  /* Guard for nil value. */
      else if (xrefop == IR_HREF)
	emitir(IRTG(oldv == niltvg(J2G(J)) ? IR_EQ : IR_NE, IRT_PGC),
	       xref, lj_ir_kkptr(J, niltvg(J2G(J))));
      if (ix->idxchain && lj_record_mm_lookup(J, ix, MM_newindex)) {
	lj_assertJ(hasmm, "inconsistent metamethod handling");
	goto handlemm;
      }
      lj_assertJ(!hasmm, "inconsistent metamethod handling");
      if (oldv == niltvg(J2G(J))) {  /* Need to insert a new key. */
	TRef key = ix->key;
	if (tref_isinteger(key)) {  /* NEWREF needs a TValue as a key. */
	  key = emitir(IRTN(IR_CONV), key, IRCONV_NUM_INT);
	} else if (tref_isnum(key)) {
	  if (tref_isk(key)) {
	    if (tvismzero(&ix->keyv))
	      key = lj_ir_knum_zero(J);  /* Canonicalize -0.0 to +0.0. */
	  } else {
	    emitir(IRTG(IR_EQ, IRT_NUM), key, key);  /* Check for !NaN. */
	  }
	}
	xref = emitir(IRT(IR_NEWREF, IRT_PGC), ix->tab, key);
	keybarrier = 0;  /* NEWREF already takes care of the key barrier. */
#ifdef LUAJIT_ENABLE_TABLE_BUMP
	if ((J->flags & JIT_F_OPT_SINK))  /* Avoid a separate flag. */
	  rec_idx_bump(J, ix);
#endif
      }
    } else if (!lj_opt_fwd_wasnonnil(J, loadop, tref_ref(xref))) {
      /* Cannot derive that the previous value was non-nil, must do checks. */
      if (xrefop == IR_HREF)  /* Guard against store to niltv. */
	emitir(IRTG(IR_NE, IRT_PGC), xref, lj_ir_kkptr(J, niltvg(J2G(J))));
      if (ix->idxchain) {  /* Metamethod lookup required? */
	/* A check for NULL metatable is cheaper (hoistable) than a load. */
	if (!mt) {
	  TRef mtref = emitir(IRT(IR_FLOAD, IRT_TAB), ix->tab, IRFL_TAB_META);
	  emitir(IRTG(IR_EQ, IRT_TAB), mtref, lj_ir_knull(J, IRT_TAB));
	} else {
	  IRType t = itype2irt(oldv);
	  emitir(IRTG(loadop, t), xref, 0);  /* Guard for non-nil value. */
	}
      }
    } else {
      keybarrier = 0;  /* Previous non-nil value kept the key alive. */
    }
    /* Convert int to number before storing. */
    if (!LJ_DUALNUM && tref_isinteger(ix->val))
      ix->val = emitir(IRTN(IR_CONV), ix->val, IRCONV_NUM_INT);
#if LJ_54 && LJ_DUALNUM && LJ_TARGET_ARM64
    ix->val = rec_lua54_boxraw_i64(J, ix->val);
#endif
    emitir(IRT(loadop+IRDELTA_L2S, tref_type(ix->val)), xref, ix->val);
    if (keybarrier || tref_isgcv(ix->val))
      emitir(IRT(IR_TBAR, IRT_NIL), ix->tab, 0);
    /* Invalidate neg. metamethod cache for stores with certain string keys. */
    if (!nommstr(J, ix->key)) {
      TRef fref = emitir(IRT(IR_FREF, IRT_PGC), ix->tab, IRFL_TAB_NOMM);
      emitir(IRT(IR_FSTORE, IRT_U8), fref, lj_ir_kint(J, 0));
    }
    J->needsnap = 1;
    return 0;
  }
}

/* Determine result type of table traversal. */
static IRType rec_next_types(GCtab *t, uint32_t idx)
{
  for (; idx < t->asize; idx++) {
    cTValue *a = arrayslot(t, idx);
    if (LJ_LIKELY(!tvisnil(a)))
      return (LJ_DUALNUM ? IRT_INT : IRT_NUM) + (itype2irt(a) << 8);
  }
  idx -= t->asize;
  for (; idx <= t->hmask; idx++) {
    Node *n = &noderef(t->node)[idx];
    if (!tvisnil(&n->val))
      return itype2irt(&n->key) + (itype2irt(&n->val) << 8);
  }
  return IRT_NIL + (IRT_NIL << 8);
}

/* Record a table traversal step aka next(). */
int lj_record_next(jit_State *J, RecordIndex *ix)
{
  IRType t, tkey, tval;
  TRef trvk;
  t = rec_next_types(tabV(&ix->tabv), ix->keyv.u32.lo);
  tkey = (t & 0xff); tval = (t >> 8);
  trvk = lj_ir_call(J, IRCALL_lj_vm_next, ix->tab, ix->key);
  if (ix->mobj || tkey == IRT_NIL) {
    TRef idx = emitir(IRTI(IR_HIOP), trvk, trvk);
    /* Always check for invalid key from next() for nil result. */
    if (!ix->mobj) emitir(IRTGI(IR_NE), idx, lj_ir_kint(J, -1));
    ix->mobj = idx;
  }
  ix->key = lj_record_vload(J, trvk, 1, tkey);
  if (tkey == IRT_NIL || ix->idxchain) {  /* Omit value type check. */
    ix->val = TREF_NIL;
    return 1;
  } else {  /* Need value. */
    ix->val = lj_record_vload(J, trvk, 0, tval);
    return 2;
  }
}

static void rec_tsetm(jit_State *J, BCReg ra, BCReg rn, int32_t i)
{
  RecordIndex ix;
  cTValue *basev = J->L->base;
  GCtab *t = tabV(&basev[ra-1]);
  settabV(J->L, &ix.tabv, t);
  ix.tab = getslot(J, ra-1);
  ix.idxchain = 0;
#ifdef LUAJIT_ENABLE_TABLE_BUMP
  if ((J->flags & JIT_F_OPT_SINK)) {
    if (t->asize < i+rn-ra)
      lj_tab_reasize(J->L, t, i+rn-ra);
    setnilV(&ix.keyv);
    rec_idx_bump(J, &ix);
  }
#endif
  for (; ra < rn; i++, ra++) {
    setintV(&ix.keyv, i);
    ix.key = lj_ir_kint(J, i);
    copyTV(J->L, &ix.valv, &basev[ra]);
    ix.val = getslot(J, ra);
    lj_record_idx(J, &ix);
  }
}

/* -- Upvalue access ------------------------------------------------------ */

/* Check whether upvalue is immutable and ok to constify. */
static int rec_upvalue_constify(jit_State *J, GCupval *uvp)
{
#if LJ_54 && LJ_TARGET_ARM64
  /* debug.setupvalue() can mutate closed upvalues from outside recorded code.
  ** Keep ARM64/Lua 5.4 traces loading through the upvalue instead of baking a
  ** stale immutable value and relying on an expensive global trace flush.
  */
  UNUSED(J); UNUSED(uvp);
  return 0;
#else
  if (uvp->immutable) {
    cTValue *o = uvval(uvp);
    /* Don't constify objects that may retain large amounts of memory. */
#if LJ_HASFFI
    if (tviscdata(o)) {
      GCcdata *cd = cdataV(o);
      if (!cdataisv(cd) && !(cd->marked & LJ_GC_CDATA_FIN)) {
	CType *ct = ctype_raw(ctype_ctsG(J2G(J)), cd->ctypeid);
	if (!ctype_hassize(ct->info) || ct->size <= 16)
	  return 1;
      }
      return 0;
    }
#else
    UNUSED(J);
#endif
    if (!(tvistab(o) || tvisudata(o) || tvisthread(o)))
      return 1;
  }
  return 0;
#endif
}

/* Record upvalue load/store. */
static TRef rec_upvalue(jit_State *J, uint32_t uv, TRef val)
{
  GCupval *uvp = &gcref(J->fn->l.uvptr[uv])->uv;
  TRef fn = getcurrf(J);
  IRRef uref;
  int needbarrier = 0;
  if (rec_upvalue_constify(J, uvp)) {  /* Try to constify immutable upvalue. */
    TRef tr, kfunc;
    lj_assertJ(val == 0, "bad usage");
    if (!tref_isk(fn)) {  /* Late specialization of current function. */
      if (J->pt->flags >= PROTO_CLC_POLY)
	goto noconstify;
      kfunc = lj_ir_kfunc(J, J->fn);
      emitir(IRTG(IR_EQ, IRT_FUNC), fn, kfunc);
#if LJ_FR2
      J->base[-2] = kfunc;
#else
      J->base[-1] = kfunc | TREF_FRAME;
#endif
      fn = kfunc;
    }
    tr = lj_record_constify(J, uvval(uvp));
    if (tr)
      return tr;
  }
noconstify:
  /* IRMlit operands must stay below REF_BIAS. Keep a 7 bit alias hash so
  ** Lua 5.4 high-index upvalues (128..199) remain recordable.
  */
  uv = IRUREF_ENCODE(uv, hashrot(uvp->dhash, uvp->dhash + HASH_BIAS));
  if (!uvp->closed) {
    /* In current stack? */
    if (uvval(uvp) >= tvref(J->L->stack) &&
	uvval(uvp) < tvref(J->L->maxstack)) {
      int32_t slot = (int32_t)(uvval(uvp) - (J->L->base - J->baseslot));
      if (slot >= 0) {  /* Aliases an SSA slot? */
	uref = tref_ref(emitir(IRT(IR_UREFO, IRT_PGC), fn, uv));
	emitir(IRTG(IR_EQ, IRT_PGC),
	       REF_BASE,
	       emitir(IRT(IR_ADD, IRT_PGC), uref,
		      lj_ir_kintpgc(J, (slot - 1 - LJ_FR2) * -8)));
	slot -= (int32_t)J->baseslot;  /* Note: slot number may be negative! */
	if (val == 0) {
	  return getslot(J, slot);
	} else {
	  J->base[slot] = val;
	  if (slot >= (int32_t)J->maxslot) J->maxslot = (BCReg)(slot+1);
	  return 0;
	}
      }
    }
    /* IR_UREFO+IRT_IGC is not checked for open-ness at runtime.
    ** Always marked as a guard, since it might get promoted to IRT_PGC later.
    */
    uref = emitir(IRTG(IR_UREFO, tref_isgcv(val) ? IRT_PGC : IRT_IGC), fn, uv);
    uref = tref_ref(uref);
    emitir(IRTG(IR_UGT, IRT_PGC),
	   emitir(IRT(IR_SUB, IRT_PGC), uref, REF_BASE),
	   lj_ir_kintpgc(J, (J->baseslot + J->maxslot) * 8));
  } else {
    /* If fn is constant, then so is the GCupval*, and the upvalue cannot
    ** transition back to open, so no guard is required in this case.
    */
    IRType t = (tref_isk(fn) ? 0 : IRT_GUARD) | IRT_PGC;
    uref = tref_ref(emitir(IRT(IR_UREFC, t), fn, uv));
    needbarrier = 1;
  }
  if (val == 0) {  /* Upvalue load */
    IRType t = itype2irt(uvval(uvp));
    TRef res = emitir(IRTG(IR_ULOAD, t), uref, 0);
    if (irtype_ispri(t)) res = TREF_PRI(t);  /* Canonicalize primitive refs. */
    return res;
  } else {  /* Upvalue store. */
    /* Convert int to number before storing. */
    if (!LJ_DUALNUM && tref_isinteger(val))
      val = emitir(IRTN(IR_CONV), val, IRCONV_NUM_INT);
    emitir(IRT(IR_USTORE, tref_type(val)), uref, val);
    if (needbarrier && tref_isgcv(val))
      emitir(IRT(IR_OBAR, IRT_NIL), uref, val);
    J->needsnap = 1;
    return 0;
  }
}

/* -- Record calls to Lua functions --------------------------------------- */

/* Check unroll limits for calls. */
static void check_call_unroll(jit_State *J, TraceNo lnk)
{
  cTValue *frame = J->L->base - 1;
  void *pc = mref(frame_func(frame)->l.pc, void);
  int32_t depth = J->framedepth;
  int32_t count = 0;
  if ((J->pt->flags & PROTO_VARARG)) depth--;  /* Vararg frame still missing. */
  for (; depth > 0; depth--) {  /* Count frames with same prototype. */
    if (frame_iscont(frame)) depth--;
    frame = frame_prev(frame);
    if (mref(frame_func(frame)->l.pc, void) == pc)
      count++;
  }
  if (J->pc == J->startpc) {
    if (count + J->tailcalled > J->param[JIT_P_recunroll]) {
      J->pc++;
      if (J->framedepth + J->retdepth == 0)
	lj_record_stop(J, LJ_TRLINK_TAILREC, J->cur.traceno);  /* Tail-rec. */
      else
	lj_record_stop(J, LJ_TRLINK_UPREC, J->cur.traceno);  /* Up-recursion. */
    }
  } else {
    if (count > J->param[JIT_P_callunroll]) {
      if (lnk) {  /* Possible tail- or up-recursion. */
	lj_trace_flush(J, lnk);  /* Flush trace that only returns. */
	/* Set a small, pseudo-random hotcount for a quick retry of JFUNC*. */
	hotcount_set(J2GG(J), J->pc+1, lj_prng_u64(&J2G(J)->prng) & 15u);
      }
      lj_trace_err(J, LJ_TRERR_CUNROLL);
    }
  }
}

/* Record Lua function setup. */
static void rec_func_setup(jit_State *J)
{
  GCproto *pt = J->pt;
  BCReg s, numparams = pt->numparams;
  if ((pt->flags & PROTO_NOJIT))
    lj_trace_err(J, LJ_TRERR_CJITOFF);
  if (J->baseslot + pt->framesize >= LJ_MAX_JSLOTS)
    lj_trace_err(J, LJ_TRERR_STACKOV);
  /* Fill up missing parameters with nil. */
  for (s = J->maxslot; s < numparams; s++)
    J->base[s] = TREF_NIL;
  /* The remaining slots should never be read before they are written. */
  J->maxslot = numparams;
}

/* Record Lua vararg function setup. */
static void rec_func_vararg(jit_State *J)
{
  GCproto *pt = J->pt;
  BCReg s, fixargs, vframe = J->maxslot+1+LJ_FR2;
  lj_assertJ((pt->flags & PROTO_VARARG), "FUNCV in non-vararg function");
  if (J->baseslot + vframe + pt->framesize >= LJ_MAX_JSLOTS)
    lj_trace_err(J, LJ_TRERR_STACKOV);
  J->base[vframe-1-LJ_FR2] = J->base[-1-LJ_FR2];  /* Copy function up. */
#if LJ_FR2
  J->base[vframe-1] = TREF_FRAME;
#endif
  /* Copy fixarg slots up and set their original slots to nil. */
  fixargs = pt->numparams < J->maxslot ? pt->numparams : J->maxslot;
  for (s = 0; s < fixargs; s++) {
    J->base[vframe+s] = J->base[s];
    J->base[s] = TREF_NIL;
  }
  J->maxslot = fixargs;
  J->framedepth++;
  J->base += vframe;
  J->baseslot += vframe;
}

/* Record entry to a Lua function. */
static void rec_func_lua(jit_State *J)
{
  rec_func_setup(J);
  check_call_unroll(J, 0);
}

/* Record entry to an already compiled function. */
static void rec_func_jit(jit_State *J, TraceNo lnk)
{
  GCtrace *T;
  rec_func_setup(J);
  T = traceref(J, lnk);
  if (T->linktype == LJ_TRLINK_RETURN) {  /* Trace returns to interpreter? */
    check_call_unroll(J, lnk);
    /* Temporarily unpatch JFUNC* to continue recording across function. */
    J->patchins = *J->pc;
    J->patchpc = (BCIns *)J->pc;
    *J->patchpc = T->startins;
    return;
  }
  J->instunroll = 0;  /* Cannot continue across a compiled function. */
  if (J->pc == J->startpc && J->framedepth + J->retdepth == 0)
    lj_record_stop(J, LJ_TRLINK_TAILREC, J->cur.traceno);  /* Extra tail-rec. */
  else
    lj_record_stop(J, LJ_TRLINK_ROOT, lnk);  /* Link to the function. */
}

/* -- Vararg handling ----------------------------------------------------- */

/* Detect y = select(x, ...) idiom. */
static int select_detect(jit_State *J)
{
  BCIns ins = J->pc[1];
  if (bc_op(ins) == BC_CALLM && bc_b(ins) == 2 && bc_c(ins) == 1) {
    cTValue *func = &J->L->base[bc_a(ins)];
    if (tvisfunc(func) && funcV(func)->c.ffid == FF_select) {
      TRef kfunc = lj_ir_kfunc(J, funcV(func));
      emitir(IRTG(IR_EQ, IRT_FUNC), getslot(J, bc_a(ins)), kfunc);
      return 1;
    }
  }
  return 0;
}

/* Record vararg instruction. */
static void rec_varg(jit_State *J, BCReg dst, ptrdiff_t nresults)
{
  int32_t numparams = J->pt->numparams;
  ptrdiff_t nvararg = frame_delta(J->L->base-1) - numparams - 1 - LJ_FR2;
  lj_assertJ(frame_isvarg(J->L->base-1), "VARG in non-vararg frame");
  if (LJ_FR2 && dst > J->maxslot)
    J->base[dst-1] = 0;  /* Prevent resurrection of unrelated slot. */
  if (J->framedepth > 0) {  /* Simple case: varargs defined on-trace. */
    ptrdiff_t i;
    if (nvararg < 0) nvararg = 0;
    if (nresults != 1) {
      if (nresults == -1) nresults = nvararg;
      J->maxslot = dst + (BCReg)nresults;
    } else if (dst >= J->maxslot) {
      J->maxslot = dst + 1;
    }
    if (J->baseslot + J->maxslot >= LJ_MAX_JSLOTS)
      lj_trace_err(J, LJ_TRERR_STACKOV);
    for (i = 0; i < nresults; i++)
      J->base[dst+i] = i < nvararg ? getslot(J, i - nvararg - 1 - LJ_FR2) : TREF_NIL;
  } else {  /* Unknown number of varargs passed to trace. */
    TRef fr = emitir(IRTI(IR_SLOAD), LJ_FR2, IRSLOAD_READONLY|IRSLOAD_FRAME);
    int32_t frofs = 8*(1+LJ_FR2+numparams)+FRAME_VARG;
    if (nresults >= 0) {  /* Known fixed number of results. */
      ptrdiff_t i;
      if (nvararg > 0) {
	ptrdiff_t nload = nvararg >= nresults ? nresults : nvararg;
	TRef vbase;
	if (nvararg >= nresults)
	  emitir(IRTGI(IR_GE), fr, lj_ir_kint(J, frofs+8*(int32_t)nresults));
	else
	  emitir(IRTGI(IR_EQ), fr,
		 lj_ir_kint(J, (int32_t)frame_ftsz(J->L->base-1)));
	vbase = emitir(IRT(IR_SUB, IRT_IGC), REF_BASE, fr);
	vbase = emitir(IRT(IR_ADD, IRT_PGC), vbase,
		       lj_ir_kintpgc(J, frofs-8*(1+LJ_FR2)));
	for (i = 0; i < nload; i++) {
	  IRType t = itype2irt(&J->L->base[i-1-LJ_FR2-nvararg]);
	  J->base[dst+i] = lj_record_vload(J, vbase, (MSize)i, t);
	}
      } else {
	emitir(IRTGI(IR_LE), fr, lj_ir_kint(J, frofs));
	nvararg = 0;
      }
      for (i = nvararg; i < nresults; i++)
	J->base[dst+i] = TREF_NIL;
      if (nresults != 1 || dst >= J->maxslot) {
	J->maxslot = dst + (BCReg)nresults;
      }
    } else if (select_detect(J)) {  /* y = select(x, ...) */
      TRef tridx = getslot(J, dst-1);
      TRef tr = TREF_NIL;
      ptrdiff_t idx = lj_ffrecord_select_mode(J, tridx, &J->L->base[dst-1]);
      if (idx < 0) goto nyivarg;
      if (idx != 0 && !tref_isinteger(tridx)) {
	if (tref_isstr(tridx))
	  tridx = emitir(IRTG(IR_STRTO, IRT_NUM), tridx, 0);
	tridx = emitir(IRTGI(IR_CONV), tridx, IRCONV_INT_NUM|IRCONV_INDEX);
      }
      if (idx != 0 && tref_isk(tridx)) {
	emitir(IRTGI(idx <= nvararg ? IR_GE : IR_LT),
	       fr, lj_ir_kint(J, frofs+8*(int32_t)idx));
	frofs -= 8;  /* Bias for 1-based index. */
      } else if (idx <= nvararg) {  /* Compute size. */
	TRef tmp = emitir(IRTI(IR_ADD), fr, lj_ir_kint(J, -frofs));
	if (numparams)
	  emitir(IRTGI(IR_GE), tmp, lj_ir_kint(J, 0));
	tr = emitir(IRTI(IR_BSHR), tmp, lj_ir_kint(J, 3));
	if (idx != 0) {
	  tridx = emitir(IRTI(IR_ADD), tridx, lj_ir_kint(J, -1));
	  rec_idx_abc(J, tr, tridx, (uint32_t)nvararg);
	}
      } else {
	TRef tmp = lj_ir_kint(J, frofs);
	if (idx != 0) {
	  TRef tmp2 = emitir(IRTI(IR_BSHL), tridx, lj_ir_kint(J, 3));
	  tmp = emitir(IRTI(IR_ADD), tmp2, tmp);
	} else {
	  tr = lj_ir_kint(J, 0);
	}
	emitir(IRTGI(IR_LT), fr, tmp);
      }
      if (idx != 0 && idx <= nvararg) {
	IRType t;
	TRef aref, vbase = emitir(IRT(IR_SUB, IRT_IGC), REF_BASE, fr);
	vbase = emitir(IRT(IR_ADD, IRT_PGC), vbase,
		       lj_ir_kintpgc(J, frofs-(8<<LJ_FR2)));
	t = itype2irt(&J->L->base[idx-2-LJ_FR2-nvararg]);
	aref = emitir(IRT(IR_AREF, IRT_PGC), vbase, tridx);
	tr = lj_record_vload(J, aref, 0, t);
      }
      J->base[dst-2-LJ_FR2] = tr;
      J->maxslot = dst-1-LJ_FR2;
      J->bcskip = 2;  /* Skip CALLM + select. */
    } else {
    nyivarg:
      setintV(&J->errinfo, BC_VARG);
      lj_trace_err_info(J, LJ_TRERR_NYIBC);
    }
  }
}

/* -- Record allocations -------------------------------------------------- */

static TRef rec_tnew(jit_State *J, uint32_t ah)
{
  uint32_t asize = ah & 0x7ff;
  uint32_t hbits = ah >> 11;
  TRef tr;
  if (asize == 0x7ff) asize = 0x801;
  tr = emitir(IRTG(IR_TNEW, IRT_TAB), asize, hbits);
#ifdef LUAJIT_ENABLE_TABLE_BUMP
  J->rbchash[(tr & (RBCHASH_SLOTS-1))].ref = tref_ref(tr);
  setmref(J->rbchash[(tr & (RBCHASH_SLOTS-1))].pc, J->pc);
  setgcref(J->rbchash[(tr & (RBCHASH_SLOTS-1))].pt, obj2gco(J->pt));
#endif
  return tr;
}

/* -- Concatenation ------------------------------------------------------- */

typedef struct RecCatDataCP {
  TValue savetv[5+LJ_FR2];
  jit_State *J;
  BCReg baseslot, topslot;
  TRef tr;
} RecCatDataCP;

static TValue *rec_mm_concat_cp(lua_State *L, lua_CFunction dummy, void *ud)
{
  RecCatDataCP *rcd = (RecCatDataCP *)ud;
  jit_State *J = rcd->J;
  BCReg baseslot = rcd->baseslot, topslot = rcd->topslot;
  TRef *top = &J->base[topslot];
  BCReg s;
  RecordIndex ix;
  UNUSED(L); UNUSED(dummy);
  lj_assertJ(baseslot < topslot, "bad CAT arg");
  for (s = baseslot; s <= topslot; s++)
    (void)getslot(J, s);  /* Ensure all arguments have a reference. */
  if (
#if LJ_54
      (rec_lua54_tref_isnumeric(top[0]) || tref_isstr(top[0])) &&
      (rec_lua54_tref_isnumeric(top[-1]) || tref_isstr(top[-1]))
#else
      tref_isnumber_str(top[0]) && tref_isnumber_str(top[-1])
#endif
     ) {
    TRef tr, hdr, *trp, *xbase, *base = &J->base[baseslot];
    /* First convert numbers to strings. */
    for (trp = top; trp >= base; trp--) {
#if LJ_54
      if (rec_lua54_tref_isnumeric(*trp))
	*trp = lj_ir_tostr(J, *trp);
#else
      if (tref_isnumber(*trp))
	*trp = emitir(IRT(IR_TOSTR, IRT_STR), *trp,
		      tref_isnum(*trp) ? IRTOSTR_NUM : IRTOSTR_INT);
#endif
      else if (!tref_isstr(*trp))
	break;
    }
    xbase = ++trp;
    tr = hdr = emitir(IRT(IR_BUFHDR, IRT_PGC),
		      lj_ir_kptr(J, &J2G(J)->tmpbuf), IRBUFHDR_RESET);
    do {
      tr = emitir(IRTG(IR_BUFPUT, IRT_PGC), tr, *trp++);
    } while (trp <= top);
    tr = emitir(IRTG(IR_BUFSTR, IRT_STR), tr, hdr);
    J->maxslot = (BCReg)(xbase - J->base);
    if (xbase == base) {
      rcd->tr = tr;  /* Return simple concatenation result. */
      return NULL;
    }
    /* Pass partial result. */
    rcd->topslot = topslot = J->maxslot--;
    /* Save updated range of slots. */
    memcpy(rcd->savetv, &L->base[topslot-1], sizeof(rcd->savetv));
    *xbase = tr;
    top = xbase;
    setstrV(J->L, &ix.keyv, &J2G(J)->strempty);  /* Simulate string result. */
  } else {
    J->maxslot = topslot-1;
    copyTV(J->L, &ix.keyv, &J->L->base[topslot]);
  }
  copyTV(J->L, &ix.tabv, &J->L->base[topslot-1]);
  ix.tab = top[-1];
  ix.key = top[0];
  rec_mm_arith(J, &ix, MM_concat);  /* Call __concat metamethod. */
  rcd->tr = 0;  /* No result yet. */
  return NULL;
}

static TRef rec_cat(jit_State *J, BCReg baseslot, BCReg topslot)
{
  lua_State *L = J->L;
  ptrdiff_t delta = L->top - L->base;
  TValue errobj;
  RecCatDataCP rcd;
  int errcode;
  rcd.J = J;
  rcd.baseslot = baseslot;
  rcd.topslot = topslot;
  /* Save slots. */
  memcpy(rcd.savetv, &L->base[topslot-1], sizeof(rcd.savetv));
  errcode = lj_vm_cpcall(L, NULL, &rcd, rec_mm_concat_cp);
  if (errcode) copyTV(L, &errobj, L->top-1);
  /* Restore slots. */
  memcpy(&L->base[rcd.topslot-1], rcd.savetv, sizeof(rcd.savetv));
  if (errcode) {
    L->top = L->base + delta;
    copyTV(L, L->top++, &errobj);
    return (TRef)(-errcode);
  }
  return rcd.tr;
}

/* -- Record bytecode ops ------------------------------------------------- */

/* Prepare for comparison. */
static void rec_comp_prep(jit_State *J)
{
  /* Prevent merging with snapshot #0 (GC exit) since we fixup the PC. */
  if (J->cur.nsnap == 1 && J->cur.snap[0].ref == J->cur.nins)
    emitir_raw(IRT(IR_NOP, IRT_NIL), 0, 0);
  lj_snap_add(J);
}

/* Fixup comparison. */
static void rec_comp_fixup(jit_State *J, const BCIns *pc, int cond)
{
  BCIns jmpins = pc[1];
  const BCIns *npc = pc + 2 + (cond ? bc_j(jmpins) : 0);
  SnapShot *snap = &J->cur.snap[J->cur.nsnap-1];
  /* Set PC to opposite target to avoid re-recording the comp. in side trace. */
#if LJ_FR2
  SnapEntry *flink = &J->cur.snapmap[snap->mapofs + snap->nent];
  uint64_t pcbase;
  memcpy(&pcbase, flink, sizeof(uint64_t));
  pcbase = (pcbase & 0xff) | (u64ptr(npc) << 8);
  memcpy(flink, &pcbase, sizeof(uint64_t));
#else
  J->cur.snapmap[snap->mapofs + snap->nent] = SNAP_MKPC(npc);
#endif
  J->needsnap = 1;
  if (bc_a(jmpins) < J->maxslot) J->maxslot = bc_a(jmpins);
  lj_snap_shrink(J);  /* Shrink last snapshot if possible. */
}

/* Record the next bytecode instruction (_before_ it's executed). */
void lj_record_ins(jit_State *J)
{
  cTValue *lbase;
  RecordIndex ix;
  const BCIns *pc;
  BCIns ins;
  BCOp op;
  TRef ra, rb, rc;

  /* Perform post-processing action before recording the next instruction. */
  if (LJ_UNLIKELY(J->postproc != LJ_POST_NONE)) {
    switch (J->postproc) {
    case LJ_POST_FIXCOMP:  /* Fixup comparison. */
      pc = (const BCIns *)(uintptr_t)J2G(J)->tmptv.u64;
      rec_comp_fixup(J, pc, (!tvistruecond(&J2G(J)->tmptv2) ^ (bc_op(*pc)&1)));
      /* fallthrough */
    case LJ_POST_FIXGUARD:  /* Fixup and emit pending guard. */
    case LJ_POST_FIXGUARDSNAP:  /* Fixup and emit pending guard and snapshot. */
      if (!tvistruecond(&J2G(J)->tmptv2)) {
	J->fold.ins.o ^= 1;  /* Flip guard to opposite. */
	if (J->postproc == LJ_POST_FIXGUARDSNAP) {
	  SnapShot *snap = &J->cur.snap[J->cur.nsnap-1];
	  J->cur.snapmap[snap->mapofs+snap->nent-1]--;  /* False -> true. */
	}
      }
      lj_opt_fold(J);  /* Emit pending guard. */
      /* fallthrough */
    case LJ_POST_FIXBOOL:
      if (!tvistruecond(&J2G(J)->tmptv2)) {
	BCReg s;
	TValue *tv = J->L->base;
	for (s = 0; s < J->maxslot; s++)  /* Fixup stack slot (if any). */
	  if (J->base[s] == TREF_TRUE && tvisfalse(&tv[s])) {
	    J->base[s] = TREF_FALSE;
	    break;
	  }
      }
      break;
    case LJ_POST_FIXCONST:
      {
	BCReg s;
	TValue *tv = J->L->base;
	for (s = 0; s < J->maxslot; s++)  /* Constify stack slots (if any). */
	  if (J->base[s] == TREF_NIL && !tvisnil(&tv[s]))
	    J->base[s] = lj_record_constify(J, &tv[s]);
      }
      break;
    case LJ_POST_FFRETRY:  /* Suppress recording of retried fast function. */
      if (bc_op(*J->pc) >= BC__MAX)
	return;
      break;
    default: lj_assertJ(0, "bad post-processing mode"); break;
    }
    J->postproc = LJ_POST_NONE;
  }

  /* Need snapshot before recording next bytecode (e.g. after a store). */
  if (J->needsnap) {
    J->needsnap = 0;
    if (J->pt && bc_op(*J->pc) < BC_FUNCF) lj_snap_purge(J);
    lj_snap_add(J);
    J->mergesnap = 1;
  }

  /* Skip some bytecodes. */
  if (LJ_UNLIKELY(J->bcskip > 0)) {
    J->bcskip--;
    return;
  }

  /* Record only closed loops for root traces. */
  pc = J->pc;
  if (J->framedepth == 0 &&
     (MSize)((char *)pc - (char *)J->bc_min) >= J->bc_extent)
    lj_trace_err(J, LJ_TRERR_LLEAVE);

#ifdef LUA_USE_ASSERT
  rec_check_slots(J);
  rec_check_ir(J);
#endif

#if LJ_HASPROFILE
  rec_profile_ins(J, pc);
#endif

  /* Keep a copy of the runtime values of var/num/str operands. */
#define rav	(&ix.valv)
#define rbv	(&ix.tabv)
#define rcv	(&ix.keyv)

  lbase = J->L->base;
  ins = *pc;
  op = bc_op(ins);
  ra = bc_a(ins);
  ix.val = 0;
  switch (bcmode_a(op)) {
  case BCMvar:
    copyTV(J->L, rav, &lbase[ra]); ix.val = ra = getslot(J, ra); break;
  default: break;  /* Handled later. */
  }
  rb = bc_b(ins);
  rc = bc_c(ins);
  switch (bcmode_b(op)) {
  case BCMnone: rb = 0; rc = bc_d(ins); break;  /* Upgrade rc to 'rd'. */
  case BCMvar:
    copyTV(J->L, rbv, &lbase[rb]); ix.tab = rb = getslot(J, rb); break;
  default: break;  /* Handled later. */
  }
  switch (bcmode_c(op)) {
  case BCMvar:
    copyTV(J->L, rcv, &lbase[rc]); ix.key = rc = getslot(J, rc); break;
  case BCMpri: setpriV(rcv, ~rc); ix.key = rc = TREF_PRI(IRT_NIL+rc); break;
  case BCMnum: { cTValue *tv = proto_knumtv(J->pt, rc);
    copyTV(J->L, rcv, tv); ix.key = rc = tvisint(tv) ? lj_ir_kint(J, intV(tv)) :
#if LJ_54 && LJ_DUALNUM
    tvisi64(tv) ? lj_ir_kgc(J, gcV(tv), IRT_INT64) :
#endif
    tv->u32.hi == LJ_KEYINDEX ? (lj_ir_kint(J, 0) | TREF_KEYINDEX) :
#if LJ_54 && LJ_DUALNUM
    lj_ir_knum(J, numV(tv)); } break;
#else
    lj_ir_knumint(J, numV(tv)); } break;
#endif
  case BCMstr: { GCstr *s = gco2str(proto_kgc(J->pt, ~(ptrdiff_t)rc));
    setstrV(J->L, rcv, s); ix.key = rc = lj_ir_kstr(J, s); } break;
  default: break;  /* Handled later. */
  }

  switch (op) {

  /* -- Comparison ops ---------------------------------------------------- */

  case BC_ISLT: case BC_ISGE: case BC_ISLE: case BC_ISGT:
#if LJ_HASFFI
    if (tref_iscdata(ra) || tref_iscdata(rc)) {
      rec_mm_comp_cdata(J, &ix, op, ((int)op & 2) ? MM_le : MM_lt);
      break;
    }
#endif
    /* Emit nothing for two numeric or string consts. */
    if (!(tref_isk2(ra,rc) && tref_isnumber_str(ra) && tref_isnumber_str(rc))) {
      IRType ta = tref_isinteger(ra) ? IRT_INT : tref_type(ra);
      IRType tc = tref_isinteger(rc) ? IRT_INT : tref_type(rc);
      int irop;
#if LJ_54 && LJ_DUALNUM
      if ((rec_lua54_tref_isi64(ra) || rec_lua54_tref_isi64(rc) ||
	   tvisi64(rav) || tvisi64(rcv)) &&
	  rec_lua54_tref_isinteger(ra) && rec_lua54_tref_isinteger(rc) &&
	  rec_lua54_tv_isinteger(rav) && rec_lua54_tv_isinteger(rcv)) {
	rec_comp_prep(J);
	irop = (int)op - (int)BC_ISLT + (int)IR_LT;
	if (!rec_lua54_i64cmp(rav, rcv, (IROp)irop))
	  irop ^= 1;
	emitir(IRTG(irop, IRT_I64),
	       rec_lua54_i64ref(J, ra), rec_lua54_i64ref(J, rc));
	rec_comp_fixup(J, J->pc, ((int)op ^ irop) & 1);
	break;
      }
      if (((ta == IRT_INT64 && tc == IRT_NUM) ||
	   (ta == IRT_NUM && tc == IRT_INT64)
#if LJ_TARGET_ARM64
	   || (tref_type(ra) == IRT_I64 && tc == IRT_NUM)
	   || (ta == IRT_NUM && tref_type(rc) == IRT_I64)
#endif
	  ) &&
	  ((rec_lua54_tv_isinteger(rav) && tvisnum(rcv)) ||
	   (tvisnum(rav) && rec_lua54_tv_isinteger(rcv)))) {
	int emitop, mode, lefti64 = rec_lua54_tref_isi64(ra);
	TRef cmp;
	rec_comp_prep(J);
	irop = (int)op - (int)BC_ISLT + (int)IR_LT;
	mode = rec_lua54_cmpmode((IROp)irop);
	emitop = irop;
	if (!rec_lua54_i64numcmp(rav, rcv, (IROp)irop))
	  emitop ^= 1;
	cmp = lefti64 ?
	  lj_ir_call(J, IRCALL_lj_obj_i64cmpnum, rec_lua54_i64ref(J, ra),
		     rc, lj_ir_kint(J, mode)) :
	  lj_ir_call(J, IRCALL_lj_obj_numcmpi64, ra,
		     rec_lua54_i64ref(J, rc), lj_ir_kint(J, mode));
	emitir(IRTG(emitop == irop ? IR_NE : IR_EQ, IRT_INT),
	       cmp, lj_ir_kint(J, 0));
	rec_comp_fixup(J, J->pc, ((int)op ^ emitop) & 1);
	break;
      }
#endif
      if (ta != tc) {
	/* Widen mixed number/int comparisons to number/number comparison. */
	if (ta == IRT_INT && tc == IRT_NUM) {
	  ra = emitir(IRTN(IR_CONV), ra, IRCONV_NUM_INT);
	  ta = IRT_NUM;
	} else if (ta == IRT_NUM && tc == IRT_INT) {
	  rc = emitir(IRTN(IR_CONV), rc, IRCONV_NUM_INT);
	} else if (LJ_52) {
	  ta = IRT_NIL;  /* Force metamethod for different types. */
	} else if (!((ta == IRT_FALSE || ta == IRT_TRUE) &&
		     (tc == IRT_FALSE || tc == IRT_TRUE))) {
	  break;  /* Interpreter will throw for two different types. */
	}
      }
      rec_comp_prep(J);
      irop = (int)op - (int)BC_ISLT + (int)IR_LT;
      if (ta == IRT_NUM) {
	if ((irop & 1)) irop ^= 4;  /* ISGE/ISGT are unordered. */
	if (!lj_ir_numcmp(numberVnum(rav), numberVnum(rcv), (IROp)irop))
	  irop ^= 5;
      } else if (ta == IRT_INT) {
	if (!lj_ir_numcmp(numberVnum(rav), numberVnum(rcv), (IROp)irop))
	  irop ^= 1;
      } else if (ta == IRT_STR) {
#if LJ_54
	/* Lua 5.4 string ordering is locale-dependent through strcoll().
	** Record a side-effect call so the compare is re-run under the active
	** C locale and cannot be CSEd across os.setlocale() changes.
	*/
	if (!rec_lua54_strcmp_locale(strV(rav), strV(rcv), (IROp)irop))
	  irop ^= 1;
	ra = lj_ir_call(J, IRCALL_lj_str_cmp_locale, ra, rc);
	rc = lj_ir_kint(J, 0);
	ta = IRT_INT;
#else
	if (!lj_ir_strcmp(strV(rav), strV(rcv), (IROp)irop)) irop ^= 1;
	ra = lj_ir_call(J, IRCALL_lj_str_cmp, ra, rc);
	rc = lj_ir_kint(J, 0);
	ta = IRT_INT;
#endif
      } else {
	rec_mm_comp(J, &ix, (int)op);
	break;
      }
      emitir(IRTG(irop, ta), ra, rc);
      rec_comp_fixup(J, J->pc, ((int)op ^ irop) & 1);
    }
    break;

  case BC_ISEQV: case BC_ISNEV:
  case BC_ISEQS: case BC_ISNES:
  case BC_ISEQN: case BC_ISNEN:
  case BC_ISEQP: case BC_ISNEP:
#if LJ_HASFFI
    if (tref_iscdata(ra) || tref_iscdata(rc)) {
      rec_mm_comp_cdata(J, &ix, op, MM_eq);
      break;
    }
#endif
    /* Emit nothing for two non-table, non-udata consts. */
    if (!(tref_isk2(ra, rc) && !(tref_istab(ra) || tref_isudata(ra)))) {
      int diff;
      rec_comp_prep(J);
      diff = lj_record_objcmp(J, ra, rc, rav, rcv);
      if (diff == 2 || !(tref_istab(ra) || tref_isudata(ra)))
	rec_comp_fixup(J, J->pc, ((int)op & 1) == !diff);
      else if (diff == 1)  /* Only check __eq if different, but same type. */
	rec_mm_equal(J, &ix, (int)op);
    }
    break;

  /* -- Unary test and copy ops ------------------------------------------- */

  case BC_ISTC: case BC_ISFC:
    if ((op & 1) == tref_istruecond(rc))
      rc = 0;  /* Don't store if condition is not true. */
    /* fallthrough */
  case BC_IST: case BC_ISF:  /* Type specialization suffices. */
    if (bc_a(pc[1]) < J->maxslot)
      J->maxslot = bc_a(pc[1]);  /* Shrink used slots. */
    break;

  case BC_ISTYPE: case BC_ISNUM:
    /* These coercions need to correspond with lj_meta_istype(). */
    if (LJ_DUALNUM && rc == ~LJ_TNUMX+1)
      ra = lj_opt_narrow_toint(J, ra);
    else if (rc == ~LJ_TNUMX+2)
      ra = lj_ir_tonum(J, ra);
    else if (rc == ~LJ_TSTR+1)
      ra = lj_ir_tostr(J, ra);
    /* else: type specialization suffices. */
    J->base[bc_a(ins)] = ra;
    break;

  /* -- Unary ops --------------------------------------------------------- */

  case BC_NOT:
    /* Type specialization already forces const result. */
    rc = tref_istruecond(rc) ? TREF_FALSE : TREF_TRUE;
    break;

  case BC_LEN:
    if (tref_isstr(rc))
      rc = emitir(IRTI(IR_FLOAD), rc, IRFL_STR_LEN);
    else if (!LJ_52 && tref_istab(rc))
      rc = emitir(IRTI(IR_ALEN), rc, TREF_NIL);
    else
      rc = rec_mm_len(J, rc, rcv);
    break;

  /* -- Arithmetic ops ---------------------------------------------------- */

#if LJ_54
  case BC_BAND: case BC_BOR: case BC_BXOR: case BC_BSHL: case BC_BSHR:
  case BC_BNOT: {
    int64_t ib = 0, ic = 0;
    TRef irb, irc = 0;
    int unary = (op == BC_BNOT);
    if (unary) {  /* AD format: the operand sits in rc/rcv. */
      rb = rc;
      copyTV(J->L, rbv, rcv);
    }
#if LJ_DUALNUM
    if ((irb = rec_lua54_bitop_toint64ref(J, rb, rbv, &ib)) != 0 &&
	(unary ||
	 (irc = rec_lua54_bitop_toint64ref(J, rc, rcv, &ic)) != 0)) {
      uint64_t ub = (uint64_t)ib, uc = (uint64_t)ic;
      int64_t rv;
      TRef tr;
      switch (op) {
      case BC_BAND:
	rv = (int64_t)(ub & uc);
	tr = emitir(IRT(IR_BAND, IRT_I64), irb, irc);
	break;
      case BC_BOR:
	rv = (int64_t)(ub | uc);
	tr = emitir(IRT(IR_BOR, IRT_I64), irb, irc);
	break;
      case BC_BXOR:
	rv = (int64_t)(ub ^ uc);
	tr = emitir(IRT(IR_BXOR, IRT_I64), irb, irc);
	break;
      case BC_BSHL: case BC_BSHR: {
	IROp irop = (op == BC_BSHL) ? IR_BSHL : IR_BSHR;
	rv = rec_lua54_shiftint(ib, ic, op == BC_BSHL);
	tr = rec_lua54_shiftref(J, irb, irc, ic, irop);
	break;
      }
      default:  /* BC_BNOT */
	rv = (int64_t)~ub;
	tr = emitir(IRT(IR_BNOT, IRT_I64), irb, 0);
	break;
      }
#if LJ_TARGET_ARM64
      if (tref_isinteger(tr)) {
	rc = tr;
      } else if (rec_lua54_box_loopcarried_i64(J, bc_a(ins), lbase) &&
		 rec_lua54_next_loopback_keeps_slot(J, bc_a(ins))) {
	rc = rec_lua54_store_owned_i64(J, J->base[bc_a(ins)], tr, bc_a(ins));
      } else if (rec_lua54_next_bitop_operand(J, bc_a(ins))) {
	rc = rec_lua54_i64result_raw(J, tr, rv);
      } else {
	rc = rec_lua54_i64result_raw(J, tr, rv);
      }
#else
      rc = tref_isinteger(tr) ? tr : rec_lua54_i64result(J, tr, rv);
#endif
      break;
    }
#endif
    /* An operand without an integer representation dispatches to the bitwise
    ** metamethod (__band/__bor/__bxor/__shl/__shr/__bnot), exactly as the
    ** arithmetic ops dispatch to __add etc. For the unary BNOT, rb was set
    ** equal to rc above, but the operand decode left ix.tab unset. A non-table
    ** operand has no such metamethod, so rec_mm_arith aborts to the interpreter
    ** via NOMM, matching the previous NYI behavior (and Lua 5.4 semantics). */
    ix.tab = rb;
#if LJ_54 && LJ_DUALNUM && LJ_TARGET_ARM64
    /* An operand still held as an unboxed IRT_I64 (e.g. a large mask produced
    ** by a preceding arith op and kept raw for this bitop) must be boxed before
    ** rec_mm_arith stores it into the metamethod's call-argument slot, where it
    ** would otherwise be mishandled as a raw 64-bit value. */
    ix.tab = rec_lua54_boxraw_i64(J, ix.tab);
    ix.key = rec_lua54_boxraw_i64(J, ix.key);
#endif
    rc = rec_mm_arith(J, &ix, bcmode_mm(op));
    break;
  }
#endif

  case BC_UNM:
#if LJ_54 && LJ_DUALNUM
    if (rec_lua54_tref_isinteger(rc) && rec_lua54_tv_isinteger(rcv)) {
#if LJ_TARGET_ARM64
      rc = rec_lua54_unm_int(J, rc, rcv,
			     rec_lua54_next_unm_operand(J, bc_a(ins)) ||
			     rec_lua54_next_mod_operand(J, bc_a(ins)));
#else
      rc = rec_lua54_unm_int(J, rc, rcv, 0);
#endif
      break;
    } else {
      int64_t ic;
      TRef irc;
      if (rec_lua54_tv_toint64(rcv, &ic) &&
	  (irc = rec_lua54_toint64ref(J, rc, rcv))) {
#if LJ_TARGET_ARM64
	rc = rec_lua54_unm_intref(J, irc, ic,
				  rec_lua54_next_unm_operand(J, bc_a(ins)) ||
				  rec_lua54_next_mod_operand(J, bc_a(ins)));
#else
	rc = rec_lua54_unm_intref(J, irc, ic, 0);
#endif
	break;
      }
    }
#endif
    if (tref_isnumber_str(rc)) {
      rc = lj_opt_narrow_unm(J, rc, rcv);
    } else {
      ix.tab = rc;
      copyTV(J->L, &ix.tabv, rcv);
      rc = rec_mm_arith(J, &ix, MM_unm);
    }
    break;

  case BC_ADDNV: case BC_SUBNV: case BC_MULNV: case BC_DIVNV: case BC_MODNV:
    /* Swap rb/rc and rbv/rcv. rav is temp. */
    ix.tab = rc; ix.key = rc = rb; rb = ix.tab;
    copyTV(J->L, rav, rbv);
    copyTV(J->L, rbv, rcv);
    copyTV(J->L, rcv, rav);
    if (op == BC_MODNV)
      goto recmod;
    /* fallthrough */
  case BC_ADDVN: case BC_SUBVN: case BC_MULVN: case BC_DIVVN:
  case BC_ADDVV: case BC_SUBVV: case BC_MULVV: case BC_DIVVV: {
    MMS mm = bcmode_mm(op);
#if LJ_54 && LJ_DUALNUM
    IROp irop = (int)mm - (int)MM_add + (int)IR_ADD;
#if LJ_TARGET_ARM64
    int rawtmp = rec_lua54_next_tgetv_key(J, bc_a(ins)) ||
		 rec_lua54_next_comp_operand(J, bc_a(ins)) ||
		 rec_lua54_next_unm_operand(J, bc_a(ins)) ||
		 rec_lua54_next_arith_bitop_operand(J, bc_a(ins)) ||
		 rec_lua54_next_string_format_arg(J, bc_a(ins)) ||
		 rec_lua54_next_math_minmax_arg(J, bc_a(ins));
#endif
    if (rec_lua54_tref_isnumeric(rb) && rec_lua54_tref_isnumeric(rc)) {
      if (irop <= IR_MUL && rec_lua54_tv_isinteger(rbv) &&
	  rec_lua54_tv_isinteger(rcv)) {
#if LJ_TARGET_ARM64
	TRef tr = rec_lua54_arith_i32(J, rb, rc, rbv, rcv, mm);
	if (tr) {
	  rc = tr;
	} else if (rec_lua54_loopcarried_arith_i64(J, ins, rb, rc, lbase)) {
	  rc = rec_lua54_arith_intref_owned(J, rec_lua54_i64ref(J, rb),
					    rec_lua54_i64ref(J, rc),
					    rec_lua54_tv_i64(rbv),
					    rec_lua54_tv_i64(rcv), mm,
					    J->base[bc_a(ins)], bc_a(ins));
	} else {
	  rc = rec_lua54_arith_int(J, rb, rc, rbv, rcv, mm, rawtmp);
	}
#else
	rc = rec_lua54_arith_int(J, rb, rc, rbv, rcv, mm, 0);
#endif
      } else {
	rc = emitir(IRTN(irop), rec_lua54_numref(J, rb),
		    rec_lua54_numref(J, rc));
      }
      break;
    }
    if (irop <= IR_MUL) {
      int64_t ib, ic;
      TRef irb, irc;
	if (rec_lua54_tv_toint64(rbv, &ib) &&
	    rec_lua54_tv_toint64(rcv, &ic) &&
	    (irb = rec_lua54_toint64ref(J, rb, rbv)) &&
	    (irc = rec_lua54_toint64ref(J, rc, rcv))) {
	  rc = rec_lua54_arith_intref(J, irb, irc, ib, ic, mm,
#if LJ_TARGET_ARM64
				      rawtmp ||
#endif
				      tref_type(rb) == IRT_I64 ||
				      tref_type(rc) == IRT_I64);
	  break;
	}
      }
#endif
    if (tref_isnumber_str(rb) && tref_isnumber_str(rc)) {
#if LJ_54 && LJ_DUALNUM
      if (irop <= IR_MUL && tref_isinteger(rb) && tref_isinteger(rc)) {
	rc = emitir(IRTI(irop), rb, rc);
	break;
      }
#endif
      rc = lj_opt_narrow_arith(J, rb, rc, rbv, rcv,
			       (int)mm - (int)MM_add + (int)IR_ADD);
    } else {
      rc = rec_mm_arith(J, &ix, mm);
    }
    break;
    }

  case BC_MODVN: case BC_MODVV:
  recmod:
#if LJ_54 && LJ_DUALNUM
    if (tvisint(rbv) && tvisint(rcv) && intV(rcv) != 0 &&
	tref_isinteger(rb) && tref_isinteger(rc)) {
      rc = lj_opt_narrow_mod(J, rb, rc, rbv, rcv);
      break;
    }
    {
      int64_t ib, ic;
      TRef irb, irc;
      if (rec_lua54_tv_toint64(rbv, &ib) && rec_lua54_tv_toint64(rcv, &ic) &&
	  ic != 0 &&
	  (irb = rec_lua54_toint64ref(J, rb, rbv)) != 0 &&
	  (irc = rec_lua54_toint64ref(J, rc, rcv)) != 0) {
	TRef tr;
	emitir(IRTG(IR_NE, IRT_I64), irc, lj_ir_kint64(J, 0));
	tr = lj_ir_call(J, IRCALL_lj_obj_i64mod, irb, irc);
	rc = rec_lua54_i64result(J, tr, lj_obj_i64mod(ib, ic));
	break;
      }
    }
    if (tref_isnumber_str(rb) && tref_isnumber_str(rc)) {
#if LJ_TARGET_ARM64
      TRef nb = rec_lua54_tonumref(J, rb, rbv);
      TRef nc = rec_lua54_tonumref(J, rc, rcv);
      if (nb && nc) {
	rc = lj_ir_call(J, IRCALL_lj_vm_lua54fmod, nb, nc);
	break;
      }
#endif
      /* Lua 5.4 float modulo uses the official fmod formula; the narrowing
      ** recorder would bake the floor-based one, so stay interpreted.
      */
      setintV(&J->errinfo, (int32_t)op);
      lj_trace_err_info(J, LJ_TRERR_NYIBC);
    }
    rc = rec_mm_arith(J, &ix, MM_mod);
    break;
#else
    if (tref_isnumber_str(rb) && tref_isnumber_str(rc))
      rc = lj_opt_narrow_mod(J, rb, rc, rbv, rcv);
    else
      rc = rec_mm_arith(J, &ix, MM_mod);
    break;
#endif

#if LJ_54
  case BC_IDIV:
#if LJ_DUALNUM
#if LJ_TARGET_ARM64
    if (tvisint(rbv) && tvisint(rcv) && intV(rcv) > 0 &&
	tref_isinteger(rb) && tref_isinteger(rc) && tref_isk(rc)) {
      int32_t k = intV(rcv);
	    if ((k & (k-1)) == 0)
	      rc = emitir(IRTI(IR_BSAR), rb, lj_ir_kint(J, lj_fls((uint32_t)k)));
	    else
	      rc = emitir(IRTI(IR_DIV), rb, rc);
	    break;
	  }
#endif
    {
      int64_t ib, ic;
      TRef irb, irc;
      if (rec_lua54_tv_toint64(rbv, &ib) && rec_lua54_tv_toint64(rcv, &ic) &&
	  ic != 0 &&
	  (irb = rec_lua54_toint64ref(J, rb, rbv)) != 0 &&
	  (irc = rec_lua54_toint64ref(J, rc, rcv)) != 0) {
	TRef tr;
	emitir(IRTG(IR_NE, IRT_I64), irc, lj_ir_kint64(J, 0));
	tr = lj_ir_call(J, IRCALL_lj_obj_i64idiv, irb, irc);
	rc = rec_lua54_i64result(J, tr, lj_obj_i64idiv(ib, ic));
	break;
      }
    }
#endif
    if (!tref_isnumber_str(rb) || !tref_isnumber_str(rc)) {
      rc = rec_mm_arith(J, &ix, MM_idiv);
      break;
    }
    setintV(&J->errinfo, (int32_t)op);  /* Float '//' stays interpreted. */
    lj_trace_err_info(J, LJ_TRERR_NYIBC);
    break;
#endif

  case BC_POW:
#if LJ_54 && LJ_DUALNUM
    {
      TRef nb = rec_lua54_tonumref(J, rb, rbv);
      TRef nc = rec_lua54_tonumref(J, rc, rcv);
      if (nb && nc) {
	rc = emitir(IRTN(IR_POW), nb, nc);
	break;
      }
    }
#endif
    if (tref_isnumber_str(rb) && tref_isnumber_str(rc)) {
#if LJ_54 && LJ_DUALNUM
      if (tvisstr(rbv) || tvisstr(rcv))
	lj_trace_err(J, LJ_TRERR_BADTYPE);
#endif
      rc = lj_opt_narrow_arith(J, rb, rc, rbv, rcv, IR_POW);
      break;
    }
    rc = rec_mm_arith(J, &ix, MM_pow);
    break;

  /* -- Miscellaneous ops ------------------------------------------------- */

  case BC_CAT:
#if LJ_54 && LJ_TARGET_ARM64
    {
      TRef trb = getslot(J, rb), trc = getslot(J, rc);
      int len1 = (tref_isstr(trb) && tref_isk(trb) &&
		  ir_kstr(IR(tref_ref(trb)))->len == 1) ||
		 (tref_isstr(trc) && tref_isk(trc) &&
		  ir_kstr(IR(tref_ref(trc)))->len == 1);
      if (tref_isstr(trb) && tref_isstr(trc) && len1) {
	J->pt->flags |= PROTO_NOJIT;
	setintV(&J->errinfo, (int32_t)op);
	lj_trace_err_info(J, LJ_TRERR_NYIBC);
      }
    }
#endif
    rc = rec_cat(J, rb, rc);
    if (rc >= 0xffffff00)
      lj_err_throw(J->L, -(int32_t)rc);  /* Propagate errors. */
    break;

  /* -- Constant and move ops --------------------------------------------- */

  case BC_MOV:
    /* Clear gap of method call to avoid resurrecting previous refs. */
    if (ra > J->maxslot) {
#if LJ_FR2
      memset(J->base + J->maxslot, 0, (ra - J->maxslot) * sizeof(TRef));
#else
      J->base[ra-1] = 0;
#endif
    }
    break;
  case BC_KSTR: case BC_KPRI:
    break;
  case BC_KNUM: {
    cTValue *tv = proto_knumtv(J->pt, bc_d(ins));
    rc = tvisint(tv) ? lj_ir_kint(J, intV(tv)) :
#if LJ_54 && LJ_DUALNUM
      tvisi64(tv) ? lj_ir_kgc(J, gcV(tv), IRT_INT64) :
      lj_ir_knum(J, numV(tv));
#else
      lj_ir_knumint(J, numV(tv));
#endif
    break;
    }
  case BC_KSHORT:
    rc = lj_ir_kint(J, (int32_t)(int16_t)rc);
    break;
  case BC_KNIL:
    if (LJ_FR2 && ra > J->maxslot)
      J->base[ra-1] = 0;
    while (ra <= rc)
      J->base[ra++] = TREF_NIL;
    if (rc >= J->maxslot) J->maxslot = rc+1;
    break;
#if LJ_HASFFI
  case BC_KCDATA:
    rc = lj_ir_kgc(J, proto_kgc(J->pt, ~(ptrdiff_t)rc), IRT_CDATA);
    break;
#endif

  /* -- Upvalue and function ops ------------------------------------------ */

  case BC_UGET:
    rc = rec_upvalue(J, rc, 0);
    break;
  case BC_USETV: case BC_USETS: case BC_USETN: case BC_USETP:
    rec_upvalue(J, ra, rc);
    break;

  /* -- Table ops --------------------------------------------------------- */

  case BC_GGET: case BC_GSET:
    settabV(J->L, &ix.tabv, tabref(J->fn->l.env));
    ix.tab = emitir(IRT(IR_FLOAD, IRT_TAB), getcurrf(J), IRFL_FUNC_ENV);
    ix.idxchain = LJ_MAX_IDXCHAIN;
    rc = lj_record_idx(J, &ix);
    break;

  case BC_TGETB: case BC_TSETB:
    setintV(&ix.keyv, (int32_t)rc);
    ix.key = lj_ir_kint(J, (int32_t)rc);
    /* fallthrough */
  case BC_TGETV: case BC_TGETS: case BC_TSETV: case BC_TSETS:
    ix.idxchain = LJ_MAX_IDXCHAIN;
    rc = lj_record_idx(J, &ix);
    break;
  case BC_TGETR: case BC_TSETR:
    ix.idxchain = 0;
    rc = lj_record_idx(J, &ix);
    break;

  case BC_TSETM:
    rec_tsetm(J, ra, (BCReg)(J->L->top - J->L->base), (int32_t)rcv->u32.lo);
    J->maxslot = ra;  /* The table slot at ra-1 is the highest used slot. */
    break;

  case BC_TNEW:
    rc = rec_tnew(J, rc);
    break;
  case BC_TDUP:
    rc = emitir(IRTG(IR_TDUP, IRT_TAB),
		lj_ir_ktab(J, gco2tab(proto_kgc(J->pt, ~(ptrdiff_t)rc))), 0);
#ifdef LUAJIT_ENABLE_TABLE_BUMP
    J->rbchash[(rc & (RBCHASH_SLOTS-1))].ref = tref_ref(rc);
    setmref(J->rbchash[(rc & (RBCHASH_SLOTS-1))].pc, pc);
    setgcref(J->rbchash[(rc & (RBCHASH_SLOTS-1))].pt, obj2gco(J->pt));
#endif
    break;

  /* -- Calls and vararg handling ----------------------------------------- */

  case BC_ITERC:
#if LJ_54
    if (J->L->closelist != NULL)
      lj_trace_err_info(J, LJ_TRERR_NYIBC);
#endif
    J->base[ra] = getslot(J, ra-3);
    J->base[ra+1+LJ_FR2] = getslot(J, ra-2);
    J->base[ra+2+LJ_FR2] = getslot(J, ra-1);
    { /* Do the actual copy now because lj_record_call needs the values. */
      TValue *b = &J->L->base[ra];
      copyTV(J->L, b, b-3);
      copyTV(J->L, b+1+LJ_FR2, b-2);
      copyTV(J->L, b+2+LJ_FR2, b-1);
    }
    lj_record_call(J, ra, (ptrdiff_t)rc-1);
    break;

  /* L->top is set to L->base+ra+rc+NARGS-1+1. See lj_dispatch_ins(). */
  case BC_CALLM:
    rc = (BCReg)(J->L->top - J->L->base) - ra - LJ_FR2;
    /* fallthrough */
  case BC_CALL:
    lj_record_call(J, ra, (ptrdiff_t)rc-1);
    break;

  case BC_CALLMT:
    rc = (BCReg)(J->L->top - J->L->base) - ra - LJ_FR2;
    /* fallthrough */
  case BC_CALLT:
    lj_record_tailcall(J, ra, (ptrdiff_t)rc-1);
    break;

  case BC_VARG:
    rec_varg(J, ra, (ptrdiff_t)rb-1);
    break;

  /* -- Returns ----------------------------------------------------------- */

  case BC_RETM:
    /* L->top is set to L->base+ra+rc+NRESULTS-1, see lj_dispatch_ins(). */
    rc = (BCReg)(J->L->top - J->L->base) - ra + 1;
    /* fallthrough */
  case BC_RET: case BC_RET0: case BC_RET1:
#if LJ_HASPROFILE
    rec_profile_ret(J);
#endif
    lj_record_ret(J, ra, (ptrdiff_t)rc-1);
    break;

  /* -- Loops and branches ------------------------------------------------ */

  case BC_FORI:
    if (rec_for(J, pc, 0) != LOOPEV_LEAVE)
      J->loopref = J->cur.nins;
    break;
  case BC_JFORI:
    lj_assertJ(bc_op(pc[(ptrdiff_t)rc-BCBIAS_J]) == BC_JFORL,
	       "JFORI does not point to JFORL");
    if (rec_for(J, pc, 0) != LOOPEV_LEAVE)  /* Link to existing loop. */
      lj_record_stop(J, LJ_TRLINK_ROOT, bc_d(pc[(ptrdiff_t)rc-BCBIAS_J]));
    /* Continue tracing if the loop is not entered. */
    break;

  case BC_FORL:
    rec_loop_interp(J, pc, rec_for(J, pc+((ptrdiff_t)rc-BCBIAS_J), 1));
    break;
  case BC_ITERL:
    rec_loop_interp(J, pc, rec_iterl(J, *pc));
    break;
  case BC_ITERN:
    rec_loop_interp(J, pc, rec_itern(J, ra, rb));
    break;
  case BC_LOOP:
    rec_loop_interp(J, pc, rec_loop(J, ra, 1));
    break;

  case BC_JFORL:
    rec_loop_jit(J, rc, rec_for(J, pc+bc_j(traceref(J, rc)->startins), 1));
    break;
  case BC_JITERL:
    rec_loop_jit(J, rc, rec_iterl(J, traceref(J, rc)->startins));
    break;
  case BC_JLOOP:
    rec_loop_jit(J, rc, rec_loop(J, ra,
				 !bc_isret(bc_op(traceref(J, rc)->startins)) &&
				 bc_op(traceref(J, rc)->startins) != BC_ITERN));
    break;

  case BC_IFORL:
  case BC_IITERL:
  case BC_ILOOP:
  case BC_IFUNCF:
  case BC_IFUNCV:
    lj_trace_err(J, LJ_TRERR_BLACKL);
    break;

  case BC_JMP:
    if (ra < J->maxslot)
      J->maxslot = ra;  /* Shrink used slots. */
    break;

  case BC_ISNEXT:
    rec_isnext(J, ra);
    break;

  /* -- Function headers -------------------------------------------------- */

  case BC_FUNCF:
    rec_func_lua(J);
    break;
  case BC_JFUNCF:
    rec_func_jit(J, rc);
    break;

  case BC_FUNCV:
    rec_func_vararg(J);
    rec_func_lua(J);
    break;
  case BC_JFUNCV:
    /* Cannot happen. No hotcall counting for varag funcs. */
    lj_assertJ(0, "unsupported vararg hotcall");
    break;

  case BC_FUNCC:
  case BC_FUNCCW:
    lj_ffrecord_func(J);
    break;

  default:
    if (op >= BC__MAX) {
      lj_ffrecord_func(J);
      break;
    }
    /* fallthrough */
  case BC_UCLO:
  case BC_FNEW:
    setintV(&J->errinfo, (int32_t)op);
    lj_trace_err_info(J, LJ_TRERR_NYIBC);
    break;
  }

  /* rc == 0 if we have no result yet, e.g. pending __index metamethod call. */
  if (bcmode_a(op) == BCMdst && rc) {
    J->base[ra] = rc;
    if (ra >= J->maxslot) {
#if LJ_FR2
      if (ra > J->maxslot) J->base[ra-1] = 0;
#endif
      J->maxslot = ra+1;
    }
  }

#undef rav
#undef rbv
#undef rcv

  /* Limit the number of recorded IR instructions and constants. */
  if (J->cur.nins > REF_FIRST+(IRRef)J->param[JIT_P_maxrecord] ||
      J->cur.nk < REF_BIAS-(IRRef)J->param[JIT_P_maxirconst])
    lj_trace_err(J, LJ_TRERR_TRACEOV);
}

/* -- Recording setup ----------------------------------------------------- */

/* Setup recording for a root trace started by a hot loop. */
static const BCIns *rec_setup_root(jit_State *J)
{
  /* Determine the next PC and the bytecode range for the loop. */
  const BCIns *pcj, *pc = J->pc;
  BCIns ins = *pc;
  BCReg ra = bc_a(ins);
  switch (bc_op(ins)) {
  case BC_FORL:
    J->bc_extent = (MSize)(-bc_j(ins))*sizeof(BCIns);
    pc += 1+bc_j(ins);
    J->bc_min = pc;
    break;
  case BC_ITERL:
    if (bc_op(pc[-1]) == BC_JLOOP)
      lj_trace_err(J, LJ_TRERR_LINNER);
    lj_assertJ(bc_op(pc[-1]) == BC_ITERC, "no ITERC before ITERL");
    J->maxslot = ra + bc_b(pc[-1]) - 1;
    J->bc_extent = (MSize)(-bc_j(ins))*sizeof(BCIns);
    pc += 1+bc_j(ins);
    lj_assertJ(bc_op(pc[-1]) == BC_JMP, "ITERL does not point to JMP+1");
    J->bc_min = pc;
    break;
  case BC_ITERN:
    lj_assertJ(bc_op(pc[1]) == BC_ITERL, "no ITERL after ITERN");
    J->maxslot = ra;
    J->bc_extent = (MSize)(-bc_j(pc[1]))*sizeof(BCIns);
    J->bc_min = pc+2 + bc_j(pc[1]);
    J->state = LJ_TRACE_RECORD_1ST;  /* Record the first ITERN, too. */
    break;
  case BC_LOOP:
    /* Only check BC range for real loops, but not for "repeat until true". */
    pcj = pc + bc_j(ins);
    ins = *pcj;
    if (bc_op(ins) == BC_JMP && bc_j(ins) < 0) {
      J->bc_min = pcj+1 + bc_j(ins);
      J->bc_extent = (MSize)(-bc_j(ins))*sizeof(BCIns);
    }
    J->maxslot = ra;
    pc++;
    break;
  case BC_RET:
  case BC_RET0:
  case BC_RET1:
    /* No bytecode range check for down-recursive root traces. */
    J->maxslot = ra + bc_d(ins) - 1;
    break;
  case BC_FUNCF:
    /* No bytecode range check for root traces started by a hot call. */
    J->maxslot = J->pt->numparams;
    pc++;
    break;
  case BC_CALLM:
  case BC_CALL:
  case BC_ITERC:
    /* No bytecode range check for stitched traces. */
    pc++;
    break;
  default:
    lj_assertJ(0, "bad root trace start bytecode %d", bc_op(ins));
    break;
  }
  return pc;
}

/* Setup for recording a new trace. */
void lj_record_setup(jit_State *J)
{
  uint32_t i;

  /* Initialize state related to current trace. */
  memset(J->slot, 0, sizeof(J->slot));
  memset(J->chain, 0, sizeof(J->chain));
#ifdef LUAJIT_ENABLE_TABLE_BUMP
  memset(J->rbchash, 0, sizeof(J->rbchash));
#endif
  memset(J->bpropcache, 0, sizeof(J->bpropcache));
  J->scev.idx = REF_NIL;
  setmref(J->scev.pc, NULL);

  J->baseslot = 1+LJ_FR2;  /* Invoking function is at base[-1-LJ_FR2]. */
  J->base = J->slot + J->baseslot;
  J->maxslot = 0;
  J->framedepth = 0;
  J->retdepth = 0;

  J->instunroll = J->param[JIT_P_instunroll];
  J->loopunroll = J->param[JIT_P_loopunroll];
  J->tailcalled = 0;
  J->loopref = 0;

  J->bc_min = NULL;  /* Means no limit. */
  J->bc_extent = ~(MSize)0;

  /* Emit instructions for fixed references. Also triggers initial IR alloc. */
  emitir_raw(IRT(IR_BASE, IRT_PGC), J->parent, J->exitno);
  for (i = 0; i <= 2; i++) {
    IRIns *ir = IR(REF_NIL-i);
    ir->i = 0;
    ir->t.irt = (uint8_t)(IRT_NIL+i);
    ir->o = IR_KPRI;
    ir->prev = 0;
  }
  J->cur.nk = REF_TRUE;

  J->startpc = J->pc;
  setmref(J->cur.startpc, J->pc);
  if (J->parent) {  /* Side trace. */
    GCtrace *T = traceref(J, J->parent);
    TraceNo root = T->root ? T->root : J->parent;
    J->cur.root = (uint16_t)root;
    J->cur.startins = BCINS_AD(BC_JMP, 0, 0);
    /* Check whether we could at least potentially form an extra loop. */
    if (J->exitno == 0 && T->snap[0].nent == 0) {
      /* We can narrow a FORL for some side traces, too. */
      if (J->pc > proto_bc(J->pt) && bc_op(J->pc[-1]) == BC_JFORI &&
	  bc_d(J->pc[bc_j(J->pc[-1])-1]) == root) {
	lj_snap_add(J);
	rec_for_loop(J, J->pc-1, &J->scev, 1);
	goto sidecheck;
      }
    } else {
      J->startpc = NULL;  /* Prevent forming an extra loop. */
    }
    lj_snap_replay(J, T);
  sidecheck:
    if ((traceref(J, J->cur.root)->nchild >= J->param[JIT_P_maxside] ||
	 T->snap[J->exitno].count >= J->param[JIT_P_hotexit] +
				     J->param[JIT_P_tryside])) {
      if (bc_op(*J->pc) == BC_JLOOP) {
	BCIns startins = traceref(J, bc_d(*J->pc))->startins;
	if (bc_op(startins) == BC_ITERN)
	  rec_itern(J, bc_a(startins), bc_b(startins));
      }
      lj_record_stop(J, LJ_TRLINK_INTERP, 0);
    }
  } else {  /* Root trace. */
    J->cur.root = 0;
    J->cur.startins = *J->pc;
    J->pc = rec_setup_root(J);
    /* Note: the loop instruction itself is recorded at the end and not
    ** at the start! So snapshot #0 needs to point to the *next* instruction.
    ** The one exception is BC_ITERN, which sets LJ_TRACE_RECORD_1ST.
    */
    lj_snap_add(J);
    if (bc_op(J->cur.startins) == BC_FORL)
      rec_for_loop(J, J->pc-1, &J->scev, 1);
    else if (bc_op(J->cur.startins) == BC_ITERC)
      J->startpc = NULL;
    if (1 + J->pt->framesize >= LJ_MAX_JSLOTS)
      lj_trace_err(J, LJ_TRERR_STACKOV);
  }
#if LJ_HASPROFILE
  J->prev_pt = NULL;
  J->prev_line = -1;
#endif
#ifdef LUAJIT_ENABLE_CHECKHOOK
  /* Regularly check for instruction/line hooks from compiled code and
  ** exit to the interpreter if the hooks are set.
  **
  ** This is a compile-time option and disabled by default, since the
  ** hook checks may be quite expensive in tight loops.
  **
  ** Note this is only useful if hooks are *not* set most of the time.
  ** Use this only if you want to *asynchronously* interrupt the execution.
  **
  ** You can set the instruction hook via lua_sethook() with a count of 1
  ** from a signal handler or another native thread. Please have a look
  ** at the first few functions in luajit.c for an example (Ctrl-C handler).
  */
  {
    TRef tr = emitir(IRT(IR_XLOAD, IRT_U8),
		     lj_ir_kptr(J, &J2G(J)->hookmask), IRXLOAD_VOLATILE);
    tr = emitir(IRTI(IR_BAND), tr, lj_ir_kint(J, (LUA_MASKLINE|LUA_MASKCOUNT)));
    emitir(IRTGI(IR_EQ), tr, lj_ir_kint(J, 0));
  }
#endif
}

#undef IR
#undef emitir_raw
#undef emitir

#endif
