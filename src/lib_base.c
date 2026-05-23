/*
** Base and coroutine library.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2011 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <string.h>

#define lib_base_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_func.h"
#include "lj_state.h"
#include "lj_frame.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#include "lj_cconv.h"
#endif
#include "lj_bc.h"
#include "lj_ff.h"
#include "lj_dispatch.h"
#include "lj_char.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "lj_lib.h"
#include "lj_close.h"

#include "luajit.h"

/* -- Base library: checks ------------------------------------------------ */

#define LJLIB_MODULE_base

#if LJ_54
static const char *base_callname54(lua_State *L, const char *fname);

static void base_argerror_named54(lua_State *L, int narg, const char *fname,
				  const char *msg)
{
  const char *callname = base_callname54(L, fname);
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      narg, callname, msg));
}

static const char *base_argtypename54(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (o < L->top) {
    MSize tlen;
    const char *tname = lj_meta_objtypename(L, o, &tlen);
    UNUSED(tlen);
    return tname;
  }
  return lj_obj_typename[0];
}

static void base_argtype_named54(lua_State *L, int narg, const char *fname,
				 const char *xname)
{
  base_argerror_named54(L, narg, fname,
    lj_strfmt_pushf(L, "%s expected, got %s", xname,
		    base_argtypename54(L, narg)));
}

static const char *base_callname54(lua_State *L, const char *fname)
{
  /* Base-library argument errors share the Lua 5.4 call-name rule: direct
  ** pcall(cfunc, ...) keeps the public fallback, while real callsites and
  ** local/upvalue aliases report the source-level name.
  */
  return lj_debug_callname54(L, fname, NULL);
}

static void base_argtype_callname54(lua_State *L, int narg, const char *fname,
				    const char *xname)
{
  base_argtype_named54(L, narg, base_callname54(L, fname), xname);
}

static void base_checkany_named54(lua_State *L, int narg, const char *fname)
{
  if (L->base + narg-1 >= L->top)
    base_argerror_named54(L, narg, fname, "value expected");
}

static void base_checkfunc_named54(lua_State *L, int narg, const char *fname)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && tvisfunc(o)))
    base_argtype_named54(L, narg, fname, "function");
}

static GCstr *base_checkstr_named54(lua_State *L, int narg, const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o < L->top) {
    if (tvisstr(o)) {
      return strV(o);
    } else if (tvisnumber(o) || tvisi64(o)) {
      GCstr *s = lj_strfmt_number(L, o);
      setstrV(L, o, s);
      return s;
    }
  }
  base_argtype_named54(L, narg, fname, "string");
  return NULL;  /* unreachable */
}

static GCstr *base_checkstr_exact_named54(lua_State *L, int narg,
					  const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o < L->top && tvisstr(o))
    return strV(o);
  base_argtype_named54(L, narg, fname, "string");
  return NULL;  /* unreachable */
}

static GCstr *base_optstr_named54(lua_State *L, int narg, const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o >= L->top || tvisnil(o))
    return NULL;
  return base_checkstr_named54(L, narg, fname);
}

static int base_checkopt_named54(lua_State *L, int narg, int def,
				 const char *lst, const char *fname)
{
  GCstr *s = def >= 0 ? base_optstr_named54(L, narg, fname) :
			 base_checkstr_named54(L, narg, fname);
  if (s) {
    const char *opt = strdata(s);
    MSize len = s->len;
    int i;
    for (i = 0; *(const uint8_t *)lst; i++) {
      if (*(const uint8_t *)lst == len && memcmp(opt, lst+1, len) == 0)
	return i;
      lst += 1+*(const uint8_t *)lst;
    }
    base_argerror_named54(L, narg, fname,
			  lj_strfmt_pushf(L, "invalid option '%s'", opt));
  }
  return def;
}

static GCtab *base_checktab_named54(lua_State *L, int narg, const char *fname)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && tvistab(o)))
    base_argtype_named54(L, narg, fname, "table");
  return tabV(o);
}

static lua_Integer base_checkinteger_named54(lua_State *L, int narg,
					     const char *fname)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  lua_Number n;
  int64_t k;
  if (o >= L->top)
    base_argtype_named54(L, narg, fname, "number");
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      base_argtype_named54(L, narg, fname, "number");
    o = &tmp;
  }
  if (tvisint(o))
    return intV(o);
  if (tvisi64(o))
    return (lua_Integer)i64V(o);
  if (!tvisnum(o))
    base_argtype_named54(L, narg, fname, "number");
  n = numV(o);
  if (!(n >= (-9223372036854775807.0 - 1.0) &&
	n < 9223372036854775808.0)) {
    base_argerror_named54(L, narg, fname,
			  "number has no integer representation");
  }
  k = lj_num2i64(n);
  if ((lua_Number)k != n) {
    base_argerror_named54(L, narg, fname,
			  "number has no integer representation");
  }
  return (lua_Integer)k;
}

static int32_t base_optint_named54(lua_State *L, int narg,
				   const char *fname)
{
  cTValue *o = L->base + narg-1;
  if (o < L->top && !tvisnil(o))
    return (int32_t)base_checkinteger_named54(L, narg, fname);
  return 0;
}
#endif

LJLIB_ASM(assert)		LJLIB_REC(.)
{
#if LJ_54
  base_checkany_named54(L, 1, "assert");
#else
  lj_lib_checkany(L, 1);
#endif
  if (L->top == L->base+1)
    lj_err_caller(L, LJ_ERR_ASSERT);
#if LJ_54
  else if (tvisstr(L->base+1))
    lj_err_callermsg(L, strdata(strV(L->base+1)));
#else
  else if (tvisstr(L->base+1) || tvisnumber(L->base+1) || tvisi64(L->base+1))
    lj_err_callermsg(L, strdata(lj_lib_checkstr(L, 2)));
#endif
  else
    lj_err_run(L);
  return FFH_UNREACHABLE;
}

/* ORDER LJ_T */
LJLIB_PUSH("nil")
LJLIB_PUSH("boolean")
LJLIB_PUSH(top-1)  /* boolean */
LJLIB_PUSH("userdata")
LJLIB_PUSH("string")
LJLIB_PUSH("upval")
LJLIB_PUSH("thread")
LJLIB_PUSH("proto")
LJLIB_PUSH("function")
LJLIB_PUSH("trace")
LJLIB_PUSH("cdata")
LJLIB_PUSH("number")  /* int64 */
LJLIB_PUSH("table")
LJLIB_PUSH(top-10)  /* userdata */
LJLIB_PUSH("number")
LJLIB_ASM_(type)		LJLIB_REC(.)
/* Recycle the lj_lib_checkany(L, 1) from assert. */

#if LJ_54
static int lj_cf_type54(lua_State *L)
{
  base_checkany_named54(L, 1, "type");
  lua_pushstring(L, lua_typename(L, lua_type(L, 1)));
  return 1;
}
#endif

/* -- Base library: iterators --------------------------------------------- */

/* This solves a circular dependency problem -- change FF_next_N as needed. */
LJ_STATIC_ASSERT((int)FF_next == FF_next_N);
LJ_STATIC_ASSERT((int)FF_xpcall == FF_xpcall_N);

LJLIB_ASM(next)			LJLIB_REC(.)
{
#if LJ_54
  base_checktab_named54(L, 1, "next");
#else
  lj_lib_checktab(L, 1);
#endif
  lj_err_msg(L, LJ_ERR_NEXTIDX);
  return FFH_UNREACHABLE;
}

#if LJ_54
static int base_isglobalenv(lua_State *L, GCtab *t)
{
  return t == tabref(L->env);
}

static int base_isenvkey(cTValue *o)
{
  if (tvisstr(o)) {
    GCstr *k = strV(o);
    return k->len == 4 && memcmp(strdata(k), "_ENV", 4) == 0;
  }
  return 0;
}

