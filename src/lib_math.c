/*
** Math library.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#include <math.h>

#define lib_math_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_lib.h"
#include "lj_str.h"
#include "lj_strscan.h"
#include "lj_vm.h"
#include "lj_prng.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_math

#define LJ_MATH_MAXINTEGER	((lua_Integer)2147483647)
#define LJ_MATH_MININTEGER	((lua_Integer)(-LJ_MATH_MAXINTEGER - 1))

#if LJ_54
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
  if (!tvisnumber(o))
    return 0;
  if (isnum)
    *isnum = 1;
  if (tvisint(o)) {
    *ip = intV(o);
    return 1;
  }
  n = numV(o);
  if (!(n >= (double)LJ_MATH_MININTEGER &&
	n <= (double)LJ_MATH_MAXINTEGER))
    return 0;
  ni = lj_vm_floor(n);
  if (n != ni)
    return 0;
  *ip = (int32_t)n;
  return 1;
}

static int32_t math_checkrandomint(lua_State *L, int narg)
{
  int32_t i;
  int isnum;
  if (!math_toint32(L, narg, &i, &isnum)) {
    if (isnum)
      luaL_argerror(L, narg, "number has no integer representation");
    lj_err_argt(L, narg, LUA_TNUMBER);
  }
  return i;
}

static void math_pushintegernum(lua_State *L, lua_Number n)
{
  double ni;
  if (n >= (double)LJ_MATH_MININTEGER &&
      n <= (double)LJ_MATH_MAXINTEGER) {
    ni = lj_vm_floor(n);
    if (n == ni) {
      /* The compatibility layer currently has 32 bit integer storage; return
      ** an integer whenever the rounded result fits that representation.
      */
      setintV(L->top++, (int32_t)n);
      return;
    }
  }
  setnumV(L->top++, n);
}
#endif

LJLIB_ASM(math_abs)		LJLIB_REC(.)
{
  lj_lib_checknumber(L, 1);
  return FFH_RETRY;
}
LJLIB_ASM_(math_floor)		LJLIB_REC(math_round IRFPM_FLOOR)
LJLIB_ASM_(math_ceil)		LJLIB_REC(math_round IRFPM_CEIL)

LJLIB_ASM(math_sqrt)		LJLIB_REC(math_unary IRFPM_SQRT)
{
  lj_lib_checknum(L, 1);
  return FFH_RETRY;
}
LJLIB_ASM_(math_log10)		LJLIB_REC(math_call IRCALL_log10)
LJLIB_ASM_(math_exp)		LJLIB_REC(math_call IRCALL_exp)
LJLIB_ASM_(math_sin)		LJLIB_REC(math_call IRCALL_sin)
LJLIB_ASM_(math_cos)		LJLIB_REC(math_call IRCALL_cos)
LJLIB_ASM_(math_tan)		LJLIB_REC(math_call IRCALL_tan)
LJLIB_ASM_(math_asin)		LJLIB_REC(math_call IRCALL_asin)
LJLIB_ASM_(math_acos)		LJLIB_REC(math_call IRCALL_acos)
LJLIB_ASM_(math_atan)		LJLIB_REC(math_call IRCALL_atan)
LJLIB_ASM_(math_sinh)		LJLIB_REC(math_call IRCALL_sinh)
LJLIB_ASM_(math_cosh)		LJLIB_REC(math_call IRCALL_cosh)
LJLIB_ASM_(math_tanh)		LJLIB_REC(math_call IRCALL_tanh)
LJLIB_ASM_(math_frexp)
LJLIB_ASM_(math_modf)

