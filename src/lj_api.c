/*
** Public Lua/C API.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_api_c
#define LUA_CORE

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_func.h"
#include "lj_udata.h"
#include "lj_meta.h"
#include "lj_state.h"
#include "lj_bc.h"
#include "lj_frame.h"
#include "lj_trace.h"
#include "lj_vm.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"

/* -- Common helper functions --------------------------------------------- */

#define lj_checkapi_slot(idx) \
  lj_checkapi((idx) <= (L->top - L->base), "stack slot %d out of range", (idx))

static TValue *index2adr(lua_State *L, int idx)
{
  if (idx > 0) {
    TValue *o = L->base + (idx - 1);
    return o < L->top ? o : niltv(L);
  } else if (idx > LUA_REGISTRYINDEX) {
    lj_checkapi(idx != 0 && -idx <= L->top - L->base,
		"bad stack slot %d", idx);
    return L->top + idx;
  } else if (idx == LUA_GLOBALSINDEX) {
    TValue *o = &G(L)->tmptv;
    settabV(L, o, tabref(L->env));
    return o;
  } else if (idx == LUA_REGISTRYINDEX) {
    return registry(L);
  } else {
    GCfunc *fn = curr_func(L);
    lj_checkapi(fn->c.gct == ~LJ_TFUNC && !isluafunc(fn),
		"calling frame is not a C function");
    if (idx == LUA_ENVIRONINDEX) {
      TValue *o = &G(L)->tmptv;
      settabV(L, o, tabref(fn->c.env));
      return o;
    } else {
      idx = LUA_GLOBALSINDEX - idx;
      return idx <= fn->c.nupvalues ? &fn->c.upvalue[idx-1] : niltv(L);
    }
  }
}

static LJ_AINLINE TValue *index2adr_check(lua_State *L, int idx)
{
  TValue *o = index2adr(L, idx);
  lj_checkapi(o != niltv(L), "invalid stack slot %d", idx);
  return o;
}

static TValue *index2adr_stack(lua_State *L, int idx)
{
  if (idx > 0) {
    TValue *o = L->base + (idx - 1);
    if (o < L->top) {
      return o;
    } else {
      lj_checkapi(0, "invalid stack slot %d", idx);
      return niltv(L);
    }
    return o < L->top ? o : niltv(L);
  } else {
    lj_checkapi(idx != 0 && -idx <= L->top - L->base,
		"invalid stack slot %d", idx);
    return L->top + idx;
  }
}

static GCtab *getcurrenv(lua_State *L)
{
  GCfunc *fn = curr_func(L);
  return fn->c.gct == ~LJ_TFUNC ? tabref(fn->c.env) : tabref(L->env);
}

/* -- Miscellaneous API functions ----------------------------------------- */

LUA_API int lua_status(lua_State *L)
{
  return L->status;
}

LUA_API int lua_checkstack(lua_State *L, int size)
{
  if (size > LUAI_MAXCSTACK || (L->top - L->base + size) > LUAI_MAXCSTACK) {
    return 0;  /* Stack overflow. */
  } else if (size > 0) {
    int avail = (int)(mref(L->maxstack, TValue) - L->top);
    if (size > avail &&
	lj_state_cpgrowstack(L, (MSize)(size - avail)) != LUA_OK) {
      L->top--;
      return 0;  /* Out of memory. */
    }
  }
  return 1;
}

#if LJ_54
LUA_API int lua_setcstacklimit(lua_State *L, unsigned int limit)
{
  UNUSED(L);
  UNUSED(limit);
  /* LuaJIT keeps its own C stack guard. Expose Lua 5.4's C API entrypoint as
  ** the same stable compatibility limit used by debug.setcstacklimit().
  */
  return 200;
}
#endif

LUALIB_API void luaL_checkstack(lua_State *L, int size, const char *msg)
{
  if (!lua_checkstack(L, size))
    lj_err_callerv(L, LJ_ERR_STKOVM, msg);
}

LUA_API void lua_xmove(lua_State *L, lua_State *to, int n)
{
  TValue *f, *t;
  if (L == to) return;
  lj_checkapi_slot(n);
  lj_checkapi(G(L) == G(to), "move across global states");
  lj_state_checkstack(to, (MSize)n);
  f = L->top;
  t = to->top = to->top + n;
  while (--n >= 0) copyTV(to, --t, --f);
  L->top = f;
}

LUA_API const lua_Number *lua_version(lua_State *L)
{
  static const lua_Number version = LUA_VERSION_NUM;
  UNUSED(L);
  return &version;
}

LUA_API void *lua_getextraspace(lua_State *L)
{
  return &L->exdata;
}

#if LJ_54
LUA_API void *lua_getextraspace54(lua_State *L)
{
  return lua_getextraspace(L);
}
#endif

/* -- Stack manipulation -------------------------------------------------- */

LUA_API int lua_gettop(lua_State *L)
{
  return (int)(L->top - L->base);
}

LUA_API int lua_absindex(lua_State *L, int idx)
{
  if (idx > 0 || idx <= LUA_REGISTRYINDEX)
    return idx;
  return (int)(L->top - L->base) + idx + 1;
}

LUA_API void lua_settop(lua_State *L, int idx)
{
  if (idx >= 0) {
    lj_checkapi(idx <= tvref(L->maxstack) - L->base, "bad stack slot %d", idx);
    if (L->base + idx > L->top) {
      if (L->base + idx >= tvref(L->maxstack))
	lj_state_growstack(L, (MSize)idx - (MSize)(L->top - L->base));
      do { setnilV(L->top++); } while (L->top < L->base + idx);
    } else {
      L->top = L->base + idx;
    }
  } else {
    lj_checkapi(-(idx+1) <= (L->top - L->base), "bad stack slot %d", idx);
    L->top += idx+1;  /* Shrinks top (idx < 0). */
  }
}

LUA_API void lua_remove(lua_State *L, int idx)
{
  TValue *p = index2adr_stack(L, idx);
  while (++p < L->top) copyTV(L, p-1, p);
  L->top--;
}

LUA_API void lua_insert(lua_State *L, int idx)
{
  TValue *q, *p = index2adr_stack(L, idx);
  for (q = L->top; q > p; q--) copyTV(L, q, q-1);
  copyTV(L, p, L->top);
}

static void api_stack_reverse(lua_State *L, TValue *from, TValue *to)
{
  while (from < to) {
    TValue tmp;
    copyTV(L, &tmp, from);
    copyTV(L, from, to);
    copyTV(L, to, &tmp);
    from++;
    to--;
  }
}

LUA_API void lua_rotate(lua_State *L, int idx, int n)
{
  TValue *p = index2adr_stack(L, idx);
  int nslots = (int)(L->top - p);
  lj_checkapi(nslots >= 0, "invalid stack slot %d", idx);
  if (nslots <= 1)
    return;
  n %= nslots;
  if (n < 0)
    n += nslots;
  if (n) {
    TValue *m = L->top - n;
    api_stack_reverse(L, p, m-1);
    api_stack_reverse(L, m, L->top-1);
    api_stack_reverse(L, p, L->top-1);
  }
}

static void copy_slot(lua_State *L, TValue *f, int idx)
{
  if (idx == LUA_GLOBALSINDEX) {
    lj_checkapi(tvistab(f), "stack slot %d is not a table", idx);
    /* NOBARRIER: A thread (i.e. L) is never black. */
    setgcref(L->env, obj2gco(tabV(f)));
  } else if (idx == LUA_ENVIRONINDEX) {
    GCfunc *fn = curr_func(L);
    if (fn->c.gct != ~LJ_TFUNC)
      lj_err_msg(L, LJ_ERR_NOENV);
    lj_checkapi(tvistab(f), "stack slot %d is not a table", idx);
    setgcref(fn->c.env, obj2gco(tabV(f)));
    lj_gc_barrier(L, fn, f);
  } else {
    TValue *o = index2adr_check(L, idx);
    copyTV(L, o, f);
    if (idx < LUA_GLOBALSINDEX)  /* Need a barrier for upvalues. */
      lj_gc_barrier(L, curr_func(L), f);
  }
}

LUA_API void lua_replace(lua_State *L, int idx)
{
  lj_checkapi_slot(1);
  copy_slot(L, L->top - 1, idx);
  L->top--;
}

LUA_API void lua_copy(lua_State *L, int fromidx, int toidx)
{
  copy_slot(L, index2adr(L, fromidx), toidx);
}

LUA_API void lua_pushvalue(lua_State *L, int idx)
{
  copyTV(L, L->top, index2adr(L, idx));
  incr_top(L);
}

/* -- Stack getters ------------------------------------------------------- */

