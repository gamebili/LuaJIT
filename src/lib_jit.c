/*
** JIT library.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lib_jit_c
#define LUA_LIB

#include <limits.h>
#include <math.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_meta.h"
#include "lj_str.h"
#include "lj_strfmt.h"
#include "lj_strscan.h"
#include "lj_tab.h"
#include "lj_state.h"
#include "lj_close.h"
#include "lj_bc.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#if LJ_HASJIT
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_ircall.h"
#include "lj_iropt.h"
#include "lj_target.h"
#endif
#include "lj_trace.h"
#include "lj_dispatch.h"
#include "lj_vm.h"
#include "lj_vmevent.h"
#include "lj_lib.h"

#include "luajit.h"

/* -- jit.* functions ----------------------------------------------------- */

#define LJLIB_MODULE_jit

static int setjitmode(lua_State *L, int mode)
{
  int idx = 0;
  if (L->base == L->top || tvisnil(L->base)) {  /* jit.on/off/flush([nil]) */
    mode |= LUAJIT_MODE_ENGINE;
  } else {
    /* jit.on/off/flush(func|proto, nil|true|false) */
    if (tvisfunc(L->base) || tvisproto(L->base))
      idx = 1;
    else if (!tvistrue(L->base))  /* jit.on/off/flush(true, nil|true|false) */
      goto err;
    if (L->base+1 < L->top && tvisbool(L->base+1))
      mode |= boolV(L->base+1) ? LUAJIT_MODE_ALLFUNC : LUAJIT_MODE_ALLSUBFUNC;
    else
      mode |= LUAJIT_MODE_FUNC;
  }
  if (luaJIT_setmode(L, idx, mode) != 1) {
    if ((mode & LUAJIT_MODE_MASK) == LUAJIT_MODE_ENGINE)
      lj_err_caller(L, LJ_ERR_NOJIT);
  err:
    lj_err_argt(L, 1, LUA_TFUNCTION);
  }
  return 0;
}

LJLIB_CF(jit_on)
{
  return setjitmode(L, LUAJIT_MODE_ON);
}

LJLIB_CF(jit_off)
{
  return setjitmode(L, LUAJIT_MODE_OFF);
}

LJLIB_CF(jit_flush)
{
#if LJ_HASJIT
  if (L->base < L->top && tvisnumber(L->base)) {
    int traceno = lj_lib_checkint(L, 1);
    luaJIT_setmode(L, traceno, LUAJIT_MODE_FLUSH|LUAJIT_MODE_TRACE);
    return 0;
  }
#endif
  return setjitmode(L, LUAJIT_MODE_FLUSH);
}

#if LJ_HASJIT
/* Push a string for every flag bit that is set. */
static void flagbits_to_strings(lua_State *L, uint32_t flags, uint32_t base,
				const char *str)
{
  for (; *str; base <<= 1, str += 1+*str)
    if (flags & base)
      setstrV(L, L->top++, lj_str_new(L, str+1, *(uint8_t *)str));
}
#endif

LJLIB_CF(jit_status)
{
#if LJ_HASJIT
  jit_State *J = L2J(L);
  L->top = L->base;
  setboolV(L->top++, (J->flags & JIT_F_ON) ? 1 : 0);
  flagbits_to_strings(L, J->flags, JIT_F_CPU, JIT_F_CPUSTRING);
  flagbits_to_strings(L, J->flags, JIT_F_OPT, JIT_F_OPTSTRING);
  return (int)(L->top - L->base);
#else
  setboolV(L->top++, 0);
  return 1;
#endif
}

LJLIB_CF(jit_security)
{
  int idx = lj_lib_checkopt(L, 1, -1, LJ_SECURITY_MODESTRING);
  setintV(L->top++, ((LJ_SECURITY_MODE >> (2*idx)) & 3));
  return 1;
}

LJLIB_CF(jit_attach)
{
#ifdef LUAJIT_DISABLE_VMEVENT
  luaL_error(L, "vmevent API disabled");
#else
  GCfunc *fn = lj_lib_checkfunc(L, 1);
  GCstr *s = lj_lib_optstr(L, 2);
  luaL_findtable(L, LUA_REGISTRYINDEX, LJ_VMEVENTS_REGKEY, LJ_VMEVENTS_HSIZE);
  if (s) {  /* Attach to given event. */
    const uint8_t *p = (const uint8_t *)strdata(s);
    uint32_t h = s->len;
    while (*p) h = h ^ (lj_rol(h, 6) + *p++);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, VMEVENT_HASHIDX(h));
    G(L)->vmevmask = VMEVENT_NOCACHE;  /* Invalidate cache. */
  } else {  /* Detach if no event given. */
    setnilV(L->top++);
    while (lua_next(L, -2)) {
      L->top--;
      if (tvisfunc(L->top) && funcV(L->top) == fn) {
	setnilV(lj_tab_set(L, tabV(L->top-2), L->top-1));
      }
    }
  }
#endif
  return 0;
}

#if LJ_54
#define LJ_LUA54_I32_MAX	((int64_t)2147483647)
#define LJ_LUA54_I32_MIN	((int64_t)(-LJ_LUA54_I32_MAX - 1))
#define LJ_LUA54_I64_LIMIT	9223372036854775808.0

static int lua54_toint64(lua_State *L, int narg, int64_t *ip, int *isnum)
{
  cTValue *o = L->base + narg-1;
  double n, ni;
  if (isnum) *isnum = 0;
  if (o >= L->top)
    return 0;
  if (!tvisnumber(o))
    return 0;
  if (isnum) *isnum = 1;
  if (tvisint(o)) {
    *ip = (int64_t)intV(o);
    return 1;
  }
  n = numV(o);
  if (!(n >= -LJ_LUA54_I64_LIMIT && n < LJ_LUA54_I64_LIMIT))
    return 0;
  ni = lj_vm_floor(n);
  if (n != ni)
    return 0;
  *ip = lj_num2i64(n);
  return 1;
}

