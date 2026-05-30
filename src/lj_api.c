/*
** Public Lua/C API.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_api_c
#define LUA_CORE

#include <limits.h>
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
#include "lj_close.h"
#include "lj_dispatch.h"
#include "lj_bc.h"
#include "lj_frame.h"
#include "lj_trace.h"
#include "lj_vm.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"

#if LJ_54
#if (defined(__ELF__) || defined(__MACH__) || defined(__psp2__)) && \
    !((defined(__sun__) && defined(__svr4__)) || defined(__CELLOS_LV2__))
/* LUA_API expands to extern+visibility on ELF/Mach-O. That form is fine for
** function declarations, but Clang warns when it is used on a data definition.
*/
__attribute__((visibility("default"))) const char lua_ident[] =
#else
LUA_API const char lua_ident[] =
#endif
  "$LuaVersion: " LUA_COPYRIGHT " $"
  "$LuaAuthors: " LUA_AUTHORS " $";
#endif

/* -- Common helper functions --------------------------------------------- */

#define lj_checkapi_slot(idx) \
  lj_checkapi((idx) <= (L->top - L->base), "stack slot %d out of range", (idx))

static void api_checknelems(lua_State *L, int n)
{
  if (n < 0 || L->top - L->base < n)
    lj_err_msg(L, LJ_ERR_BADVAL);
  lj_checkapi_slot(n);
}

static void api_checkcallargs(lua_State *L, int nargs, int nresults)
{
  if (nargs < 0 || nargs == INT_MAX || nresults < LUA_MULTRET ||
      (nresults != LUA_MULTRET && nresults > LUAI_MAXSTACK))
    lj_err_msg(L, LJ_ERR_BADVAL);
  api_checknelems(L, nargs+1);
}

#if LJ_54
#if LUAI_IS32INT
#define LJ_54_REGISTRYINDEX	(-1000000 - 1000)
#else
#define LJ_54_REGISTRYINDEX	(-15000 - 1000)
#endif
#define LJ_54_LJ_REGISTRYINDEX	(-LUAI_MAXSTACK - 1000)
#endif