LUA_API int lua_type(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  if (tvisnumber(o)) {
    return LUA_TNUMBER;
#if LJ_64 && !LJ_GC64
  } else if (tvislightud(o)) {
    return LUA_TLIGHTUSERDATA;
#endif
  } else if (o == niltv(L)) {
    return LUA_TNONE;
  } else {  /* Magic internal/external tag conversion. ORDER LJ_T */
    uint32_t t = ~itype(o);
#if LJ_64
    int tt = (int)((U64x(75a06,98042110) >> 4*t) & 15u);
#else
    int tt = (int)(((t < 8 ? 0x98042110u : 0x75a06u) >> 4*(t&7)) & 15u);
#endif
    lj_assertL(tt != LUA_TNIL || tvisnil(o), "bad tag conversion");
    return tt;
  }
}

LUALIB_API void luaL_checktype(lua_State *L, int idx, int tt)
{
  if (lua_type(L, idx) != tt)
    lj_err_argt(L, idx, tt);
}

LUALIB_API void luaL_checkany(lua_State *L, int idx)
{
  if (index2adr(L, idx) == niltv(L))
    lj_err_arg(L, idx, LJ_ERR_NOVAL);
}

LUA_API const char *lua_typename(lua_State *L, int t)
{
  UNUSED(L);
  return lj_obj_typename[t+1];
}

LUA_API int lua_iscfunction(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return tvisfunc(o) && !isluafunc(funcV(o));
}

LUA_API int lua_isnumber(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  return (tvisnumber(o) || (tvisstr(o) && lj_strscan_number(strV(o), &tmp)));
}

LUA_API int lua_isinteger(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  if (tvisint(o))
    return 1;
  if (tvisnum(o)) {
    lua_Number n = numV(o);
    lua_Number ni = lj_vm_floor(n);
    return n == ni && n >= (lua_Number)LUA_MININTEGER &&
	   n <= (lua_Number)LUA_MAXINTEGER;
  }
  return 0;
}

LUA_API int lua_isstring(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return (tvisstr(o) || tvisnumber(o));
}

LUA_API int lua_isuserdata(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return (tvisudata(o) || tvislightud(o));
}

LUA_API int lua_rawequal(lua_State *L, int idx1, int idx2)
{
  cTValue *o1 = index2adr(L, idx1);
  cTValue *o2 = index2adr(L, idx2);
  return (o1 == niltv(L) || o2 == niltv(L)) ? 0 : lj_obj_equal(o1, o2);
}

LUA_API int lua_equal(lua_State *L, int idx1, int idx2)
{
  cTValue *o1 = index2adr(L, idx1);
  cTValue *o2 = index2adr(L, idx2);
  if (tvisint(o1) && tvisint(o2)) {
    return intV(o1) == intV(o2);
  } else if (tvisnumber(o1) && tvisnumber(o2)) {
    return numberVnum(o1) == numberVnum(o2);
  } else if (itype(o1) != itype(o2)) {
    return 0;
  } else if (tvispri(o1)) {
    return o1 != niltv(L) && o2 != niltv(L);
#if LJ_64 && !LJ_GC64
  } else if (tvislightud(o1)) {
    return o1->u64 == o2->u64;
#endif
  } else if (gcrefeq(o1->gcr, o2->gcr)) {
    return 1;
  } else if (!tvistabud(o1)) {
    return 0;
  } else {
    TValue *base = lj_meta_equal(L, gcV(o1), gcV(o2), 0);
    if ((uintptr_t)base <= 1) {
      return (int)(uintptr_t)base;
    } else {
      L->top = base+2;
      lj_vm_call(L, base, 1+1);
      L->top -= 2+LJ_FR2;
      return tvistruecond(L->top+1+LJ_FR2);
    }
  }
}

LUA_API int lua_lessthan(lua_State *L, int idx1, int idx2)
{
  cTValue *o1 = index2adr(L, idx1);
  cTValue *o2 = index2adr(L, idx2);
  if (o1 == niltv(L) || o2 == niltv(L)) {
    return 0;
  } else if (tvisint(o1) && tvisint(o2)) {
    return intV(o1) < intV(o2);
  } else if (tvisnumber(o1) && tvisnumber(o2)) {
    return numberVnum(o1) < numberVnum(o2);
  } else {
    TValue *base = lj_meta_comp(L, o1, o2, 0);
    if ((uintptr_t)base <= 1) {
      return (int)(uintptr_t)base;
    } else {
      L->top = base+2;
      lj_vm_call(L, base, 1+1);
      L->top -= 2+LJ_FR2;
      return tvistruecond(L->top+1+LJ_FR2);
    }
  }
}

static int api_lessequal(lua_State *L, int idx1, int idx2)
{
  cTValue *o1 = index2adr(L, idx1);
  cTValue *o2 = index2adr(L, idx2);
  if (o1 == niltv(L) || o2 == niltv(L)) {
    return 0;
  } else if (tvisint(o1) && tvisint(o2)) {
    return intV(o1) <= intV(o2);
  } else if (tvisnumber(o1) && tvisnumber(o2)) {
    return numberVnum(o1) <= numberVnum(o2);
  } else {
    TValue *base = lj_meta_comp(L, o1, o2, 2);
    if ((uintptr_t)base <= 1) {
      return (int)(uintptr_t)base;
    } else {
      L->top = base+2;
      lj_vm_call(L, base, 1+1);
      L->top -= 2+LJ_FR2;
      return tvistruecond(L->top+1+LJ_FR2);
    }
  }
}

LUA_API int lua_compare(lua_State *L, int idx1, int idx2, int op)
{
  switch (op) {
  case LUA_OPEQ:
    return lua_equal(L, idx1, idx2);
  case LUA_OPLT:
    return lua_lessthan(L, idx1, idx2);
  case LUA_OPLE:
    return api_lessequal(L, idx1, idx2);
  default:
    lj_checkapi(0, "invalid comparison op %d", op);
    return 0;
  }
}

LUA_API lua_Number lua_tonumber(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  if (LJ_LIKELY(tvisnumber(o)))
    return numberVnum(o);
  else if (tvisstr(o) && lj_strscan_num(strV(o), &tmp))
    return numV(&tmp);
  else
    return 0;
}

LUA_API lua_Number lua_tonumberx(lua_State *L, int idx, int *ok)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  if (LJ_LIKELY(tvisnumber(o))) {
    if (ok) *ok = 1;
    return numberVnum(o);
  } else if (tvisstr(o) && lj_strscan_num(strV(o), &tmp)) {
    if (ok) *ok = 1;
    return numV(&tmp);
  } else {
    if (ok) *ok = 0;
    return 0;
  }
}

LUALIB_API lua_Number luaL_checknumber(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  if (LJ_LIKELY(tvisnumber(o)))
    return numberVnum(o);
  else if (!(tvisstr(o) && lj_strscan_num(strV(o), &tmp)))
    lj_err_argt(L, idx, LUA_TNUMBER);
  return numV(&tmp);
}

LUALIB_API lua_Number luaL_optnumber(lua_State *L, int idx, lua_Number def)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  if (LJ_LIKELY(tvisnumber(o)))
    return numberVnum(o);
  else if (tvisnil(o))
    return def;
  else if (!(tvisstr(o) && lj_strscan_num(strV(o), &tmp)))
    lj_err_argt(L, idx, LUA_TNUMBER);
  return numV(&tmp);
}

#if LJ_54
static int luaV_tointeger54(cTValue *o, lua_Integer *ip, int *isnum)
{
  TValue tmp;
  lua_Number n;
  int64_t k;
  if (isnum)
    *isnum = 0;
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
    *ip = (lua_Integer)intV(o);
    return 1;
  }
  n = numV(o);
  /* Lua 5.4 integer conversion is exact. Fractions remain numbers, but they
  ** are not valid integers for lua_tointegerx/luaL_checkinteger.
  */
  if (!(n >= (lua_Number)LUA_MININTEGER && n <= (lua_Number)LUA_MAXINTEGER))
    return 0;
  k = lj_num2i64(n);
  if ((lua_Number)k != n)
    return 0;
  *ip = (lua_Integer)k;
  return 1;
}
#endif

LUA_API lua_Integer lua_tointeger(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
#if LJ_54
  lua_Integer i;
  return luaV_tointeger54(o, &i, NULL) ? i : 0;
#else
  TValue tmp;
  lua_Number n;
  if (LJ_LIKELY(tvisint(o))) {
    return intV(o);
  } else if (LJ_LIKELY(tvisnum(o))) {
    n = numV(o);
  } else {
    if (!(tvisstr(o) && lj_strscan_number(strV(o), &tmp)))
      return 0;
    if (tvisint(&tmp))
      return intV(&tmp);
    n = numV(&tmp);
  }
  return lj_num2int_type(n, lua_Integer);
#endif
}

