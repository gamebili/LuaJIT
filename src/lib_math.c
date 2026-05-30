/*
** Math library.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#include <math.h>
#include <string.h>
#include <time.h>

#define lib_math_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_debug.h"
#include "lj_err.h"
#include "lj_ff.h"
#include "lj_frame.h"
#include "lj_lib.h"
#include "lj_meta.h"
#include "lj_str.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "lj_vm.h"
#include "lj_prng.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_math

#define LJ_MATH_MAXINTEGER	LUA_MAXINTEGER
#define LJ_MATH_MININTEGER	LUA_MININTEGER
#define LJ_MATH_MAXINT32	((lua_Integer)2147483647)
#define LJ_MATH_MININT32	((lua_Integer)(-LJ_MATH_MAXINT32 - 1))

#if LJ_54
#if LJ_64
#define LJ_MATH_API_MININTEGER		(-9223372036854775807.0 - 1.0)
#define LJ_MATH_API_MAXINTEGER_EXCL	9223372036854775808.0
#else
#define LJ_MATH_API_MININTEGER		((lua_Number)LUA_MININTEGER)
#define LJ_MATH_API_MAXINTEGER_EXCL	(-(lua_Number)LUA_MININTEGER)
#endif

static const char *math_argname54(lua_State *L, const char *fallback)
{
  return lj_debug_callname54(L, fallback, "math");
}

static int math_tointeger54(lua_State *L, int narg, lua_Integer *ip, int *isnum)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  lua_Number n;
  int64_t k;
  if (isnum)
    *isnum = 0;
  if (o >= L->top)
    return 0;
  if (tvisstr(o)) {
    GCstr *s = strV(o);
    StrScanFmt fmt;
    if (lj_strscan_rejectnum54(strdata(s), s->len))
      return 0;
    fmt = lj_strscan_scan((const uint8_t *)strdata(s), s->len, &tmp,
			  STRSCAN_OPT_TOINT);
    if (fmt == STRSCAN_INT) {
      if (isnum) *isnum = 1;
      *ip = (lua_Integer)tmp.i;
      return 1;
    } else if (fmt == STRSCAN_I64) {
      if (isnum) *isnum = 1;
      *ip = (lua_Integer)tmp.u64;
      return 1;
    } else if (fmt != STRSCAN_NUM) {
      return 0;
    }
    o = &tmp;
  }
  if (!tvisnumber(o) && !tvisi64(o))
    return 0;
  if (isnum)
    *isnum = 1;
  if (tvisint(o)) {
    *ip = (lua_Integer)intV(o);
    return 1;
  } else if (tvisi64(o)) {
    *ip = (lua_Integer)i64V(o);
    return 1;
  }
  n = numV(o);
  if (!(n >= LJ_MATH_API_MININTEGER && n < LJ_MATH_API_MAXINTEGER_EXCL))
    return 0;
  k = lj_num2i64(n);
  if ((lua_Number)k != n)
    return 0;
  *ip = (lua_Integer)k;
  return 1;
}

#if !LJ_DUALNUM
static int math_toint32(lua_State *L, int narg, int32_t *ip, int *isnum)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  double n, ni;
  if (isnum)
    *isnum = 0;
  if (o >= L->top)
    return 0;
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      return 0;
    o = &tmp;
  }
  if (!tvisnumber(o) && !tvisi64(o))
    return 0;
  if (isnum)
    *isnum = 1;
  if (tvisint(o)) {
    *ip = intV(o);
    return 1;
  } else if (tvisi64(o)) {
    int64_t i = i64V(o);
    if (!checki32(i))
      return 0;
    *ip = (int32_t)i;
    return 1;
  }
  n = numV(o);
  if (!(n >= (double)LJ_MATH_MININT32 &&
	n <= (double)LJ_MATH_MAXINT32))
    return 0;
  ni = lj_vm_floor(n);
  if (n != ni)
    return 0;
  *ip = (int32_t)n;
  return 1;
}
#endif

static void math_argtype_named54(lua_State *L, int narg, const char *fname,
				 const char *xname);

static lua_Integer math_checkinteger_named54(lua_State *L, int narg,
					     const char *fname)
{
  lua_Integer i;
  int isnum;
  if (!math_tointeger54(L, narg, &i, &isnum)) {
    if (isnum)
      lj_err_callermsg(L,
	lj_strfmt_pushf(L, "bad argument #%d to '%s' "
			"(number has no integer representation)", narg,
			math_argname54(L, fname)));
    math_argtype_named54(L, narg, fname, "number");
  }
  return i;
}

static void math_pushintegernum(lua_State *L, lua_Number n)
{
  double ni;
  if (n >= LJ_MATH_API_MININTEGER && n < LJ_MATH_API_MAXINTEGER_EXCL) {
    ni = lj_vm_floor(n);
    if (n == ni) {
      lj_obj_setint64(L, L->top, (int64_t)lj_num2i64(n));
      L->top++;
      return;
    }
  }
  setnumV(L->top++, n);
}

static void math_argtype_named54(lua_State *L, int narg, const char *fname,
				 const char *xname)
{
  TValue *o = L->base + narg-1;
  const char *tname;
  MSize tlen;
  if (o < L->top) {
    tname = lj_meta_objtypename(L, o, &tlen);
    UNUSED(tlen);
  } else {
    tname = lj_obj_typename[0];
  }
  fname = math_argname54(L, fname);
  lj_err_callermsg(L,
    lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s expected, got %s)",
		    narg, fname, xname, tname));
}

static void math_argvalue_named54(lua_State *L, int narg, const char *fname)
{
  fname = math_argname54(L, fname);
  lj_err_callermsg(L,
    lj_strfmt_pushf(L, "bad argument #%d to '%s' (value expected)",
		    narg, fname));
}

static lua_Number math_checknum_named54(lua_State *L, int narg,
					const char *fname)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  if (o >= L->top)
    math_argtype_named54(L, narg, fname, "number");
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      math_argtype_named54(L, narg, fname, "number");
    o = &tmp;
  }
  if (tvisint(o))
    return (lua_Number)intV(o);
  if (tvisi64(o))
    return (lua_Number)i64V(o);
  if (tvisnum(o))
    return numV(o);
  math_argtype_named54(L, narg, fname, "number");
  return 0;  /* unreachable */
}