static TValue *index2adr(lua_State *L, int idx)
{
  if (idx > 0) {
    TValue *o = L->base + (idx - 1);
    return o < L->top ? o : niltv(L);
  } else if (idx > LUA_REGISTRYINDEX) {
    if (idx == 0 || -idx > L->top - L->base)
      lj_err_msg(L, LJ_ERR_BADVAL);
    lj_checkapi(idx != 0 && -idx <= L->top - L->base,
		"bad stack slot %d", idx);
    return L->top + idx;
#if LJ_54
  } else if (idx == LJ_54_REGISTRYINDEX || idx == LJ_54_LJ_REGISTRYINDEX) {
    /* External Lua 5.4 headers use a different registry pseudo-index formula.
    ** Accept it here without changing LuaJIT's internal/default ABI values.
    */
    return registry(L);
  } else if (idx < LJ_54_REGISTRYINDEX) {
    GCfunc *fn = curr_func(L);
    lj_checkapi(fn->c.gct == ~LJ_TFUNC && !isluafunc(fn),
		"calling frame is not a C function");
    idx = LJ_54_REGISTRYINDEX - idx;
    return idx <= fn->c.nupvalues ? &fn->c.upvalue[idx-1] : niltv(L);
  } else if (idx < LJ_54_LJ_REGISTRYINDEX) {
    GCfunc *fn = curr_func(L);
    /* Accept modules compiled against earlier compat headers from this branch;
    ** they used LuaJIT's internal LUAI_MAXSTACK in the official 5.4 formula.
    */
    lj_checkapi(fn->c.gct == ~LJ_TFUNC && !isluafunc(fn),
		"calling frame is not a C function");
    idx = LJ_54_LJ_REGISTRYINDEX - idx;
    return idx <= fn->c.nupvalues ? &fn->c.upvalue[idx-1] : niltv(L);
#endif
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

static cTValue *api_checktable(lua_State *L, int idx)
{
  cTValue *o;
  if (idx > 0) {
    o = L->base + (idx - 1);
    if (o >= L->top)
      lj_err_msg(L, LJ_ERR_BADVAL);
  } else if (idx > LUA_REGISTRYINDEX) {
    if (idx == 0 || -idx > L->top - L->base)
      lj_err_msg(L, LJ_ERR_BADVAL);
    o = L->top + idx;
  } else {
    o = index2adr(L, idx);
  }
  if (!tvistab(o))
    lj_err_msg(L, LJ_ERR_BADVAL);
  lj_checkapi(tvistab(o), "stack slot %d is not a table", idx);
  return o;
}

static LJ_AINLINE TValue *index2adr_check(lua_State *L, int idx)
{
  TValue *o = index2adr(L, idx);
  lj_checkapi(o != niltv(L), "invalid stack slot %d", idx);
  return o;
}

static LJ_AINLINE TValue *index2adr_valid(lua_State *L, int idx)
{
  if (idx > 0) {
    TValue *o = L->base + (idx - 1);
    if (o >= L->top)
      lj_err_msg(L, LJ_ERR_BADVAL);
    return o;
  } else if (idx > LUA_REGISTRYINDEX) {
    if (idx == 0 || -idx > L->top - L->base)
      lj_err_msg(L, LJ_ERR_BADVAL);
    return L->top + idx;
  } else {
    TValue *o = index2adr(L, idx);
    if (o == niltv(L))
      lj_err_msg(L, LJ_ERR_BADVAL);
    return o;
  }
}

static TValue *index2adr_stack(lua_State *L, int idx)
{
  if (idx > 0) {
    TValue *o = L->base + (idx - 1);
    if (o >= L->top)
      lj_err_msg(L, LJ_ERR_BADVAL);
    return o;
  } else {
    if (idx == 0 || -idx > L->top - L->base)
      lj_err_msg(L, LJ_ERR_BADVAL);
    lj_checkapi(idx != 0 && -idx <= L->top - L->base,
		"invalid stack slot %d", idx);
    return L->top + idx;
  }
}

#if LJ_54
#define api_tvisnumber(o)	(tvisnumber(o) || tvisi64(o))
#else
#define api_tvisnumber(o)	tvisnumber(o)
#endif

#if LJ_54
static LJ_AINLINE int api_integerV54(cTValue *o, lua_Integer *ip)
{
  if (tvisint(o)) {
    *ip = (lua_Integer)intV(o);
    return 1;
  } else if (tvisi64(o)) {
    *ip = (lua_Integer)i64V(o);
    return 1;
  }
  return 0;
}

static int api_numeq_i64(lua_Number n, lua_Integer i)
{
  int64_t k;
  if (!(n >= (-9223372036854775807.0 - 1.0) && n < 9223372036854775808.0))
    return 0;
  k = lj_num2i64(n);
  return k == (int64_t)i && (lua_Number)k == n;
}

static int api_i64lt_num(lua_Integer i, lua_Number n)
{
  lua_Number nf;
  int64_t k;
  if (!(n == n))
    return 0;
  if (n <= (-9223372036854775807.0 - 1.0))
    return 0;
  if (n >= 9223372036854775808.0)
    return 1;
  nf = lj_vm_floor(n);
  k = lj_num2i64(nf);
  return (int64_t)i < k || ((int64_t)i == k && nf < n);
}

static int api_numlt_i64(lua_Number n, lua_Integer i)
{
  lua_Number nf;
  int64_t k;
  if (!(n == n))
    return 0;
  if (n < (-9223372036854775807.0 - 1.0))
    return 1;
  if (n == (-9223372036854775807.0 - 1.0))
    return (int64_t)i > (-9223372036854775807LL - 1LL);
  if (n >= 9223372036854775808.0)
    return 0;
  nf = lj_vm_floor(n);
  k = lj_num2i64(nf);
  return k < (int64_t)i;
}

static int api_numeq54(cTValue *o1, cTValue *o2)
{
  lua_Integer i1, i2;
  if (api_integerV54(o1, &i1)) {
    return api_integerV54(o2, &i2) ? i1 == i2 : api_numeq_i64(numV(o2), i1);
  } else if (api_integerV54(o2, &i2)) {
    return api_numeq_i64(numV(o1), i2);
  }
  return numberVnum(o1) == numberVnum(o2);
}

static int api_numlt54(cTValue *o1, cTValue *o2)
{
  lua_Integer i1, i2;
  if (api_integerV54(o1, &i1)) {
    return api_integerV54(o2, &i2) ? i1 < i2 : api_i64lt_num(i1, numV(o2));
  } else if (api_integerV54(o2, &i2)) {
    return api_numlt_i64(numV(o1), i2);
  }
  return numberVnum(o1) < numberVnum(o2);
}

static int api_numle54(cTValue *o1, cTValue *o2)
{
  return api_numeq54(o1, o2) || api_numlt54(o1, o2);
}
#endif

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

LUA_API void lua_setlevel(lua_State *from, lua_State *to)
{
  /* Lua 5.1 kept this exported as a debug-library compatibility hook. LuaJIT
  ** does not expose Lua's old C-call depth counter, but default ABI users still
  ** need the symbol to link.
  */
  UNUSED(from);
  UNUSED(to);
}

LUA_API int lua_checkstack(lua_State *L, int size)
{
  if (size < 0 || size > LUAI_MAXCSTACK ||
      (L->top - L->base + size) > LUAI_MAXCSTACK) {
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
  if (!lua_checkstack(L, size)) {
#if LJ_54
    if (msg == NULL)
      /* Lua 5.4 treats a NULL auxiliary message as no extra text; avoid the
      ** old formatted "(null)" suffix in stack overflow diagnostics.
      */
      lj_err_caller(L, LJ_ERR_STKOV);
#endif
    lj_err_callerv(L, LJ_ERR_STKOVM, msg);
  }
}

LUA_API void lua_xmove(lua_State *L, lua_State *to, int n)
{
  TValue *f, *t;
  if (n < 0 || n > (int)(L->top - L->base))
    lj_err_msg(L, LJ_ERR_BADVAL);
  if (L == to) return;
  if (G(L) != G(to))
    lj_err_msg(L, LJ_ERR_BADVAL);
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

#if LJ_54
LUA_API lua_Number lua_version54(lua_State *L)
{
  /* Keep LuaJIT's pointer-returning lua_version() ABI for internal/default
  ** callers, while external Lua 5.4 headers expose the official value return.
  */
  return *lua_version(L);
}
#endif

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
  if (idx == 0 || -idx > L->top - L->base)
    lj_err_msg(L, LJ_ERR_BADVAL);
  return (int)(L->top - L->base) + idx + 1;
}

#if LJ_54
static TValue *api_close_popped(lua_State *L, TValue *newtop)
{
  if (newtop < L->top && L->closelist != NULL) {
    ptrdiff_t newtopofs = savestack(L, newtop);
    /* Lua 5.4 closes marked C API stack slots before they are removed by
    ** lua_settop()/lua_pop(). The close-list bridge keeps stack-slot offsets,
    ** so close before changing L->top, then restore the requested new top.
    ** The close call may grow/reallocate the stack through lua_pcall(), so the
    ** target top must be kept as an offset rather than a raw TValue pointer.
    */
    int status = lj_close_unwind_status(L, newtop, LUA_OK);
    if (status != LUA_OK)
      lua_error(L);
    newtop = restorestack(L, newtopofs);
    L->top = newtop;
    /* Closing popped C API slots runs an internal protected call on the same
    ** stack segment. Finish the pending cycle at a stable top so later GC
    ** steps and yieldable close continuations cannot observe half-dead frame
    ** residue from the internal pcall.
    */
    if (G(L)->gc.threshold != LJ_MAX_MEM)
      lj_gc_fullgc(L);
  }
  return newtop;
}
#endif

LUA_API void lua_settop(lua_State *L, int idx)
{
  if (idx >= 0) {
    TValue *newtop;
    if (idx > LUAI_MAXCSTACK)
      lj_err_msg(L, LJ_ERR_BADVAL);
    lj_checkapi(idx <= tvref(L->maxstack) - L->base, "bad stack slot %d", idx);
    newtop = L->base + idx;
    if (newtop > L->top) {
      if (newtop >= tvref(L->maxstack))
	lj_state_growstack(L, (MSize)idx - (MSize)(L->top - L->base));
      do { setnilV(L->top++); } while (L->top < newtop);
    } else {
#if LJ_54
      newtop = api_close_popped(L, newtop);
#endif
      L->top = newtop;
    }
  } else {
    TValue *newtop;
    if (-(idx+1) > (L->top - L->base))
      lj_err_msg(L, LJ_ERR_BADVAL);
    lj_checkapi(-(idx+1) <= (L->top - L->base), "bad stack slot %d", idx);
    newtop = L->top + idx+1;  /* Shrinks top (idx < 0). */
#if LJ_54
    newtop = api_close_popped(L, newtop);
#endif
    L->top = newtop;
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
  unsigned int nn = n < 0 ? (unsigned int)(-(n + 1)) + 1u : (unsigned int)n;
  if (nn > (unsigned int)nslots)
    lj_err_msg(L, LJ_ERR_BADVAL);
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
    if (!tvistab(f))
      lj_err_msg(L, LJ_ERR_BADVAL);
    lj_checkapi(tvistab(f), "stack slot %d is not a table", idx);
    /* NOBARRIER: A thread (i.e. L) is never black. */
    setgcref(L->env, obj2gco(tabV(f)));
  } else if (idx == LUA_ENVIRONINDEX) {
    GCfunc *fn = curr_func(L);
    if (fn->c.gct != ~LJ_TFUNC)
      lj_err_msg(L, LJ_ERR_NOENV);
    if (!tvistab(f))
      lj_err_msg(L, LJ_ERR_BADVAL);
    lj_checkapi(tvistab(f), "stack slot %d is not a table", idx);
    setgcref(fn->c.env, obj2gco(tabV(f)));
    lj_gc_barrier(L, fn, f);
  } else {
    TValue *o = index2adr_valid(L, idx);
    copyTV(L, o, f);
    if (idx < LUA_GLOBALSINDEX)  /* Need a barrier for upvalues. */
      lj_gc_barrier(L, curr_func(L), f);
  }
}

LUA_API void lua_replace(lua_State *L, int idx)
{
  api_checknelems(L, 1);
  copy_slot(L, L->top - 1, idx);
  L->top--;
}

LUA_API void lua_copy(lua_State *L, int fromidx, int toidx)
{
  copy_slot(L, index2adr_valid(L, fromidx), toidx);
}

LUA_API void lua_pushvalue(lua_State *L, int idx)
{
  copyTV(L, L->top, index2adr_valid(L, idx));
  incr_top(L);
}

/* -- Stack getters ------------------------------------------------------- */

LUA_API int lua_type(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  if (api_tvisnumber(o)) {
    return LUA_TNUMBER;
#if LJ_64 && !LJ_GC64
  } else if (tvislightud(o)) {
    return LUA_TLIGHTUSERDATA;
#endif
  } else if (o == niltv(L)) {
    return LUA_TNONE;
  } else {  /* Magic internal/external tag conversion. ORDER LJ_T */
    uint32_t t = ~itype(o);
    switch (t) {
    case ~LJ_TNIL: return LUA_TNIL;
    case ~LJ_TFALSE:
    case ~LJ_TTRUE: return LUA_TBOOLEAN;
    case ~LJ_TLIGHTUD: return LUA_TLIGHTUSERDATA;
    case ~LJ_TSTR: return LUA_TSTRING;
    case ~LJ_TFUNC: return LUA_TFUNCTION;
    case ~LJ_TCDATA: return LUA_TCDATA;
    case ~LJ_TTAB: return LUA_TTABLE;
    case ~LJ_TUDATA: return LUA_TUSERDATA;
    case ~LJ_TTHREAD: return LUA_TTHREAD;
    case ~LJ_TPROTO: return LUA_TPROTO;
    default:
      lj_assertL(0, "bad tag conversion");
      return LUA_TNIL;
    }
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
  if (t < LUA_TNONE || t > LUA_TCDATA)
    lj_err_msg(L, LJ_ERR_BADVAL);
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
  return (api_tvisnumber(o) ||
	  (tvisstr(o) && lj_strscan_number(strV(o), &tmp)));
}

LUA_API int lua_isinteger(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
#if LJ_54 && LJ_DUALNUM
  /* Lua 5.4's C API observes the TValue integer/float subtype, not merely
  ** whether a float has an exact integer representation.
  */
  return tvisinteger(o);
#else
  if (tvisint(o))
    return 1;
  if (tvisnum(o)) {
    lua_Number n = numV(o);
    lua_Number ni = lj_vm_floor(n);
    return n == ni && n >= (lua_Number)LUA_MININTEGER &&
	   n <= (lua_Number)LUA_MAXINTEGER;
  }
  return 0;
#endif
}

LUA_API int lua_isstring(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return (tvisstr(o) || api_tvisnumber(o));
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
#if LJ_54
  if (api_tvisnumber(o1) && api_tvisnumber(o2)) {
    return api_numeq54(o1, o2);
  } else
#endif
  if (tvisint(o1) && tvisint(o2)) {
    return intV(o1) == intV(o2);
  } else if (api_tvisnumber(o1) && api_tvisnumber(o2)) {
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
#if LJ_54
  } else if (api_tvisnumber(o1) && api_tvisnumber(o2)) {
    return api_numlt54(o1, o2);
#endif
  } else if (tvisint(o1) && tvisint(o2)) {
    return intV(o1) < intV(o2);
  } else if (api_tvisnumber(o1) && api_tvisnumber(o2)) {
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
#if LJ_54
  } else if (api_tvisnumber(o1) && api_tvisnumber(o2)) {
    return api_numle54(o1, o2);
#endif
  } else if (tvisint(o1) && tvisint(o2)) {
    return intV(o1) <= intV(o2);
  } else if (api_tvisnumber(o1) && api_tvisnumber(o2)) {
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
  cTValue *o1 = index2adr(L, idx1);
  cTValue *o2 = index2adr(L, idx2);
  if (o1 == niltv(L) || o2 == niltv(L))
    return 0;
  switch (op) {
  case LUA_OPEQ:
    return lua_equal(L, idx1, idx2);
  case LUA_OPLT:
    return lua_lessthan(L, idx1, idx2);
  case LUA_OPLE:
    return api_lessequal(L, idx1, idx2);
  default:
    lj_err_msg(L, LJ_ERR_BADVAL);
    lj_checkapi(0, "invalid comparison op %d", op);
    return 0;
  }
}

LUA_API lua_Number lua_tonumber(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  if (LJ_LIKELY(api_tvisnumber(o)))
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
  if (LJ_LIKELY(api_tvisnumber(o))) {
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
  if (LJ_LIKELY(api_tvisnumber(o)))
    return numberVnum(o);
  else if (!(tvisstr(o) && lj_strscan_num(strV(o), &tmp)))
    lj_err_argt(L, idx, LUA_TNUMBER);
  return numV(&tmp);
}

LUALIB_API lua_Number luaL_optnumber(lua_State *L, int idx, lua_Number def)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  if (LJ_LIKELY(api_tvisnumber(o)))
    return numberVnum(o);
  else if (tvisnil(o))
    return def;
  else if (!(tvisstr(o) && lj_strscan_num(strV(o), &tmp)))
    lj_err_argt(L, idx, LUA_TNUMBER);
  return numV(&tmp);
}

#if LJ_54
#if LJ_64
#define LJ_LUA54_API_MININTEGER		(-9223372036854775807.0 - 1.0)
#define LJ_LUA54_API_MAXINTEGER_EXCL	9223372036854775808.0
#else
#define LJ_LUA54_API_MININTEGER		((lua_Number)LUA_MININTEGER)
#define LJ_LUA54_API_MAXINTEGER_EXCL	(-(lua_Number)LUA_MININTEGER)
#endif

static int luaV_tointeger54(cTValue *o, lua_Integer *ip, int *isnum)
{
  TValue tmp;
  lua_Number n;
  int64_t k;
  if (isnum)
    *isnum = 0;
  if (tvisstr(o)) {
    GCstr *s = strV(o);
    StrScanFmt fmt;
    if (lj_strscan_rejectnum54(strdata(s), s->len))
      return 0;
    fmt = lj_strscan_scan((const uint8_t *)strdata(s), s->len, &tmp,
			  STRSCAN_OPT_TOINT);
    if (fmt == STRSCAN_ERROR)
      return 0;
    if (isnum)
      *isnum = 1;
    if (fmt == STRSCAN_INT) {
      *ip = (lua_Integer)tmp.i;
      return 1;
    } else if (fmt == STRSCAN_I64) {
      *ip = (lua_Integer)tmp.u64;
      return 1;
    }
    o = &tmp;
  }
  if (!api_tvisnumber(o))
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
  /* Lua 5.4 integer conversion is exact. Fractions remain numbers, but they
  ** are not valid integers for lua_tointegerx/luaL_checkinteger.
  */
  if (!(n >= LJ_LUA54_API_MININTEGER &&
	n < LJ_LUA54_API_MAXINTEGER_EXCL))
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
  } else if (api_tvisnumber(o)) {
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
  } else if (api_tvisnumber(o)) {
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
  } else if (api_tvisnumber(o)) {
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
  if (lst == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
#if LJ_54
  cTValue *o = index2adr(L, idx);
  const char *s;
  if (tvisnil(o)) {
    if (def == NULL)
      lj_err_argt(L, idx, LUA_TSTRING);
    s = def;
  } else {
    /* Lua 5.4 only uses the default for absent/nil arguments. A present
    ** non-string/non-number option must still raise the normal type error.
    */
    s = luaL_checklstring(L, idx, NULL);
  }
#else
  const char *s = lua_tolstring(L, idx, NULL);
  if (s == NULL && (s = def) == NULL)
    lj_err_argt(L, idx, LUA_TSTRING);
#endif
  for (i = 0; lst[i]; i++)
    if (strcmp(lst[i], s) == 0)
      return (int)i;
  lj_err_argv(L, idx, LJ_ERR_INVOPTM, s);
}

LUA_API size_t lua_stringtonumber(lua_State *L, const char *s)
{
  TValue tv;
  size_t len;
  if (s == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  len = strlen(s);
#if LJ_54
  if (lj_strscan_rejectnum54(s, (MSize)len))
    return 0;  /* Keep lua_stringtonumber() aligned with Lua 5.4 tonumber(). */
#endif
  StrScanFmt fmt = lj_strscan_scan((const uint8_t *)s, (MSize)len, &tv,
				   LJ_DUALNUM ? STRSCAN_OPT_TOINT :
						STRSCAN_OPT_TONUM);
  if (fmt == STRSCAN_ERROR)
    return 0;
  if (LJ_DUALNUM && fmt == STRSCAN_INT) {
    setitype(&tv, LJ_TISNUM);
    copyTV(L, L->top, &tv);
#if LJ_54 && LJ_DUALNUM
  } else if (fmt == STRSCAN_I64) {
    lj_obj_setint64(L, L->top, (int64_t)tv.u64);
#endif
  } else {
    copyTV(L, L->top, &tv);
  }
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
  } else if (api_tvisnumber(o)) {
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
  o = index2adr_valid(L, idx);
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
#if LJ_54
  lj_obj_setint64(L, L->top, (int64_t)n);
#else
  setintptrV(L->top, n);
#endif
  incr_top(L);
}

LUA_API void lua_pushlstring(lua_State *L, const char *str, size_t len)
{
  GCstr *s;
  lj_gc_check(L);
  if (str == NULL && len != 0)
    lj_err_msg(L, LJ_ERR_BADVAL);
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
  if (fmt == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  return lj_strfmt_pushvf(L, fmt, argp);
}

LUA_API const char *lua_pushfstring(lua_State *L, const char *fmt, ...)
{
  const char *ret;
  va_list argp;
  lj_gc_check(L);
  if (fmt == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  va_start(argp, fmt);
  ret = lj_strfmt_pushvf(L, fmt, argp);
  va_end(argp);
  return ret;
}

LUA_API void lua_pushcclosure(lua_State *L, lua_CFunction f, int n)
{
  GCfunc *fn;
  lj_gc_check(L);
  if (f == NULL || n < 0 || n > UCHAR_MAX)
    lj_err_msg(L, LJ_ERR_BADVAL);
  api_checknelems(L, n);
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
  if (narray < 0)
    narray = 0;
  if (nrec < 0)
    nrec = 0;
  lj_gc_check(L);
  settabV(L, L->top, lj_tab_new_ah(L, narray, nrec));
  incr_top(L);
}

LUALIB_API int luaL_newmetatable(lua_State *L, const char *tname)
{
  GCtab *regt;
  GCstr *name;
  cTValue *oldv;
  TValue *tv;
  TValue *nameslot;
  if (tname == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  regt = tabV(registry(L));
  name = lj_str_newz(L, tname);
  nameslot = L->top;
  setstrV(L, nameslot, name);
  incr_top(L);
  oldv = lj_tab_getstr(regt, name);
  if (oldv == NULL || tvisnil(oldv)) {
    GCtab *mt = lj_tab_new(L, 0, 1);
    settabV(L, L->top++, mt);
#if LJ_54
    /* Lua 5.4 records the registered metatable name for tostring/errors. */
    setstrV(L, lj_tab_setstr(L, mt, lj_str_newlit(L, "__name")),
	    name);
#endif
    tv = lj_tab_setstr(L, regt, name);
    settabV(L, tv, mt);
    lj_gc_anybarriert(L, regt);
    copyTV(L, nameslot, nameslot+1);
    L->top = nameslot+1;
    return 1;
  } else {
    L->top = nameslot;
    copyTV(L, L->top++, oldv);
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
  if (nuvalue < 0 || nuvalue >= SHRT_MAX)
    lj_err_msg(L, LJ_ERR_BADVAL);
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
  if (n < 0 || n > (int)(L->top - L->base))
    lj_err_msg(L, LJ_ERR_BADVAL);
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

#if LJ_54
LUA_API void lua_toclose(lua_State *L, int idx)
{
  TValue *o = index2adr_stack(L, idx);
  if (!lj_close_canmark(L, o))
    lj_err_callermsg(L, "given index below or equal a marked one");
  if (lj_close_isfalse(o))
    return;
  /* Lua 5.4 only allows marking slots above the current active close slot.
  ** Enforce that before the false/nil fast path: ignored false values still
  ** cannot be used to bypass the stack-order contract.
  */
  if (!lj_close_check(L, o))
    lj_err_callermsg(L, "attempt to close non-closable value");
  lj_close_mark(L, o);
}

LUA_API void lua_closeslot(lua_State *L, int idx)
{
  TValue *o = index2adr_stack(L, idx);
  if (!lj_close_islast(L, o))
    lj_err_callermsg(L, "no variable to close at given level");
  lj_close_unmark(L, o);
  if (!lj_close_call(L, o, NULL, 1))
    lj_err_callermsg(L, "attempt to close non-closable value");
}
#endif

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

#if LJ_54
static lua_Integer api_shiftinteger54(lua_Integer a, lua_Integer sh, int left)
{
  lua_Unsigned u = (lua_Unsigned)a;
  lua_Integer width = (lua_Integer)(8u * sizeof(lua_Unsigned));
  if (sh < 0) {
    if (sh <= -width)
      return 0;
    sh = -sh;
    left = !left;
  } else if (sh >= width) {
    return 0;
  }
  return left ? (lua_Integer)(u << sh) : (lua_Integer)(u >> sh);
}

static int api_rawarith_bit54(lua_State *L, TValue *res, cTValue *a,
			      cTValue *b, int op)
{
  lua_Integer ia, ib;
  lua_Unsigned ua, ub;
  if (op == LUA_OPBNOT) {
    if (tvisstr(a) || !luaV_tointeger54(a, &ia, NULL))
      return 0;
    lj_obj_setint64(L, res, (int64_t)(lua_Integer)~(lua_Unsigned)ia);
    return 1;
  }
  if (tvisstr(a) || tvisstr(b) ||
      !luaV_tointeger54(a, &ia, NULL) ||
      !luaV_tointeger54(b, &ib, NULL))
    return 0;
  ua = (lua_Unsigned)ia;
  ub = (lua_Unsigned)ib;
  switch (op) {
  case LUA_OPBAND:
    lj_obj_setint64(L, res, (int64_t)(lua_Integer)(ua & ub));
    return 1;
  case LUA_OPBOR:
    lj_obj_setint64(L, res, (int64_t)(lua_Integer)(ua | ub));
    return 1;
  case LUA_OPBXOR:
    lj_obj_setint64(L, res, (int64_t)(lua_Integer)(ua ^ ub));
    return 1;
  case LUA_OPSHL:
    lj_obj_setint64(L, res, (int64_t)api_shiftinteger54(ia, ib, 1));
    return 1;
  case LUA_OPSHR:
    lj_obj_setint64(L, res, (int64_t)api_shiftinteger54(ia, ib, 0));
    return 1;
  default:
    return 0;
  }
}

static int api_isbitop54(int op)
{
  return op == LUA_OPBNOT || op == LUA_OPBAND || op == LUA_OPBOR ||
	 op == LUA_OPBXOR || op == LUA_OPSHL || op == LUA_OPSHR;
}

static int api_tobitinteger54(cTValue *o, lua_Integer *ip)
{
  return !tvisstr(o) && luaV_tointeger54(o, ip, NULL);
}

static void api_arith_biterror54(lua_State *L, cTValue *a, cTValue *b,
				 int unary)
{
  lua_Integer i;
  cTValue *bad;
  if (api_tvisnumber(a) && (unary || api_tvisnumber(b))) {
    if (!api_tobitinteger54(a, &i) ||
	(!unary && !api_tobitinteger54(b, &i)))
      lj_err_caller(L, LJ_ERR_NUMINT);
  }
  bad = !api_tvisnumber(a) ? a : b;
  {
    MSize tlen;
    const char *tname = lj_meta_objtypename(L, bad, &tlen);
    UNUSED(tlen);
    lj_err_callermsg(L, lj_strfmt_pushf(L,
      "attempt to perform bitwise operation on a %s value", tname));
  }
}
#endif

#if LJ_54 && LJ_DUALNUM
static lua_Integer api_idivinteger54(lua_State *L, lua_Integer a,
				     lua_Integer b)
{
  lua_Integer q, r;
  if (b == 0)
    lj_err_callermsg(L, "attempt to divide by zero");
  if (a == LUA_MININTEGER && b == (lua_Integer)-1)
    return LUA_MININTEGER;
  q = a / b;
  r = a % b;
  if (r != 0 && ((r ^ b) < 0))
    q--;
  return q;
}

static lua_Integer api_modinteger54(lua_State *L, lua_Integer a,
				    lua_Integer b)
{
  lua_Integer r;
  if (b == 0)
    lj_err_callermsg(L, "attempt to perform 'n%0'");
  if (a == LUA_MININTEGER && b == (lua_Integer)-1)
    return 0;
  r = a % b;
  if (r != 0 && ((r ^ b) < 0))
    r += b;
  return r;
}

static int api_rawarith_int(lua_State *L, TValue *res, cTValue *a, cTValue *b,
			    int op)
{
  lua_Integer ia, ib;
  lua_Unsigned ua, ub;
  if (op == LUA_OPUNM) {
    if (!tvisinteger(a)) return 0;
    ia = tvisint(a) ? (lua_Integer)intV(a) : (lua_Integer)i64V(a);
    lj_obj_setint64(L, res,
      (int64_t)(lua_Integer)((lua_Unsigned)0 - (lua_Unsigned)ia));
    return 1;
  }
  if (!tvisinteger(a) || !tvisinteger(b))
    return 0;
  ia = tvisint(a) ? (lua_Integer)intV(a) : (lua_Integer)i64V(a);
  ib = tvisint(b) ? (lua_Integer)intV(b) : (lua_Integer)i64V(b);
  ua = (lua_Unsigned)ia;
  ub = (lua_Unsigned)ib;
  switch (op) {
  case LUA_OPADD:
    lj_obj_setint64(L, res, (int64_t)(lua_Integer)(ua + ub));
    return 1;
  case LUA_OPSUB:
    lj_obj_setint64(L, res, (int64_t)(lua_Integer)(ua - ub));
    return 1;
  case LUA_OPMUL:
    lj_obj_setint64(L, res, (int64_t)(lua_Integer)(ua * ub));
    return 1;
  case LUA_OPMOD:
    /* Keep lua_arith aligned with Lua 5.4's integer modulo subtype rules. */
    lj_obj_setint64(L, res, (int64_t)api_modinteger54(L, ia, ib));
    return 1;
  case LUA_OPIDIV:
    lj_obj_setint64(L, res, (int64_t)api_idivinteger54(L, ia, ib));
    return 1;
  default:
    return 0;
  }
}
#endif

static int api_rawarith(lua_State *L, TValue *res, cTValue *a, cTValue *b,
			int op)
{
  TValue ta, tb;
  int32_t ia, ib;
  lua_Number na, nb, nr;
  copyTV(L, &ta, a);
  copyTV(L, &tb, b);
#if LJ_54
  switch (op) {
  case LUA_OPBNOT:
  case LUA_OPBAND: case LUA_OPBOR: case LUA_OPBXOR:
  case LUA_OPSHL: case LUA_OPSHR:
    return api_rawarith_bit54(L, res, a, b, op);
  default:
    break;
  }
#endif
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
#if LJ_54 && LJ_DUALNUM
  if (!lj_strscan_numberobj54(L, &ta))
    return 0;
  if (op != LUA_OPUNM && !lj_strscan_numberobj54(L, &tb))
    return 0;
  if (api_rawarith_int(L, res, &ta, &tb, op))
    return 1;
#else
  if (!lj_strscan_numberobj(&ta))
    return 0;
  if (op != LUA_OPUNM && !lj_strscan_numberobj(&tb))
    return 0;
#endif
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
#if LJ_54
    lj_obj_setint64(L, res, (int64_t)(lua_Integer)nr);
#else
    setintptrV(res, (lua_Integer)nr);
#endif
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
  int unary, need;
  TValue *res, *a, *b;
  cTValue *mo;
  if ((unsigned)op > LUA_OPBNOT)
    lj_err_msg(L, LJ_ERR_BADVAL);
  unary = (op == LUA_OPUNM || op == LUA_OPBNOT);
  need = unary ? 1 : 2;
  api_checknelems(L, need);
  res = L->top - need;
  a = res;
  b = unary ? res : res+1;
#if LJ_54
  if (tvisstr(a) || (!unary && tvisstr(b))) {
    mo = api_arith_meta(L, a, b, op, unary);
    if (mo) {
      api_call_arith_meta(L, res, a, b, mo);
      return;
    }
  }
#endif
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
#if LJ_54
  if (api_isbitop54(op))
    api_arith_biterror54(L, a, b, unary);
#endif
  lj_err_optype(L, a, LJ_ERR_OPARITH);
}

/* -- Object getters ------------------------------------------------------ */

LUA_API void lua_gettable(lua_State *L, int idx)
{
  cTValue *t;
  cTValue *v;
  api_checknelems(L, 1);
  t = index2adr_valid(L, idx);
  v = lj_meta_tget(L, t, L->top-1);
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
  cTValue *v, *t = index2adr_valid(L, idx);
  TValue key;
  if (k == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
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
  cTValue *t;
  api_checknelems(L, 1);
  t = api_checktable(L, idx);
  copyTV(L, L->top-1, lj_tab_get(L, tabV(t), L->top-1));
}

LUA_API void lua_rawgeti(lua_State *L, int idx, int n)
{
  cTValue *v, *t = api_checktable(L, idx);
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
  if (checki32(n)) {
    lua_rawgeti(L, idx, (int)n);
  } else {
    idx = lua_absindex(L, idx);
    lua_pushinteger(L, n);
    lua_rawget(L, idx);
  }
  return lua_type(L, -1);
}

LUA_API int lua_rawgetp54(lua_State *L, int idx, const void *p)
{
  lua_rawgetp(L, idx, p);
  return lua_type(L, -1);
}

LUA_API int lua_getglobal54(lua_State *L, const char *name)
{
  if (name == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  lua_getfield(L, -1, name);
  lua_remove(L, -2);
  return lua_type(L, -1);
}

LUA_API void lua_setglobal54(lua_State *L, const char *name)
{
  if (name == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
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
  if (field == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  if (lua_getmetatable(L, idx)) {
    cTValue *tv = lj_tab_getstr(tabV(L->top-1), lj_str_newz(L, field));
    if (tv && !tvisnil(tv)) {
      copyTV(L, L->top-1, tv);
#if LJ_54
      /* Lua 5.4 changed luaL_getmetafield() to return the pushed field's
      ** type code. Keep default LuaJIT on the historical boolean surface.
      */
      return lua_type(L, -1);
#else
      return 1;
#endif
    }
    L->top--;
  }
  return 0;
}

LUA_API void lua_getfenv(lua_State *L, int idx)
{
  cTValue *o = index2adr_valid(L, idx);
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
  cTValue *o = index2adr_valid(L, idx);
  GCudata *ud;
  cTValue *tv;
  if (!tvisudata(o))
    return LUA_TNONE;
  ud = udataV(o);
  if (n < 1 || n > api_udata_uvcount(L, ud)) {
    setnilV(L->top);
    incr_top(L);
    return LUA_TNONE;
  }
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
  cTValue *t;
  int more;
  api_checknelems(L, 1);
  t = api_checktable(L, idx);
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
  cTValue *f = index2adr_valid(L, idx);
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
  cTValue *f = index2adr_valid(L, idx);
  GCfunc *fn;
  if (!tvisfunc(f))
    lj_err_msg(L, LJ_ERR_BADVAL);
  fn = funcV(f);
#if LJ_54
  if (lj_debug_hasenvuv(fn)) {
    if (n == 1)
      return (void *)&fn->c.env;
    n--;
  }
  if (n <= 0)
    return NULL;
  n--;
  /* Match Lua 5.4's public API: invalid upvalue indices return NULL so
  ** debug.upvalueid can report nil without turning a query into an error. */
  if ((uint32_t)n >= (isluafunc(fn) ? fn->l.nupvalues : fn->c.nupvalues))
    return NULL;
#else
  n--;
  if ((uint32_t)n >= (isluafunc(fn) ? fn->l.nupvalues : fn->c.nupvalues))
    return NULL;
#endif
  return isluafunc(fn) ? (void *)gcref(fn->l.uvptr[n]) :
			 (void *)&fn->c.upvalue[n];
}

LUA_API void lua_upvaluejoin(lua_State *L, int idx1, int n1, int idx2, int n2)
{
  cTValue *f1 = index2adr_valid(L, idx1);
  cTValue *f2 = index2adr_valid(L, idx2);
  GCfunc *fn1, *fn2;
  if (!tvisfunc(f1) || !tvisfunc(f2))
    lj_err_msg(L, LJ_ERR_BADVAL);
  fn1 = funcV(f1);
  fn2 = funcV(f2);
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
      if (!isluafunc(fn2) || (uint32_t)(n2-1) >= fn2->l.nupvalues)
	lj_err_msg(L, LJ_ERR_BADVAL);
      (void)lj_debug_uvnamev(f2, (uint32_t)(n2-1), &tv, &o);
      if (!tvistab(tv))
	lj_err_msg(L, LJ_ERR_BADVAL);
      t = tabV(tv);
    }
    setgcref(fn1->c.env, obj2gco(t));
    lj_gc_objbarrier(L, fn1, t);
    return;
  }
  if (env1) n1--;
  if (env2 && n2 == 1) {
    GCtab *t = tabref(fn2->c.env);
    GCupval *uv;
    TValue *tv;
    n1--;
    if (!isluafunc(fn1) || (uint32_t)n1 >= fn1->l.nupvalues)
      lj_err_msg(L, LJ_ERR_BADVAL);
    uv = &gcref(fn1->l.uvptr[n1])->uv;
    tv = uvval(uv);
    settabV(L, tv, t);
    lj_gc_barrier(L, obj2gco(uv), tv);
    return;
  }
  if (env2) n2--;
#endif
  n1--; n2--;
  if (!isluafunc(fn1) || !isluafunc(fn2) ||
      (uint32_t)n1 >= fn1->l.nupvalues ||
      (uint32_t)n2 >= fn2->l.nupvalues)
    lj_err_msg(L, LJ_ERR_BADVAL);
  setgcrefr(fn1->l.uvptr[n1], fn2->l.uvptr[n2]);
  lj_gc_objbarrier(L, fn1, gcref(fn1->l.uvptr[n1]));
}

LUALIB_API void *luaL_testudata(lua_State *L, int idx, const char *tname)
{
  cTValue *o;
  if (tname == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  o = index2adr_valid(L, idx);
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
  cTValue *t = index2adr_valid(L, idx);
  api_checknelems(L, 2);
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
  cTValue *t = index2adr_valid(L, idx);
  if (k == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  api_checknelems(L, 1);
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
  api_checknelems(L, 1);
  idx = lua_absindex(L, idx);
  lua_pushinteger(L, n);
  lua_insert(L, -2);
  lua_settable(L, idx);
}

LUA_API void lua_rawset(lua_State *L, int idx)
{
  GCtab *t;
  TValue *dst, *key;
  api_checknelems(L, 2);
  t = tabV(api_checktable(L, idx));
  key = L->top-2;
  dst = lj_tab_set(L, t, key);
  copyTV(L, dst, key+1);
  lj_gc_anybarriert(L, t);
  L->top = key;
}

LUA_API void lua_rawseti(lua_State *L, int idx, int n)
{
  GCtab *t;
  TValue *dst, *src;
  api_checknelems(L, 1);
  t = tabV(api_checktable(L, idx));
  dst = lj_tab_setint(L, t, n);
  src = L->top-1;
  copyTV(L, dst, src);
  lj_gc_barriert(L, t, dst);
  L->top = src;
}

#if LJ_54
LUA_API void lua_rawseti54(lua_State *L, int idx, lua_Integer n)
{
  if (checki32(n)) {
    lua_rawseti(L, idx, (int)n);
  } else {
    api_checknelems(L, 1);
    idx = lua_absindex(L, idx);
    lua_pushinteger(L, n);
    lua_insert(L, -2);
    lua_rawset(L, idx);
  }
}
#endif

LUA_API void lua_rawsetp(lua_State *L, int idx, const void *p)
{
  api_checknelems(L, 1);
  idx = lua_absindex(L, idx);
  lua_pushlightuserdata(L, (void *)p);
  lua_insert(L, -2);
  lua_rawset(L, idx);
}

LUA_API int lua_setmetatable(lua_State *L, int idx)
{
  global_State *g;
  GCtab *mt;
  cTValue *o = index2adr_valid(L, idx);
  api_checknelems(L, 1);
  if (tvisnil(L->top-1)) {
    mt = NULL;
  } else {
    if (!tvistab(L->top-1))
      lj_err_msg(L, LJ_ERR_BADVAL);
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
	if (gc && !tvisnil(gc)) {
	  t->flags54 = (uint8_t)((t->flags54 | LJ_TAB_HAS_GC) &
				 (uint8_t)~LJ_TAB_GC_PENDING);
	  lj_gc_arm_finalizer54(g);
	}
      }
#endif
    }
  } else if (tvisudata(o)) {
    GCudata *ud = udataV(o);
    setgcref(ud->metatable, obj2gco(mt));
    if (mt) {
      lj_gc_objbarrier(L, ud, mt);
#if LJ_54
      {
	cTValue *gc = lj_tab_getstr(mt, mmname_str(g, MM_gc));
	if (gc && !tvisnil(gc)) {
	  ud->marked &= (uint8_t)~LJ_GC_FINALIZED;
	  lj_gc_arm_finalizer54(g);
	}
      }
#endif
    }
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
  api_checknelems(L, 1);
  o = index2adr_valid(L, idx);
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
  cTValue *o = index2adr_valid(L, idx);
  GCtab *t;
  api_checknelems(L, 1);
  if (!tvistab(L->top-1))
    lj_err_msg(L, LJ_ERR_BADVAL);
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
  cTValue *f = index2adr_valid(L, idx);
  TValue *val;
  GCobj *o;
  const char *name;
  api_checknelems(L, 1);
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
  api_checkcallargs(L, nargs, nresults);
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
  api_checkcallargs(L, nargs, nresults);
  if (errfunc == 0) {
    ef = 0;
  } else {
    cTValue *o = index2adr_stack(L, errfunc);
    if (!tvisfunc(o))
      lj_err_msg(L, LJ_ERR_BADVAL);
    ef = savestack(L, o);
  }
  status = lj_vm_pcall(L, api_call_base(L, nargs), nresults+1, ef);
  if (status) hook_restore(g, oldh);
  return status;
}

#if LJ_54
enum {
  LUA54_CAPI_CONT_NONE,
  LUA54_CAPI_CONT_YIELDK,
  LUA54_CAPI_CONT_CALLK,
  LUA54_CAPI_CONT_PCALLK
};

LUA_API void (lua_callk)(lua_State *L, int nargs, int nresults,
			 lua_KContext ctx, lua_KFunction k)
{
  if (k != NULL && cframe_canyield(L->cframe)) {
    void *oldcf = L->cframe;
    int status;
    lj_checkapi(L->status == LUA_OK || L->status == LUA_ERRERR,
		"thread called in wrong state %d", L->status);
    api_checkcallargs(L, nargs, nresults);
    L->capi_yield_ctx = ctx;
    L->capi_yield_k = k;
    L->capi_yield_nresults = nresults;
    L->capi_yield_kind = LUA54_CAPI_CONT_CALLK;
    setnilV(&L->capi_yield_errfunc);
    status = lj_vm_resume(L, api_call_base(L, nargs), nresults+1, 0);
    if (status == LUA_YIELD) {
      /* vm_resume is the only existing VM entry that marks the callee frame as
      ** yieldable. Reattach the outer resumable C frame and propagate the
      ** suspension so the C caller of lua_callk() is not resumed prematurely.
      */
      L->cframe = oldcf;
      lj_err_throw(L, LUA_YIELD);
    }
    L->capi_yield_ctx = 0;
    L->capi_yield_k = NULL;
    L->capi_yield_nresults = 0;
    L->capi_yield_kind = LUA54_CAPI_CONT_NONE;
    setnilV(&L->capi_yield_errfunc);
    if (status != LUA_OK)
      lj_err_throw(L, status);
    return;
  }
  lua_call(L, nargs, nresults);
}

LUA_API int (lua_pcallk)(lua_State *L, int nargs, int nresults, int errfunc,
			 lua_KContext ctx, lua_KFunction k)
{
  if (k != NULL && cframe_canyield(L->cframe)) {
    void *oldcf = L->cframe;
    ptrdiff_t ef;
    int status;
    lj_checkapi(L->status == LUA_OK || L->status == LUA_ERRERR,
		"thread called in wrong state %d", L->status);
    api_checkcallargs(L, nargs, nresults);
    if (errfunc == 0) {
      ef = 0;
      setnilV(&L->capi_yield_errfunc);
    } else {
      cTValue *o = index2adr_stack(L, errfunc);
      if (!tvisfunc(o))
	lj_err_msg(L, LJ_ERR_BADVAL);
      ef = savestack(L, o);
      copyTV(L, &L->capi_yield_errfunc, o);
    }
    L->capi_yield_ctx = ctx;
    L->capi_yield_k = k;
    L->capi_yield_nresults = nresults;
    L->capi_yield_kind = LUA54_CAPI_CONT_PCALLK;
    status = lj_vm_resume(L, api_call_base(L, nargs), nresults+1, ef);
    if (status == LUA_YIELD) {
      L->cframe = oldcf;
      lj_err_throw(L, LUA_YIELD);
    }
    L->capi_yield_ctx = 0;
    L->capi_yield_k = NULL;
    L->capi_yield_nresults = 0;
    L->capi_yield_kind = LUA54_CAPI_CONT_NONE;
    setnilV(&L->capi_yield_errfunc);
    return status;
  }
  return lua_pcall(L, nargs, nresults, errfunc);
}
#endif

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
  if (func == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  lj_checkapi(L->status == LUA_OK || L->status == LUA_ERRERR,
	      "thread called in wrong state %d", L->status);
  status = lj_vm_cpcall(L, func, ud, cpcall);
  if (status) hook_restore(g, oldh);
  return status;
}

LUALIB_API int luaL_callmeta(lua_State *L, int idx, const char *field)
{
  idx = lua_absindex(L, idx);
  (void)index2adr_valid(L, idx);
  if (luaL_getmetafield(L, idx, field)) {
    TValue *top = L->top--;
    if (LJ_FR2) setnilV(top++);
    copyTV(L, top++, index2adr_valid(L, idx));
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
  api_checknelems(L, nresults);
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

#if LJ_54
LUA_API int (lua_yieldk)(lua_State *L, int nresults, lua_KContext ctx,
			 lua_KFunction k)
{
  api_checknelems(L, nresults);
  if (L->capi_cont_yieldable) {
    cTValue *f = L->top - nresults;
    /* A Lua 5.4 C continuation is resumed from lua_resume54(), not from an
    ** ordinary VM C frame. Preserve the continuation-yield contract here:
    ** move yielded values to the coroutine result base, save the next
    ** continuation if one was supplied, and let the resume wrapper return
    ** LUA_YIELD to the caller instead of treating this as a C-boundary yield.
    */
    if (k != NULL) {
      L->capi_yield_ctx = ctx;
      L->capi_yield_k = k;
      L->capi_yield_kind = LUA54_CAPI_CONT_YIELDK;
      setnilV(&L->capi_yield_errfunc);
    } else {
      L->capi_yield_ctx = 0;
      L->capi_yield_k = NULL;
      L->capi_yield_kind = LUA54_CAPI_CONT_NONE;
      setnilV(&L->capi_yield_errfunc);
    }
    if (f > L->base) {
      TValue *t = L->base;
      while (--nresults >= 0) copyTV(L, t++, f++);
      L->top = t;
    }
    L->status = LUA_YIELD;
    return -1;
  }
  if (k != NULL) {
    /* Store the Lua 5.4 continuation on the coroutine object before the VM
    ** unwinds the C stack. The resume wrapper consumes it and exposes the
    ** resume arguments as the continuation stack.
    */
    if (!cframe_canyield(L->cframe))
      lj_err_msg(L, LJ_ERR_CYIELD);
    L->capi_yield_ctx = ctx;
    L->capi_yield_k = k;
    L->capi_yield_kind = LUA54_CAPI_CONT_YIELDK;
    setnilV(&L->capi_yield_errfunc);
  }
  return lua_yield(L, nresults);
}
#endif

static int api_resume_error(lua_State *L, ErrMsg em)
{
  L->top = L->base;
  setstrV(L, L->top, lj_err_str(L, em));
  incr_top(L);
  return LUA_ERRRUN;
}

static int api_checkresumeargs(lua_State *L, int nargs)
{
  ptrdiff_t need;
  if (nargs < 0 || nargs == INT_MAX)
    return 0;
  need = (ptrdiff_t)nargs + (L->status == LUA_OK ? 1 : 0);
  return L->top - L->base >= need;
}

LUA_API int lua_resume(lua_State *L, int nargs)
{
  if (nargs < 0 || nargs == INT_MAX)
    return api_resume_error(L, LJ_ERR_BADVAL);
#if LJ_54
  if (L->status == LUA_OK && L->top == L->base) {
    /* A reset/dead coroutine has no initial function left on its stack. Lua
    ** 5.4 reports this as a dead coroutine instead of trying to call nil.
    */
    return api_resume_error(L, LJ_ERR_CODEAD);
  }
#endif
  if (L->cframe == NULL && L->status <= LUA_YIELD) {
    if (!api_checkresumeargs(L, nargs))
      return api_resume_error(L, LJ_ERR_BADVAL);
    return lj_vm_resume(L,
      L->status == LUA_OK ? api_call_base(L, nargs) : L->top - nargs,
      0, 0);
  }
#if LJ_54
  {
    int dead = (L->status > LUA_YIELD ||
		(L->status == LUA_OK && L->top == L->base));
    return api_resume_error(L, dead ? LJ_ERR_CODEAD : LJ_ERR_COSUSP);
  }
#else
  return api_resume_error(L, LJ_ERR_COSUSP);
#endif
}

#if LJ_54
typedef struct Lua54YieldKCtx {
  lua_KFunction k;
  lua_KContext ctx;
  int status;
  int nres;
} Lua54YieldKCtx;

typedef struct Lua54PCallKErrfuncCtx {
  ptrdiff_t stackbase;
} Lua54PCallKErrfuncCtx;

static int lua54_debug_hook_thread_active(lua_State *L)
{
  cTValue *tv;
  TValue key;
  tv = lj_tab_getstr(tabV(registry(L)), lj_str_newlit(L, "_HOOKKEY"));
  if (!(tv && tvistab(tv)))
    return 0;
  setthreadV(L, &key, L);
  tv = lj_tab_get(L, tabV(tv), &key);
  return tv && tvisfunc(tv);
}

static TValue *cp_lua54_yieldk_cont(lua_State *L, lua_CFunction dummy,
				    void *ud)
{
  Lua54YieldKCtx *yk = (Lua54YieldKCtx *)ud;
  UNUSED(dummy);
  yk->nres = yk->k(L, yk->status, yk->ctx);
  if (yk->nres < 0) {
    if (L->status == LUA_YIELD)
      return NULL;
    lj_err_msg(L, LJ_ERR_BADVAL);
  }
  if (yk->nres > L->top - L->base)
    lj_err_msg(L, LJ_ERR_BADVAL);
  lj_checkapi(yk->nres >= 0 && yk->nres <= L->top - L->base,
	      "not enough results returned by lua_yieldk continuation");
  return NULL;
}

static TValue *cp_lua54_pcallk_errfunc(lua_State *L, lua_CFunction dummy,
				       void *ud)
{
  Lua54PCallKErrfuncCtx *ctx = (Lua54PCallKErrfuncCtx *)ud;
  TValue *stackbase, *top;
  UNUSED(dummy);
  lj_state_checkstack(L, LUA_MINSTACK * 2);
  stackbase = restorestack(L, ctx->stackbase);
  L->top = stackbase + 1;
  top = L->top;
  copyTV(L, top++, &L->capi_yield_errfunc);
  if (LJ_FR2) setnilV(top++);
  copyTV(L, top++, stackbase);
  L->top = top;
  cframe_nres(L->cframe) = 1+1;  /* One message-handler result. */
  return top-1;
}

static int lua54_apply_pcallk_errfunc(lua_State *L, TValue *stackbase,
				      int status)
{
  Lua54PCallKErrfuncCtx ctx;
  int hstatus;
  if (tvisnil(&L->capi_yield_errfunc))
    return status;
  if (!tvisfunc(&L->capi_yield_errfunc) || status == LUA_ERRERR) {
    setstrV(L, stackbase, lj_err_str(L, LJ_ERR_ERRERR));
    L->top = stackbase + 1;
    return LUA_ERRERR;
  }
  ctx.stackbase = savestack(L, stackbase);
  hstatus = lj_vm_cpcall(L, NULL, &ctx, cp_lua54_pcallk_errfunc);
  stackbase = restorestack(L, ctx.stackbase);
  L->base = stackbase;
  if (hstatus == LUA_OK) {
    copyTV(L, stackbase, L->top - 1);
    L->top = stackbase + 1;
    return status;
  }
  setstrV(L, stackbase, lj_err_str(L, LJ_ERR_ERRERR));
  L->top = stackbase + 1;
  return LUA_ERRERR;
}

static int resume_lua54_yieldk_cont(lua_State *L, int nargs, int *nresults)
{
  TValue *stackbase = tvref(L->stack) + 1 + LJ_FR2;
  TValue *argbase = L->top - nargs;
  ptrdiff_t stackbaseofs;
  Lua54YieldKCtx yk;
  TValue errtv;
  int status, i;
  lj_checkapi(nargs >= 0 && argbase >= stackbase,
	      "not enough stack values to resume continuation");
  /* Replace the previous yield results with the new resume arguments. This
  ** matches Lua 5.4's C continuation view: stack index 1 is the first resume
  ** value, not the old value yielded to the caller.
  */
  for (i = 0; i < nargs; i++)
    copyTV(L, stackbase + i, argbase + i);
  L->base = stackbase;
  L->top = stackbase + nargs;
  yk.k = L->capi_yield_k;
  yk.ctx = L->capi_yield_ctx;
  yk.status = LUA_YIELD;
  yk.nres = 0;
  L->capi_yield_k = NULL;
  L->capi_yield_ctx = 0;
  L->capi_yield_kind = LUA54_CAPI_CONT_NONE;
  setnilV(&L->capi_yield_errfunc);
  L->status = LUA_OK;
  stackbaseofs = savestack(L, stackbase);
  L->capi_cont_yieldable = 1;
  status = lj_vm_cpcall(L, NULL, &yk, cp_lua54_yieldk_cont);
  L->capi_cont_yieldable = 0;
  L->cframe = NULL;
  stackbase = restorestack(L, stackbaseofs);
  if (status == LUA_OK && L->status == LUA_YIELD) {
    if (nresults) *nresults = (int)(L->top - stackbase);
    return LUA_YIELD;
  }
  if (status == LUA_OK) {
    TValue *resbase = L->top - yk.nres;
    for (i = 0; i < yk.nres; i++)
      copyTV(L, stackbase + i, resbase + i);
    L->base = stackbase;
    L->top = stackbase + yk.nres;
    L->status = LUA_OK;
    if (nresults) *nresults = yk.nres;
    return LUA_OK;
  }
  L->status = (uint8_t)status;
  if (L->top > stackbase)
    copyTV(L, &errtv, L->top-1);
  else
    setnilV(&errtv);
  L->base = stackbase;
  copyTV(L, stackbase, &errtv);
  L->top = stackbase + 1;
  if (nresults) *nresults = 1;
  return status;
}

static int resume_lua54_callk_cont(lua_State *L, int status, int *nresults)
{
  TValue *stackbase = tvref(L->stack) + 1 + LJ_FR2;
  TValue *callbase = L->base;
  ptrdiff_t stackbaseofs = savestack(L, stackbase);
  Lua54YieldKCtx yk;
  TValue errtv;
  int i, nres, kind;
  yk.k = L->capi_yield_k;
  yk.ctx = L->capi_yield_ctx;
  yk.status = 0;
  yk.nres = 0;
  nres = L->capi_yield_nresults;
  kind = L->capi_yield_kind;
  L->capi_yield_k = NULL;
  L->capi_yield_ctx = 0;
  L->capi_yield_nresults = 0;
  L->capi_yield_kind = LUA54_CAPI_CONT_NONE;
  if (status != LUA_OK && status != LUA_YIELD &&
      kind != LUA54_CAPI_CONT_PCALLK) {
    setnilV(&L->capi_yield_errfunc);
    if (nresults) *nresults = lua_gettop(L);
    return status;
  }
  /* The yielded callee has now returned. Run the saved Lua 5.4 continuation
  ** with the callee results still on the coroutine stack.
  */
  L->base = stackbase;
  L->status = LUA_OK;
  if (status != LUA_OK && status != LUA_YIELD) {
    lj_checkapi(L->top > stackbase,
		"not enough error results returned by lua_pcallk callee");
    copyTV(L, stackbase, L->top - 1);
    L->top = stackbase + 1;
    status = lua54_apply_pcallk_errfunc(L, stackbase, status);
    stackbase = restorestack(L, stackbaseofs);
    L->status = LUA_OK;
  } else if (nres >= 0) {
    TValue *resbase = L->top - nres;
    lj_checkapi(nres <= L->top - stackbase,
		"not enough results returned by lua_callk callee");
    for (i = 0; i < nres; i++)
      copyTV(L, stackbase + i, resbase + i);
    L->top = stackbase + nres;
  } else {
    /* LUA_MULTRET leaves the yielded callee's dynamic result window in the
    ** resumed C call frame. Move that whole window down so the Lua 5.4
    ** continuation sees stack index 1 as the first callee result.
    */
    TValue *resbase = callbase;
    nres = (int)(L->top - resbase);
    lj_checkapi(nres >= 0 && resbase >= stackbase,
		"bad multret results returned by lua_callk callee");
    for (i = 0; i < nres; i++)
      copyTV(L, stackbase + i, resbase + i);
    L->top = stackbase + nres;
  }
  yk.status = status == LUA_OK ? LUA_YIELD : status;
  setnilV(&L->capi_yield_errfunc);
  stackbaseofs = savestack(L, stackbase);
  L->capi_cont_yieldable = 1;
  status = lj_vm_cpcall(L, NULL, &yk, cp_lua54_yieldk_cont);
  L->capi_cont_yieldable = 0;
  L->cframe = NULL;
  stackbase = restorestack(L, stackbaseofs);
  if (status == LUA_OK && L->status == LUA_YIELD) {
    if (nresults) *nresults = (int)(L->top - stackbase);
    return LUA_YIELD;
  }
  if (status == LUA_OK) {
    TValue *resbase = L->top - yk.nres;
    for (i = 0; i < yk.nres; i++)
      copyTV(L, stackbase + i, resbase + i);
    L->base = stackbase;
    L->top = stackbase + yk.nres;
    L->status = LUA_OK;
    if (nresults) *nresults = yk.nres;
    return LUA_OK;
  }
  L->status = (uint8_t)status;
  if (L->top > stackbase)
    copyTV(L, &errtv, L->top-1);
  else
    setnilV(&errtv);
  L->base = stackbase;
  copyTV(L, stackbase, &errtv);
  L->top = stackbase + 1;
  if (nresults) *nresults = 1;
  return status;
}

static void lua54_resume_restore_from(lua_State *L, lua_State *from)
{
  if (from != NULL)
    setgcref(G(L)->cur_L, obj2gco(from));
}

LUA_API int lua_resume54(lua_State *L, lua_State *from, int nargs,
			 int *nresults)
{
  global_State *g = G(L);
  uint8_t oldmask = 0;
  int suspend_debug_hooks = 0;
  int resume_error, status;
  /* The VM still implements LuaJIT's legacy resume ABI. This wrapper exposes
  ** Lua 5.4's result-count out parameter without changing the internal ABI.
  */
  if (L->status == LUA_YIELD && L->capi_yield_k != NULL &&
      L->capi_yield_kind == LUA54_CAPI_CONT_YIELDK) {
    if (!api_checkresumeargs(L, nargs)) {
      status = api_resume_error(L, LJ_ERR_BADVAL);
      if (nresults) *nresults = lua_gettop(L);
      lua54_resume_restore_from(L, from);
      return status;
    }
    status = resume_lua54_yieldk_cont(L, nargs, nresults);
    lua54_resume_restore_from(L, from);
    return status;
  }
  if (g->hook_debug && !lua54_debug_hook_thread_active(L)) {
    oldmask = g->hookmask;
    g->hookmask = (uint8_t)(oldmask & ~HOOK_EVENTMASK);
    suspend_debug_hooks = 1;
    lj_dispatch_update(g);
  }
  resume_error = (L->status > LUA_YIELD ||
		  (L->status == LUA_OK && L->top == L->base));
  status = lua_resume(L, nargs);
  if (suspend_debug_hooks) {
    g->hookmask = oldmask;
    lj_dispatch_update(g);
  }
  if (L->capi_yield_k != NULL &&
      (L->capi_yield_kind == LUA54_CAPI_CONT_CALLK ||
       L->capi_yield_kind == LUA54_CAPI_CONT_PCALLK) &&
      status != LUA_YIELD) {
    status = resume_lua54_callk_cont(L, status, nresults);
    lua54_resume_restore_from(L, from);
    return status;
  }
  if (nresults)
    *nresults = (status == LUA_OK || status == LUA_YIELD || !resume_error) ?
		lua_gettop(L) : 0;
  lua54_resume_restore_from(L, from);
  return status;
}
#endif

LUA_API int lua_resetthread(lua_State *L)
{
#if LJ_54
  return lua_closethread(L, NULL);
#else
  /* Full Lua 5.4 reset closes to-be-closed variables. This compatibility path
  ** covers LuaJIT coroutines without <close> state by clearing frames, open
  ** upvalues and status back to a fresh suspended stack.
  */
  lj_func_closeuv(L, tvref(L->stack));
  L->status = LUA_OK;
  L->cframe = NULL;
  L->base = L->top = tvref(L->stack) + 1 + LJ_FR2;
  return LUA_OK;
#endif
}

#if LJ_54
LUA_API int lua_closethread(lua_State *L, lua_State *from)
{
  TValue *base = tvref(L->stack) + 1 + LJ_FR2;
  ptrdiff_t baseofs = savestack(L, base);
  int status = L->status == LUA_YIELD ? LUA_OK : L->status;
  int closestatus;
  TValue errtv;
  (void)from;
  /* Lua 5.4 closes pending to-be-closed values while resetting a coroutine.
  ** A yielded coroutine closes with nil error; an errored coroutine passes its
  ** current error object, and a __close error replaces that object.
  */
  L->capi_yield_k = NULL;
  L->capi_yield_ctx = 0;
  L->capi_yield_nresults = 0;
  L->capi_yield_kind = LUA54_CAPI_CONT_NONE;
  setnilV(&L->capi_yield_errfunc);
  L->capi_cont_yieldable = 0;
  L->status = LUA_OK;
  L->cframe = NULL;
  closestatus = lj_close_unwind_status(L, base, status);
  base = restorestack(L, baseofs);
  if (closestatus != LUA_OK)
    status = closestatus;
  /* Preserve the final error object before resetting the coroutine stack and
  ** closing upvalues. An errored coroutine without active close variables can
  ** otherwise leave the original error in a slot that is invalidated below.
  */
  if (status != LUA_OK) {
    if (L->top > tvref(L->stack))
      copyTV(L, &errtv, L->top-1);
    else
      setnilV(&errtv);
  }
  lj_func_closeuv(L, tvref(L->stack));
  {
    TValue *o, *stack = tvref(L->stack), *limit = tvref(L->maxstack);
    /* Resetting a coroutine discards all suspended frames. Clear the whole
    ** GC-scanned stack area after saving the final error, otherwise stale
    ** frame temporaries from close/error paths can be marked during a full GC.
    */
    for (o = stack; o < limit; o++)
      setnilV(o);
  }
  L->base = base;
  if (status == LUA_OK) {
    L->top = base;
  } else {
    copyTV(L, base, &errtv);
    L->top = base+1;
  }
  /* lua_closethread() may run __close metamethods on the target coroutine via
  ** protected calls. Restore the active VM thread to the caller before the
  ** library function resumes using its own stack.
  */
  if (from != NULL)
    setgcref(G(L)->cur_L, obj2gco(from));
  return status;
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

static void gc_fullgc_preserve_stop54(lua_State *L)
{
  global_State *g = G(L);
  int wasstopped = (g->gc.threshold == LJ_MAX_MEM);
  lj_gc_fullgc(L);
  if (wasstopped)
    g->gc.threshold = LJ_MAX_MEM;
}

static int gc_gen_manual_major54(global_State *g)
{
#if LJ_HASJIT
  jit_State *J = G2J(g);
  if (tvref(g->jit_base) != NULL || J->state != LJ_TRACE_IDLE)
    return 1;  /* Defer minor atomic/finalizers while on trace or recording. */
  return 0;
#else
  UNUSED(g);
  return 0;
#endif
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
  int data2 = 0;
  int data3 = 0;
  va_list argp;
  if (g->hookmask & HOOK_GC)
    return -1;
  va_start(argp, what);
  /* Lua 5.4 exposes lua_gc() as a vararg API. */
  switch (what) {
  case LUA_GCSTEP:
  case LUA_GCSETPAUSE:
  case LUA_GCSETSTEPMUL:
    data = va_arg(argp, int);
    break;
  case LUA_GCGEN:
    data = va_arg(argp, int);
    data2 = va_arg(argp, int);
    break;
  case LUA_GCINC:
    data = va_arg(argp, int);
    data2 = va_arg(argp, int);
    data3 = va_arg(argp, int);
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
    data = (g->gc.threshold == LJ_MAX_MEM);
    lj_gc_fullgc(L);
    if (LJ_54 && data) {
      /* Explicit full collections are allowed while stopped, but Lua 5.4 does
      ** not treat them as collectgarbage("restart").
      */
      g->gc.threshold = LJ_MAX_MEM;
    }
    break;
  case LUA_GCCOUNT:
    res = (int)(g->gc.total >> 10);
    break;
  case LUA_GCCOUNTB:
    res = (int)(g->gc.total & 0x3ff);
    break;
  case LUA_GCSTEP: {
    GCSize a;
    int wasstopped = (g->gc.threshold == LJ_MAX_MEM);
#if LJ_54
    int wasgen = g->gc_mode54;
    int waspause = (g->gc.state == GCSpause);
    int wasclean = (g->gc.total <= g->gc.estimate);
    if (wasgen && gc_gen_manual_major54(g)) {
      gc_fullgc_preserve_stop54(L);
      res = 0;
      break;
    }
    if (data < 0) {
      res = 0;
      break;
    }
    if (data == 0) {
      if (wasgen) {
	g->gc.threshold = g->gc.total;
	(void)lj_gc_step(L);
	res = 0;
      } else {
	int stepres;
	g->gc.debt = 0;
	g->gc.threshold = g->gc.total;
	stepres = lj_gc_step(L);
	res = stepres > 0 || (waspause && wasclean);
      }
      if (wasstopped)
	g->gc.threshold = LJ_MAX_MEM;
      break;
    }
#endif
    a = (GCSize)data << 10;
    g->gc.threshold = (a <= g->gc.total) ? (g->gc.total - a) : 0;
    while (g->gc.total >= g->gc.threshold)
      if (lj_gc_step(L) > 0) {
#if LJ_54
	res = wasgen ? 0 : 1;
#else
	res = 1;
#endif
	break;
      }
    if (LJ_54 && wasstopped) {
      /* A manual Lua 5.4 GC step may do collection work while the collector is
      ** stopped, but it must not implicitly restart automatic GC scheduling.
      */
      g->gc.threshold = LJ_MAX_MEM;
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
    res = g->gc_mode54 ? LUA_GCGEN : LUA_GCINC;
#if LJ_54
    {
      int wasinc = !g->gc_mode54;
      if (data != 0)
	g->gc_genminormul54 = (MSize)(uint8_t)data;
      if (data2 != 0)
	g->gc_genmajormul54 = gc_param_lua54(data2);
      g->gc_mode54 = 1;
      g->gc_genlastatomic54 = 0;
      if (wasinc) {
	gc_fullgc_preserve_stop54(L);
      }
    }
#else
    g->gc_mode54 = 1;
#endif
    break;
  case LUA_GCINC:
    res = g->gc_mode54 ? LUA_GCGEN : LUA_GCINC;
#if LJ_54
    if (data != 0)
      g->gc.pause = gc_param_lua54(data);
    if (data2 != 0)
      g->gc.stepmul = gc_param_lua54(data2);
    if (data3 != 0)
      g->gc_stepsize54 = (MSize)(uint8_t)data3;
    if (g->gc_mode54)
      lj_gc_gen_whitelist54(g);
#endif
    g->gc_mode54 = 0;
    break;
  default:
    res = -1;  /* Invalid option. */
  }
  return res;
}

LUA_API void lua_setwarnf(lua_State *L, lua_WarnFunction f, void *ud)
{
  global_State *g = G(L);
  g->warnf = f;
  g->warnud = ud;
  /* Official Lua replaces the warning callback directly.  A NULL callback is
  ** not the default writer; it suppresses lua_warning() until the embedder
  ** installs another callback.
  */
  g->warn_disabled = (f == NULL);
}

LUA_API void lua_warning(lua_State *L, const char *msg, int tocont)
{
  global_State *g = G(L);
  if (msg == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  if (g->warnf) {
    g->warnf(g->warnud, msg, tocont);
  } else if (g->warn_disabled) {
    return;
  } else if (!tocont && !g->warn_cont && msg && msg[0] == '@') {
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
  if (f == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  g->allocd = ud;
  g->allocf = f;
}