LUA_API lua_Integer lua_tointegerx(lua_State *L, int idx, int *ok)
{
  cTValue *o = index2adr(L, idx);
#if LJ_54
  lua_Integer i;
  int success = luaV_tointeger54(o, &i, NULL);
  if (ok) *ok = success;
  return success ? i : 0;
#else
  TValue tmp;
  lua_Number n;
  if (LJ_LIKELY(tvisint(o))) {
    if (ok) *ok = 1;
    return intV(o);
  } else if (LJ_LIKELY(tvisnum(o))) {
    n = numV(o);
  } else {
    if (!(tvisstr(o) && lj_strscan_number(strV(o), &tmp))) {
      if (ok) *ok = 0;
      return 0;
    }
    if (tvisint(&tmp)) {
      if (ok) *ok = 1;
      return intV(&tmp);
    }
    n = numV(&tmp);
  }
  if (ok) *ok = 1;
  return lj_num2int_type(n, lua_Integer);
#endif
}

LUALIB_API lua_Integer luaL_checkinteger(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
#if LJ_54
  lua_Integer i;
  int isnum = 0;
  if (luaV_tointeger54(o, &i, &isnum))
    return i;
  if (isnum)
    lj_err_arg(L, idx, LJ_ERR_NUMINT);
  lj_err_argt(L, idx, LUA_TNUMBER);
  return 0;  /* unreachable */
#else
  TValue tmp;
  lua_Number n;
  if (LJ_LIKELY(tvisint(o))) {
    return intV(o);
  } else if (LJ_LIKELY(tvisnum(o))) {
    n = numV(o);
  } else {
    if (!(tvisstr(o) && lj_strscan_number(strV(o), &tmp)))
      lj_err_argt(L, idx, LUA_TNUMBER);
    if (tvisint(&tmp))
      return (lua_Integer)intV(&tmp);
    n = numV(&tmp);
  }
  return lj_num2int_type(n, lua_Integer);
#endif
}

LUALIB_API lua_Integer luaL_optinteger(lua_State *L, int idx, lua_Integer def)
{
  cTValue *o = index2adr(L, idx);
#if LJ_54
  lua_Integer i;
  int isnum = 0;
  if (tvisnil(o))
    return def;
  if (luaV_tointeger54(o, &i, &isnum))
    return i;
  if (isnum)
    lj_err_arg(L, idx, LJ_ERR_NUMINT);
  lj_err_argt(L, idx, LUA_TNUMBER);
  return 0;  /* unreachable */
#else
  TValue tmp;
  lua_Number n;
  if (LJ_LIKELY(tvisint(o))) {
    return intV(o);
  } else if (LJ_LIKELY(tvisnum(o))) {
    n = numV(o);
  } else if (tvisnil(o)) {
    return def;
  } else {
    if (!(tvisstr(o) && lj_strscan_number(strV(o), &tmp)))
      lj_err_argt(L, idx, LUA_TNUMBER);
    if (tvisint(&tmp))
      return (lua_Integer)intV(&tmp);
    n = numV(&tmp);
  }
  return lj_num2int_type(n, lua_Integer);
#endif
}

LUA_API int lua_toboolean(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return tvistruecond(o);
}

LUA_API const char *lua_tolstring(lua_State *L, int idx, size_t *len)
{
  TValue *o = index2adr(L, idx);
  GCstr *s;
  if (LJ_LIKELY(tvisstr(o))) {
    s = strV(o);
  } else if (tvisnumber(o)) {
    lj_gc_check(L);
    o = index2adr(L, idx);  /* GC may move the stack. */
    s = lj_strfmt_number(L, o);
    setstrV(L, o, s);
  } else {
    if (len != NULL) *len = 0;
    return NULL;
  }
  if (len != NULL) *len = s->len;
  return strdata(s);
}

LUALIB_API const char *luaL_checklstring(lua_State *L, int idx, size_t *len)
{
  TValue *o = index2adr(L, idx);
  GCstr *s;
  if (LJ_LIKELY(tvisstr(o))) {
    s = strV(o);
  } else if (tvisnumber(o)) {
    lj_gc_check(L);
    o = index2adr(L, idx);  /* GC may move the stack. */
    s = lj_strfmt_number(L, o);
    setstrV(L, o, s);
  } else {
    lj_err_argt(L, idx, LUA_TSTRING);
  }
  if (len != NULL) *len = s->len;
  return strdata(s);
}

LUALIB_API const char *luaL_optlstring(lua_State *L, int idx,
				       const char *def, size_t *len)
{
  TValue *o = index2adr(L, idx);
  GCstr *s;
  if (LJ_LIKELY(tvisstr(o))) {
    s = strV(o);
  } else if (tvisnil(o)) {
    if (len != NULL) *len = def ? strlen(def) : 0;
    return def;
  } else if (tvisnumber(o)) {
    lj_gc_check(L);
    o = index2adr(L, idx);  /* GC may move the stack. */
    s = lj_strfmt_number(L, o);
    setstrV(L, o, s);
  } else {
    lj_err_argt(L, idx, LUA_TSTRING);
  }
  if (len != NULL) *len = s->len;
  return strdata(s);
}

LUALIB_API int luaL_checkoption(lua_State *L, int idx, const char *def,
				const char *const lst[])
{
  ptrdiff_t i;
  const char *s = lua_tolstring(L, idx, NULL);
  if (s == NULL && (s = def) == NULL)
    lj_err_argt(L, idx, LUA_TSTRING);
  for (i = 0; lst[i]; i++)
    if (strcmp(lst[i], s) == 0)
      return (int)i;
  lj_err_argv(L, idx, LJ_ERR_INVOPTM, s);
}

LUA_API size_t lua_stringtonumber(lua_State *L, const char *s)
{
  TValue tv;
  size_t len = strlen(s);
#if LJ_54
  if (lj_strscan_rejectnum54(s, (MSize)len))
    return 0;  /* Keep lua_stringtonumber() aligned with Lua 5.4 tonumber(). */
#endif
  StrScanFmt fmt = lj_strscan_scan((const uint8_t *)s, (MSize)len, &tv,
				   LJ_DUALNUM ? STRSCAN_OPT_TOINT :
						STRSCAN_OPT_TONUM);
  if (fmt == STRSCAN_ERROR)
    return 0;
  if (LJ_DUALNUM && fmt == STRSCAN_INT)
    setitype(&tv, LJ_TISNUM);
  copyTV(L, L->top, &tv);
  incr_top(L);
  return len + 1;
}

LUA_API size_t lua_objlen(lua_State *L, int idx)
{
  TValue *o = index2adr(L, idx);
  if (tvisstr(o)) {
    return strV(o)->len;
  } else if (tvistab(o)) {
    return (size_t)lj_tab_len(tabV(o));
  } else if (tvisudata(o)) {
    return udataV(o)->len;
  } else if (tvisnumber(o)) {
    GCstr *s = lj_strfmt_number(L, o);
    setstrV(L, o, s);
    return s->len;
  } else {
    return 0;
  }
}

LUA_API size_t lua_rawlen(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  if (tvisstr(o))
    return strV(o)->len;
  else if (tvistab(o))
    return (size_t)lj_tab_len(tabV(o));
  else if (tvisudata(o))
    return udataV(o)->len;
  else
    return 0;
}

#if LJ_54
LUA_API lua_Unsigned lua_rawlen54(lua_State *L, int idx)
{
  /* Lua 5.4 headers expose lua_rawlen as lua_Unsigned; keep the old ABI
  ** entrypoint above for LuaJIT/internal callers and adapt only externally. */
  return (lua_Unsigned)lua_rawlen(L, idx);
}
#endif

static void api_call_len_meta(lua_State *L, cTValue *o, cTValue *mo)
{
  TValue *top;
  lj_state_checkstack(L, 3);
  top = L->top;
  copyTV(L, top, mo);
  copyTV(L, top+1, o);
  copyTV(L, top+2, o);
  L->top = top+3;
  lua_call(L, 2, 1);
}

LUA_API void lua_len(lua_State *L, int idx)
{
  cTValue *o;
  cTValue *mo;
  idx = lua_absindex(L, idx);
  lj_state_checkstack(L, 3);
  o = index2adr_check(L, idx);
  if (tvisstr(o)) {
    setintptrV(L->top, strV(o)->len);
    incr_top(L);
    return;
  }
  mo = lj_meta_lookup(L, o, MM_len);
  if (!tvisnil(mo)) {
    /* Lua 5.4 calls unary metamethods with a duplicated operand. */
    api_call_len_meta(L, o, mo);
  } else if (tvistab(o)) {
    setintptrV(L->top, lj_tab_len(tabV(o)));
    incr_top(L);
  } else if (tvisudata(o)) {
    setintptrV(L->top, udataV(o)->len);
    incr_top(L);
  } else {
    lj_err_optype(L, o, LJ_ERR_OPLEN);
  }
}

LUA_API lua_CFunction lua_tocfunction(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  if (tvisfunc(o)) {
    BCOp op = bc_op(*mref(funcV(o)->c.pc, BCIns));
    if (op == BC_FUNCC || op == BC_FUNCCW)
      return funcV(o)->c.f;
  }
  return NULL;
}