static int lj_cf_next54(lua_State *L)
{
  GCtab *t = base_checktab_named54(L, 1, "next");
  int hide_env = base_isglobalenv(L, t);
  if (lua_gettop(L) < 2)
    lua_pushnil(L);
  else
    lua_settop(L, 2);
  while (lua_next(L, 1)) {
    if (!hide_env || !base_isenvkey(L->top-2))
      return 2;
    /* _ENV is an internal compatibility binding, not an official global key. */
    lua_pop(L, 1);
  }
  /* Official Lua 5.4 returns a single nil when iteration is exhausted. */
  lua_pushnil(L);
  return 1;
}
#endif

#if LJ_52 || LJ_HASFFI
static int ffh_pairs(lua_State *L, MMS mm)
{
  TValue *o;
  cTValue *mo;
#if LJ_54
  if (mm == MM_pairs) {
    base_checkany_named54(L, 1, "pairs");
    o = L->base;
    mo = lj_meta_lookup(L, o, mm);
    if (!tvisnil(mo)) {
      L->top = o+1;  /* Only keep one argument. */
      copyTV(L, L->base-1-LJ_FR2, mo);  /* Replace callable. */
      return FFH_TAILCALL;
    }
    if (LJ_FR2) { copyTV(L, o-1, o); o--; }
    /* Lua 5.4 delays table validation until next() runs. Keep this path as
    ** a fast function so a yielding __pairs metamethod can tailcall via VM.
    */
    setfuncV(L, o-1, funcV(lj_lib_upvalue(L, 1)));
    setnilV(o+1);
    return FFH_RES(3);
  }
#endif
  o = lj_lib_checkany(L, 1);
  mo = lj_meta_lookup(L, o, mm);
  if ((LJ_52 || tviscdata(o)) && !tvisnil(mo)) {
    L->top = o+1;  /* Only keep one argument. */
    copyTV(L, L->base-1-LJ_FR2, mo);  /* Replace callable. */
    return FFH_TAILCALL;
  } else {
    if (!tvistab(o)) lj_err_argt(L, 1, LUA_TTABLE);
    if (LJ_FR2) { copyTV(L, o-1, o); o--; }
    setfuncV(L, o-1, funcV(lj_lib_upvalue(L, 1)));
    if (mm == MM_pairs) setnilV(o+1); else setintV(o+1, 0);
    return FFH_RES(3);
  }
}
#else
#define ffh_pairs(L, mm)	(lj_lib_checktab(L, 1), FFH_UNREACHABLE)
#endif

LJLIB_PUSH(lastcl)
LJLIB_ASM(pairs)		LJLIB_REC(xpairs 0)
{
  return ffh_pairs(L, MM_pairs);
}

LJLIB_NOREGUV LJLIB_ASM(ipairs_aux)	LJLIB_REC(.)
{
  lj_lib_checktab(L, 1);
  lj_lib_checkint(L, 2);
  return FFH_UNREACHABLE;
}

LJLIB_PUSH(lastcl)
LJLIB_ASM(ipairs)		LJLIB_REC(xpairs 1)
{
  return ffh_pairs(L, MM_ipairs);
}

#if LJ_54
#define LUA54_IPAIRS_AUX_REGKEY	"_LUA54_IPAIRS_AUX"

static int lj_cf_ipairs_aux54(lua_State *L)
{
  lua_Integer i = luaL_checkinteger(L, 2);
  /* Official ipairs uses luaL_intop(+, i, 1), so advancing maxinteger wraps
  ** to mininteger. Avoid C signed overflow while preserving that surface.
  */
  i = (i == LUA_MAXINTEGER) ? LUA_MININTEGER : i + 1;
  lua_pushinteger(L, i);
  lua_pushinteger(L, i);
  /* Lua 5.4 ipairs uses normal indexed access, so __index can provide values. */
  lua_gettable(L, 1);
  if (lua_isnil(L, -1))
    return 1;
  return 2;
}

static int lj_cf_ipairs54(lua_State *L)
{
  base_checkany_named54(L, 1, "ipairs");
  /* Lua 5.4 exposes one stable ipairs auxiliary function, but it is not a
  ** visible debug upvalue of ipairs(). Keep the stable function in the registry.
  */
  lua_getfield(L, LUA_REGISTRYINDEX, LUA54_IPAIRS_AUX_REGKEY);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, 0);
  return 3;
}

#endif

LJLIB_CF(warn)
{
  int32_t i, n = (int32_t)(L->top - L->base);
#if LJ_54
  if (n == 0)
    base_argtype_named54(L, 1, "warn", "string");
  for (i = 0; i < n; i++) {
    /* Lua 5.4 validates every argument before composing a warning. If a later
    ** argument errors, emitting earlier pieces would leave the default warning
    ** writer in continuation mode and corrupt the next warning line.  Use the
    ** base-library checker so direct pcall(warn, ...) reports 'warn', not '?'.
    */
    base_checkstr_named54(L, (int)i+1, "warn");
  }
#else
  if (n == 0)
    lj_err_argt(L, 1, LUA_TSTRING);
  for (i = 0; i < n; i++) {
    /* Lua 5.4 validates every argument before composing a warning. If a later
    ** argument errors, emitting earlier pieces would leave the default warning
    ** writer in continuation mode and corrupt the next warning line.
    */
    lj_lib_checkstr(L, (int)i+1);
  }
#endif
  for (i = 0; i < n; i++) {
    GCstr *s = strV(L->base + i);
    const char *str = strdata(s);
    /* Route every part through lua_warning().  Lua 5.4 treats @on/@off as
    ** control messages only in the warning function, and only when the part is
    ** not continued; user-installed callbacks must see those strings verbatim.
    */
    lua_warning(L, str, i != n-1);
  }
  return 0;
}

/* -- Base library: getters and setters ----------------------------------- */

LJLIB_ASM_(getmetatable)	LJLIB_REC(.)
/* Recycle the lj_lib_checkany(L, 1) from assert. */

LJLIB_ASM(setmetatable)		LJLIB_REC(.)
{
#if LJ_54
  GCtab *t = base_checktab_named54(L, 1, "setmetatable");
  GCtab *mt;
  TValue *mo = L->base+1;
  if (mo >= L->top) {
    /* Lua 5.4 distinguishes an omitted metatable from an explicit nil:
    ** setmetatable(t) is an argument error, setmetatable(t, nil) clears it.
    */
    base_argtype_named54(L, 2, "setmetatable", "nil or table");
    mt = NULL;  /* unreachable */
  } else if (tvisnil(mo)) {
    mt = NULL;
  } else if (tvistab(mo)) {
    mt = tabV(mo);
  } else {
    base_argtype_named54(L, 2, "setmetatable", "nil or table");
    mt = NULL;  /* unreachable */
  }
#else
  GCtab *t = lj_lib_checktab(L, 1);
  GCtab *mt = lj_lib_checktabornil(L, 2);
#endif
  if (!tvisnil(lj_meta_lookup(L, L->base, MM_metatable)))
    lj_err_caller(L, LJ_ERR_PROTMT);
  setgcref(t->metatable, obj2gco(mt));
  if (mt) {
    lj_gc_objbarriert(L, t, mt);
#if LJ_54
    /* Keep the fast setmetatable path in sync with lua_setmetatable():
    ** tables only become finalizable if __gc existed when mt was assigned.
    */
    {
      global_State *g = G(L);
      cTValue *gc = lj_tab_getstr(mt, mmname_str(g, MM_gc));
      if (gc && !tvisnil(gc)) {
	t->flags54 = (uint8_t)((t->flags54 | LJ_TAB_HAS_GC) &
			       (uint8_t)~LJ_TAB_GC_PENDING);
	lj_gc_arm_finalizer54(g);
#if LJ_HASJIT
	/* LuaJIT traces do not run table finalizers until the trace exits. Keep
	** the function that armed __gc interpreted so allocation-driven finalizer
	** loops observe the callback like PUC Lua 5.4.
	*/
	if (!(g->hookmask & HOOK_GC))
	  luaJIT_setmode(L, 0, LUAJIT_MODE_FUNC|LUAJIT_MODE_OFF);
#endif
      }
    }
#endif
  }
  settabV(L, L->base-1-LJ_FR2, t);
  return FFH_RES(1);
}