static int64_t lua54_checkintop64(lua_State *L, int narg)
{
  int64_t i;
  int isnum;
  if (!lua54_toint64(L, narg, &i, &isnum)) {
    if (isnum)
      lj_err_caller(L, LJ_ERR_NUMINT);
    else {
      cTValue *o = L->base + narg-1;
      MSize tlen;
      const char *tname;
      if (o >= L->top)
	lj_err_argt(L, narg, LUA_TNUMBER);
      tname = lj_meta_objtypename(L, o, &tlen);
      UNUSED(tlen);
      lj_err_callermsg(L, lj_strfmt_pushf(L,
	"attempt to perform bitwise operation on a %s value", tname));
    }
  }
  return i;
}

static void lua54_binop_error(lua_State *L, const char *opname)
{
  cTValue *a = L->base < L->top ? L->base : niltv(L);
  cTValue *b = L->base+1 < L->top ? L->base+1 : niltv(L);
  MSize alen, blen;
  const char *at = lj_meta_objtypename(L, a, &alen);
  const char *bt = lj_meta_objtypename(L, b, &blen);
  UNUSED(alen); UNUSED(blen);
  /* Lua 5.4 reports lowered operator helpers as source-level arithmetic
  ** failures, keeping both operand types and __name-derived type names.
  */
  lj_err_callermsg(L, lj_strfmt_pushf(L,
    "attempt to %s a '%s' with a '%s'", opname, at, bt));
}

static int lua54_tonumop(lua_State *L, int narg, int *isint, int32_t *ip,
			 double *np)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  if (o >= L->top)
    return 0;
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      return 0;
    o = &tmp;
  } else if (!tvisnumber(o)) {
    return 0;
  }
  if (tvisint(o)) {
    *isint = 1;
    *ip = intV(o);
    *np = (double)*ip;
  } else {
    *isint = 0;
    *np = numV(o);
  }
  return 1;
}

static cTValue *lua54_getmetafield(lua_State *L, cTValue *o, GCstr *mm)
{
  GCtab *mt;
  if (tvistab(o))
    mt = tabref(tabV(o)->metatable);
  else if (tvisudata(o))
    mt = tabref(udataV(o)->metatable);
  else
    mt = tabref(basemt_obj(G(L), o));
  if (mt) {
    cTValue *mo = lj_tab_getstr(mt, mm);
    if (mo && !tvisnil(mo))
      return mo;
  }
  return NULL;
}

static int lua54_callbinmeta(lua_State *L, const char *mmname, int unary)
{
  GCstr *mm = lj_str_newz(L, mmname);
  cTValue *mo = lua54_getmetafield(L, L->base, mm);
  if (!mo && !unary && L->base+1 < L->top)
    mo = lua54_getmetafield(L, L->base+1, mm);
  if (!mo)
    return 0;
  /* The operators are currently lowered to helper calls; explicitly calling
  ** the Lua 5.4 metamethod here preserves the language surface.
  */
  copyTV(L, L->top++, mo);
  copyTV(L, L->top++, L->base);
  copyTV(L, L->top++, unary ? L->base : L->base+1);
  lua_call(L, 2, 1);
  /* The lowered helper is itself a C function.  Normalize the metamethod's
  ** single result to the helper result slot so original operands do not leak
  ** as extra returns when string/table metamethods return multiple values.
  */
  copyTV(L, L->base, L->top-1);
  L->top = L->base + 1;
  return 1;
}

static int lua54_pushbinint(lua_State *L, int32_t v)
{
  TValue *base = L->base;
  setintV(base, v);
  /* The lowered helpers are compiled as ordinary calls but may be used as the
  ** last expression in a multi-assignment. LuaJIT does not nil-fill stale
  ** temporary argument slots for C helpers, so clear the two operand slots
  ** after writing the single official Lua 5.4 result.
  */
  setnilV(base + 1);
  setnilV(base + 2);
  L->top = base + 1;
  return 1;
}

static int lua54_pushbinint64(lua_State *L, int64_t v)
{
  TValue *base = L->base;
  /* The current compat layer still has 32-bit integer TValue storage. Keep
  ** exact wider bitwise results as numbers until the full 64-bit TValue batch
  ** replaces this bridge.
  */
  if (v >= LJ_LUA54_I32_MIN && v <= LJ_LUA54_I32_MAX)
    setintV(base, (int32_t)v);
  else
    setnumV(base, (lua_Number)v);
  setnilV(base + 1);
  setnilV(base + 2);
  L->top = base + 1;
  return 1;
}

static int lua54_pushbinnum(lua_State *L, lua_Number n)
{
  TValue *base = L->base;
  setnumV(base, n);
  setnilV(base + 1);
  setnilV(base + 2);
  L->top = base + 1;
  return 1;
}

static int lj_cf_jit__lua54_idiv(lua_State *L)
{
  int ia, ib;
  int32_t a = 0, b = 0;
  double na, nb;
  if ((tvisstr(L->base) || tvisstr(L->base+1)) &&
      lua54_callbinmeta(L, "__idiv", 0))
    return 1;
  if (!lua54_tonumop(L, 1, &ia, &a, &na) ||
      !lua54_tonumop(L, 2, &ib, &b, &nb)) {
    if (lua54_callbinmeta(L, "__idiv", 0))
      return 1;
    lua54_binop_error(L, "idiv");
  }
  if (ia && ib) {
    int64_t ai = a, bi = b, q, r;
    if (bi == 0)
      return luaL_error(L, "attempt to divide by zero");
    q = ai / bi;
    r = ai % bi;
    if (r != 0 && ((r < 0) != (bi < 0)))
      q--;
    if (q >= LJ_LUA54_I32_MIN && q <= LJ_LUA54_I32_MAX)
      return lua54_pushbinint(L, (int32_t)q);
    return lua54_pushbinnum(L, (lua_Number)q);
  }
  return lua54_pushbinnum(L, lj_vm_floor(na / nb));
}