LUA_API void *lua_touserdata(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  if (tvisudata(o))
    return uddata(udataV(o));
  else if (tvislightud(o))
    return lightudV(G(L), o);
  else
    return NULL;
}

LUA_API lua_State *lua_tothread(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return (!tvisthread(o)) ? NULL : threadV(o);
}

LUA_API const void *lua_topointer(lua_State *L, int idx)
{
  return lj_obj_ptr(G(L), index2adr(L, idx));
}

/* -- Stack setters (object creation) ------------------------------------- */

LUA_API void lua_pushnil(lua_State *L)
{
  setnilV(L->top);
  incr_top(L);
}

LUA_API void lua_pushnumber(lua_State *L, lua_Number n)
{
  setnumV(L->top, n);
  if (LJ_UNLIKELY(tvisnan(L->top)))
    setnanV(L->top);  /* Canonicalize injected NaNs. */
  incr_top(L);
}

LUA_API void lua_pushinteger(lua_State *L, lua_Integer n)
{
  setintptrV(L->top, n);
  incr_top(L);
}

LUA_API void lua_pushlstring(lua_State *L, const char *str, size_t len)
{
  GCstr *s;
  lj_gc_check(L);
  s = lj_str_new(L, str, len);
  setstrV(L, L->top, s);
  incr_top(L);
}

LUA_API void lua_pushstring(lua_State *L, const char *str)
{
  if (str == NULL) {
    setnilV(L->top);
  } else {
    GCstr *s;
    lj_gc_check(L);
    s = lj_str_newz(L, str);
    setstrV(L, L->top, s);
  }
  incr_top(L);
}

#if LJ_54
LUA_API const char *lua_pushlstring54(lua_State *L, const char *str,
				      size_t len)
{
  lua_pushlstring(L, str, len);
  return lua_tolstring(L, -1, NULL);
}

LUA_API const char *lua_pushstring54(lua_State *L, const char *str)
{
  lua_pushstring(L, str);
  return lua_tolstring(L, -1, NULL);
}
#endif

LUA_API const char *lua_pushvfstring(lua_State *L, const char *fmt,
				     va_list argp)
{
  lj_gc_check(L);
  return lj_strfmt_pushvf(L, fmt, argp);
}

LUA_API const char *lua_pushfstring(lua_State *L, const char *fmt, ...)
{
  const char *ret;
  va_list argp;
  lj_gc_check(L);
  va_start(argp, fmt);
  ret = lj_strfmt_pushvf(L, fmt, argp);
  va_end(argp);
  return ret;
}

LUA_API void lua_pushcclosure(lua_State *L, lua_CFunction f, int n)
{
  GCfunc *fn;
  lj_gc_check(L);
  lj_checkapi_slot(n);
  fn = lj_func_newC(L, (MSize)n, getcurrenv(L));
  fn->c.f = f;
  L->top -= n;
  while (n--)
    copyTV(L, &fn->c.upvalue[n], L->top+n);
  setfuncV(L, L->top, fn);
  lj_assertL(iswhite(obj2gco(fn)), "new GC object is not white");
  incr_top(L);
}

LUA_API void lua_pushboolean(lua_State *L, int b)
{
  setboolV(L->top, (b != 0));
  incr_top(L);
}

LUA_API void lua_pushlightuserdata(lua_State *L, void *p)
{
#if LJ_64
  p = lj_lightud_intern(L, p);
#endif
  setrawlightudV(L->top, p);
  incr_top(L);
}

LUA_API void lua_createtable(lua_State *L, int narray, int nrec)
{
  lj_gc_check(L);
  settabV(L, L->top, lj_tab_new_ah(L, narray, nrec));
  incr_top(L);
}

LUALIB_API int luaL_newmetatable(lua_State *L, const char *tname)
{
  GCtab *regt = tabV(registry(L));
  TValue *tv = lj_tab_setstr(L, regt, lj_str_newz(L, tname));
  if (tvisnil(tv)) {
    GCtab *mt = lj_tab_new(L, 0, 1);
    settabV(L, tv, mt);
    settabV(L, L->top++, mt);
#if LJ_54
    /* Lua 5.4 records the registered metatable name for tostring/errors. */
    setstrV(L, lj_tab_setstr(L, mt, lj_str_newlit(L, "__name")),
	    lj_str_newz(L, tname));
#endif
    lj_gc_anybarriert(L, regt);
    return 1;
  } else {
    copyTV(L, L->top++, tv);
    return 0;
  }
}

LUA_API int lua_pushthread(lua_State *L)
{
  setthreadV(L, L->top, L);
  incr_top(L);
  return (mainthread(G(L)) == L);
}

LUA_API lua_State *lua_newthread(lua_State *L)
{
  lua_State *L1;
  lj_gc_check(L);
  L1 = lj_state_new(L);
  setthreadV(L, L->top, L1);
  incr_top(L);
  return L1;
}

LUA_API void *lua_newuserdata(lua_State *L, size_t size)
{
  GCudata *ud;
  lj_gc_check(L);
  if (size > LJ_MAX_UDATA)
    lj_err_msg(L, LJ_ERR_UDATAOV);
  ud = lj_udata_new(L, (MSize)size, getcurrenv(L));
  setudataV(L, L->top, ud);
  incr_top(L);
  return uddata(ud);
}

LUA_API void *lua_newuserdatauv(lua_State *L, size_t size, int nuvalue)
{
  GCtab *uv;
  GCudata *ud;
  lj_checkapi(nuvalue >= 0, "negative number of user values");
  lj_gc_check(L);
  if (size > LJ_MAX_UDATA)
    lj_err_msg(L, LJ_ERR_UDATAOV);
  uv = lj_tab_new(L, 0, (MSize)nuvalue + 1);
  /* LuaJIT userdata has one environment reference. In Lua 5.4 mode we use
  ** that table to store declared indexed user values and their declared count.
  */
  setintV(lj_tab_setstr(L, uv, lj_str_newlit(L, "__nuv")), nuvalue);
  ud = lj_udata_new(L, (MSize)size, uv);
  setudataV(L, L->top, ud);
  incr_top(L);
  return uddata(ud);
}

LUA_API void lua_concat(lua_State *L, int n)
{
  lj_checkapi_slot(n);
  if (n >= 2) {
    n--;
    do {
      TValue *top = lj_meta_cat(L, L->top-1, -n);
      if (top == NULL) {
	L->top -= n;
	break;
      }
      n -= (int)(L->top - (top - 2*LJ_FR2));
      L->top = top+2;
      lj_vm_call(L, top, 1+1);
      L->top -= 1+LJ_FR2;
      copyTV(L, L->top-1, L->top+LJ_FR2);
    } while (--n > 0);
  } else if (n == 0) {  /* Push empty string. */
    setstrV(L, L->top, &G(L)->strempty);
    incr_top(L);
  }
  /* else n == 1: nothing to do. */
}

static cTValue *api_getmetafield(lua_State *L, cTValue *o, const char *mmname)
{
  GCtab *mt;
  if (tvistab(o))
    mt = tabref(tabV(o)->metatable);
  else if (tvisudata(o))
    mt = tabref(udataV(o)->metatable);
  else
    mt = tabref(basemt_obj(G(L), o));
  if (mt) {
    cTValue *mo = lj_tab_getstr(mt, lj_str_newz(L, mmname));
    if (mo && !tvisnil(mo))
      return mo;
  }
  return NULL;
}

static int api_toint32(cTValue *o, int32_t *ip)
{
  lua_Number n, ni;
  if (tvisint(o)) {
    *ip = intV(o);
    return 1;
  } else if (!tvisnum(o)) {
    return 0;
  }
  n = numV(o);
  if (!(n >= (lua_Number)LUA_MININTEGER && n <= (lua_Number)LUA_MAXINTEGER))
    return 0;
  ni = lj_vm_floor(n);
  if (n != ni)
    return 0;
  *ip = (int32_t)n;
  return 1;
}

static int32_t api_shift32(int32_t a, int32_t sh, int left)
{
  int64_t s = sh;
  if (s < 0) {
    s = -s;
    left = !left;
  }
  if (s >= 32)
    return 0;
  return left ? (int32_t)((uint32_t)a << s) :
		(int32_t)((uint32_t)a >> s);
}