#endif

LJLIB_ASM(math_abs)		LJLIB_REC(.)
{
#if LJ_54
  cTValue *o = L->base;
  if (o < L->top && tvisint(o)) {
    int32_t i = intV(o);
    if (i < 0) {
      lj_obj_setint64(L, L->base-1-LJ_FR2,
	(int64_t)(lua_Integer)((lua_Unsigned)0 - (lua_Unsigned)(lua_Integer)i));
    } else {
      setintV(L->base-1-LJ_FR2, i);
    }
  } else if (o < L->top && tvisi64(o)) {
    lua_Integer i = (lua_Integer)i64V(o);
    if (i < 0)
      i = (lua_Integer)((lua_Unsigned)0 - (lua_Unsigned)i);
    lj_obj_setint64(L, L->base-1-LJ_FR2, (int64_t)i);
  } else {
    setnumV(L->base-1-LJ_FR2, fabs(math_checknum_named54(L, 1, "math.abs")));
  }
  return FFH_RES(1);
#else
  lj_lib_checknumber(L, 1);
  return FFH_RETRY;
#endif
}
LJLIB_ASM_(math_floor)		LJLIB_REC(math_round IRFPM_FLOOR)
LJLIB_ASM_(math_ceil)		LJLIB_REC(math_round IRFPM_CEIL)

