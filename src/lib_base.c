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

/* -- Base library: checks ------------------------------------------------ */

#define LJLIB_MODULE_base

#if LJ_54
static void base_argerror_named54(lua_State *L, int narg, const char *fname,
				  const char *msg)
{
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      narg, fname, msg));
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
    } else if (tvisnumber(o)) {
      GCstr *s = lj_strfmt_number(L, o);
      setstrV(L, o, s);
      return s;
    }
  }
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

static int32_t base_checkint_named54(lua_State *L, int narg,
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
  if (!tvisnum(o))
    base_argtype_named54(L, narg, fname, "number");
  n = numV(o);
  if (!(n >= -2147483648.0 && n <= 2147483647.0)) {
    base_argerror_named54(L, narg, fname,
			  "number has no integer representation");
  }
  k = lj_num2i64(n);
  if ((lua_Number)k != n) {
    base_argerror_named54(L, narg, fname,
			  "number has no integer representation");
  }
  return (int32_t)k;
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
  else if (tvisstr(L->base+1) || tvisnumber(L->base+1))
    lj_err_callermsg(L, strdata(lj_lib_checkstr(L, 2)));
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
LJLIB_PUSH("table")
LJLIB_PUSH(top-9)  /* userdata */
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
  return 0;
}
#endif

#if LJ_52 || LJ_HASFFI
static int ffh_pairs(lua_State *L, MMS mm)
{
  TValue *o = lj_lib_checkany(L, 1);
  cTValue *mo = lj_meta_lookup(L, o, mm);
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
static int lj_cf_ipairs_aux54(lua_State *L)
{
  lua_Integer i = luaL_checkinteger(L, 2) + 1;
  lua_pushinteger(L, i);
  lua_pushinteger(L, i);
  /* Lua 5.4 ipairs uses normal indexed access, so __index can provide values. */
  lua_gettable(L, 1);
  if (lua_isnil(L, -1))
    return 0;
  return 2;
}

static int lj_cf_ipairs54(lua_State *L)
{
  base_checkany_named54(L, 1, "ipairs");
  lua_pushcfunction(L, lj_cf_ipairs_aux54);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, 0);
  return 3;
}

static int lj_cf_pairs54(lua_State *L)
{
  base_checkany_named54(L, 1, "pairs");
  if (luaL_getmetafield(L, 1, "__pairs")) {
    lua_pushvalue(L, 1);
    lua_call(L, 1, 3);
    return 3;
  }
  /* Lua 5.4 pairs() does not require a table until next() is actually called.
  ** This lets custom metatables or later iterator calls define the failure.
  */
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushvalue(L, 1);
  lua_pushnil(L);
  return 3;
}
#endif

LJLIB_CF(warn)
{
  int32_t i, n = (int32_t)(L->top - L->base);
  if (n == 0)
    lj_err_argt(L, 1, LUA_TSTRING);
  for (i = 0; i < n; i++) {
    GCstr *s = lj_lib_checkstr(L, (int)i+1);
    const char *str;
    str = strdata(s);
    if (i == 0 && s->len > 0 && str[0] == '@') {
      if (s->len == 3 && memcmp(str, "@on", 3) == 0)
	G(L)->warn_on = 1;
      else if (s->len == 4 && memcmp(str, "@off", 4) == 0)
	G(L)->warn_on = 0;
      return 0;
    }
    /* Route Lua warn() through lua_warning() so the default output prefix and
    ** user-installed C warning callbacks share one implementation.
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
  if (mo >= L->top || tvisnil(mo)) {
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
      cTValue *gc = lj_tab_getstr(mt, mmname_str(G(L), MM_gc));
      if (gc && !tvisnil(gc))
	t->flags54 |= LJ_TAB_HAS_GC;
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
    int32_t i = base_checkint_named54(L, 1, "select");
#else
    int32_t i = lj_lib_checkint(L, 1);
#endif
    if (i < 0) i = n + i; else if (i > n) i = n;
    if (i < 1)
      lj_err_arg(L, 1, LJ_ERR_IDXRNG);
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
  int32_t base = (L->base+1 < L->top && !tvisnil(L->base+1)) ?
		 base_checkint_named54(L, 2, "tonumber") : 10;
#else
  int32_t base = lj_lib_optint(L, 2, 10);
#endif
  if (base == 10) {
    TValue *o = lj_lib_checkany(L, 1);
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
    const char *p = strdata(lj_lib_checkstr(L, 1));
    char *ep;
    unsigned int neg = 0;
    unsigned long ul;
    if (base < 2 || base > 36)
      lj_err_arg(L, 2, LJ_ERR_BASERNG);
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
    copyTV(L, L->base-1-LJ_FR2, mo);  /* Replace callable. */
    return FFH_TAILCALL;
  }
  lj_gc_check(L);
  setstrV(L, L->base-1-LJ_FR2, lj_strfmt_obj(L, L->base));
  return FFH_RES(1);
}