LJLIB_ASM(math_log)		LJLIB_REC(math_log)
{
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
LJLIB_ASM_(math_fmod)

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
static int lj_cf_math_type(lua_State *L)
{
  cTValue *o = L->base;
  if (o >= L->top || !tvisnumber(o))
    setnilV(L->top++);
  else if (tvisint(o))
    setstrV(L, L->top++, lj_str_newlit(L, "integer"));
  else {
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
  }
  return 1;
}

static int lj_cf_math_tointeger(lua_State *L)
{
  int32_t i;
  int isnum;
  lj_lib_checkany(L, 1);  /* Lua 5.4 errors only when the value is absent. */
  if (math_toint32(L, 1, &i, &isnum))
    setintV(L->top++, i);
  else
    setnilV(L->top++);
  return 1;
}

static int lj_cf_math_floor54(lua_State *L)
{
  lua_Number n = lj_lib_checknum(L, 1);
  math_pushintegernum(L, lj_vm_floor(n));
  return 1;
}

static int lj_cf_math_ceil54(lua_State *L)
{
  lua_Number n = lj_lib_checknum(L, 1);
  math_pushintegernum(L, -lj_vm_floor(-n));
  return 1;
}

static int lj_cf_math_modf54(lua_State *L)
{
  lua_Number ip, fp = modf(lj_lib_checknum(L, 1), &ip);
  math_pushintegernum(L, ip);
  setnumV(L->top++, fp);
  return 2;
}

static int lj_cf_math_ult(lua_State *L)
{
  int32_t a, b;
  int isnum;
  if (!math_toint32(L, 1, &a, &isnum)) {
    if (isnum)
      luaL_argerror(L, 1, "number has no integer representation");
    lj_err_argt(L, 1, LUA_TNUMBER);
  }
  if (!math_toint32(L, 2, &b, &isnum)) {
    if (isnum)
      luaL_argerror(L, 2, "number has no integer representation");
    lj_err_argt(L, 2, LUA_TNUMBER);
  }
  /* The compatibility mode currently uses LuaJIT's internal 32 bit integers. */
  setboolV(L->top++, (uint32_t)a < (uint32_t)b);
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
static void random_pushint(lua_State *L, PRNGState *rs, int32_t lo, int32_t hi)
{
  uint64_t span = (uint64_t)((int64_t)hi - (int64_t)lo) + 1u;
  int32_t r = (int32_t)((int64_t)lo + (int64_t)(lj_prng_u64(rs) % span));
  setintV(L->top++, r);
}
#endif

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

/* PRNG extract function. */
LJLIB_PUSH(top-2)  /* Upvalue holds userdata with PRNGState. */
LJLIB_CF(math_random)		LJLIB_REC(.)
{
  int n = (int)(L->top - L->base);
  PRNGState *rs = (PRNGState *)(uddata(udataV(lj_lib_upvalue(L, 1))));
  U64double u;
  double d;
#if LJ_54
  if (n > 2)
    return luaL_error(L, "wrong number of arguments");
  if (n > 0) {
    int32_t r1 = math_checkrandomint(L, 1);
    /* Lua 5.4 returns integers for bounded random calls; keep that path
    ** separate from LuaJIT's historical floating point range scaling.
    */
    if (n == 1) {
      if (r1 == 0) {
	random_pushint(L, rs, (int32_t)LJ_MATH_MININTEGER,
		       (int32_t)LJ_MATH_MAXINTEGER);
      } else {
	if (r1 < 1)
	  luaL_argerror(L, 1, "interval is empty");
	random_pushint(L, rs, 1, r1);
      }
    } else {
      int32_t r2 = math_checkrandomint(L, 2);
      if (r1 > r2)
	luaL_argerror(L, 1, "interval is empty");
      random_pushint(L, rs, r1, r2);
    }
    return 1;
  }
#endif
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
}

/* PRNG seed function. */
LJLIB_PUSH(top-2)  /* Upvalue holds userdata with PRNGState. */
LJLIB_CF(math_randomseed)
{
  PRNGState *rs = (PRNGState *)(uddata(udataV(lj_lib_upvalue(L, 1))));
  if (L->base != L->top) {
#if LJ_54
    lua_Number s1 = lj_lib_checknum(L, 1);
    lua_Number s2 = L->base+1 < L->top ? lj_lib_checknum(L, 2) : 0;
    /* LuaJIT keeps one PRNG seed value; fold Lua 5.4's two visible seeds
    ** into that internal state, but still return the accepted seed pair.
    */
    random_seed(rs, s1 + s2 * 3.14159265358979323846);
    copyTV(L, L->top++, L->base);
    if (L->base+1 < L->top-1)
      copyTV(L, L->top++, L->base+1);
    else
      setintV(L->top++, 0);
    return 2;
#else
    random_seed(rs, lj_lib_checknum(L, 1));
#endif
  } else if (!lj_prng_seed_secure(rs)) {
    lj_err_caller(L, LJ_ERR_PRNGSD);
  }
#if LJ_54
  /* Lua 5.4 returns the actual seed pair for the implicit seeding path.
  ** LuaJIT has a single PRNG state, so expose two generated 32 bit seeds
  ** from the freshly seeded state instead of the old placeholder 0, 0.
  */
  setintV(L->top++, (int32_t)lj_prng_u64(rs));
  setintV(L->top++, (int32_t)lj_prng_u64(rs));
  return 2;
#endif
  return 0;
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_math(lua_State *L)
{
  PRNGState *rs = (PRNGState *)lua_newuserdata(L, sizeof(PRNGState));
  lj_prng_seed_fixed(rs);
  LJ_LIB_REG(L, LUA_MATHLIBNAME, math);
#if LJ_54
  /* These Lua 5.1/LuaJIT aliases are not part of the Lua 5.4 math library. */
  lua_pushnil(L); lua_setfield(L, -2, "atan2");
  lua_pushnil(L); lua_setfield(L, -2, "pow");
  lua_pushnil(L); lua_setfield(L, -2, "log10");
  lua_pushnil(L); lua_setfield(L, -2, "sinh");
  lua_pushnil(L); lua_setfield(L, -2, "cosh");
  lua_pushnil(L); lua_setfield(L, -2, "tanh");
  lua_pushnil(L); lua_setfield(L, -2, "frexp");
  lua_pushnil(L); lua_setfield(L, -2, "ldexp");
  lua_pushcfunction(L, lj_cf_math_type);
  lua_setfield(L, -2, "type");
  lua_pushcfunction(L, lj_cf_math_tointeger);
  lua_setfield(L, -2, "tointeger");
  lua_pushcfunction(L, lj_cf_math_ult);
  lua_setfield(L, -2, "ult");
  lua_pushcfunction(L, lj_cf_math_floor54);
  lua_setfield(L, -2, "floor");
  lua_pushcfunction(L, lj_cf_math_ceil54);
  lua_setfield(L, -2, "ceil");
  lua_pushcfunction(L, lj_cf_math_modf54);
  lua_setfield(L, -2, "modf");
  lua_pushinteger(L, LJ_MATH_MAXINTEGER);
  lua_setfield(L, -2, "maxinteger");
  lua_pushinteger(L, LJ_MATH_MININTEGER);
  lua_setfield(L, -2, "mininteger");
#endif
  return 1;
}