LJLIB_ASM(math_sqrt)		LJLIB_REC(math_unary IRFPM_SQRT)
{
#if LJ_54
  setnumV(L->base-1-LJ_FR2, sqrt(math_checknum_named54(L, 1, "math.sqrt")));
  return FFH_RES(1);
#else
  lj_lib_checknum(L, 1);
  return FFH_RETRY;
#endif
}
LJLIB_ASM_(math_log10)		LJLIB_REC(math_call IRCALL_log10)
LJLIB_ASM(math_exp)		LJLIB_REC(math_call IRCALL_exp)
{
#if LJ_54
  setnumV(L->base-1-LJ_FR2, exp(math_checknum_named54(L, 1, "math.exp")));
  return FFH_RES(1);
#else
  lj_lib_checknum(L, 1);
  return FFH_RETRY;
#endif
}
LJLIB_ASM(math_sin)		LJLIB_REC(math_call IRCALL_sin)
{
#if LJ_54
  setnumV(L->base-1-LJ_FR2, sin(math_checknum_named54(L, 1, "math.sin")));
  return FFH_RES(1);
#else
  lj_lib_checknum(L, 1);
  return FFH_RETRY;
#endif
}
LJLIB_ASM(math_cos)		LJLIB_REC(math_call IRCALL_cos)
{
#if LJ_54
  setnumV(L->base-1-LJ_FR2, cos(math_checknum_named54(L, 1, "math.cos")));
  return FFH_RES(1);
#else
  lj_lib_checknum(L, 1);
  return FFH_RETRY;
#endif
}
LJLIB_ASM(math_tan)		LJLIB_REC(math_call IRCALL_tan)
{
#if LJ_54
  setnumV(L->base-1-LJ_FR2, tan(math_checknum_named54(L, 1, "math.tan")));
  return FFH_RES(1);
#else
  lj_lib_checknum(L, 1);
  return FFH_RETRY;
#endif
}
LJLIB_ASM(math_asin)		LJLIB_REC(math_call IRCALL_asin)
{
#if LJ_54
  setnumV(L->base-1-LJ_FR2, asin(math_checknum_named54(L, 1, "math.asin")));
  return FFH_RES(1);
#else
  lj_lib_checknum(L, 1);
  return FFH_RETRY;
#endif
}
LJLIB_ASM(math_acos)		LJLIB_REC(math_call IRCALL_acos)
{
#if LJ_54
  setnumV(L->base-1-LJ_FR2, acos(math_checknum_named54(L, 1, "math.acos")));
  return FFH_RES(1);
#else
  lj_lib_checknum(L, 1);
  return FFH_RETRY;
#endif
}
LJLIB_ASM_(math_atan)		LJLIB_REC(math_atan IRCALL_atan)
LJLIB_ASM_(math_sinh)		LJLIB_REC(math_call IRCALL_sinh)
LJLIB_ASM_(math_cosh)		LJLIB_REC(math_call IRCALL_cosh)
LJLIB_ASM_(math_tanh)		LJLIB_REC(math_call IRCALL_tanh)
LJLIB_ASM_(math_frexp)
LJLIB_ASM_(math_modf)		LJLIB_REC(math_modf)

LJLIB_ASM(math_log)		LJLIB_REC(math_log)
{
#if LJ_54
  if (L->base+1 < L->top && !tvisnil(L->base+1)) {
    double x = math_checknum_named54(L, 1, "math.log");
    double y = math_checknum_named54(L, 2, "math.log");
#ifdef LUAJIT_NO_LOG2
    x = log(x); y = 1.0 / log(y);
#else
    x = lj_vm_log2(x); y = 1.0 / lj_vm_log2(y);
#endif
    setnumV(L->base-1-LJ_FR2, x*y);  /* Do NOT join the expression to x / y. */
    return FFH_RES(1);
  }
  setnumV(L->base-1-LJ_FR2, log(math_checknum_named54(L, 1, "math.log")));
  return FFH_RES(1);
#else
  double x = lj_lib_checknum(L, 1);
  if (L->base+1 < L->top) {
    double y = lj_lib_checknum(L, 2);
#ifdef LUAJIT_NO_LOG2
    x = log(x); y = 1.0 / log(y);
#else
    x = lj_vm_log2(x); y = 1.0 / lj_vm_log2(y);
#endif
    setnumV(L->base-1-LJ_FR2, x*y);  /* Do NOT join the expression to x / y. */
    return FFH_RES(1);
  }
#endif
  return FFH_RETRY;
}

LJLIB_LUA(math_deg) /* function(x) return x * 57.29577951308232 end */
LJLIB_LUA(math_rad) /* function(x) return x * 0.017453292519943295 end */

LJLIB_ASM(math_atan2)		LJLIB_REC(.)
{
  lj_lib_checknum(L, 1);
  lj_lib_checknum(L, 2);
  return FFH_RETRY;
}
LJLIB_ASM_(math_pow)		LJLIB_REC(.)
LJLIB_ASM_(math_fmod)		LJLIB_REC(lua54_fmod)