static int lj_cf_jit__lua54_mod(lua_State *L)
{
  int ia, ib;
  int32_t a = 0, b = 0;
  double na, nb;
  if ((tvisstr(L->base) || tvisstr(L->base+1)) &&
      lua54_callbinmeta(L, "__mod", 0))
    return 1;
  if (!lua54_tonumop(L, 1, &ia, &a, &na) ||
      !lua54_tonumop(L, 2, &ib, &b, &nb)) {
    if (lua54_callbinmeta(L, "__mod", 0))
      return 1;
    lua54_binop_error(L, "mod");
  }
  if (ia && ib) {
    if (b == 0)
      return luaL_error(L, "attempt to perform 'n%%0'");
    return lua54_pushbinint(L, lj_vm_modi(a, b));
  }
  return lua54_pushbinnum(L, lj_vm_foldarith(na, nb, MM_mod-MM_add));
}

static int lj_cf_jit__lua54_band(lua_State *L)
{
  int ia, ib;
  int64_t a, b;
  if (!lua54_toint64(L, 1, &a, &ia) || !lua54_toint64(L, 2, &b, &ib)) {
    if (lua54_callbinmeta(L, "__band", 0))
      return 1;
    a = lua54_checkintop64(L, 1);
    b = lua54_checkintop64(L, 2);
  }
  return lua54_pushbinint64(L, (int64_t)((uint64_t)a & (uint64_t)b));
}

static int lj_cf_jit__lua54_bor(lua_State *L)
{
  int ia, ib;
  int64_t a, b;
  if (!lua54_toint64(L, 1, &a, &ia) || !lua54_toint64(L, 2, &b, &ib)) {
    if (lua54_callbinmeta(L, "__bor", 0))
      return 1;
    a = lua54_checkintop64(L, 1);
    b = lua54_checkintop64(L, 2);
  }
  return lua54_pushbinint64(L, (int64_t)((uint64_t)a | (uint64_t)b));
}

static int lj_cf_jit__lua54_bxor(lua_State *L)
{
  int ia, ib;
  int64_t a, b;
  if (!lua54_toint64(L, 1, &a, &ia) || !lua54_toint64(L, 2, &b, &ib)) {
    if (lua54_callbinmeta(L, "__bxor", 0))
      return 1;
    a = lua54_checkintop64(L, 1);
    b = lua54_checkintop64(L, 2);
  }
  return lua54_pushbinint64(L, (int64_t)((uint64_t)a ^ (uint64_t)b));
}

static int lj_cf_jit__lua54_bnot(lua_State *L)
{
  int isnum;
  int64_t a;
  if (!lua54_toint64(L, 1, &a, &isnum)) {
    if (lua54_callbinmeta(L, "__bnot", 1))
      return 1;
    a = lua54_checkintop64(L, 1);
  }
  return lua54_pushbinint64(L, (int64_t)~(uint64_t)a);
}

static int64_t lua54_shift64(int64_t a, int64_t sh, int left)
{
  int64_t s = sh;
  if (s < 0) {
    s = -s;
    left = !left;
  }
  if (s >= 64)
    return 0;
  return left ? (int64_t)((uint64_t)a << s) :
		(int64_t)((uint64_t)a >> s);
}

static int lj_cf_jit__lua54_shl(lua_State *L)
{
  int ia, ib;
  int64_t a, sh;
  if (!lua54_toint64(L, 1, &a, &ia) || !lua54_toint64(L, 2, &sh, &ib)) {
    if (lua54_callbinmeta(L, "__shl", 0))
      return 1;
    a = lua54_checkintop64(L, 1);
    sh = lua54_checkintop64(L, 2);
  }
  return lua54_pushbinint64(L, lua54_shift64(a, sh, 1));
}

static int lj_cf_jit__lua54_shr(lua_State *L)
{
  int ia, ib;
  int64_t a, sh;
  if (!lua54_toint64(L, 1, &a, &ia) || !lua54_toint64(L, 2, &sh, &ib)) {
    if (lua54_callbinmeta(L, "__shr", 0))
      return 1;
    a = lua54_checkintop64(L, 1);
    sh = lua54_checkintop64(L, 2);
  }
  return lua54_pushbinint64(L, lua54_shift64(a, sh, 0));
}

static int lj_cf_jit__lua54_forstep(lua_State *L)
{
  TValue tmp;
  cTValue *o = L->base;
  if (o >= L->top)
    lj_err_argt(L, 1, LUA_TNUMBER);
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      lj_err_argt(L, 1, LUA_TNUMBER);
    o = &tmp;
  } else if (!tvisnumber(o)) {
    lj_err_argt(L, 1, LUA_TNUMBER);
  }
  if (tvisint(o) ? intV(o) == 0 : tviszero(o))
    return luaL_error(L, "'for' step is zero");
  /* Returning the normalized value keeps string-number steps from reaching
  ** FORI as strings, while the VM still handles the real loop mechanics.
  */
  copyTV(L, L->top++, o);
  return 1;
}

static int lj_cf_jit__lua54_checkclose(lua_State *L)
{
  cTValue *o = L->base;
  const char *name = luaL_checkstring(L, 2);
  int32_t slotdelta = lj_lib_checkint(L, 3);
  TValue *slot = L->base + slotdelta;
  if (o >= L->top || lj_close_isfalse(o))
    return 0;
  /* Keep the variable-specific Lua 5.4 declaration error here, but delegate
  ** the actual closable test to the shared runtime helper used by C API and
  ** the later VM unwind implementation.
  */
  if (!lj_close_getmethod(L, o))
    return luaL_error(L, "variable '%s' got a non-closable value", name);
  /* The parser passes a temporary copy as the helper argument. Track the real
  ** local register by a stack-relative delta from that argument slot, so the
  ** mark survives after the temporary call registers are recycled.
  */
  lj_close_mark(L, slot);
  return 0;
}