static int api_rawarith(lua_State *L, TValue *res, cTValue *a, cTValue *b,
			int op)
{
  TValue ta, tb;
  int32_t ia, ib;
  lua_Number na, nb, nr;
  copyTV(L, &ta, a);
  copyTV(L, &tb, b);
  switch (op) {
  case LUA_OPBNOT:
    if (!api_toint32(a, &ia))
      return 0;
    setintV(res, (int32_t)~(uint32_t)ia);
    return 1;
  case LUA_OPBAND: case LUA_OPBOR: case LUA_OPBXOR:
  case LUA_OPSHL: case LUA_OPSHR:
    if (!api_toint32(a, &ia) || !api_toint32(b, &ib))
      return 0;
    if (op == LUA_OPBAND)
      setintV(res, (int32_t)((uint32_t)ia & (uint32_t)ib));
    else if (op == LUA_OPBOR)
      setintV(res, (int32_t)((uint32_t)ia | (uint32_t)ib));
    else if (op == LUA_OPBXOR)
      setintV(res, (int32_t)((uint32_t)ia ^ (uint32_t)ib));
    else
      setintV(res, api_shift32(ia, ib, op == LUA_OPSHL));
    return 1;
  default:
    break;
  }
  if (!lj_strscan_numberobj(&ta))
    return 0;
  if (op != LUA_OPUNM && !lj_strscan_numberobj(&tb))
    return 0;
  na = numberVnum(&ta);
  nb = numberVnum(&tb);
  switch (op) {
  case LUA_OPADD: nr = lj_vm_foldarith(na, nb, MM_add-MM_add); break;
  case LUA_OPSUB: nr = lj_vm_foldarith(na, nb, MM_sub-MM_add); break;
  case LUA_OPMUL: nr = lj_vm_foldarith(na, nb, MM_mul-MM_add); break;
  case LUA_OPDIV: nr = lj_vm_foldarith(na, nb, MM_div-MM_add); break;
  case LUA_OPMOD: nr = lj_vm_foldarith(na, nb, MM_mod-MM_add); break;
  case LUA_OPPOW: nr = lj_vm_foldarith(na, nb, MM_pow-MM_add); break;
  case LUA_OPUNM: nr = lj_vm_foldarith(na, na, MM_unm-MM_add); break;
  case LUA_OPIDIV:
    if (nb == 0)
      lj_err_callermsg(L, "attempt to divide by zero");
    nr = lj_vm_floor(na / nb);
    break;
  default:
    return 0;
  }
  if ((op == LUA_OPIDIV || op == LUA_OPUNM) &&
      nr >= (lua_Number)LUA_MININTEGER && nr <= (lua_Number)LUA_MAXINTEGER &&
      nr == lj_vm_floor(nr)) {
    setintptrV(res, (lua_Integer)nr);
  } else {
    setnumV(res, nr);
  }
  return 1;
}

static cTValue *api_arith_meta(lua_State *L, cTValue *a, cTValue *b,
			       int op, int unary)
{
  cTValue *mo = NULL;
  switch (op) {
  case LUA_OPADD: mo = lj_meta_lookup(L, a, MM_add); break;
  case LUA_OPSUB: mo = lj_meta_lookup(L, a, MM_sub); break;
  case LUA_OPMUL: mo = lj_meta_lookup(L, a, MM_mul); break;
  case LUA_OPDIV: mo = lj_meta_lookup(L, a, MM_div); break;
  case LUA_OPMOD: mo = lj_meta_lookup(L, a, MM_mod); break;
  case LUA_OPPOW: mo = lj_meta_lookup(L, a, MM_pow); break;
  case LUA_OPUNM: mo = lj_meta_lookup(L, a, MM_unm); break;
  case LUA_OPIDIV: mo = api_getmetafield(L, a, "__idiv"); break;
  case LUA_OPBAND: mo = api_getmetafield(L, a, "__band"); break;
  case LUA_OPBOR: mo = api_getmetafield(L, a, "__bor"); break;
  case LUA_OPBXOR: mo = api_getmetafield(L, a, "__bxor"); break;
  case LUA_OPSHL: mo = api_getmetafield(L, a, "__shl"); break;
  case LUA_OPSHR: mo = api_getmetafield(L, a, "__shr"); break;
  case LUA_OPBNOT: mo = api_getmetafield(L, a, "__bnot"); break;
  default: break;
  }
  if (mo && !tvisnil(mo))
    return mo;
  if (unary)
    return NULL;
  switch (op) {
  case LUA_OPADD: mo = lj_meta_lookup(L, b, MM_add); break;
  case LUA_OPSUB: mo = lj_meta_lookup(L, b, MM_sub); break;
  case LUA_OPMUL: mo = lj_meta_lookup(L, b, MM_mul); break;
  case LUA_OPDIV: mo = lj_meta_lookup(L, b, MM_div); break;
  case LUA_OPMOD: mo = lj_meta_lookup(L, b, MM_mod); break;
  case LUA_OPPOW: mo = lj_meta_lookup(L, b, MM_pow); break;
  case LUA_OPIDIV: mo = api_getmetafield(L, b, "__idiv"); break;
  case LUA_OPBAND: mo = api_getmetafield(L, b, "__band"); break;
  case LUA_OPBOR: mo = api_getmetafield(L, b, "__bor"); break;
  case LUA_OPBXOR: mo = api_getmetafield(L, b, "__bxor"); break;
  case LUA_OPSHL: mo = api_getmetafield(L, b, "__shl"); break;
  case LUA_OPSHR: mo = api_getmetafield(L, b, "__shr"); break;
  default: break;
  }
  return mo && !tvisnil(mo) ? mo : NULL;
}

static void api_call_arith_meta(lua_State *L, TValue *res, cTValue *a,
				cTValue *b, cTValue *mo)
{
  ptrdiff_t resofs = savestack(L, res);
  ptrdiff_t aofs = savestack(L, a);
  ptrdiff_t bofs = savestack(L, b);
  TValue *top;
  lj_state_checkstack(L, 3);
  res = restorestack(L, resofs);
  a = restorestack(L, aofs);
  b = restorestack(L, bofs);
  top = L->top;
  copyTV(L, top, mo);
  copyTV(L, top+1, a);
  copyTV(L, top+2, b);
  L->top = top+3;
  lua_call(L, 2, 1);
  res = restorestack(L, resofs);
  copyTV(L, res, L->top-1);
  L->top = res+1;
}

LUA_API void lua_arith(lua_State *L, int op)
{
  int unary = (op == LUA_OPUNM || op == LUA_OPBNOT);
  TValue *res, *a, *b;
  cTValue *mo;
  lj_checkapi_slot(unary ? 1 : 2);
  res = L->top - (unary ? 1 : 2);
  a = res;
  b = unary ? res : res+1;
  if (api_rawarith(L, res, a, b, op)) {
    if (!unary)
      L->top--;
    return;
  }
  mo = api_arith_meta(L, a, b, op, unary);
  if (mo) {
    /* Lua 5.4 unary arithmetic metamethods receive the operand twice. */
    api_call_arith_meta(L, res, a, b, mo);
    return;
  }
  lj_err_optype(L, a, LJ_ERR_OPARITH);
}

/* -- Object getters ------------------------------------------------------ */

LUA_API void lua_gettable(lua_State *L, int idx)
{
  cTValue *t = index2adr_check(L, idx);
  cTValue *v = lj_meta_tget(L, t, L->top-1);
  if (v == NULL) {
    L->top += 2;
    lj_vm_call(L, L->top-2, 1+1);
    L->top -= 2+LJ_FR2;
    v = L->top+1+LJ_FR2;
  }
  copyTV(L, L->top-1, v);
}

LUA_API void lua_getfield(lua_State *L, int idx, const char *k)
{
  cTValue *v, *t = index2adr_check(L, idx);
  TValue key;
  setstrV(L, &key, lj_str_newz(L, k));
  v = lj_meta_tget(L, t, &key);
  if (v == NULL) {
    L->top += 2;
    lj_vm_call(L, L->top-2, 1+1);
    L->top -= 2+LJ_FR2;
    v = L->top+1+LJ_FR2;
  }
  copyTV(L, L->top, v);
  incr_top(L);
}

LUA_API void lua_geti(lua_State *L, int idx, lua_Integer n)
{
  idx = lua_absindex(L, idx);
  lua_pushinteger(L, n);
  lua_gettable(L, idx);
}

LUA_API void lua_rawget(lua_State *L, int idx)
{
  cTValue *t = index2adr(L, idx);
  lj_checkapi(tvistab(t), "stack slot %d is not a table", idx);
  copyTV(L, L->top-1, lj_tab_get(L, tabV(t), L->top-1));
}

LUA_API void lua_rawgeti(lua_State *L, int idx, int n)
{
  cTValue *v, *t = index2adr(L, idx);
  lj_checkapi(tvistab(t), "stack slot %d is not a table", idx);
  v = lj_tab_getint(tabV(t), n);
  if (v) {
    copyTV(L, L->top, v);
  } else {
    setnilV(L->top);
  }
  incr_top(L);
}

LUA_API void lua_rawgetp(lua_State *L, int idx, const void *p)
{
  idx = lua_absindex(L, idx);
  lua_pushlightuserdata(L, (void *)p);
  lua_rawget(L, idx);
}

#if LJ_54
LUA_API int lua_gettable54(lua_State *L, int idx)
{
  lua_gettable(L, idx);
  return lua_type(L, -1);
}

LUA_API int lua_getfield54(lua_State *L, int idx, const char *k)
{
  lua_getfield(L, idx, k);
  return lua_type(L, -1);
}