LJLIB_ASM(math_ldexp)		LJLIB_REC(.)
{
  lj_lib_checknum(L, 1);
#if LJ_DUALNUM && !LJ_TARGET_X86ORX64
  lj_lib_checkint(L, 2);
#else
  lj_lib_checknum(L, 2);
#endif
  return FFH_RETRY;
}

LJLIB_ASM(math_min)		LJLIB_REC(math_minmax IR_MIN)
{
  int i = 0;
  do { lj_lib_checknumber(L, ++i); } while (L->base+i < L->top);
  return FFH_RETRY;
}
LJLIB_ASM_(math_max)		LJLIB_REC(math_minmax IR_MAX)

LJLIB_PUSH(3.14159265358979323846) LJLIB_SET(pi)
LJLIB_PUSH(1e310) LJLIB_SET(huge)

#if LJ_54
static int math_minmax54(lua_State *L, int ismax)
{
  int i, top = lua_gettop(L);
  int best = 1;
  if (L->base >= L->top)
    math_argvalue_named54(L, 1, ismax ? "math.max" : "math.min");
  /* Lua 5.4 math.min/max use ordinary < comparisons, so strings and objects
  ** with __lt are valid; only the zero-argument case is a value error.
  */
  for (i = 2; i <= top; i++) {
    int take = ismax ? lua_compare(L, best, i, LUA_OPLT) :
		       lua_compare(L, i, best, LUA_OPLT);
    if (take)
      best = i;
  }
  lua_pushvalue(L, best);
  return 1;
}

static int lj_cf_math_min54(lua_State *L)
{
  return math_minmax54(L, 0);
}

static int lj_cf_math_max54(lua_State *L)
{
  return math_minmax54(L, 1);
}

static int lj_cf_math_type(lua_State *L)
{
  cTValue *o = L->base;
  /* Lua 5.4 returns nil for non-numbers, but still errors when the argument is
  ** absent; keep the official function name in that argument error.
  */
  if (o >= L->top)
    math_argvalue_named54(L, 1, "math.type");
  if (!tvisnumber(o) && !tvisi64(o))
    setnilV(L->top++);
  else if (tvisinteger(o))
    setstrV(L, L->top++, lj_str_newlit(L, "integer"));
  else {
#if LJ_DUALNUM
    /* With dual numbers the TValue tag already carries Lua 5.4's integer vs.
    ** float distinction; exact float values such as 1.0 must stay "float".
    */
    setstrV(L, L->top++, lj_str_newlit(L, "float"));
#else
    int32_t i;
    int isnum;
    /* Non-dual-number builds cannot preserve Lua 5.4's exact integer tag.
    ** Report exact 32 bit integer-valued numbers as integers so results from
    ** floor/ceil/modf/tointeger keep the expected compatibility surface.
    */
    if (math_toint32(L, 1, &i, &isnum))
      setstrV(L, L->top++, lj_str_newlit(L, "integer"));
    else
      setstrV(L, L->top++, lj_str_newlit(L, "float"));
#endif
  }
  return 1;
}

static int lj_cf_math_tointeger(lua_State *L)
{
  lua_Integer i;
  int isnum;
  /* Lua 5.4 errors only when the value is absent. */
  if (L->base >= L->top)
    math_argvalue_named54(L, 1, "math.tointeger");
  if (math_tointeger54(L, 1, &i, &isnum)) {
    lj_obj_setint64(L, L->top, (int64_t)i);
    L->top++;
  }
  else
    setnilV(L->top++);
  return 1;
}

static int lj_cf_math_floor54(lua_State *L)
{
  cTValue *o = L->base;
  if (o < L->top && tvisinteger(o)) {
    copyTV(L, L->top++, o);
    return 1;
  }
  lua_Number n = math_checknum_named54(L, 1, "math.floor");
  math_pushintegernum(L, lj_vm_floor(n));
  return 1;
}

static int lj_cf_math_ceil54(lua_State *L)
{
  cTValue *o = L->base;
  if (o < L->top && tvisinteger(o)) {
    copyTV(L, L->top++, o);
    return 1;
  }
  lua_Number n = math_checknum_named54(L, 1, "math.ceil");
  math_pushintegernum(L, -lj_vm_floor(-n));
  return 1;
}