static int lj_cf_jit__lua54_nopclose(lua_State *L)
{
  (void)L;
  return 0;
}

static int lj_cf_jit__lua54_closeerror(lua_State *L)
{
  return luaL_error(L, "%s", luaL_checkstring(L, 1));
}

static int lua54_pushclosecall(lua_State *L, cTValue *fn, cTValue *self)
{
  TValue *top = L->top;
  copyTV(L, top++, fn);
  copyTV(L, top++, self);
  setnilV(top++);
  L->top = top;
  return 3;
}

static int lua54_pushnopclose(lua_State *L)
{
  lj_state_checkstack(L, 3);
  lua_pushcfunction(L, lj_cf_jit__lua54_nopclose);
  setnilV(L->top++);
  setnilV(L->top++);
  return 3;
}

static int lua54_pushcloseerror(lua_State *L)
{
  TValue *top;
  lj_state_checkstack(L, 3);
  lua_pushcfunction(L, lj_cf_jit__lua54_closeerror);
  top = L->top;
  setstrV(L, top++, lj_str_newlit(L,
    "attempt to call a nil value (metamethod 'close')"));
  setnilV(top++);
  L->top = top;
  return 3;
}

static int lj_cf_jit__lua54_closevalue(lua_State *L)
{
  int32_t slotdelta = lj_lib_checkint(L, 2);
  TValue *o = L->base + slotdelta;
  ptrdiff_t slotofs;
  cTValue *mo;
  if (o >= L->top)
    return lua54_pushnopclose(L);
  if (lj_close_isfalse(o))
    return lua54_pushnopclose(L);
  lj_close_unmark(L, o);
  if (!lj_close_getmethod(L, o))
    return lua54_pushcloseerror(L);
  slotofs = savestack(L, o);
  lj_state_checkstack(L, 3);
  o = restorestack(L, slotofs);
  mo = lj_close_getmethod(L, o);
  if (!mo)
    return lua54_pushcloseerror(L);
  return lua54_pushclosecall(L, mo, o);
}

static int lj_cf_jit__lua54_packreturn(lua_State *L)
{
  int n = lua_gettop(L);
  int i;
  lua_createtable(L, n, 1);
  for (i = 1; i <= n; i++) {
    lua_pushvalue(L, i);
    lua_rawseti(L, -2, i);
  }
  lua_pushinteger(L, n);
  lua_setfield(L, -2, "n");
  return 1;
}

static int lj_cf_jit__lua54_unpackreturn(lua_State *L)
{
  lua_Integer n64;
  int n, i;
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_getfield(L, 1, "n");
  n64 = luaL_checkinteger(L, -1);
  lua_pop(L, 1);
  if (n64 < 0 || n64 > INT_MAX)
    return luaL_error(L, "too many return values");
  n = (int)n64;
  /* Dynamic return values are packed while close handlers run. Unpacking
  ** through this private helper preserves nil holes and gives the later VM
  ** return-close path a single bridge point to replace.
  */
  luaL_checkstack(L, n, "too many return values");
  for (i = 1; i <= n; i++)
    lua_rawgeti(L, 1, i);
  return n;
}
#endif

/* Metadata is copied from values pushed by luaopen_jit() before LJ_LIB_REG.
** buildvm only accepts literal top-N offsets here. Keep lua54compat first so
** the original jit.os/arch/version_num/version offsets stay unchanged.
*/
LJLIB_PUSH(top-6) LJLIB_SET(lua54compat)
LJLIB_PUSH(top-5) LJLIB_SET(os)
LJLIB_PUSH(top-4) LJLIB_SET(arch)
LJLIB_PUSH(top-3) LJLIB_SET(version_num)
LJLIB_PUSH(top-2) LJLIB_SET(version)

#include "lj_libdef.h"

/* -- jit.util.* functions ------------------------------------------------ */

#define LJLIB_MODULE_jit_util

/* -- Reflection API for Lua functions ------------------------------------ */

static void setintfield(lua_State *L, GCtab *t, const char *name, int32_t val)
{
  setintV(lj_tab_setstr(L, t, lj_str_newz(L, name)), val);
}

/* local info = jit.util.funcinfo(func [,pc]) */
LJLIB_CF(jit_util_funcinfo)
{
  GCproto *pt = lj_lib_checkLproto(L, 1, 1);
  if (pt) {
    BCPos pc = (BCPos)lj_lib_optint(L, 2, 0);
    GCtab *t;
    lua_createtable(L, 0, 16);  /* Increment hash size if fields are added. */
    t = tabV(L->top-1);
    setintfield(L, t, "linedefined", pt->firstline);
    setintfield(L, t, "lastlinedefined", pt->firstline + pt->numline);
    setintfield(L, t, "stackslots", pt->framesize);
    setintfield(L, t, "params", pt->numparams);
    setintfield(L, t, "bytecodes", (int32_t)pt->sizebc);
    setintfield(L, t, "gcconsts", (int32_t)pt->sizekgc);
    setintfield(L, t, "nconsts", (int32_t)pt->sizekn);
    setintfield(L, t, "upvalues", (int32_t)pt->sizeuv);
    if (pc < pt->sizebc)
      setintfield(L, t, "currentline", lj_debug_line(pt, pc));
    lua_pushboolean(L, (pt->flags & PROTO_VARARG));
    lua_setfield(L, -2, "isvararg");
    lua_pushboolean(L, (pt->flags & PROTO_CHILD));
    lua_setfield(L, -2, "children");
    setstrV(L, L->top++, proto_chunkname(pt));
    lua_setfield(L, -2, "source");
    lj_debug_pushloc(L, pt, pc);
    lua_setfield(L, -2, "loc");
    setprotoV(L, lj_tab_setstr(L, t, lj_str_newlit(L, "proto")), pt);
  } else {
    GCfunc *fn = funcV(L->base);
    GCtab *t;
    lua_createtable(L, 0, 4);  /* Increment hash size if fields are added. */
    t = tabV(L->top-1);
    if (!iscfunc(fn))
      setintfield(L, t, "ffid", fn->c.ffid);
    setintptrV(lj_tab_setstr(L, t, lj_str_newlit(L, "addr")),
	       (intptr_t)(void *)fn->c.f);
    setintfield(L, t, "upvalues", fn->c.nupvalues);
  }
  return 1;
}