LJLIB_CF(getfenv)		LJLIB_REC(.)
{
  GCfunc *fn;
  cTValue *o = L->base;
  if (!(o < L->top && tvisfunc(o))) {
    int level = lj_lib_optint(L, 1, 1);
    if (level < 0)
      lj_err_arg(L, 1, LJ_ERR_INVLVL);
    o = lj_debug_frame(L, level, &level);
    if (o == NULL)
      lj_err_arg(L, 1, LJ_ERR_INVLVL);
    if (LJ_FR2) o--;
  }
  fn = &gcval(o)->fn;
  settabV(L, L->top++, isluafunc(fn) ? tabref(fn->l.env) : tabref(L->env));
  return 1;
}

LJLIB_CF(setfenv)
{
  GCfunc *fn;
  GCtab *t = lj_lib_checktab(L, 2);
  cTValue *o = L->base;
  if (!(o < L->top && tvisfunc(o))) {
    int level = lj_lib_checkint(L, 1);
    if (level == 0) {
      /* NOBARRIER: A thread (i.e. L) is never black. */
      setgcref(L->env, obj2gco(t));
      return 0;
    }
    if (level < 0)
      lj_err_arg(L, 1, LJ_ERR_INVLVL);
    o = lj_debug_frame(L, level, &level);
    if (o == NULL)
      lj_err_arg(L, 1, LJ_ERR_INVLVL);
    if (LJ_FR2) o--;
  }
  fn = &gcval(o)->fn;
  if (!isluafunc(fn))
    lj_err_caller(L, LJ_ERR_SETFENV);
  setgcref(fn->l.env, obj2gco(t));
  lj_gc_objbarrier(L, obj2gco(fn), t);
  setfuncV(L, L->top++, fn);
  return 1;
}

LJLIB_ASM(rawget)		LJLIB_REC(.)
{
#if LJ_54
  base_checktab_named54(L, 1, "rawget");
  base_checkany_named54(L, 2, "rawget");
#else
  lj_lib_checktab(L, 1);
  lj_lib_checkany(L, 2);
#endif
  return FFH_UNREACHABLE;
}

#if LJ_54
static int lj_cf_rawget54(lua_State *L)
{
  GCtab *t = base_checktab_named54(L, 1, "rawget");
  base_checkany_named54(L, 2, "rawget");
  if (base_isglobalenv(L, t) && base_isenvkey(L->base+1)) {
    /* Keep the current compatibility binding usable for bare _ENV lookups,
    ** but do not expose it as a raw global table entry.
    */
    lua_pushnil(L);
    return 1;
  }
  lua_settop(L, 2);
  lua_rawget(L, 1);
  return 1;
}
#endif

LJLIB_CF(rawset)		LJLIB_REC(.)
{
#if LJ_54
  base_checktab_named54(L, 1, "rawset");
  base_checkany_named54(L, 2, "rawset");
  base_checkany_named54(L, 3, "rawset");
  /* Nil keys are still rejected by lua_rawset() itself with the VM error. */
  lua_settop(L, 3);
  lua_rawset(L, 1);
#else
  lj_lib_checktab(L, 1);
  lj_lib_checkany(L, 2);
  L->top = 1+lj_lib_checkany(L, 3);
  lua_rawset(L, 1);
#endif
  return 1;
}

LJLIB_CF(rawequal)		LJLIB_REC(.)
{
#if LJ_54
  cTValue *o1;
  cTValue *o2;
  base_checkany_named54(L, 1, "rawequal");
  base_checkany_named54(L, 2, "rawequal");
  o1 = L->base;
  o2 = L->base+1;
#else
  cTValue *o1 = lj_lib_checkany(L, 1);
  cTValue *o2 = lj_lib_checkany(L, 2);
#endif
  setboolV(L->top-1, lj_obj_equal(o1, o2));
  return 1;
}

#if LJ_52
LJLIB_CF(rawlen)		LJLIB_REC(.)
{
  cTValue *o = L->base;
  int32_t len;
  if (L->top > o && tvisstr(o))
    len = (int32_t)strV(o)->len;
#if LJ_54
  else {
    if (!(L->top > o && tvistab(o)))
      base_argtype_named54(L, 1, "rawlen", "table or string");
    len = (int32_t)lj_tab_len(tabV(o));
  }
#else
  else {
    len = (int32_t)lj_tab_len(lj_lib_checktab(L, 1));
  }
#endif
  setintV(L->top-1, len);
  return 1;
}
#endif

LJLIB_CF(unpack)
{
  GCtab *t = lj_lib_checktab(L, 1);
  int32_t n, i = lj_lib_optint(L, 2, 1);
  int32_t e = (L->base+3-1 < L->top && !tvisnil(L->base+3-1)) ?
	      lj_lib_checkint(L, 3) : (int32_t)lj_tab_len(t);
  uint32_t nu;
  if (i > e) return 0;
  nu = (uint32_t)e - (uint32_t)i;
  n = (int32_t)(nu+1);
  if (nu >= LUAI_MAXCSTACK || !lua_checkstack(L, n))
    lj_err_caller(L, LJ_ERR_UNPACK);
  do {
    cTValue *tv = lj_tab_getint(t, i);
    if (tv) {
      copyTV(L, L->top++, tv);
    } else {
      setnilV(L->top++);
    }
    if (i >= e) break;
    i++;
  } while (1);
  return n;
}

LJLIB_CF(select)		LJLIB_REC(.)
{
  int32_t n = (int32_t)(L->top - L->base);
  if (n >= 1 && tvisstr(L->base) && *strVdata(L->base) == '#') {
    setintV(L->top-1, n-1);
    return 1;
  } else {
#if LJ_54
    lua_Integer i = base_checkinteger_named54(L, 1, "select");
#else
    int32_t i = lj_lib_checkint(L, 1);
#endif
    if (i < 0) i = n + i; else if (i > n) i = n;
    if (i < 1)
#if LJ_54
      base_argerror_named54(L, 1, "select", "index out of range");
#else
      lj_err_arg(L, 1, LJ_ERR_IDXRNG);
#endif
    return n - i;
  }
}

/* -- Base library: conversions ------------------------------------------- */