static int lj_cf_math_modf54(lua_State *L)
{
  cTValue *o = L->base;
  if (o < L->top && tvisinteger(o)) {
    copyTV(L, L->top++, o);
    setnumV(L->top++, 0);
    return 2;
  }
  lua_Number ip, fp = modf(math_checknum_named54(L, 1, "math.modf"), &ip);
  math_pushintegernum(L, ip);
  setnumV(L->top++, fp);
  return 2;
}

static int lj_cf_math_atan54(lua_State *L)
{
  lua_Number y = math_checknum_named54(L, 1, "math.atan");
  /* Lua 5.4 folds the old atan2 surface into math.atan(y [, x]); nil keeps
  ** the official default x=1, while any present non-nil value is checked.
  */
  lua_Number x = (L->base+1 < L->top && !tvisnil(L->base+1)) ?
		 math_checknum_named54(L, 2, "math.atan") : 1.0;
  setnumV(L->top++, atan2(y, x));
  return 1;
}

static int lj_cf_math_fmod54(lua_State *L)
{
  cTValue *a = L->base;
  cTValue *b = L->base + 1;
  if (a < L->top && b < L->top && tvisinteger(a) && tvisinteger(b)) {
    lua_Integer x = tvisint(a) ? (lua_Integer)intV(a) : (lua_Integer)i64V(a);
    lua_Integer y = tvisint(b) ? (lua_Integer)intV(b) : (lua_Integer)i64V(b);
    if (y == 0)
      lj_err_callermsg(L,
	lj_strfmt_pushf(L, "bad argument #2 to '%s' (zero)",
			math_argname54(L, "math.fmod")));
    if (x == LUA_MININTEGER && y == (lua_Integer)-1)
      lj_obj_setint64(L, L->top, 0);  /* Avoid mininteger / -1 overflow. */
    else
      lj_obj_setint64(L, L->top, (int64_t)(x % y));
    L->top++;
  } else {
    /* PUC Lua checks the divisor first here, so math.fmod() and
    ** math.fmod(nil) both report argument #2 as the missing no-value slot.
    */
    lua_Number y = math_checknum_named54(L, 2, "math.fmod");
    lua_Number x = math_checknum_named54(L, 1, "math.fmod");
    setnumV(L->top++, fmod(x, y));
  }
  return 1;
}

static int lj_cf_math_deg54(lua_State *L)
{
  setnumV(L->top++, math_checknum_named54(L, 1, "math.deg") *
		      57.29577951308232);
  return 1;
}

static int lj_cf_math_rad54(lua_State *L)
{
  setnumV(L->top++, math_checknum_named54(L, 1, "math.rad") *
		      0.017453292519943295);
  return 1;
}

#if LJ_54 && defined(LUA_COMPAT_MATHLIB)
static int lj_cf_math_atan2_compat54(lua_State *L)
{
  lua_Number y = math_checknum_named54(L, 1, "math.atan2");
  lua_Number x = (L->base+1 < L->top && !tvisnil(L->base+1)) ?
		 math_checknum_named54(L, 2, "math.atan2") : 1.0;
  setnumV(L->top++, atan2(y, x));
  return 1;
}

static int lj_cf_math_pow_compat54(lua_State *L)
{
  lua_Number x = math_checknum_named54(L, 1, "math.pow");
  lua_Number y = math_checknum_named54(L, 2, "math.pow");
  setnumV(L->top++, pow(x, y));
  return 1;
}

static int lj_cf_math_log10_compat54(lua_State *L)
{
  setnumV(L->top++, log10(math_checknum_named54(L, 1, "math.log10")));
  return 1;
}

static int lj_cf_math_sinh_compat54(lua_State *L)
{
  setnumV(L->top++, sinh(math_checknum_named54(L, 1, "math.sinh")));
  return 1;
}

static int lj_cf_math_cosh_compat54(lua_State *L)
{
  setnumV(L->top++, cosh(math_checknum_named54(L, 1, "math.cosh")));
  return 1;
}

static int lj_cf_math_tanh_compat54(lua_State *L)
{
  setnumV(L->top++, tanh(math_checknum_named54(L, 1, "math.tanh")));
  return 1;
}