/* local ins, m = jit.util.funcbc(func, pc) */
LJLIB_CF(jit_util_funcbc)
{
  GCproto *pt = lj_lib_checkLproto(L, 1, 0);
  BCPos pc = (BCPos)lj_lib_checkint(L, 2);
  if (pc < pt->sizebc) {
    BCIns ins = proto_bc(pt)[pc];
    BCOp op = bc_op(ins);
    lj_assertL(op < BC__MAX, "bad bytecode op %d", op);
    setintV(L->top, ins);
    setintV(L->top+1, lj_bc_mode[op]);
    L->top += 2;
    return 2;
  }
  return 0;
}

/* local k = jit.util.funck(func, idx) */
LJLIB_CF(jit_util_funck)
{
  GCproto *pt = lj_lib_checkLproto(L, 1, 0);
  ptrdiff_t idx = (ptrdiff_t)lj_lib_checkint(L, 2);
  if (idx >= 0) {
    if (idx < (ptrdiff_t)pt->sizekn) {
      copyTV(L, L->top-1, proto_knumtv(pt, idx));
      return 1;
    }
  } else {
    if (~idx < (ptrdiff_t)pt->sizekgc) {
      GCobj *gc = proto_kgc(pt, idx);
      setgcV(L, L->top-1, gc, ~gc->gch.gct);
      return 1;
    }
  }
  return 0;
}

/* local name = jit.util.funcuvname(func, idx) */
LJLIB_CF(jit_util_funcuvname)
{
  GCproto *pt = lj_lib_checkLproto(L, 1, 0);
  uint32_t idx = (uint32_t)lj_lib_checkint(L, 2);
  if (idx < pt->sizeuv) {
    setstrV(L, L->top-1, lj_str_newz(L, lj_debug_uvname(pt, idx)));
    return 1;
  }
  return 0;
}

/* -- Reflection API for traces ------------------------------------------- */

#if LJ_HASJIT

/* Check trace argument. Must not throw for non-existent trace numbers. */
static GCtrace *jit_checktrace(lua_State *L)
{
  TraceNo tr = (TraceNo)lj_lib_checkint(L, 1);
  jit_State *J = L2J(L);
  if (tr > 0 && tr < J->sizetrace)
    return traceref(J, tr);
  return NULL;
}

/* Names of link types. ORDER LJ_TRLINK */
static const char *const jit_trlinkname[] = {
  "none", "root", "loop", "tail-recursion", "up-recursion", "down-recursion",
  "interpreter", "return", "stitch"
};

/* local info = jit.util.traceinfo(tr) */
LJLIB_CF(jit_util_traceinfo)
{
  GCtrace *T = jit_checktrace(L);
  if (T) {
    GCtab *t;
    lua_createtable(L, 0, 8);  /* Increment hash size if fields are added. */
    t = tabV(L->top-1);
    setintfield(L, t, "nins", (int32_t)T->nins - REF_BIAS - 1);
    setintfield(L, t, "nk", REF_BIAS - (int32_t)T->nk);
    setintfield(L, t, "link", T->link);
    setintfield(L, t, "nexit", T->nsnap);
    setstrV(L, L->top++, lj_str_newz(L, jit_trlinkname[T->linktype]));
    lua_setfield(L, -2, "linktype");
    /* There are many more fields. Add them only when needed. */
    return 1;
  }
  return 0;
}

/* local m, ot, op1, op2, prev = jit.util.traceir(tr, idx) */
LJLIB_CF(jit_util_traceir)
{
  GCtrace *T = jit_checktrace(L);
  IRRef ref = (IRRef)lj_lib_checkint(L, 2) + REF_BIAS;
  if (T && ref >= REF_BIAS && ref < T->nins) {
    IRIns *ir = &T->ir[ref];
    int32_t m = lj_ir_mode[ir->o];
    setintV(L->top-2, m);
    setintV(L->top-1, ir->ot);
    setintV(L->top++, (int32_t)ir->op1 - (irm_op1(m)==IRMref ? REF_BIAS : 0));
    setintV(L->top++, (int32_t)ir->op2 - (irm_op2(m)==IRMref ? REF_BIAS : 0));
    setintV(L->top++, ir->prev);
    return 5;
  }
  return 0;
}

/* local k, t [, slot] = jit.util.tracek(tr, idx) */
LJLIB_CF(jit_util_tracek)
{
  GCtrace *T = jit_checktrace(L);
  IRRef ref = (IRRef)lj_lib_checkint(L, 2) + REF_BIAS;
  if (T && ref >= T->nk && ref < REF_BIAS) {
    IRIns *ir = &T->ir[ref];
    int32_t slot = -1;
    if (ir->o == IR_KSLOT) {
      slot = ir->op2;
      ir = &T->ir[ir->op1];
    }
#if LJ_HASFFI
    if (ir->o == IR_KINT64) ctype_loadffi(L);
#endif
    lj_ir_kvalue(L, L->top-2, ir);
    setintV(L->top-1, (int32_t)irt_type(ir->t));
    if (slot == -1)
      return 2;
    setintV(L->top++, slot);
    return 3;
  }
  return 0;
}