LJLIB_ASM(tonumber)		LJLIB_REC(.)
{
#if LJ_54
  /* The explicit base is an integer parameter in Lua 5.4; do not silently
  ** truncate fractions before the range check.
  */
  int hasbase = (L->base+1 < L->top && !tvisnil(L->base+1));
  lua_Integer ibase = hasbase ? base_checkinteger_named54(L, 2, "tonumber") : 10;
  int32_t base = 10;
#else
  int32_t base = lj_lib_optint(L, 2, 10);
#endif
#if LJ_54
  if (!hasbase) {
#else
  if (base == 10) {
#endif
    TValue *o = lj_lib_checkany(L, 1);
#if LJ_54
    if (tvisstr(o)) {
      GCstr *s = strV(o);
      TValue tmp;
      StrScanFmt fmt;
      if (lj_strscan_rejectnum54(strdata(s), s->len))
	goto badbase;
      fmt = lj_strscan_scan((const uint8_t *)strdata(s), s->len, &tmp,
			    STRSCAN_OPT_TOINT);
      if (fmt == STRSCAN_INT) {
	setintV(L->base-1-LJ_FR2, tmp.i);
	return FFH_RES(1);
      } else if (fmt == STRSCAN_I64) {
	lj_obj_setint64(L, L->base-1-LJ_FR2, (int64_t)tmp.u64);
	return FFH_RES(1);
      } else if (fmt == STRSCAN_NUM) {
	setnumV(L->base-1-LJ_FR2, tmp.n);
	return FFH_RES(1);
      }
      goto badbase;
    }
#endif
    if (lj_strscan_numberobj(o)) {
      copyTV(L, L->base-1-LJ_FR2, o);
      return FFH_RES(1);
    }
#if LJ_HASFFI
    if (tviscdata(o)) {
      CTState *cts = ctype_cts(L);
      CType *ct = lj_ctype_rawref(cts, cdataV(o)->ctypeid);
      if (ctype_isenum(ct->info)) ct = ctype_child(cts, ct);
      if (ctype_isnum(ct->info) || ctype_iscomplex(ct->info)) {
	if (LJ_DUALNUM && ctype_isinteger_or_bool(ct->info) &&
	    ct->size <= 4 && !(ct->size == 4 && (ct->info & CTF_UNSIGNED))) {
	  int32_t i;
	  lj_cconv_ct_tv(cts, ctype_get(cts, CTID_INT32), (uint8_t *)&i, o, 0);
	  setintV(L->base-1-LJ_FR2, i);
	  return FFH_RES(1);
	}
	lj_cconv_ct_tv(cts, ctype_get(cts, CTID_DOUBLE),
		       (uint8_t *)&(L->base-1-LJ_FR2)->n, o, 0);
	return FFH_RES(1);
      }
    }
#endif
  } else {
#if LJ_54
    GCstr *s;
    const char *p, *pe;
    lua_Unsigned u = 0;
#else
    const char *p = strdata(lj_lib_checkstr(L, 1));
    char *ep;
#endif
    unsigned int neg = 0;
#if !LJ_54
    unsigned long ul;
#endif
#if LJ_54
    if (ibase < 2 || ibase > 36)
      base_argerror_named54(L, 2, "tonumber", "base out of range");
    base = (int32_t)ibase;
#else
    if (base < 2 || base > 36)
      lj_err_arg(L, 2, LJ_ERR_BASERNG);
#endif
#if LJ_54
    /* With an explicit base, Lua 5.4 requires the first argument to be an
    ** actual string and scans the full Lua string length, so embedded NUL
    ** bytes are invalid trailing data instead of C string terminators.
    */
    s = base_checkstr_exact_named54(L, 1, "tonumber");
    p = strdata(s);
    pe = p + s->len;
    while (p < pe && lj_char_isspace((unsigned char)(*p))) p++;
    if (p < pe && *p == '-') { p++; neg = 1; }
    else if (p < pe && *p == '+') { p++; }
    if (p < pe && lj_char_isalnum((unsigned char)(*p))) {
      do {
	uint32_t digit = lj_char_isdigit((unsigned char)*p) ?
			 (uint32_t)(*p - '0') :
			 (uint32_t)((*p | 0x20) - 'a' + 10);
	if (digit >= (uint32_t)base)
	  goto badbase;
	u = u * (lua_Unsigned)base + (lua_Unsigned)digit;
	p++;
      } while (p < pe && lj_char_isalnum((unsigned char)(*p)));
      while (p < pe && lj_char_isspace((unsigned char)(*p))) p++;
      if (p == pe) {
	lj_obj_setint64(L, L->base-1-LJ_FR2,
	  (int64_t)(lua_Integer)(neg ? ~u+1u : u));
	return FFH_RES(1);
      }
    }
#else
    while (lj_char_isspace((unsigned char)(*p))) p++;
    if (*p == '-') { p++; neg = 1; } else if (*p == '+') { p++; }
    if (lj_char_isalnum((unsigned char)(*p))) {
      /* C strtoul accepts a 0x prefix for base 16, but Lua 5.4's tonumber
      ** with an explicit base treats the prefix as ordinary invalid input.
      */
      if (LJ_54 && base == 16 && p[0] == '0' && ((p[1] | 0x20) == 'x'))
	goto badbase;
      ul = strtoul(p, &ep, base);
      if (p != ep) {
	while (lj_char_isspace((unsigned char)(*ep))) ep++;
	if (*ep == '\0') {
	  if (LJ_DUALNUM && LJ_LIKELY(ul < 0x80000000u+neg)) {
	    if (neg) ul = ~ul+1u;
	    setintV(L->base-1-LJ_FR2, (int32_t)ul);
	  } else {
	    lua_Number n = (lua_Number)ul;
	    if (neg) n = -n;
	    setnumV(L->base-1-LJ_FR2, n);
	  }
	  return FFH_RES(1);
	}
      }
    }
#endif
  }
badbase:
  setnilV(L->base-1-LJ_FR2);
  return FFH_RES(1);
}

LJLIB_ASM(tostring)		LJLIB_REC(.)
{
#if LJ_54
  TValue *o = L->base;
  base_checkany_named54(L, 1, "tostring");
#else
  TValue *o = lj_lib_checkany(L, 1);
#endif
  cTValue *mo;
  L->top = o+1;  /* Only keep one argument. */
  if (!tvisnil(mo = lj_meta_lookup(L, o, MM_tostring))) {
#if LJ_54
    copyTV(L, L->top++, mo);
    copyTV(L, L->top++, o);
    lua_call(L, 1, 1);
    /* Lua 5.4 follows luaL_tolstring(): __tostring may return a string or
    ** number, but other values are a hard error.
    */
    if (tvisnumber(L->top-1) || tvisi64(L->top-1)) {
      GCstr *s = lj_strfmt_obj(L, L->top-1);
      setstrV(L, L->top-1, s);
    } else if (!tvisstr(L->top-1)) {
      lj_err_callermsg(L, "'__tostring' must return a string");
    }
    copyTV(L, L->base-1-LJ_FR2, L->top-1);
    return FFH_RES(1);
#else
    copyTV(L, L->base-1-LJ_FR2, mo);  /* Replace callable. */
    return FFH_TAILCALL;
#endif
  }
  lj_gc_check(L);
  setstrV(L, L->base-1-LJ_FR2, lj_strfmt_obj(L, L->base));
  return FFH_RES(1);
}

/* -- Base library: throw and catch errors -------------------------------- */

LJLIB_CF(error)
{
#if LJ_54
  lua_Integer level = (L->base+1 < L->top && !tvisnil(L->base+1)) ?
		      base_checkinteger_named54(L, 2, "error") : 1;
#else
  int32_t level = lj_lib_optint(L, 2, 1);
#endif
  lua_settop(L, 1);
  if (
#if LJ_54
      tvisstr(L->base) &&
#else
      lua_isstring(L, 1) &&
#endif
      level > 0) {
    luaL_where(L, level > 2147483647LL ? 2147483647 : (int)level);
    lua_pushvalue(L, 1);
    lua_concat(L, 2);
  }
  return lua_error(L);
}

LJLIB_ASM(pcall)		LJLIB_REC(.)
{
#if LJ_54
  const char *fname = curr_func(L)->c.ffid == FF_xpcall ? "xpcall" : "pcall";
  base_checkany_named54(L, 1, fname);
  if (curr_func(L)->c.ffid == FF_xpcall)
    base_checkfunc_named54(L, 2, fname);
#else
  lj_lib_checkany(L, 1);
  lj_lib_checkfunc(L, 2);  /* For xpcall only. */
#endif
  return FFH_UNREACHABLE;
}
LJLIB_ASM_(xpcall)		LJLIB_REC(.)

/* -- Base library: load Lua code ----------------------------------------- */

static int load_aux(lua_State *L, int status, int envarg, int hasenv)
{
  if (status == LUA_OK) {
    /*
    ** Set environment table for top-level function.
    ** Don't do this for non-native bytecode, which returns a prototype.
    */
    if (tvisfunc(L->top-1)) {
      GCfunc *fn = funcV(L->top-1);
      TValue *env = L->base + envarg - 1;
#if LJ_54
      if (hasenv) {
	if (tvistab(env)) {
	  GCtab *t = tabV(env);
	  setgcref(fn->c.env, obj2gco(t));
	  lj_gc_objbarrier(L, fn, t);
	}
	/* Lua 5.4's load env argument initializes the first real upvalue to
	** the exact argument value. Keep fn->c.env table-only for legacy
	** VGLOBAL bytecode, but do not discard non-table upvalue envs.
	*/
	lj_func_inituv_env(L, fn, env);
      }
#else
      if (tvistab(env)) {
	GCtab *t = tabV(env);
	setgcref(fn->c.env, obj2gco(t));
	lj_gc_objbarrier(L, fn, t);
      }
#endif
    }
    return 1;
  } else {
    setnilV(L->top-2);
    return 2;
  }
}

LJLIB_CF(loadfile)
{
  int hasenv = (int)(L->top - L->base) >= 3;
#if LJ_54
  GCstr *fname = base_optstr_named54(L, 1, "loadfile");
  GCstr *mode = base_optstr_named54(L, 2, "loadfile");
#else
  GCstr *fname = lj_lib_optstr(L, 1);
  GCstr *mode = lj_lib_optstr(L, 2);
#endif
  int status;
  lua_settop(L, 3);  /* Ensure env arg exists. */
  status = luaL_loadfilex(L, fname ? strdata(fname) : NULL,
			  mode ? strdata(mode) : NULL);
  return load_aux(L, status, 3, hasenv);
}

#if LJ_54
typedef struct LoadReaderCtx {
  MSize func;
  MSize chunk;
  MSize where;
} LoadReaderCtx;
#endif

static const char *reader_func(lua_State *L, void *ud, size_t *size)
{
#if LJ_54
  LoadReaderCtx *ctx = (LoadReaderCtx *)ud;
#endif
  luaL_checkstack(L, 2, "too many nested functions");
#if LJ_54
  if (ctx != NULL)
    copyTV(L, L->top++, tvref(L->stack) + ctx->func);
  else
#endif
  copyTV(L, L->top++, L->base);
  lua_call(L, 0, 1);  /* Call user-supplied function. */
#if LJ_54
  if (ctx != NULL) {
    TValue *o = L->top-1;
    if (tvisnil(o)) {
      L->top--;
      *size = 0;
      return NULL;
    } else if (tvisstr(o) || tvisnumber(o) || tvisi64(o)) {
      GCstr *s;
      if (tvisstr(o)) {
	s = strV(o);
      } else {
	s = lj_strfmt_number(L, o);
      }
      setstrV(L, tvref(L->stack) + ctx->chunk, s);
      L->top--;
      *size = s->len;
      return strdata(s);
    } else {
      TValue *where = tvref(L->stack) + ctx->where;
      if (tvisstr(where)) {
	lj_strfmt_pushf(L, "%s%s", strVdata(where), err2msg(LJ_ERR_RDRSTR));
	lua_error(L);
      }
      lj_err_caller(L, LJ_ERR_RDRSTR);
      return NULL;
    }
  }
#endif
  L->top--;
  if (tvisnil(L->top)) {
    *size = 0;
    return NULL;
  } else if (tvisstr(L->top) || tvisnumber(L->top) || tvisi64(L->top)) {
    copyTV(L, L->base+4, L->top);  /* Anchor string in reserved stack slot. */
    return lua_tolstring(L, 5, size);
  } else {
#if LJ_54
    if (ud != NULL && tvisstr(L->base+5)) {
      /* lua_loadx() calls this reader outside the original Lua frame. Keep the
      ** caller location captured by load() so reader type errors match Lua 5.4.
      */
      lua_pushstring(L, err2msg(LJ_ERR_RDRSTR));
      lua_concat(L, 2);
      lua_error(L);
    }
#endif
    lj_err_caller(L, LJ_ERR_RDRSTR);
    return NULL;
  }
}

LJLIB_CF(load)
{
  int hasenv = (int)(L->top - L->base) >= 4;
#if LJ_54
  GCstr *name = base_optstr_named54(L, 2, "load");
  GCstr *mode = base_optstr_named54(L, 3, "load");
#else
  GCstr *name = lj_lib_optstr(L, 2);
  GCstr *mode = lj_lib_optstr(L, 3);
#endif
  int status;
  if (L->base < L->top &&
      (tvisstr(L->base) || tvisnumber(L->base) || tvisi64(L->base) ||
       tvisbuf(L->base))) {
    const char *s;
    MSize len;
    if (tvisbuf(L->base)) {
      SBufExt *sbx = bufV(L->base);
      s = sbx->r;
      len = sbufxlen(sbx);
      if (!name) name = &G(L)->strempty;  /* Buffers are not NUL-terminated. */
    } else {
      GCstr *str = lj_lib_checkstr(L, 1);
      s = strdata(str);
      len = str->len;
    }
    lua_settop(L, 4);  /* Ensure env arg exists. */
    status = luaL_loadbufferx(L, s, len, name ? strdata(name) : s,
			      mode ? strdata(mode) : NULL);
  } else {
#if LJ_54
    base_checkfunc_named54(L, 1, "load");
#else
    lj_lib_checkfunc(L, 1);
#endif
#if LJ_54
    lua_settop(L, 6);  /* Slots 5/6 anchor reader data and caller location. */
    luaL_where(L, 1);
    copyTV(L, L->base+5, L->top-1);
    L->top--;
    {
      LoadReaderCtx ctx;
      TValue *stack = tvref(L->stack);
      ctx.func = (MSize)(L->base - stack);
      ctx.chunk = ctx.func + 4;
      ctx.where = ctx.func + 5;
      status = lua_loadx(L, reader_func, (void *)&ctx,
		       name ? strdata(name) : "=(load)",
		       mode ? strdata(mode) : NULL);
    }
#else
    lua_settop(L, 5);  /* Reserve a slot for the string from the reader. */
    status = lua_loadx(L, reader_func, NULL, name ? strdata(name) : "=(load)",
		       mode ? strdata(mode) : NULL);
#endif
  }
  return load_aux(L, status, 4, hasenv);
}

LJLIB_CF(loadstring)
{
  return lj_cf_load(L);
}

LJLIB_CF(dofile)
{
#if LJ_54
  GCstr *fname = base_optstr_named54(L, 1, "dofile");
#else
  GCstr *fname = lj_lib_optstr(L, 1);
#endif
  setnilV(L->top);
  L->top = L->base+1;
  if (luaL_loadfile(L, fname ? strdata(fname) : NULL) != LUA_OK)
    lua_error(L);
  lua_call(L, 0, LUA_MULTRET);
  return (int)(L->top - L->base) - 1;
}

#if LJ_54
static GCstr *base_optstr_dofile_load54(lua_State *L)
{
  TValue *o = L->base;
  if (o >= L->top || tvisnil(o))
    return NULL;
  if (tvisstr(o))
    return strV(o);
  if (tvisnumber(o) || tvisi64(o)) {
    GCstr *s = lj_strfmt_number(L, o);
    setstrV(L, o, s);
    return s;
  }
  /* dofile() is a Lua wrapper around this C loader to preserve yieldability.
  ** Skip that wrapper so argument errors point at the user's call site.
  */
  luaL_where(L, 2);
  lua_pushfstring(L, "bad argument #1 to 'dofile' (string expected, got %s)",
		  base_argtypename54(L, 1));
  lua_concat(L, 2);
  lua_error(L);
  return NULL;  /* unreachable */
}

static int lj_cf_dofile_load54(lua_State *L)
{
  GCstr *fname = base_optstr_dofile_load54(L);
  setnilV(L->top);
  L->top = L->base+1;
  if (luaL_loadfile(L, fname ? strdata(fname) : NULL) != LUA_OK)
    lua_error(L);
  return 1;
}
#endif

/* -- Base library: GC control -------------------------------------------- */

LJLIB_CF(gcinfo)
{
  setintV(L->top++, (int32_t)(G(L)->gc.total >> 10));
  return 1;
}

LJLIB_CF(collectgarbage)
{
  int opt;
  int32_t data;
#if LJ_54
  if (L->base < L->top && tvisstr(L->base)) {
    GCstr *s = strV(L->base);
    const char *optstr = strdata(s);
    if ((s->len == 12 && memcmp(optstr, "generational", 12) == 0) ||
	(s->len == 11 && memcmp(optstr, "incremental", 11) == 0)) {
      int isgen = (s->len == 12);
      const char *old = G(L)->gc_mode54 ? "generational" : "incremental";
      int32_t data = base_optint_named54(L, 2, "collectgarbage");
      int32_t data2 = base_optint_named54(L, 3, "collectgarbage");
      int32_t data3 = 0;
      if (!isgen)
	data3 = base_optint_named54(L, 4, "collectgarbage");
      if (G(L)->hookmask & HOOK_GC) {
	/* Lua 5.4 makes collectgarbage non-reentrant from __gc callbacks, but
	** still validates the option before returning nil.
	*/
	setnilV(L->top++);
	return 1;
      }
      if (isgen)
	(void)lua_gc(L, LUA_GCGEN, data, data2);
      else
	(void)lua_gc(L, LUA_GCINC, data, data2, data3);
      lua_pushstring(L, old);
      return 1;
    }
  }
#endif
#if LJ_54
  opt = base_checkopt_named54(L, 1, LUA_GCCOLLECT,  /* ORDER LUA_GC* */
    "\4stop\7restart\7collect\5count\1\377\4step\10setpause\12setstepmul\1\377\11isrunning",
    "collectgarbage");
  /* Lua 5.4 ignores extra arguments for no-parameter GC commands such as
  ** "count", "collect", "stop", "restart" and "isrunning". Only commands
  ** that define a numeric tuning argument validate argument #2 here.
  */
  if (opt == LUA_GCSTEP || opt == LUA_GCSETPAUSE ||
      opt == LUA_GCSETSTEPMUL) {
    data = (L->base+1 < L->top && !tvisnil(L->base+1)) ?
	   (int32_t)base_checkinteger_named54(L, 2, "collectgarbage") : 0;
  } else {
    data = 0;
  }
#else
  opt = lj_lib_checkopt(L, 1, LUA_GCCOLLECT,  /* ORDER LUA_GC* */
    "\4stop\7restart\7collect\5count\1\377\4step\10setpause\12setstepmul\1\377\11isrunning");
  data = lj_lib_optint(L, 2, 0);
#endif
#if LJ_54
  if (G(L)->hookmask & HOOK_GC) {
    /* Finalizers run with the collector protected against reentry. Match
    ** Lua 5.4 by returning nil after all argument validation has completed.
    */
    setnilV(L->top++);
    return 1;
  }
#endif
  if (opt == LUA_GCCOUNT) {
    setnumV(L->top, (lua_Number)G(L)->gc.total/1024.0);
  } else {
    int res = lua_gc(L, opt, data);
    if (opt == LUA_GCSTEP || opt == LUA_GCISRUNNING)
      setboolV(L->top, res);
    else
      setintV(L->top, res);
  }
  L->top++;
  return 1;
}

/* -- Base library: miscellaneous functions ------------------------------- */

LJLIB_PUSH(top-2)  /* Upvalue holds weak table. */
LJLIB_CF(newproxy)
{
  lua_settop(L, 1);
  lua_newuserdata(L, 0);
  if (lua_toboolean(L, 1) == 0) {  /* newproxy(): without metatable. */
    return 1;
  } else if (lua_isboolean(L, 1)) {  /* newproxy(true): with metatable. */
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_pushboolean(L, 1);
    lua_rawset(L, lua_upvalueindex(1));  /* Remember mt in weak table. */
  } else {  /* newproxy(proxy): inherit metatable. */
    int validproxy = 0;
    if (lua_getmetatable(L, 1)) {
      lua_rawget(L, lua_upvalueindex(1));
      validproxy = lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
    if (!validproxy)
      lj_err_arg(L, 1, LJ_ERR_NOPROXY);
    lua_getmetatable(L, 1);
  }
  lua_setmetatable(L, 2);
  return 1;
}

#if !LJ_54
LJLIB_PUSH("tostring")
#endif
LJLIB_CF(print)
{
  ptrdiff_t i, nargs = L->top - L->base;
#if LJ_54
  for (i = 0; i < nargs; i++) {
    size_t size;
    /* Lua 5.4 print() uses the protected internal tostring conversion, not the
    ** mutable global "tostring"; rebinding the global must not affect output.
    */
    const char *str = luaL_tolstring(L, (int)i+1, &size);
    if (i)
      putchar('\t');
    fwrite(str, 1, size, stdout);
    L->top--;
  }
#else
  cTValue *tv = lj_tab_getstr(tabref(L->env), strV(lj_lib_upvalue(L, 1)));
  int shortcut;
  if (tv && !tvisnil(tv)) {
    copyTV(L, L->top++, tv);
  } else {
    setstrV(L, L->top++, strV(lj_lib_upvalue(L, 1)));
    lua_gettable(L, LUA_GLOBALSINDEX);
    tv = L->top-1;
  }
  shortcut = (tvisfunc(tv) && funcV(tv)->c.ffid == FF_tostring) &&
	     !gcrefu(basemt_it(G(L), LJ_TNUMX));
  for (i = 0; i < nargs; i++) {
    cTValue *o = &L->base[i];
    const char *str;
    size_t size;
    MSize len;
    if (shortcut && (str = lj_strfmt_wstrnum(L, o, &len)) != NULL) {
      size = len;
    } else {
      copyTV(L, L->top+1, o);
      copyTV(L, L->top, L->top-1);
      L->top += 2;
      lua_call(L, 1, 1);
      str = lua_tolstring(L, -1, &size);
      if (!str)
	lj_err_caller(L, LJ_ERR_PRTOSTR);
      L->top--;
    }
    if (i)
      putchar('\t');
    fwrite(str, 1, size, stdout);
  }
#endif
  putchar('\n');
  return 0;
}

LJLIB_PUSH(top-3)
LJLIB_SET(_VERSION)

#include "lj_libdef.h"

/* -- Coroutine library --------------------------------------------------- */

#define LJLIB_MODULE_coroutine

LJLIB_CF(coroutine_status)
{
  const char *s;
  lua_State *co;
  if (!(L->top > L->base && tvisthread(L->base))) {
#if LJ_54
    base_argtype_callname54(L, 1, "coroutine.status", "thread");
#else
    lj_err_arg(L, 1, LJ_ERR_NOCORO);
#endif
  }
  co = threadV(L->base);
  if (co == L) s = "running";
  else if (co->status == LUA_YIELD) s = "suspended";
  else if (co->status != LUA_OK) s = "dead";
  else if (co->base > tvref(co->stack)+1+LJ_FR2) s = "normal";
  else if (co->top == co->base) s = "dead";
  else s = "suspended";
  lua_pushstring(L, s);
  return 1;
}

LJLIB_CF(coroutine_running)
{
#if LJ_52
  int ismain = lua_pushthread(L);
  setboolV(L->top++, ismain);
  return 2;
#else
  if (lua_pushthread(L))
    setnilV(L->top++);
  return 1;
#endif
}

LJLIB_CF(coroutine_isyieldable)
{
#if LJ_54
  if (L->base < L->top) {
    lua_State *co;
    if (!tvisthread(L->base))
      base_argtype_callname54(L, 1, "coroutine.isyieldable", "thread");
    co = threadV(L->base);
    /* Lua 5.4's optional thread argument asks about that coroutine, not the
    ** currently executing C frame. Suspended/dead non-main coroutines have no
    ** C frame, so they are yieldable by the public library definition.
    */
    setboolV(L->top++, co != mainthread(G(L)) &&
	     (co->cframe == NULL || cframe_canyield(co->cframe)));
    return 1;
  }
#endif
  setboolV(L->top++, cframe_canyield(L->cframe));
  return 1;
}

LJLIB_CF(coroutine_create)
{
  lua_State *L1;
  if (!(L->base < L->top && tvisfunc(L->base))) {
#if LJ_54
    base_argtype_callname54(L, 1, "coroutine.create", "function");
#else
    lj_err_argt(L, 1, LUA_TFUNCTION);
#endif
  }
  L1 = lua_newthread(L);
  setfuncV(L, L1->top++, funcV(L->base));
  return 1;
}

#if LJ_54
static int lua54_close_depth = 0;

static int lj_cf_coroutine_close(lua_State *L)
{
  lua_State *co;
  if (!(L->top > L->base && tvisthread(L->base)))
    base_argtype_named54(L, 1, "coroutine.close", "thread");
  co = threadV(L->base);
  if (co == L)
    lj_err_callermsg(L, "cannot close a running coroutine");
  if (co->cframe != NULL ||
      (co->status == LUA_OK && co->base > tvref(co->stack)+1+LJ_FR2))
    lj_err_callermsg(L, "cannot close a normal coroutine");
  {
    int status;
    if (lua54_close_depth >= 180)
      lj_err_callermsg(L, "C stack overflow");
    /* Chained __close handlers can recursively close older coroutines. Cap the
    ** library recursion before the native C stack is exhausted, matching Lua
    ** 5.4's observable C-stack overflow surface.
    */
    lua54_close_depth++;
    status = lua_closethread(co, L);
    lua54_close_depth--;
    if (status == LUA_OK) {
      setboolV(L->top++, 1);
      return 1;
    }
    setboolV(L->top++, 0);
    if (co->top > co->base)
      copyTV(L, L->top++, co->top-1);
    else
      setnilV(L->top++);
    /* lua_closethread() keeps the error object on the coroutine stack for the
    ** C API. coroutine.close moves that object to the caller in Lua 5.4, so
    ** clear the coroutine stack afterwards to make status() report "dead".
    */
    co->top = co->base;
    return 2;
  }
}

static int lua54_wrap_depth = 0;

static int lj_cf_coroutine_wrap_aux54(lua_State *L)
{
  lua_State *co = lua_tothread(L, lua_upvalueindex(1));
  int nargs = (int)(L->top - L->base);
  int nres = 0;
  int status;
  if (lua54_wrap_depth >= 180)
    lj_err_callermsg(L, "C stack overflow");
  if (!lua_checkstack(co, nargs))
    lj_err_mem(L);
  lua_xmove(L, co, nargs);
  lua54_wrap_depth++;
  status = lua_resume54(co, L, nargs, &nres);
  lua54_wrap_depth--;
  if (status == LUA_OK || status == LUA_YIELD) {
    if (!lua_checkstack(L, nres))
      lj_err_mem(L);
    lua_xmove(co, L, nres);
    return nres;
  }
  /* Lua 5.4's coroutine.wrap closes a coroutine that died with an error before
  ** rethrowing. This runs pending <close> variables and lets their errors
  ** replace the original coroutine error, matching lcorolib.c:luaB_auxwrap.
  */
  if (co->status != LUA_OK && co->status != LUA_YIELD)
    status = lua_closethread(co, L);
  if (co->top > co->base)
    lua_xmove(co, L, 1);
  else
    setnilV(L->top++);
  return lua_error(L);  /* propagate the coroutine or close error object */
}

static int lj_cf_coroutine_wrap54(lua_State *L)
{
  if (!(L->base < L->top && tvisfunc(L->base)))
    base_argtype_callname54(L, 1, "coroutine.wrap", "function");
  lua_newthread(L);
  setfuncV(L, threadV(L->top-1)->top++, funcV(L->base));
  lua_pushcclosure(L, lj_cf_coroutine_wrap_aux54, 1);
  return 1;
}
#endif

LJLIB_ASM(coroutine_yield)
{
#if LJ_54
  if (L == mainthread(G(L)))
    lj_err_callermsg(L, "attempt to yield from outside a coroutine");
#endif
  lj_err_caller(L, LJ_ERR_CYIELD);
  return FFH_UNREACHABLE;
}

static int ffh_resume(lua_State *L, lua_State *co, int wrap)
{
  if (co->cframe != NULL || co->status > LUA_YIELD ||
      (co->status == LUA_OK && co->top == co->base)) {
#if LJ_54
    /* Lua 5.4 reports a currently running coroutine as non-suspended for
    ** resume/wrap. coroutine.close keeps its separate "running" diagnostic.
    ** Prefer the dead diagnostic once status is terminal, even if a failed
    ** close path left a stale cframe marker behind.
    */
    ErrMsg em = (co->status > LUA_YIELD ||
		 (co->status == LUA_OK && co->top == co->base)) ?
		LJ_ERR_CODEAD : LJ_ERR_COSUSP;
#else
    ErrMsg em = co->cframe ? LJ_ERR_CORUN : LJ_ERR_CODEAD;
#endif
    if (wrap) lj_err_caller(L, em);
    setboolV(L->base-1-LJ_FR2, 0);
    setstrV(L, L->base-LJ_FR2, lj_err_str(L, em));
    return FFH_RES(2);
  }
  if (lj_state_cpgrowstack(co, (MSize)(L->top - L->base)) != LUA_OK) {
    cTValue *msg = --co->top;
    lj_err_callermsg(L, strVdata(msg));
  }
  return FFH_RETRY;
}

LJLIB_ASM(coroutine_resume)
{
  if (!(L->top > L->base && tvisthread(L->base))) {
#if LJ_54
    base_argtype_named54(L, 1, "coroutine.resume", "thread");
#else
    lj_err_arg(L, 1, LJ_ERR_NOCORO);
#endif
  }
  return ffh_resume(L, threadV(L->base), 0);
}

LJLIB_NOREG LJLIB_ASM(coroutine_wrap_aux)
{
  return ffh_resume(L, threadV(lj_lib_upvalue(L, 1)), 1);
}

/* Inline declarations. */
LJ_ASMF void lj_ff_coroutine_wrap_aux(void);
#if !(LJ_TARGET_MIPS && defined(ljamalg_c))
LJ_FUNCA_NORET void LJ_FASTCALL lj_ffh_coroutine_wrap_err(lua_State *L,
							  lua_State *co);
#endif

/* Error handler, called from assembler VM. */
void LJ_FASTCALL lj_ffh_coroutine_wrap_err(lua_State *L, lua_State *co)
{
  co->top--; copyTV(L, L->top, co->top); L->top++;
  if (tvisstr(L->top-1))
    lj_err_callermsg(L, strVdata(L->top-1));
  else
    lj_err_run(L);
}

/* Forward declaration. */
static void setpc_wrap_aux(lua_State *L, GCfunc *fn);

LJLIB_CF(coroutine_wrap)
{
  GCfunc *fn;
#if LJ_54
  if (!(L->base < L->top && tvisfunc(L->base)))
    base_argtype_callname54(L, 1, "coroutine.wrap", "function");
  lua_newthread(L);
  setfuncV(L, threadV(L->top-1)->top++, funcV(L->base));
#else
  lj_cf_coroutine_create(L);
#endif
  fn = lj_lib_pushcc(L, lj_ffh_coroutine_wrap_aux, FF_coroutine_wrap_aux, 1);
  setpc_wrap_aux(L, fn);
  return 1;
}

#include "lj_libdef.h"

/* Fix the PC of wrap_aux. Really ugly workaround. */
static void setpc_wrap_aux(lua_State *L, GCfunc *fn)
{
  setmref(fn->c.pc, &L2GG(L)->bcff[lj_lib_init_coroutine[1]+2]);
}

#if LJ_54
static int lj_cf_coroutine_argerror54(lua_State *L)
{
  const char *fallback = luaL_checkstring(L, 1);
  int nargs = luaL_checkint(L, 2);
  const char *got = nargs == 0 ? "no value" : base_argtypename54(L, 3);
  const char *name = fallback;
  lua_Debug ar;
  /* resume()/close() keep a Lua wrapper for recursion caps and close-error
  ** bookkeeping. This private helper skips that wrapper frame so argument
  ** errors still point at the user's call site or local alias.
  */
  if (lua_getstack(L, 2, &ar) && lua_getinfo(L, "n", &ar) &&
      ar.name && ar.name[0] != '\0')
    name = ar.name;
  luaL_where(L, 3);
  lua_pushfstring(L, "bad argument #1 to '%s' (thread expected, got %s)",
		  name, got);
  lua_concat(L, 2);
  return lua_error(L);
}
#endif

/* ------------------------------------------------------------------------ */

static void newproxy_weaktable(lua_State *L)
{
  /* NOBARRIER: The table is new (marked white). */
  GCtab *t = lj_tab_new(L, 0, 1);
  settabV(L, L->top++, t);
  setgcref(t->metatable, obj2gco(t));
  setstrV(L, lj_tab_setstr(L, t, lj_str_newlit(L, "__mode")),
	    lj_str_newlit(L, "kv"));
  t->nomm = (uint8_t)(~(1u<<MM_mode));
}

#if LJ_54
static int lj_cf_getmetatable54(lua_State *L)
{
  base_checkany_named54(L, 1, "getmetatable");
  if (luaL_getmetafield(L, 1, "__metatable"))
    return 1;
  if (!lua_getmetatable(L, 1))
    lua_pushnil(L);
  return 1;
}
#endif

LUALIB_API int luaopen_coroutine(lua_State *L)
{
  LJ_LIB_REG(L, LUA_COLIBNAME, coroutine);
#if LJ_54
  /* coroutine.close is a Lua 5.4 addition and is installed here too so the
  ** standalone luaopen_coroutine() entry matches luaL_openlibs().
  */
  lua_pushcfunction(L, lj_cf_coroutine_close);
  lua_setfield(L, -2, "close");
  lua_pushcfunction(L, lj_cf_coroutine_wrap54);
  lua_setfield(L, -2, "wrap");
  lua_pushcfunction(L, lj_cf_coroutine_argerror54);
  lua_setfield(L, -2, "_lua54_argerror");
  /* Keep the VM fast-function resume path for the actual coroutine transfer.
  ** The Lua wrapper caps recursive resume chains before Windows reaches a
  ** native guard page and remembers dead-coroutine errors for close().
  */
  luaL_loadstring(L,
    "local argerror = coroutine._lua54_argerror\n"
    "coroutine._lua54_argerror = nil\n"
    "local resume = coroutine.resume\n"
    "local close = coroutine.close\n"
    "local status = coroutine.status\n"
    "local select = select\n"
    "local type = type\n"
    "local error = error\n"
    "local resume_errors = setmetatable({}, { __mode = 'k' })\n"
    "local depth = 0\n"
    "local function checkthread(name, nargs, co)\n"
    "  if type(co) ~= 'thread' then argerror(name, nargs, co) end\n"
    "end\n"
    "local function finish(co, ok, ...)\n"
    "  depth = depth - 1\n"
    "  if ok == false and status(co) == 'dead' then resume_errors[co] = (...) end\n"
    "  return ok, ...\n"
    "end\n"
    "function coroutine.resume(...)\n"
    "  local nargs = select('#', ...)\n"
    "  local co = ...\n"
    "  checkthread('coroutine.resume', nargs, co)\n"
    "  if depth >= 180 then return false, 'C stack overflow' end\n"
    "  depth = depth + 1\n"
    "  return finish(co, resume(co, select(2, ...)))\n"
    "end\n"
    "function coroutine.close(...)\n"
    "  local nargs = select('#', ...)\n"
    "  local co = ...\n"
    "  checkthread('coroutine.close', nargs, co)\n"
    "  local st = status(co)\n"
    "  if st == 'running' then error('cannot close a running coroutine', 2) end\n"
    "  if st == 'normal' then error('cannot close a normal coroutine', 2) end\n"
    "  local ok, err = close(co)\n"
    "  if ok ~= false then\n"
    "    resume_errors[co] = nil\n"
    "    return ok\n"
    "  end\n"
    "  if resume_errors[co] ~= nil then\n"
    "    err = resume_errors[co]\n"
    "  end\n"
    "  resume_errors[co] = nil\n"
    "  -- Lua 5.4 returns one value on successful close, but two on error.\n"
    "  return ok, err\n"
    "end\n");
  lua_call(L, 0, 0);
#endif
  return 1;
}

#if LJ_54
int luaopen_base_luajit(lua_State *L)
#else
LUALIB_API int luaopen_base(lua_State *L)
#endif
{
  /* NOBARRIER: Table and value are the same. */
  GCtab *env = tabref(L->env);
  settabV(L, lj_tab_setstr(L, env, lj_str_newlit(L, "_G")), env);
  lua_pushliteral(L, LUA_VERSION);  /* top-3. */
  newproxy_weaktable(L);  /* top-2. */
  LJ_LIB_REG(L, "_G", base);
#if LJ_54
  /* Lua 5.4 exposes the current global environment as _ENV. Lexical _ENV
  ** shadowing is handled by the parser, but the global binding is visible too.
  */
  settabV(L, lj_tab_setstr(L, env, lj_str_newlit(L, "_ENV")), env);
  /* Lua 5.4 compatibility mode should not leak Lua 5.1/LuaJIT legacy globals
  ** that were registered by the shared base library definition above.
  */
  setnilV(lj_tab_setstr(L, env, lj_str_newlit(L, "getfenv")));
  setnilV(lj_tab_setstr(L, env, lj_str_newlit(L, "setfenv")));
  setnilV(lj_tab_setstr(L, env, lj_str_newlit(L, "newproxy")));
  setnilV(lj_tab_setstr(L, env, lj_str_newlit(L, "loadstring")));
  setnilV(lj_tab_setstr(L, env, lj_str_newlit(L, "unpack")));
  setnilV(lj_tab_setstr(L, env, lj_str_newlit(L, "gcinfo")));
  lua_pushcfunction(L, lj_cf_type54);
  lua_setglobal(L, "type");
  lua_pushcfunction(L, lj_cf_getmetatable54);
  lua_setglobal(L, "getmetatable");
  lua_getglobal(L, "pcall");
  lj_close_setrawpcall(L, L->top-1);
  lua_setfield(L, LUA_REGISTRYINDEX, "_LUA54_RAW_PCALL");
  lua_getglobal(L, "xpcall");
  /* Internal close dispatch must call the real xpcall without exposing this
  ** Lua compatibility wrapper to debug line hooks.
  */
  lj_close_setrawxpcall(L, L->top-1);
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_REGISTRYINDEX, "_LUA54_RAW_XPCALL");
  if (luaL_loadstring(L,
    "local raw_xpcall = ...\n"
    "local type = type\n"
    "local select = select\n"
    "return function(...)\n"
    "  local nargs = select('#', ...)\n"
    "  local f = select(1, ...)\n"
    "  local msgh = select(2, ...)\n"
    "  if nargs < 2 then\n"
    "    return raw_xpcall(f)\n"
    "  end\n"
    "  if type(msgh) ~= 'function' or f ~= msgh then\n"
    "    return raw_xpcall(f, msgh, select(3, ...))\n"
    "  end\n"
    "  return false, 'error in error handling'\n"
    "end\n") != LUA_OK)
    lua_error(L);
  lua_insert(L, -2);
  lua_call(L, 1, 1);
  lua_setglobal(L, "xpcall");
  lua_pushcfunction(L, lj_cf_next54);
  lua_setglobal(L, "next");
  lua_getglobal(L, "pairs");
  lua_getglobal(L, "next");
  /* The registered pairs fast function owns a cached next upvalue. Point it at
  ** the Lua 5.4-compatible next so pairs(_G) hides internal _ENV as well.
  */
  lua_setupvalue(L, -2, 1);
  lua_pop(L, 1);
  lua_pushcfunction(L, lj_cf_rawget54);
  lua_setglobal(L, "rawget");
  lua_pushcfunction(L, lj_cf_ipairs_aux54);
  lua_setfield(L, LUA_REGISTRYINDEX, LUA54_IPAIRS_AUX_REGKEY);
  lua_pushcfunction(L, lj_cf_ipairs54);
  lua_setglobal(L, "ipairs");
  lua_pushcfunction(L, lj_cf_dofile_load54);
  if (luaL_loadstring(L,
    "local load = ...\n"
    "return function(filename)\n"
    "  local f = load(filename)\n"
    "  return f()\n"
    "end\n") != LUA_OK)
    lua_error(L);
  lua_insert(L, -2);
  lua_call(L, 1, 1);
  lua_setglobal(L, "dofile");
#else
  setnilV(lj_tab_setstr(L, env, lj_str_newlit(L, "warn")));
#endif
  luaopen_coroutine(L);
  return 2;
}

#if LJ_54
LUALIB_API int luaopen_base(lua_State *L)
{
  int n = luaopen_base_luajit(L);
  /* LuaJIT's internal/legacy luaopen_base() returns base plus coroutine.
  ** Lua 5.4 external callers see the official single base-library result,
  ** while luaL_openlibs can still use the internal entry when needed.
  */
  while (n-- > 1)
    lua_pop(L, 1);
  return 1;
}
#endif