static int lj_cf_math_frexp_compat54(lua_State *L)
{
  int e;
  setnumV(L->top++, frexp(math_checknum_named54(L, 1, "math.frexp"), &e));
  setintV(L->top++, e);
  return 2;
}

static int lj_cf_math_ldexp_compat54(lua_State *L)
{
  lua_Integer ep;
  lua_Number x = math_checknum_named54(L, 1, "math.ldexp");
  ep = math_checkinteger_named54(L, 2, "math.ldexp");
  setnumV(L->top++, ldexp(x, (int)ep));
  return 1;
}
#endif

LJLIB_CF(math_ult)		LJLIB_REC(lua54_ult)
{
  lua_Integer a, b;
  int isnum;
  if (!math_tointeger54(L, 1, &a, &isnum)) {
    if (isnum)
      lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #1 to '%s' "
	"(number has no integer representation)", math_argname54(L, "math.ult")));
    math_argtype_named54(L, 1, "math.ult", "number");
  }
  if (!math_tointeger54(L, 2, &b, &isnum)) {
    if (isnum)
      lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #2 to '%s' "
	"(number has no integer representation)", math_argname54(L, "math.ult")));
    math_argtype_named54(L, 2, "math.ult", "number");
  }
  setboolV(L->top++, (lua_Unsigned)a < (lua_Unsigned)b);
  return 1;
}
#endif

/* ------------------------------------------------------------------------ */

/* This implements a Tausworthe PRNG with period 2^223. Based on:
**   Tables of maximally-equidistributed combined LFSR generators,
**   Pierre L'Ecuyer, 1991, table 3, 1st entry.
** Full-period ME-CF generator with L=64, J=4, k=223, N1=49.
*/

/* Union needed for bit-pattern conversion between uint64_t and double. */
typedef union { uint64_t u64; double d; } U64double;

#if LJ_54
static uint64_t random_rotl64(uint64_t x, int n)
{
  return (x << n) | (x >> (64 - n));
}

static uint64_t random_next54(PRNGState *rs)
{
  uint64_t state0 = rs->u[0];
  uint64_t state1 = rs->u[1];
  uint64_t state2 = rs->u[2] ^ state0;
  uint64_t state3 = rs->u[3] ^ state1;
  uint64_t res = random_rotl64(state1 * 5u, 7) * 9u;
  rs->u[0] = state0 ^ state3;
  rs->u[1] = state1 ^ state2;
  rs->u[2] = state2 ^ (state1 << 17);
  rs->u[3] = random_rotl64(state3, 45);
  return res;
}

static lua_Number random_float54(uint64_t x)
{
  /* Lua 5.4 converts the high 53 random bits to a double in [0, 1). */
  return (lua_Number)(x >> 11) * (1.0 / 9007199254740992.0);
}

static lua_Unsigned random_project54(PRNGState *rs, lua_Unsigned ran,
				     lua_Unsigned n)
{
  if ((n & (n + (lua_Unsigned)1)) == 0)
    return ran & n;
  else {
    lua_Unsigned lim = n;
    lim |= (lim >> 1);
    lim |= (lim >> 2);
    lim |= (lim >> 4);
    lim |= (lim >> 8);
    lim |= (lim >> 16);
    lim |= (lim >> 32);
    while ((ran &= lim) > n)
      ran = (lua_Unsigned)random_next54(rs);
    return ran;
  }
}

static void random_setseed54(lua_State *L, PRNGState *rs,
			     lua_Unsigned n1, lua_Unsigned n2)
{
  int i;
  rs->u[0] = (uint64_t)n1;
  rs->u[1] = 0xffu;  /* Avoid a zero xoshiro256** state. */
  rs->u[2] = (uint64_t)n2;
  rs->u[3] = 0;
  for (i = 0; i < 16; i++)
    (void)random_next54(rs);
  lj_obj_setint64(L, L->top, (int64_t)(lua_Integer)n1);
  L->top++;
  lj_obj_setint64(L, L->top, (int64_t)(lua_Integer)n2);
  L->top++;
}
#endif