/* local snap = jit.util.tracesnap(tr, sn) */
LJLIB_CF(jit_util_tracesnap)
{
  GCtrace *T = jit_checktrace(L);
  SnapNo sn = (SnapNo)lj_lib_checkint(L, 2);
  if (T && sn < T->nsnap) {
    SnapShot *snap = &T->snap[sn];
    SnapEntry *map = &T->snapmap[snap->mapofs];
    MSize n, nent = snap->nent;
    GCtab *t;
    lua_createtable(L, nent+2, 0);
    t = tabV(L->top-1);
    setintV(lj_tab_setint(L, t, 0), (int32_t)snap->ref - REF_BIAS);
    setintV(lj_tab_setint(L, t, 1), (int32_t)snap->nslots);
    for (n = 0; n < nent; n++)
      setintV(lj_tab_setint(L, t, (int32_t)(n+2)), (int32_t)map[n]);
    setintV(lj_tab_setint(L, t, (int32_t)(nent+2)), (int32_t)SNAP(255, 0, 0));
    return 1;
  }
  return 0;
}

/* local mcode, addr, loop = jit.util.tracemc(tr) */
LJLIB_CF(jit_util_tracemc)
{
  GCtrace *T = jit_checktrace(L);
  if (T && T->mcode != NULL) {
    setstrV(L, L->top-1, lj_str_new(L, (const char *)T->mcode, T->szmcode));
    setintptrV(L->top++, (intptr_t)(void *)T->mcode);
    setintV(L->top++, T->mcloop);
    return 3;
  }
  return 0;
}

/* local addr = jit.util.traceexitstub([tr,] exitno) */
LJLIB_CF(jit_util_traceexitstub)
{
#ifdef EXITSTUBS_PER_GROUP
  ExitNo exitno = (ExitNo)lj_lib_checkint(L, 1);
  jit_State *J = L2J(L);
  if (exitno < EXITSTUBS_PER_GROUP*LJ_MAX_EXITSTUBGR) {
    setintptrV(L->top-1, (intptr_t)(void *)exitstub_addr(J, exitno));
    return 1;
  }
#else
  if (L->top > L->base+1) {  /* Don't throw for one-argument variant. */
    GCtrace *T = jit_checktrace(L);
    ExitNo exitno = (ExitNo)lj_lib_checkint(L, 2);
    ExitNo maxexit = T->root ? T->nsnap+1 : T->nsnap;
    if (T && T->mcode != NULL && exitno < maxexit) {
      setintptrV(L->top-1, (intptr_t)(void *)exitstub_trace_addr(T, exitno));
      return 1;
    }
  }
#endif
  return 0;
}

/* local addr = jit.util.ircalladdr(idx) */
LJLIB_CF(jit_util_ircalladdr)
{
  uint32_t idx = (uint32_t)lj_lib_checkint(L, 1);
  if (idx < IRCALL__MAX) {
    ASMFunction func = lj_ir_callinfo[idx].func;
    setintptrV(L->top-1, (intptr_t)(void *)lj_ptr_strip(func));
    return 1;
  }
  return 0;
}

#endif

#include "lj_libdef.h"

static int luaopen_jit_util(lua_State *L)
{
  LJ_LIB_REG(L, NULL, jit_util);
  return 1;
}

/* -- jit.opt module ------------------------------------------------------ */

#if LJ_HASJIT

#define LJLIB_MODULE_jit_opt

/* Parse optimization level. */
static int jitopt_level(jit_State *J, const char *str)
{
  if (str[0] >= '0' && str[0] <= '9' && str[1] == '\0') {
    uint32_t flags;
    if (str[0] == '0') flags = JIT_F_OPT_0;
    else if (str[0] == '1') flags = JIT_F_OPT_1;
    else if (str[0] == '2') flags = JIT_F_OPT_2;
    else flags = JIT_F_OPT_3;
    J->flags = (J->flags & ~JIT_F_OPT_MASK) | flags;
    return 1;  /* Ok. */
  }
  return 0;  /* No match. */
}

/* Parse optimization flag. */
static int jitopt_flag(jit_State *J, const char *str)
{
  const char *lst = JIT_F_OPTSTRING;
  uint32_t opt;
  int set = 1;
  if (str[0] == '+') {
    str++;
  } else if (str[0] == '-') {
    str++;
    set = 0;
  } else if (str[0] == 'n' && str[1] == 'o') {
    str += str[2] == '-' ? 3 : 2;
    set = 0;
  }
  for (opt = JIT_F_OPT; ; opt <<= 1) {
    size_t len = *(const uint8_t *)lst;
    if (len == 0)
      break;
    if (strncmp(str, lst+1, len) == 0 && str[len] == '\0') {
      if (set) J->flags |= opt; else J->flags &= ~opt;
      return 1;  /* Ok. */
    }
    lst += 1+len;
  }
  return 0;  /* No match. */
}

/* Parse optimization parameter. */
static int jitopt_param(jit_State *J, const char *str)
{
  const char *lst = JIT_P_STRING;
  int i;
  for (i = 0; i < JIT_P__MAX; i++) {
    size_t len = *(const uint8_t *)lst;
    lj_assertJ(len != 0, "bad JIT_P_STRING");
    if (strncmp(str, lst+1, len) == 0 && str[len] == '=') {
      uint32_t n = 0;
      const char *p = &str[len+1];
      while (*p >= '0' && *p <= '9')
	n = n*10 + (*p++ - '0');
      if (*p || (int32_t)n < 0) return 0;  /* Malformed number. */
      if (i == JIT_P_sizemcode) {  /* Adjust to required range here. */
#if LJ_TARGET_JUMPRANGE
	uint32_t maxkb = ((1 << (LJ_TARGET_JUMPRANGE - 10)) - 64);
#else
	uint32_t maxkb = ((1 << (31 - 10)) - 64);
#endif
	n = (n + (LJ_PAGESIZE >> 10) - 1) & ~((LJ_PAGESIZE >> 10) - 1);
	if (n > maxkb) n = maxkb;
      }
      J->param[i] = (int32_t)n;
      if (i == JIT_P_hotloop)
	lj_dispatch_init_hotcount(J2G(J));
      return 1;  /* Ok. */
    }
    lst += 1+len;
  }
  return 0;  /* No match. */
}