LUA_API int lua_geti54(lua_State *L, int idx, lua_Integer n)
{
  lua_geti(L, idx, n);
  return lua_type(L, -1);
}

LUA_API int lua_rawget54(lua_State *L, int idx)
{
  lua_rawget(L, idx);
  return lua_type(L, -1);
}

LUA_API int lua_rawgeti54(lua_State *L, int idx, lua_Integer n)
{
  lua_rawgeti(L, idx, (int)n);
  return lua_type(L, -1);
}

LUA_API int lua_rawgetp54(lua_State *L, int idx, const void *p)
{
  lua_rawgetp(L, idx, p);
  return lua_type(L, -1);
}

LUA_API int lua_getglobal54(lua_State *L, const char *name)
{
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  lua_getfield(L, -1, name);
  lua_remove(L, -2);
  return lua_type(L, -1);
}

LUA_API void lua_setglobal54(lua_State *L, const char *name)
{
  /* External Lua 5.4 headers hide LUA_GLOBALSINDEX, so set globals through
  ** the registry globals table while leaving LuaJIT's internal ABI unchanged.
  */
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  lua_insert(L, -2);
  lua_setfield(L, -2, name);
  lua_pop(L, 1);
}
#endif

LUA_API int lua_getmetatable(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  GCtab *mt = NULL;
  if (tvistab(o))
    mt = tabref(tabV(o)->metatable);
  else if (tvisudata(o))
    mt = tabref(udataV(o)->metatable);
  else
    mt = tabref(basemt_obj(G(L), o));
  if (mt == NULL)
    return 0;
  settabV(L, L->top, mt);
  incr_top(L);
  return 1;
}

LUALIB_API int luaL_getmetafield(lua_State *L, int idx, const char *field)
{
  if (lua_getmetatable(L, idx)) {
    cTValue *tv = lj_tab_getstr(tabV(L->top-1), lj_str_newz(L, field));
    if (tv && !tvisnil(tv)) {
      copyTV(L, L->top-1, tv);
      return 1;
    }
    L->top--;
  }
  return 0;
}

LUA_API void lua_getfenv(lua_State *L, int idx)
{
  cTValue *o = index2adr_check(L, idx);
  if (tvisfunc(o)) {
    settabV(L, L->top, tabref(funcV(o)->c.env));
  } else if (tvisudata(o)) {
    settabV(L, L->top, tabref(udataV(o)->env));
  } else if (tvisthread(o)) {
    settabV(L, L->top, tabref(threadV(o)->env));
  } else {
    setnilV(L->top);
  }
  incr_top(L);
}

static int api_udata_uvcount(lua_State *L, GCudata *ud)
{
  GCtab *env = tabref(ud->env);
  cTValue *tv = lj_tab_getstr(env, lj_str_newlit(L, "__nuv"));
  return tv && tvisnumber(tv) ? numberVint(tv) : 0;
}

LUA_API int lua_getiuservalue(lua_State *L, int idx, int n)
{
  cTValue *o = index2adr_check(L, idx);
  GCudata *ud;
  cTValue *tv;
  if (!tvisudata(o))
    return LUA_TNONE;
  ud = udataV(o);
  if (n < 1 || n > api_udata_uvcount(L, ud))
    return LUA_TNONE;
  tv = lj_tab_getint(tabref(ud->env), n);
  if (tv)
    copyTV(L, L->top, tv);
  else
    setnilV(L->top);
  incr_top(L);
  return lua_type(L, -1);
}

LUA_API int lua_next(lua_State *L, int idx)
{
  cTValue *t = index2adr(L, idx);
  int more;
  lj_checkapi(tvistab(t), "stack slot %d is not a table", idx);
  more = lj_tab_next(tabV(t), L->top-1, L->top-1);
  if (more > 0) {
    incr_top(L);  /* Return new key and value slot. */
  } else if (!more) {  /* End of traversal. */
    L->top--;  /* Remove key slot. */
  } else {
    lj_err_msg(L, LJ_ERR_NEXTIDX);
  }
  return more;
}

LUA_API const char *lua_getupvalue(lua_State *L, int idx, int n)
{
  cTValue *f = index2adr(L, idx);
  TValue *val;
  GCobj *o;
  const char *name;
#if LJ_54
  if (tvisfunc(f)) {
    GCfunc *fn = funcV(f);
    if (lj_debug_hasenvuv(fn)) {
      if (n == 1) {
	settabV(L, L->top, tabref(fn->c.env));
	incr_top(L);
	return "_ENV";
      }
      n--;
    }
  }
#endif
  if (n <= 0)
    return NULL;
  name = lj_debug_uvnamev(f, (uint32_t)(n-1), &val, &o);
  if (name) {
    copyTV(L, L->top, val);
    incr_top(L);
  }
  return name;
}

LUA_API void *lua_upvalueid(lua_State *L, int idx, int n)
{
  GCfunc *fn = funcV(index2adr(L, idx));
#if LJ_54
  if (lj_debug_hasenvuv(fn)) {
    if (n == 1)
      return (void *)&fn->c.env;
    n--;
  }
#endif
  n--;
  lj_checkapi((uint32_t)n < fn->l.nupvalues, "bad upvalue %d", n);
  return isluafunc(fn) ? (void *)gcref(fn->l.uvptr[n]) :
			 (void *)&fn->c.upvalue[n];
}

LUA_API void lua_upvaluejoin(lua_State *L, int idx1, int n1, int idx2, int n2)
{
  GCfunc *fn1 = funcV(index2adr(L, idx1));
  GCfunc *fn2 = funcV(index2adr(L, idx2));
#if LJ_54
  int env1 = lj_debug_hasenvuv(fn1);
  int env2 = lj_debug_hasenvuv(fn2);
  if (env1 && n1 == 1) {
    GCtab *t = NULL;
    if (env2 && n2 == 1) {
      t = tabref(fn2->c.env);
    } else {
      TValue *tv;
      GCobj *o;
      if (env2) n2--;
      lj_checkapi(isluafunc(fn2), "stack slot %d is not a Lua function", idx2);
      lj_checkapi((uint32_t)(n2-1) < fn2->l.nupvalues,
		  "bad upvalue %d", n2);
      (void)lj_debug_uvnamev(index2adr(L, idx2), (uint32_t)(n2-1), &tv, &o);
      lj_checkapi(tvistab(tv), "source _ENV upvalue is not a table");
      t = tabV(tv);
    }
    setgcref(fn1->c.env, obj2gco(t));
    lj_gc_objbarrier(L, fn1, t);
    return;
  }
  if (env1) n1--;
  if (env2) n2--;
#endif
  n1--; n2--;
  lj_checkapi(isluafunc(fn1), "stack slot %d is not a Lua function", idx1);
  lj_checkapi(isluafunc(fn2), "stack slot %d is not a Lua function", idx2);
  lj_checkapi((uint32_t)n1 < fn1->l.nupvalues, "bad upvalue %d", n1+1);
  lj_checkapi((uint32_t)n2 < fn2->l.nupvalues, "bad upvalue %d", n2+1);
  setgcrefr(fn1->l.uvptr[n1], fn2->l.uvptr[n2]);
  lj_gc_objbarrier(L, fn1, gcref(fn1->l.uvptr[n1]));
}

LUALIB_API void *luaL_testudata(lua_State *L, int idx, const char *tname)
{
  cTValue *o = index2adr(L, idx);
  if (tvisudata(o)) {
    GCudata *ud = udataV(o);
    cTValue *tv = lj_tab_getstr(tabV(registry(L)), lj_str_newz(L, tname));
    if (tv && tvistab(tv) && tabV(tv) == tabref(ud->metatable))
      return uddata(ud);
  }
  return NULL;  /* value is not a userdata with a metatable */
}

LUALIB_API void *luaL_checkudata(lua_State *L, int idx, const char *tname)
{
  void *p = luaL_testudata(L, idx, tname);
  if (!p) lj_err_argtype(L, idx, tname);
  return p;
}

/* -- Object setters ------------------------------------------------------ */

LUA_API void lua_settable(lua_State *L, int idx)
{
  TValue *o;
  cTValue *t = index2adr_check(L, idx);
  lj_checkapi_slot(2);
  o = lj_meta_tset(L, t, L->top-2);
  if (o) {
    /* NOBARRIER: lj_meta_tset ensures the table is not black. */
    L->top -= 2;
    copyTV(L, o, L->top+1);
  } else {
    TValue *base = L->top;
    copyTV(L, base+2, base-3-2*LJ_FR2);
    L->top = base+3;
    lj_vm_call(L, base, 0+1);
    L->top -= 3+LJ_FR2;
  }
}