#if !LJ_54
/* PRNG seeding function. */
static void random_seed(PRNGState *rs, double d)
{
  uint32_t r = 0x11090601;  /* 64-k[i] as four 8 bit constants. */
  int i;
  for (i = 0; i < 4; i++) {
    U64double u;
    uint32_t m = 1u << (r&255);
    r >>= 8;
    u.d = d = d * 3.14159265358979323846 + 2.7182818284590452354;
    if (u.u64 < m) u.u64 += m;  /* Ensure k[i] MSB of u[i] are non-zero. */
    rs->u[i] = u.u64;
  }
  for (i = 0; i < 10; i++)
    (void)lj_prng_u64(rs);
}
#endif

/* PRNG extract function. */
LJLIB_PUSH(top-2)  /* Upvalue holds userdata with PRNGState. */
LJLIB_CF(math_random)		LJLIB_REC(.)
{
  int n = (int)(L->top - L->base);
  PRNGState *rs = (PRNGState *)(uddata(udataV(lj_lib_upvalue(L, 1))));
#if LJ_54
  uint64_t rv;
  lua_Integer low, up;
  lua_Unsigned p;
  if (n > 2)
    return luaL_error(L, "wrong number of arguments");
  rv = random_next54(rs);
  if (n == 0) {
    setnumV(L->top++, random_float54(rv));
    return 1;
  } else if (n == 1) {
    low = 1;
    up = math_checkinteger_named54(L, 1, "math.random");
    if (up == 0) {
      lj_obj_setint64(L, L->top, (int64_t)(lua_Integer)(lua_Unsigned)rv);
      L->top++;
      return 1;
    }
  } else {
    low = math_checkinteger_named54(L, 1, "math.random");
    up = math_checkinteger_named54(L, 2, "math.random");
  }
  if (low > up)
    lj_err_callermsg(L,
      lj_strfmt_pushf(L, "bad argument #1 to '%s' (interval is empty)",
		      math_argname54(L, "math.random")));
  p = random_project54(rs, (lua_Unsigned)rv,
		       (lua_Unsigned)up - (lua_Unsigned)low);
  lj_obj_setint64(L, L->top,
		  (int64_t)(lua_Integer)((lua_Unsigned)low + p));
  L->top++;
  return 1;
#else
  U64double u;
  double d;
  u.u64 = lj_prng_u64d(rs);
  d = u.d - 1.0;
  if (n > 0) {
#if LJ_DUALNUM
    int isint = 1;
    double r1;
    lj_lib_checknumber(L, 1);
    if (tvisint(L->base)) {
      r1 = (lua_Number)intV(L->base);
    } else {
      isint = 0;
      r1 = numV(L->base);
    }
#else
    double r1 = lj_lib_checknum(L, 1);
#endif
    if (n == 1) {
      d = lj_vm_floor(d*r1) + 1.0;  /* d is an int in range [1, r1] */
    } else {
#if LJ_DUALNUM
      double r2;
      lj_lib_checknumber(L, 2);
      if (tvisint(L->base+1)) {
	r2 = (lua_Number)intV(L->base+1);
      } else {
	isint = 0;
	r2 = numV(L->base+1);
      }
#else
      double r2 = lj_lib_checknum(L, 2);
#endif
      d = lj_vm_floor(d*(r2-r1+1.0)) + r1;  /* d is an int in range [r1, r2] */
    }
#if LJ_DUALNUM
    if (isint) {
      setintV(L->top-1, lj_num2int(d));
      return 1;
    }
#endif
  }  /* else: d is a double in range [0, 1] */
  setnumV(L->top++, d);
  return 1;
#endif
}