/* jit.opt.start(flags...) */
LJLIB_CF(jit_opt_start)
{
  jit_State *J = L2J(L);
  int nargs = (int)(L->top - L->base);
  if (nargs == 0) {
    J->flags = (J->flags & ~JIT_F_OPT_MASK) | JIT_F_OPT_DEFAULT;
  } else {
    int i;
    for (i = 1; i <= nargs; i++) {
      const char *str = strdata(lj_lib_checkstr(L, i));
      if (!jitopt_level(J, str) &&
	  !jitopt_flag(J, str) &&
	  !jitopt_param(J, str))
	lj_err_callerv(L, LJ_ERR_JITOPT, str);
    }
  }
  return 0;
}

#include "lj_libdef.h"

#endif

/* -- jit.profile module -------------------------------------------------- */

#if LJ_HASPROFILE

#define LJLIB_MODULE_jit_profile

/* Not loaded by default, use: local profile = require("jit.profile") */

#define KEY_PROFILE_THREAD	(U64x(81000000,00000000)|'t')
#define KEY_PROFILE_FUNC	(U64x(81000000,00000000)|'f')

static void jit_profile_callback(lua_State *L2, lua_State *L, int samples,
				 int vmstate)
{
  TValue key;
  cTValue *tv;
  key.u64 = KEY_PROFILE_FUNC;
  tv = lj_tab_get(L, tabV(registry(L)), &key);
  if (tvisfunc(tv)) {
    char vmst = (char)vmstate;
    int status;
    setfuncV(L2, L2->top++, funcV(tv));
    setthreadV(L2, L2->top++, L);
    setintV(L2->top++, samples);
    setstrV(L2, L2->top++, lj_str_new(L2, &vmst, 1));
    status = lua_pcall(L2, 3, 0, 0);  /* callback(thread, samples, vmstate) */
    if (status) {
      if (G(L2)->panic) G(L2)->panic(L2);
      exit(EXIT_FAILURE);
    }
    lj_trace_abort(G(L2));
  }
}

/* profile.start(mode, cb) */
LJLIB_CF(jit_profile_start)
{
  GCtab *registry = tabV(registry(L));
  GCstr *mode = lj_lib_optstr(L, 1);
  GCfunc *func = lj_lib_checkfunc(L, 2);
  lua_State *L2 = lua_newthread(L);  /* Thread that runs profiler callback. */
  TValue key;
  /* Anchor thread and function in registry. */
  key.u64 = KEY_PROFILE_THREAD;
  setthreadV(L, lj_tab_set(L, registry, &key), L2);
  key.u64 = KEY_PROFILE_FUNC;
  setfuncV(L, lj_tab_set(L, registry, &key), func);
  lj_gc_anybarriert(L, registry);
  luaJIT_profile_start(L, mode ? strdata(mode) : "",
		       (luaJIT_profile_callback)jit_profile_callback, L2);
  return 0;
}

/* profile.stop() */
LJLIB_CF(jit_profile_stop)
{
  GCtab *registry;
  TValue key;
  luaJIT_profile_stop(L);
  registry = tabV(registry(L));
  key.u64 = KEY_PROFILE_THREAD;
  setnilV(lj_tab_set(L, registry, &key));
  key.u64 = KEY_PROFILE_FUNC;
  setnilV(lj_tab_set(L, registry, &key));
  lj_gc_anybarriert(L, registry);
  return 0;
}

/* dump = profile.dumpstack([thread,] fmt, depth) */
LJLIB_CF(jit_profile_dumpstack)
{
  lua_State *L2 = L;
  int arg = 0;
  size_t len;
  int depth;
  GCstr *fmt;
  const char *p;
  if (L->top > L->base && tvisthread(L->base)) {
    L2 = threadV(L->base);
    arg = 1;
  }
  fmt = lj_lib_checkstr(L, arg+1);
  depth = lj_lib_checkint(L, arg+2);
  p = luaJIT_profile_dumpstack(L2, strdata(fmt), depth, &len);
  lua_pushlstring(L, p, len);
  return 1;
}

#include "lj_libdef.h"

static int luaopen_jit_profile(lua_State *L)
{
  LJ_LIB_REG(L, NULL, jit_profile);
  return 1;
}

#endif

/* -- JIT compiler initialization ----------------------------------------- */

#if LJ_HASJIT
/* Default values for JIT parameters. */
static const int32_t jit_param_default[JIT_P__MAX+1] = {
#define JIT_PARAMINIT(len, name, value)	(value),
JIT_PARAMDEF(JIT_PARAMINIT)
#undef JIT_PARAMINIT
  0
};

#if LJ_TARGET_ARM && LJ_TARGET_LINUX
#include <sys/utsname.h>
#endif