/* -- Base library: throw and catch errors -------------------------------- */

LJLIB_CF(error)
{
#if LJ_54
  int32_t level = (L->base+1 < L->top && !tvisnil(L->base+1)) ?
		  base_checkint_named54(L, 2, "error") : 1;
#else
  int32_t level = lj_lib_optint(L, 2, 1);
#endif
  lua_settop(L, 1);
  if (lua_isstring(L, 1) && level > 0) {
    luaL_where(L, level);
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

static int load_aux(lua_State *L, int status, int envarg)
{
  if (status == LUA_OK) {
    /*
    ** Set environment table for top-level function.
    ** Don't do this for non-native bytecode, which returns a prototype.
    */
    if (tvistab(L->base+envarg-1) && tvisfunc(L->top-1)) {
      GCfunc *fn = funcV(L->top-1);
      GCtab *t = tabV(L->base+envarg-1);
      setgcref(fn->c.env, obj2gco(t));
      lj_gc_objbarrier(L, fn, t);
    }
    return 1;
  } else {
    setnilV(L->top-2);
    return 2;
  }
}

LJLIB_CF(loadfile)
{
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
  return load_aux(L, status, 3);
}

static const char *reader_func(lua_State *L, void *ud, size_t *size)
{
  UNUSED(ud);
  luaL_checkstack(L, 2, "too many nested functions");
  copyTV(L, L->top++, L->base);
  lua_call(L, 0, 1);  /* Call user-supplied function. */
  L->top--;
  if (tvisnil(L->top)) {
    *size = 0;
    return NULL;
  } else if (tvisstr(L->top) || tvisnumber(L->top)) {
    copyTV(L, L->base+4, L->top);  /* Anchor string in reserved stack slot. */
    return lua_tolstring(L, 5, size);
  } else {
    lj_err_caller(L, LJ_ERR_RDRSTR);
    return NULL;
  }
}

LJLIB_CF(load)
{
#if LJ_54
  GCstr *name = base_optstr_named54(L, 2, "load");
  GCstr *mode = base_optstr_named54(L, 3, "load");
#else
  GCstr *name = lj_lib_optstr(L, 2);
  GCstr *mode = lj_lib_optstr(L, 3);
#endif
  int status;
  if (L->base < L->top &&
      (tvisstr(L->base) || tvisnumber(L->base) || tvisbuf(L->base))) {
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
    lua_settop(L, 5);  /* Reserve a slot for the string from the reader. */
    status = lua_loadx(L, reader_func, NULL, name ? strdata(name) : "=(load)",
		       mode ? strdata(mode) : NULL);
  }
  return load_aux(L, status, 4);
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
      const char *old = G(L)->gc_mode54 ? "generational" : "incremental";
      /* LuaJIT does not implement Lua 5.4's generational collector, but the
      ** option is accepted so 5.4 code can switch modes without hard failure.
      */
      G(L)->gc_mode54 = (uint8_t)(s->len == 12);
      lua_pushstring(L, old);
      return 1;
    }
  }
#endif
#if LJ_54
  opt = base_checkopt_named54(L, 1, LUA_GCCOLLECT,  /* ORDER LUA_GC* */
    "\4stop\7restart\7collect\5count\1\377\4step\10setpause\12setstepmul\1\377\11isrunning",
    "collectgarbage");
  data = (L->base+1 < L->top && !tvisnil(L->base+1)) ?
	 base_checkint_named54(L, 2, "collectgarbage") : 0;
#else
  opt = lj_lib_checkopt(L, 1, LUA_GCCOLLECT,  /* ORDER LUA_GC* */
    "\4stop\7restart\7collect\5count\1\377\4step\10setpause\12setstepmul\1\377\11isrunning");
  data = lj_lib_optint(L, 2, 0);
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

LJLIB_PUSH("tostring")
LJLIB_CF(print)
{
  ptrdiff_t i, nargs = L->top - L->base;
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
    base_argtype_named54(L, 1, "coroutine.status", "thread");
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
  setboolV(L->top++, cframe_canyield(L->cframe));
  return 1;
}

LJLIB_CF(coroutine_create)
{
  lua_State *L1;
  if (!(L->base < L->top && tvisfunc(L->base))) {
#if LJ_54
    base_argtype_named54(L, 1, "coroutine.create", "function");
#else
    lj_err_argt(L, 1, LUA_TFUNCTION);
#endif
  }
  L1 = lua_newthread(L);
  setfuncV(L, L1->top++, funcV(L->base));
  return 1;
}

#if LJ_54
static int lj_cf_coroutine_close(lua_State *L)
{
  lua_State *co;
  if (!(L->top > L->base && tvisthread(L->base)))
    base_argtype_named54(L, 1, "coroutine.close", "thread");
  co = threadV(L->base);
  if (co == L || co->cframe != NULL ||
      (co->status == LUA_OK && co->base > tvref(co->stack)+1+LJ_FR2))
    lj_err_callermsg(L, "cannot close a running coroutine");
  if (co->status > LUA_YIELD) {
    setboolV(L->top++, 0);
    if (co->top > co->base)
      copyTV(L, L->top++, co->top-1);
    else
      setnilV(L->top++);
    co->status = LUA_OK;
    co->top = co->base = tvref(co->stack) + 1 + LJ_FR2;
    return 2;
  }
  /* No <close> variables are supported yet, but close open upvalues and make
  ** the coroutine dead so Lua 5.4 callers can reliably cancel suspended work.
  */
  lj_func_closeuv(co, tvref(co->stack));
  co->status = LUA_OK;
  co->top = co->base = tvref(co->stack) + 1 + LJ_FR2;
  setboolV(L->top++, 1);
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
    ErrMsg em = co->cframe ? LJ_ERR_CORUN : LJ_ERR_CODEAD;
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
    base_argtype_named54(L, 1, "coroutine.wrap", "function");
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

LUALIB_API int luaopen_base(lua_State *L)
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
  lua_pushcfunction(L, lj_cf_next54);
  lua_setglobal(L, "next");
  lua_pushcfunction(L, lj_cf_rawget54);
  lua_setglobal(L, "rawget");
  lua_pushcfunction(L, lj_cf_ipairs54);
  lua_setglobal(L, "ipairs");
  lua_getglobal(L, "next");
  lua_pushcclosure(L, lj_cf_pairs54, 1);
  lua_setglobal(L, "pairs");
#else
  setnilV(lj_tab_setstr(L, env, lj_str_newlit(L, "warn")));
#endif
  LJ_LIB_REG(L, LUA_COLIBNAME, coroutine);
#if LJ_54
  lua_pushcfunction(L, lj_cf_coroutine_close);
  lua_setfield(L, -2, "close");
#endif
  return 2;
}