LUA_API void lua_setfield(lua_State *L, int idx, const char *k)
{
  TValue *o;
  TValue key;
  cTValue *t = index2adr_check(L, idx);
  lj_checkapi_slot(1);
  setstrV(L, &key, lj_str_newz(L, k));
  o = lj_meta_tset(L, t, &key);
  if (o) {
    /* NOBARRIER: lj_meta_tset ensures the table is not black. */
    copyTV(L, o, --L->top);
  } else {
    TValue *base = L->top;
    copyTV(L, base+2, base-3-2*LJ_FR2);
    L->top = base+3;
    lj_vm_call(L, base, 0+1);
    L->top -= 2+LJ_FR2;
  }
}

LUA_API void lua_seti(lua_State *L, int idx, lua_Integer n)
{
  idx = lua_absindex(L, idx);
  lua_pushinteger(L, n);
  lua_insert(L, -2);
  lua_settable(L, idx);
}

LUA_API void lua_rawset(lua_State *L, int idx)
{
  GCtab *t = tabV(index2adr(L, idx));
  TValue *dst, *key;
  lj_checkapi_slot(2);
  key = L->top-2;
  dst = lj_tab_set(L, t, key);
  copyTV(L, dst, key+1);
  lj_gc_anybarriert(L, t);
  L->top = key;
}

LUA_API void lua_rawseti(lua_State *L, int idx, int n)
{
  GCtab *t = tabV(index2adr(L, idx));
  TValue *dst, *src;
  lj_checkapi_slot(1);
  dst = lj_tab_setint(L, t, n);
  src = L->top-1;
  copyTV(L, dst, src);
  lj_gc_barriert(L, t, dst);
  L->top = src;
}

#if LJ_54
LUA_API void lua_rawseti54(lua_State *L, int idx, lua_Integer n)
{
  /* Lua 5.4 exposes lua_Integer here. The current compatibility integer range
  ** is still 32 bit, so the wrapper preserves the external signature while
  ** delegating to LuaJIT's existing integer table slot helper.
  */
  lua_rawseti(L, idx, (int)n);
}
#endif

LUA_API void lua_rawsetp(lua_State *L, int idx, const void *p)
{
  idx = lua_absindex(L, idx);
  lua_pushlightuserdata(L, (void *)p);
  lua_insert(L, -2);
  lua_rawset(L, idx);
}

LUA_API int lua_setmetatable(lua_State *L, int idx)
{
  global_State *g;
  GCtab *mt;
  cTValue *o = index2adr_check(L, idx);
  lj_checkapi_slot(1);
  if (tvisnil(L->top-1)) {
    mt = NULL;
  } else {
    lj_checkapi(tvistab(L->top-1), "top stack slot is not a table");
    mt = tabV(L->top-1);
  }
  g = G(L);
  if (tvistab(o)) {
    GCtab *t = tabV(o);
    setgcref(t->metatable, obj2gco(mt));
    if (mt) {
      lj_gc_objbarriert(L, t, mt);
#if LJ_54
      /* Lua 5.4 only finalizes tables whose metatable already had __gc when
      ** the metatable was assigned. The actual __gc function is still looked
      ** up when the table is finalized, matching changed/removed methods.
      */
      {
	cTValue *gc = lj_tab_getstr(mt, mmname_str(g, MM_gc));
	if (gc && !tvisnil(gc))
	  t->flags54 |= LJ_TAB_HAS_GC;
      }
#endif
    }
  } else if (tvisudata(o)) {
    setgcref(udataV(o)->metatable, obj2gco(mt));
    if (mt)
      lj_gc_objbarrier(L, udataV(o), mt);
  } else {
    /* Flush cache, since traces specialize to basemt. But not during __gc. */
    if (lj_trace_flushall(L))
      lj_err_caller(L, LJ_ERR_NOGCMM);
    o = index2adr(L, idx);  /* Stack may have been reallocated. */
    if (tvisbool(o)) {
      /* NOBARRIER: basemt is a GC root. */
      setgcref(basemt_it(g, LJ_TTRUE), obj2gco(mt));
      setgcref(basemt_it(g, LJ_TFALSE), obj2gco(mt));
    } else {
      /* NOBARRIER: basemt is a GC root. */
      setgcref(basemt_obj(g, o), obj2gco(mt));
    }
  }
  L->top--;
  return 1;
}

LUALIB_API void luaL_setmetatable(lua_State *L, const char *tname)
{
  lua_getfield(L, LUA_REGISTRYINDEX, tname);
  lua_setmetatable(L, -2);
}

LUA_API int lua_setiuservalue(lua_State *L, int idx, int n)
{
  cTValue *o;
  GCudata *ud;
  GCtab *env;
  TValue *tv;
  idx = lua_absindex(L, idx);
  lj_checkapi_slot(1);
  o = index2adr_check(L, idx);
  if (!tvisudata(o)) {
    L->top--;
    return 0;
  }
  ud = udataV(o);
  if (n < 1 || n > api_udata_uvcount(L, ud)) {
    L->top--;
    return 0;
  }
  env = tabref(ud->env);
  tv = lj_tab_setint(L, env, n);
  copyTV(L, tv, L->top-1);
  lj_gc_barriert(L, env, tv);
  L->top--;
  return 1;
}

LUA_API int lua_setfenv(lua_State *L, int idx)
{
  cTValue *o = index2adr_check(L, idx);
  GCtab *t;
  lj_checkapi_slot(1);
  lj_checkapi(tvistab(L->top-1), "top stack slot is not a table");
  t = tabV(L->top-1);
  if (tvisfunc(o)) {
    setgcref(funcV(o)->c.env, obj2gco(t));
  } else if (tvisudata(o)) {
    setgcref(udataV(o)->env, obj2gco(t));
  } else if (tvisthread(o)) {
    setgcref(threadV(o)->env, obj2gco(t));
  } else {
    L->top--;
    return 0;
  }
  lj_gc_objbarrier(L, gcV(o), t);
  L->top--;
  return 1;
}

LUA_API const char *lua_setupvalue(lua_State *L, int idx, int n)
{
  cTValue *f = index2adr(L, idx);
  TValue *val;
  GCobj *o;
  const char *name;
  lj_checkapi_slot(1);
#if LJ_54
  if (tvisfunc(f)) {
    GCfunc *fn = funcV(f);
    if (lj_debug_hasenvuv(fn)) {
      if (n == 1) {
	if (!tvistab(L->top-1))
	  return NULL;  /* Full non-table _ENV needs a real upvalue slot. */
	setgcref(fn->c.env, obj2gco(tabV(L->top-1)));
	lj_gc_objbarrier(L, fn, tabV(L->top-1));
	L->top--;
	return "_ENV";
      }
      n--;
    }
  }
#endif
  if (n <= 0)
    return NULL;
  name = lj_debug_uvnamev(f, (uint32_t)(n-1), &val, &o);
  if (name) {
    L->top--;
    copyTV(L, val, L->top);
    lj_gc_barrier(L, o, L->top);
  }
  return name;
}

/* -- Calls --------------------------------------------------------------- */

#if LJ_FR2
static TValue *api_call_base(lua_State *L, int nargs)
{
  TValue *o = L->top, *base = o - nargs;
  L->top = o+1;
  for (; o > base; o--) copyTV(L, o, o-1);
  setnilV(o);
  return o+1;
}
#else
#define api_call_base(L, nargs)	(L->top - (nargs))
#endif

LUA_API void lua_call(lua_State *L, int nargs, int nresults)
{
  lj_checkapi(L->status == LUA_OK || L->status == LUA_ERRERR,
	      "thread called in wrong state %d", L->status);
  lj_checkapi_slot(nargs+1);
  lj_vm_call(L, api_call_base(L, nargs), nresults+1);
}

LUA_API int lua_pcall(lua_State *L, int nargs, int nresults, int errfunc)
{
  global_State *g = G(L);
  uint8_t oldh = hook_save(g);
  ptrdiff_t ef;
  int status;
  lj_checkapi(L->status == LUA_OK || L->status == LUA_ERRERR,
	      "thread called in wrong state %d", L->status);
  lj_checkapi_slot(nargs+1);
  if (errfunc == 0) {
    ef = 0;
  } else {
    cTValue *o = index2adr_stack(L, errfunc);
    ef = savestack(L, o);
  }
  status = lj_vm_pcall(L, api_call_base(L, nargs), nresults+1, ef);
  if (status) hook_restore(g, oldh);
  return status;
}

static TValue *cpcall(lua_State *L, lua_CFunction func, void *ud)
{
  GCfunc *fn = lj_func_newC(L, 0, getcurrenv(L));
  TValue *top = L->top;
  fn->c.f = func;
  setfuncV(L, top++, fn);
  if (LJ_FR2) setnilV(top++);
#if LJ_64
  ud = lj_lightud_intern(L, ud);
#endif
  setrawlightudV(top++, ud);
  cframe_nres(L->cframe) = 1+0;  /* Zero results. */
  L->top = top;
  return top-1;  /* Now call the newly allocated C function. */
}