/* Arch-dependent CPU feature detection. */
static uint32_t jit_cpudetect(void)
{
  uint32_t flags = 0;
#if LJ_TARGET_X86ORX64

  uint32_t vendor[4];
  uint32_t features[4];
  if (lj_vm_cpuid(0, vendor) && lj_vm_cpuid(1, features)) {
    flags |= ((features[2] >> 0)&1) * JIT_F_SSE3;
    flags |= ((features[2] >> 19)&1) * JIT_F_SSE4_1;
    if (vendor[0] >= 7) {
      uint32_t xfeatures[4];
      lj_vm_cpuid(7, xfeatures);
      flags |= ((xfeatures[1] >> 8)&1) * JIT_F_BMI2;
    }
  }
  /* Don't bother checking for SSE2 -- the VM will crash before getting here. */

#elif LJ_TARGET_ARM

  int ver = LJ_ARCH_VERSION;  /* Compile-time ARM CPU detection. */
#if LJ_TARGET_LINUX
  if (ver < 70) {  /* Runtime ARM CPU detection. */
    struct utsname ut;
    uname(&ut);
    if (strncmp(ut.machine, "armv", 4) == 0) {
      if (ut.machine[4] >= '8') ver = 80;
      else if (ut.machine[4] == '7') ver = 70;
      else if (ut.machine[4] == '6') ver = 60;
    }
  }
#endif
  flags |= ver >= 70 ? JIT_F_ARMV7 :
	   ver >= 61 ? JIT_F_ARMV6T2_ :
	   ver >= 60 ? JIT_F_ARMV6_ : 0;
  flags |= LJ_ARCH_HASFPU == 0 ? 0 : ver >= 70 ? JIT_F_VFPV3 : JIT_F_VFPV2;

#elif LJ_TARGET_ARM64

  /* No optional CPU features to detect (for now). */

#elif LJ_TARGET_PPC

#if LJ_ARCH_SQRT
  flags |= JIT_F_SQRT;
#endif
#if LJ_ARCH_ROUND
  flags |= JIT_F_ROUND;
#endif

#elif LJ_TARGET_MIPS

  /* Compile-time MIPS CPU detection. */
#if LJ_ARCH_VERSION >= 20
  flags |= JIT_F_MIPSXXR2;
#endif
  /* Runtime MIPS CPU detection. */
#if defined(__GNUC__)
  if (!(flags & JIT_F_MIPSXXR2)) {
    int x;
#ifdef __mips16
    x = 0;  /* Runtime detection is difficult. Ensure optimal -march flags. */
#else
    /* On MIPS32R1 rotr is treated as srl. rotr r2,r2,1 -> srl r2,r2,1. */
    __asm__("li $2, 1\n\t.long 0x00221042\n\tmove %0, $2" : "=r"(x) : : "$2");
#endif
    if (x) flags |= JIT_F_MIPSXXR2;  /* Either 0x80000000 (R2) or 0 (R1). */
  }
#endif

#else
#error "Missing CPU detection for this architecture"
#endif
  return flags;
}

/* Initialize JIT compiler. */
static void jit_init(lua_State *L)
{
  jit_State *J = L2J(L);
  J->flags = jit_cpudetect() | JIT_F_ON | JIT_F_OPT_DEFAULT;
  memcpy(J->param, jit_param_default, sizeof(J->param));
#if LJ_TARGET_UNALIGNED
  G(L)->tmptv.u64 = U64x(0000504d,4d500000);
#endif
  lj_dispatch_update(G(L));
#if LJ_TARGET_UNALIGNED
  /* If you get a crash below then your toolchain indicates unaligned
  ** accesses are OK, but your kernel disagrees. I.e. fix your toolchain.
  */
  if (*(uint32_t *)((char *)&G(L)->tmptv + 2) != 0x504d4d50u) L->top = NULL;
#endif
}
#endif

LUALIB_API int luaopen_jit(lua_State *L)
{
#if LJ_HASJIT
  jit_init(L);
#endif
  setboolV(L->top++, LJ_54);
  lua_pushliteral(L, LJ_OS_NAME);
  lua_pushliteral(L, LJ_ARCH_NAME);
  lua_pushinteger(L, LUAJIT_VERSION_NUM);  /* Deprecated. */
  lua_pushliteral(L, LUAJIT_VERSION);
  LJ_LIB_REG(L, LUA_JITLIBNAME, jit);
#if LJ_54
  lua_getglobal(L, LUA_JITLIBNAME);
  lua_pushcfunction(L, lj_cf_jit__lua54_idiv);
  lua_setfield(L, -2, "_lua54_idiv");
  lua_pushcfunction(L, lj_cf_jit__lua54_mod);
  lua_setfield(L, -2, "_lua54_mod");
  lua_pushcfunction(L, lj_cf_jit__lua54_band);
  lua_setfield(L, -2, "_lua54_band");
  lua_pushcfunction(L, lj_cf_jit__lua54_bor);
  lua_setfield(L, -2, "_lua54_bor");
  lua_pushcfunction(L, lj_cf_jit__lua54_bxor);
  lua_setfield(L, -2, "_lua54_bxor");
  lua_pushcfunction(L, lj_cf_jit__lua54_bnot);
  lua_setfield(L, -2, "_lua54_bnot");
  lua_pushcfunction(L, lj_cf_jit__lua54_shl);
  lua_setfield(L, -2, "_lua54_shl");
  lua_pushcfunction(L, lj_cf_jit__lua54_shr);
  lua_setfield(L, -2, "_lua54_shr");
  lua_pushcfunction(L, lj_cf_jit__lua54_forstep);
  lua_setfield(L, -2, "_lua54_forstep");
  lua_pushcfunction(L, lj_cf_jit__lua54_checkclose);
  lua_setfield(L, -2, "_lua54_checkclose");
  lua_pushcfunction(L, lj_cf_jit__lua54_closevalue);
  lua_setfield(L, -2, "_lua54_closevalue");
  lua_pushcfunction(L, lj_cf_jit__lua54_packreturn);
  lua_setfield(L, -2, "_lua54_packreturn");
  lua_pushcfunction(L, lj_cf_jit__lua54_unpackreturn);
  lua_setfield(L, -2, "_lua54_unpackreturn");
  lua_pop(L, 1);
#endif
#if LJ_HASPROFILE
  lj_lib_prereg(L, LUA_JITLIBNAME ".profile", luaopen_jit_profile,
		tabref(L->env));
#endif
#ifndef LUAJIT_DISABLE_JITUTIL
  lj_lib_prereg(L, LUA_JITLIBNAME ".util", luaopen_jit_util, tabref(L->env));
#endif
#if LJ_HASJIT
  LJ_LIB_REG(L, "jit.opt", jit_opt);
#endif
  L->top -= 2;
  return 1;
}