/* PRNG seed function. */
LJLIB_PUSH(top-2)  /* Upvalue holds userdata with PRNGState. */
LJLIB_CF(math_randomseed)
{
  PRNGState *rs = (PRNGState *)(uddata(udataV(lj_lib_upvalue(L, 1))));
#if LJ_54
  lua_Unsigned s1, s2;
  if (L->base != L->top) {
    s1 = (lua_Unsigned)math_checkinteger_named54(L, 1, "math.randomseed");
    s2 = (L->base+1 < L->top && !tvisnil(L->base+1)) ?
	 (lua_Unsigned)math_checkinteger_named54(L, 2, "math.randomseed") : 0;
  } else {
    s1 = (lua_Unsigned)time(NULL);
    s2 = (lua_Unsigned)(uintptr_t)L;
  }
  random_setseed54(L, rs, s1, s2);
  return 2;
#else
  if (L->base != L->top) {
    random_seed(rs, lj_lib_checknum(L, 1));
  } else if (!lj_prng_seed_secure(rs)) {
    lj_err_caller(L, LJ_ERR_PRNGSD);
  }
  return 0;
#endif
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_math(lua_State *L)
{
  PRNGState *rs = (PRNGState *)lua_newuserdata(L, sizeof(PRNGState));
  lj_prng_seed_fixed(rs);
  LJ_LIB_REG(L, LUA_MATHLIBNAME, math);
#if LJ_54
#if !defined(LUA_COMPAT_MATHLIB)
  /* These Lua 5.1/LuaJIT aliases are not part of the Lua 5.4 math library. */
  lua_pushnil(L); lua_setfield(L, -2, "atan2");
  lua_pushnil(L); lua_setfield(L, -2, "pow");
  lua_pushnil(L); lua_setfield(L, -2, "log10");
  lua_pushnil(L); lua_setfield(L, -2, "sinh");
  lua_pushnil(L); lua_setfield(L, -2, "cosh");
  lua_pushnil(L); lua_setfield(L, -2, "tanh");
  lua_pushnil(L); lua_setfield(L, -2, "frexp");
  lua_pushnil(L); lua_setfield(L, -2, "ldexp");
#else
  lj_lib_pushcf(L, lj_cf_math_atan2_compat54, FF_math_atan2);
  lua_setfield(L, -2, "atan2");
  lj_lib_pushcf(L, lj_cf_math_pow_compat54, FF_math_pow);
  lua_setfield(L, -2, "pow");
  lj_lib_pushcf(L, lj_cf_math_log10_compat54, FF_math_log10);
  lua_setfield(L, -2, "log10");
  lj_lib_pushcf(L, lj_cf_math_sinh_compat54, FF_math_sinh);
  lua_setfield(L, -2, "sinh");
  lj_lib_pushcf(L, lj_cf_math_cosh_compat54, FF_math_cosh);
  lua_setfield(L, -2, "cosh");
  lj_lib_pushcf(L, lj_cf_math_tanh_compat54, FF_math_tanh);
  lua_setfield(L, -2, "tanh");
  lj_lib_pushcf(L, lj_cf_math_frexp_compat54, FF_math_frexp);
  lua_setfield(L, -2, "frexp");
  lj_lib_pushcf(L, lj_cf_math_ldexp_compat54, FF_math_ldexp);
  lua_setfield(L, -2, "ldexp");
#endif
  lua_pushcfunction(L, lj_cf_math_type);
  lua_setfield(L, -2, "type");
  lua_pushcfunction(L, lj_cf_math_tointeger);
  lua_setfield(L, -2, "tointeger");
  lj_lib_pushcf(L, lj_cf_math_ult, FF_math_ult);
  lua_setfield(L, -2, "ult");
  lj_lib_pushcf(L, lj_cf_math_min54, FF_math_min);
  lua_setfield(L, -2, "min");
  lj_lib_pushcf(L, lj_cf_math_max54, FF_math_max);
  lua_setfield(L, -2, "max");
  lj_lib_pushcf(L, lj_cf_math_floor54, FF_math_floor);
  lua_setfield(L, -2, "floor");
  lj_lib_pushcf(L, lj_cf_math_ceil54, FF_math_ceil);
  lua_setfield(L, -2, "ceil");
  lj_lib_pushcf(L, lj_cf_math_modf54, FF_math_modf);
  lua_setfield(L, -2, "modf");
  lj_lib_pushcf(L, lj_cf_math_atan54, FF_math_atan);
  lua_setfield(L, -2, "atan");
  lj_lib_pushcf(L, lj_cf_math_fmod54, FF_math_fmod);
  lua_setfield(L, -2, "fmod");
  lua_pushcfunction(L, lj_cf_math_deg54);
  lua_setfield(L, -2, "deg");
  lua_pushcfunction(L, lj_cf_math_rad54);
  lua_setfield(L, -2, "rad");
  lua_pushinteger(L, LJ_MATH_MAXINTEGER);
  lua_setfield(L, -2, "maxinteger");
  lua_pushinteger(L, LJ_MATH_MININTEGER);
  lua_setfield(L, -2, "mininteger");
#endif
  return 1;
}