LUA_API int lua_cpcall(lua_State *L, lua_CFunction func, void *ud)
{
  global_State *g = G(L);
  uint8_t oldh = hook_save(g);
  int status;
  lj_checkapi(L->status == LUA_OK || L->status == LUA_ERRERR,
	      "thread called in wrong state %d", L->status);
  status = lj_vm_cpcall(L, func, ud, cpcall);
  if (status) hook_restore(g, oldh);
  return status;
}

LUALIB_API int luaL_callmeta(lua_State *L, int idx, const char *field)
{
  if (luaL_getmetafield(L, idx, field)) {
    TValue *top = L->top--;
    if (LJ_FR2) setnilV(top++);
    copyTV(L, top++, index2adr(L, idx));
    L->top = top;
    lj_vm_call(L, top-1, 1+1);
    return 1;
  }
  return 0;
}

/* -- Coroutine yield and resume ------------------------------------------ */

LUA_API int lua_isyieldable(lua_State *L)
{
  return cframe_canyield(L->cframe);
}

LUA_API int lua_yield(lua_State *L, int nresults)
{
  void *cf = L->cframe;
  global_State *g = G(L);
  if (cframe_canyield(cf)) {
    cf = cframe_raw(cf);
    if (!hook_active(g)) {  /* Regular yield: move results down if needed. */
      cTValue *f = L->top - nresults;
      if (f > L->base) {
	TValue *t = L->base;
	while (--nresults >= 0) copyTV(L, t++, f++);
	L->top = t;
      }
      L->cframe = NULL;
      L->status = LUA_YIELD;
      return -1;
    } else {  /* Yield from hook: add a pseudo-frame. */
      TValue *top = L->top;
      hook_leave(g);
      (top++)->u64 = cframe_multres(cf);
      setcont(top, lj_cont_hook);
      if (LJ_FR2) top++;
      setframe_pc(top, cframe_pc(cf)-1);
      top++;
      setframe_gc(top, obj2gco(L), LJ_TTHREAD);
      if (LJ_FR2) top++;
      setframe_ftsz(top, ((char *)(top+1)-(char *)L->base)+FRAME_CONT);
      L->top = L->base = top+1;
#if ((defined(__GNUC__) || defined(__clang__)) && (LJ_TARGET_X64 || defined(LUAJIT_UNWIND_EXTERNAL)) && !LJ_NO_UNWIND) || LJ_TARGET_WINDOWS
      lj_err_throw(L, LUA_YIELD);
#else
      L->cframe = NULL;
      L->status = LUA_YIELD;
      lj_vm_unwind_c(cf, LUA_YIELD);
#endif
    }
  }
  lj_err_msg(L, LJ_ERR_CYIELD);
  return 0;  /* unreachable */
}

LUA_API int lua_resume(lua_State *L, int nargs)
{
  if (L->cframe == NULL && L->status <= LUA_YIELD)
    return lj_vm_resume(L,
      L->status == LUA_OK ? api_call_base(L, nargs) : L->top - nargs,
      0, 0);
  L->top = L->base;
  setstrV(L, L->top, lj_err_str(L, LJ_ERR_COSUSP));
  incr_top(L);
  return LUA_ERRRUN;
}

#if LJ_54
LUA_API int lua_resume54(lua_State *L, lua_State *from, int nargs,
			 int *nresults)
{
  int status;
  (void)from;
  /* The VM still implements LuaJIT's legacy resume ABI. This wrapper exposes
  ** Lua 5.4's result-count out parameter without changing the internal ABI.
  */
  status = lua_resume(L, nargs);
  if (nresults)
    *nresults = (status == LUA_OK || status == LUA_YIELD) ? lua_gettop(L) : 0;
  return status;
}
#endif

LUA_API int lua_resetthread(lua_State *L)
{
  /* Full Lua 5.4 reset closes to-be-closed variables. This compatibility path
  ** covers LuaJIT coroutines without <close> state by clearing frames, open
  ** upvalues and status back to a fresh suspended stack.
  */
  lj_func_closeuv(L, tvref(L->stack));
  L->status = LUA_OK;
  L->cframe = NULL;
  L->base = L->top = tvref(L->stack) + 1 + LJ_FR2;
  return LUA_OK;
}

#if LJ_54
LUA_API int lua_closethread(lua_State *L, lua_State *from)
{
  (void)from;
  /* Full lua_closethread() must close pending <close> slots. Until the VM has
  ** that state, the no-<close> path is equivalent to lua_resetthread().
  */
  return lua_resetthread(L);
}
#endif

/* -- GC and memory management -------------------------------------------- */

#if LJ_54
static MSize gc_param_lua54(int data)
{
  /* Lua 5.4 stores public GC parameters in 4-point units and clamps the
  ** exposed range to 0..1000. Keep that surface even though LuaJIT's GC is
  ** still the underlying collector.
  */
  if (data <= 0)
    return 0;
  if (data >= 1000)
    return 1000;
  return (MSize)(data & ~3);
}
#endif

#if LJ_54
LUA_API int lua_gc(lua_State *L, int what, ...)
#else
LUA_API int lua_gc(lua_State *L, int what, int data)
#endif
{
  global_State *g = G(L);
  int res = 0;
#if LJ_54
  int data = 0;
  va_list argp;
  va_start(argp, what);
  /* Lua 5.4 exposes lua_gc() as a vararg API. The LuaJIT collector still
  ** ignores GEN/INC tuning parameters, so only options that use the legacy
  ** single integer argument need to consume one here.
  */
  switch (what) {
  case LUA_GCSTEP:
  case LUA_GCSETPAUSE:
  case LUA_GCSETSTEPMUL:
    data = va_arg(argp, int);
    break;
  default:
    break;
  }
  va_end(argp);
#endif
  switch (what) {
  case LUA_GCSTOP:
    g->gc.threshold = LJ_MAX_MEM;
    break;
  case LUA_GCRESTART:
    g->gc.threshold = data == -1 ? (g->gc.total/100)*g->gc.pause : g->gc.total;
    break;
  case LUA_GCCOLLECT:
    lj_gc_fullgc(L);
    break;
  case LUA_GCCOUNT:
    res = (int)(g->gc.total >> 10);
    break;
  case LUA_GCCOUNTB:
    res = (int)(g->gc.total & 0x3ff);
    break;
  case LUA_GCSTEP: {
    GCSize a = (GCSize)data << 10;
    g->gc.threshold = (a <= g->gc.total) ? (g->gc.total - a) : 0;
    while (g->gc.total >= g->gc.threshold)
      if (lj_gc_step(L) > 0) {
	res = 1;
	break;
      }
    break;
  }
  case LUA_GCSETPAUSE:
    res = (int)(g->gc.pause);
#if LJ_54
    g->gc.pause = gc_param_lua54(data);
#else
    g->gc.pause = (MSize)data;
#endif
    break;
  case LUA_GCSETSTEPMUL:
    res = (int)(g->gc.stepmul);
#if LJ_54
    g->gc.stepmul = gc_param_lua54(data);
#else
    g->gc.stepmul = (MSize)data;
#endif
    break;
  case LUA_GCISRUNNING:
    res = (g->gc.threshold != LJ_MAX_MEM);
    break;
  case LUA_GCGEN:
    /* This reports the Lua 5.4 mode surface; LuaJIT's collector is unchanged. */
    res = g->gc_mode54 ? LUA_GCGEN : LUA_GCINC;
    g->gc_mode54 = 1;
    break;
  case LUA_GCINC:
    res = g->gc_mode54 ? LUA_GCGEN : LUA_GCINC;
    g->gc_mode54 = 0;
    break;
  default:
    res = -1;  /* Invalid option. */
  }
  return res;
}

LUA_API void lua_setwarnf(lua_State *L, lua_WarnFunction f, void *ud)
{
  G(L)->warnf = f;
  G(L)->warnud = ud;
}

LUA_API void lua_warning(lua_State *L, const char *msg, int tocont)
{
  global_State *g = G(L);
  if (g->warnf) {
    g->warnf(g->warnud, msg, tocont);
  } else if (msg && msg[0] == '@') {
    if (strcmp(msg, "@on") == 0) {
      g->warn_on = 1;
      g->warn_cont = 0;
    } else if (strcmp(msg, "@off") == 0) {
      g->warn_on = 0;
      g->warn_cont = 0;
    }
  } else if (g->warn_on && msg) {
    /* Lua 5.4's default warning function prefixes each fresh warning, but
    ** not continuation pieces emitted with tocont=true.
    */
    if (!g->warn_cont)
      fputs("Lua warning: ", stderr);
    fputs(msg, stderr);
    g->warn_cont = (uint8_t)tocont;
    if (!tocont)
      fputc('\n', stderr);
  }
}

LUA_API lua_Alloc lua_getallocf(lua_State *L, void **ud)
{
  global_State *g = G(L);
  if (ud) *ud = g->allocd;
  return g->allocf;
}

LUA_API void lua_setallocf(lua_State *L, lua_Alloc f, void *ud)
{
  global_State *g = G(L);
  g->allocd = ud;
  g->allocf = f;
}

