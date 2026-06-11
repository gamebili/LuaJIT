/*
** Table library.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lib_table_c
#define LUA_LIB

#include <math.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_frame.h"
#include "lj_buf.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_ff.h"
#include "lj_lib.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "lj_vm.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_table

LJLIB_LUA(table_foreachi) /*
  function(t, f)
    CHECK_tab(t)
    CHECK_func(f)
    for i=1,#t do
      local r = f(i, t[i])
      if r ~= nil then return r end
    end
  end
*/

LJLIB_LUA(table_foreach) /*
  function(t, f)
    CHECK_tab(t)
    CHECK_func(f)
    for k, v in PAIRS(t) do
      local r = f(k, v)
      if r ~= nil then return r end
    end
  end
*/

LJLIB_LUA(table_getn) /*
  function(t)
    CHECK_tab(t)
    return #t
  end
*/

LJLIB_CF(table_maxn)
{
  GCtab *t = lj_lib_checktab(L, 1);
  TValue *array = tvref(t->array);
  Node *node;
  lua_Number m = 0;
  ptrdiff_t i;
  for (i = (ptrdiff_t)t->asize - 1; i >= 0; i--)
    if (!tvisnil(&array[i])) {
      m = (lua_Number)(int32_t)i;
      break;
    }
  node = noderef(t->node);
  for (i = (ptrdiff_t)t->hmask; i >= 0; i--)
    if (!tvisnil(&node[i].val) && tvisnumber(&node[i].key)) {
      lua_Number n = numberVnum(&node[i].key);
      if (n > m) m = n;
    }
  setnumV(L->top-1, m);
  return 1;
}

#if LJ_54
#define LJ_TABLE_MAXINTEGER	((lua_Integer)2147483647)
#define LJ_TABLE_MININTEGER	((lua_Integer)(-LJ_TABLE_MAXINTEGER - 1))
#define LJ_TABLE_TAB_R		1
#define LJ_TABLE_TAB_W		2
#define LJ_TABLE_TAB_L		4
#define LJ_TABLE_TAB_RW		(LJ_TABLE_TAB_R|LJ_TABLE_TAB_W)

static int32_t table_array_highest(GCtab *t)
{
  TValue *array = tvref(t->array);
  ptrdiff_t i;
  for (i = (ptrdiff_t)t->asize - 1; i > 0; i--)
    if (!tvisnil(&array[i]))
      return (int32_t)i;
  return 0;
}

static int table_toint32value54(cTValue *o, int32_t *ip, int *isnum)
{
  TValue tmp;
  double n, ni;
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
    *ip = intV(o);
    return 1;
  }
  n = numV(o);
  if (!(n >= (double)LJ_TABLE_MININTEGER &&
	n <= (double)LJ_TABLE_MAXINTEGER))
    return 0;
  ni = lj_vm_floor(n);
  if (n != ni)
    return 0;
  *ip = (int32_t)n;
  return 1;
}

static int table_tointegervalue54(cTValue *o, lua_Integer *ip, int *isnum)
{
  TValue tmp;
  double n, ni;
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
  if (!(n >= (-9223372036854775807.0 - 1.0) &&
	n < 9223372036854775808.0))
    return 0;
  ni = lj_vm_floor(n);
  if (n != ni)
    return 0;
  k = lj_num2i64(n);
  if ((lua_Number)k != n)
    return 0;
  *ip = (lua_Integer)k;
  return 1;
}

static void table_argerror_named54(lua_State *L, int narg, const char *fname,
				   const char *msg)
{
  const char *dname = NULL;
  const char *kind = lj_debug_funcname(L, L->base-1, &dname);
  /* Lua 5.4 reports a source-level call name when the caller bytecode still
  ** exposes one, e.g. table.sort(1) -> 'sort' or local f=table.sort; f(1)
  ** -> 'f'. Calls routed through pcall/C frames and internal comparator calls
  ** have no useful caller slot, so keep the stable table.* fallback.
  */
  if (kind && dname)
    fname = dname;
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      narg, fname, msg));
}

static const char *table_argtypename54(lua_State *L, int narg)
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

static void table_argtype_named54(lua_State *L, int narg, const char *fname,
				  const char *xname)
{
  table_argerror_named54(L, narg, fname,
    lj_strfmt_pushf(L, "%s expected, got %s", xname,
		    table_argtypename54(L, narg)));
}

static int table_checkfield54(lua_State *L, const char *key, int n)
{
  int found;
  lua_pushstring(L, key);
  lua_rawget(L, -n);
  found = !lua_isnil(L, -1);
  return found;
}

static void table_checktab_like54(lua_State *L, int narg, int what,
				  const char *fname)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && tvistab(o))) {
    int n = 1;
    if (o < L->top && lua_getmetatable(L, narg) &&
	(!(what & LJ_TABLE_TAB_R) ||
	 table_checkfield54(L, "__index", ++n)) &&
	(!(what & LJ_TABLE_TAB_W) ||
	 table_checkfield54(L, "__newindex", ++n)) &&
	(!(what & LJ_TABLE_TAB_L) ||
	 table_checkfield54(L, "__len", ++n))) {
      lua_pop(L, n);
      return;
    }
    table_argtype_named54(L, narg, fname, "table");
  }
}

static GCstr *table_checkstr_named54(lua_State *L, int narg,
				     const char *fname)
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
  table_argtype_named54(L, narg, fname, "string");
  return NULL;  /* unreachable */
}

static GCstr *table_optstr_named54(lua_State *L, int narg,
				   const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o >= L->top || tvisnil(o))
    return NULL;
  return table_checkstr_named54(L, narg, fname);
}

static void table_checkfunc_named54(lua_State *L, int narg,
				    const char *fname)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && tvisfunc(o)))
    table_argtype_named54(L, narg, fname, "function");
}

static int32_t table_len54(lua_State *L, GCtab *t, int narg)
{
  cTValue *tabv = L->base + narg-1;
  cTValue *mo = lj_meta_lookup(L, tabv, MM_len);
  int32_t len = 0;
  if (!tvisnil(mo)) {
    copyTV(L, L->top++, mo);
    copyTV(L, L->top++, tabv);
    lua_call(L, 1, 1);
    if (!table_toint32value54(L->top-1, &len, NULL))
      luaL_error(L, "object length is not an integer");
    L->top--;
    return len;
  }
  len = (int32_t)lj_tab_len(t);
  /* LuaJIT's length search can stop before later constructor array entries
  ** after nil holes. Keep the Lua 5.4 table-library compatibility local here
  ** instead of changing the global #table behavior for default LuaJIT code.
  */
  if (len < (int32_t)t->asize-1) {
    int32_t ahigh = table_array_highest(t);
    if (ahigh > len) len = ahigh;
  }
  return len;
}

static lua_Integer table_checkinteger_named54(lua_State *L, int narg,
					      const char *fname)
{
  cTValue *o = L->base + narg-1;
  lua_Integer i;
  int isnum = 0;
  if (o < L->top && table_tointegervalue54(o, &i, &isnum))
    return i;
  if (isnum)
    table_argerror_named54(L, narg, fname,
			   "number has no integer representation");
  table_argtype_named54(L, narg, fname, "number");
  return 0;  /* unreachable */
}

static lua_Integer table_len_integer_obj54(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  lua_Integer len;
  if (o < L->top && tvistab(o)) {
    cTValue *mo = lj_meta_lookup(L, o, MM_len);
    if (tvisnil(mo))
      return (lua_Integer)table_len54(L, tabV(o), narg);
    copyTV(L, L->top++, mo);
    copyTV(L, L->top++, o);
    lua_call(L, 1, 1);
  } else {
    lua_len(L, narg);
  }
  if (!table_tointegervalue54(L->top-1, &len, NULL)) {
    L->top--;
    luaL_error(L, "object length is not an integer");
  }
  L->top--;
  return len;
}
#endif

LJLIB_CF(table_insert)		LJLIB_REC(.)
{
#if LJ_54
  lua_Integer e;
  lua_Integer n, pos;
  int nargs = (int)(L->top - L->base);
  table_checktab_like54(L, 1, LJ_TABLE_TAB_RW|LJ_TABLE_TAB_L,
			"table.insert");
  e = (lua_Integer)((lua_Unsigned)table_len_integer_obj54(L, 1) +
		    (lua_Unsigned)1);
  pos = e;
  if (nargs == 3) {
    pos = table_checkinteger_named54(L, 2, "table.insert");
    if ((lua_Unsigned)pos - (lua_Unsigned)1 >= (lua_Unsigned)e)
      table_argerror_named54(L, 2, "table.insert", "position out of bounds");
  } else if (nargs != 2) {
    lj_err_caller(L, LJ_ERR_TABINS);
  }
#else
  GCtab *t = lj_lib_checktab(L, 1);
#endif
#if LJ_54
  /* Lua 5.4 rejects non-integer positions and out-of-range insert slots before
  ** moving elements; only the compatibility build gets the stricter contract.
  */
  for (n = e; n > pos; n--) {
    /* Lua 5.4 table.insert observes __index/__newindex while shifting
    ** sequence slots, so proxy tables are updated through their metatables.
    */
    lua_geti(L, 1, n-1);
    lua_seti(L, 1, n);
  }
  lua_pushvalue(L, nargs);
  lua_seti(L, 1, pos);
#else
  int32_t n, i = (int32_t)lj_tab_len(t) + 1;
  int nargs = (int)((char *)L->top - (char *)L->base);
  if (nargs != 2*sizeof(TValue)) {
    if (nargs != 3*sizeof(TValue))
      lj_err_caller(L, LJ_ERR_TABINS);
    /* NOBARRIER: This just moves existing elements around. */
    for (n = lj_lib_checkint(L, 2); i > n; i--) {
      /* The set may invalidate the get pointer, so need to do it first! */
      TValue *dst = lj_tab_setint(L, t, i);
      cTValue *src = lj_tab_getint(t, i-1);
      if (src) {
	copyTV(L, dst, src);
      } else {
	setnilV(dst);
      }
    }
    i = n;
  }
  {
    TValue *dst = lj_tab_setint(L, t, i);
    copyTV(L, dst, L->top-1);  /* Set new value. */
    lj_gc_barriert(L, t, dst);
  }
#endif
  return 0;
}

LJLIB_LUA(table_remove) /*
  function(t, pos)
    CHECK_tab(t)
    local len = #t
    if pos == nil then
      if len ~= 0 then
	local old = t[len]
	t[len] = nil
	return old
      end
    else
      CHECK_int(pos)
      if pos >= 1 and pos <= len then
	local old = t[pos]
	for i=pos+1,len do
	  t[i-1] = t[i]
	end
	t[len] = nil
	return old
      end
    end
  end
*/

LJLIB_LUA(table_move) /*
  function(a1, f, e, t, a2)
    CHECK_tab(a1)
    CHECK_int(f)
    CHECK_int(e)
    CHECK_int(t)
    if a2 == nil then a2 = a1 end
    CHECK_tab(a2)
    if e >= f then
      local d = t - f
      if t > e or t <= f or a2 ~= a1 then
	for i=f,e do a2[i+d] = a1[i] end
      else
	for i=e,f,-1 do a2[i+d] = a1[i] end
      end
    end
    return a2
  end
*/

#if LJ_54
static int lj_cf_table_remove54(lua_State *L)
{
  lua_Integer len;
  lua_Integer pos;
  cTValue *posv = L->base + 1;
  table_checktab_like54(L, 1, LJ_TABLE_TAB_RW|LJ_TABLE_TAB_L,
			"table.remove");
  len = table_len_integer_obj54(L, 1);
  pos = len;
  if (posv < L->top && !tvisnil(posv)) {
    pos = table_checkinteger_named54(L, 2, "table.remove");
    if (pos != len &&
	(lua_Unsigned)pos - (lua_Unsigned)1 > (lua_Unsigned)len)
      table_argerror_named54(L, 2, "table.remove", "position out of bounds");
  }
  /* Lua 5.4 table.remove uses lua_Integer for both length and position,
  ** including proxy tables whose __len returns keys outside the int32 range.
  */
  lua_geti(L, 1, pos);
  for (; pos < len; pos++) {
    lua_geti(L, 1, pos + 1);
    lua_seti(L, 1, pos);
  }
  lua_pushnil(L);
  lua_seti(L, 1, pos);
  return 1;
}

static int lj_cf_table_move54(lua_State *L)
{
  lua_Integer f = table_checkinteger_named54(L, 2, "table.move");
  lua_Integer e = table_checkinteger_named54(L, 3, "table.move");
  lua_Integer tt = table_checkinteger_named54(L, 4, "table.move");
  cTValue *a2v = L->base + 4;
  int target;
  table_checktab_like54(L, 1, LJ_TABLE_TAB_R, "table.move");
  if (a2v < L->top && !tvisnil(a2v)) {
    table_checktab_like54(L, 5, LJ_TABLE_TAB_W, "table.move");
    target = 5;
  } else {
    target = 1;
  }
  if (e >= f) {
    lua_Integer i, n;
    /* Match Lua 5.4's overflow guards before moving anything. Without these
    ** checks, huge ranges such as 0..maxinteger would spin for billions of
    ** public API get/set operations before any observable error.
    */
    if ((lua_Unsigned)e - (lua_Unsigned)f >= (lua_Unsigned)LUA_MAXINTEGER)
      table_argerror_named54(L, 3, "table.move", "too many elements to move");
    n = e - f + 1;
    if (tt > LUA_MAXINTEGER - n + 1)
      table_argerror_named54(L, 4, "table.move", "destination wrap around");
    if (tt > e || tt <= f || (target != 1 &&
			      !lua_compare(L, 1, target, LUA_OPEQ))) {
      for (i = f; ; i++) {
	/* Lua 5.4 table.move observes __index/__newindex; use public table
	** accessors here instead of LuaJIT's raw array helpers.
	*/
	lua_geti(L, 1, i);
	lua_seti(L, target, tt + (i - f));
	if (i == e) break;
      }
    } else {
      for (i = e; i >= f; i--) {
	lua_geti(L, 1, i);
	lua_seti(L, target, tt + (i - f));
	if (i == f) break;
      }
    }
  }
  lua_pushvalue(L, target);
  return 1;
}
#endif

#if LJ_54
static int table_concat54(lua_State *L, GCstr *sep, lua_Integer i,
			  lua_Integer e)
{
  luaL_Buffer b;
  const char *sepstr = sep ? strdata(sep) : "";
  size_t seplen = sep ? sep->len : 0;
  lua_Integer start = i;
  luaL_buffinit(L, &b);
  while (i <= e) {
    if (i > start && seplen)
      luaL_addlstring(&b, sepstr, seplen);
    /* Lua 5.4 table.concat observes __index while reading sequence items. */
    lua_geti(L, 1, i);
    if (!lua_isstring(L, -1)) {
      GCstr *idx = lj_strfmt_i64(L, i);
      luaL_error(L, "invalid value (%s) at index %s in table for 'concat'",
		 luaL_typename(L, -1), strdata(idx));
    }
    luaL_addvalue(&b);
    /* Stop exactly at the requested end index. Incrementing maxinteger wraps
    ** to mininteger on this build and would make the loop read one extra key.
    */
    if (i == e)
      break;
    i++;
  }
  luaL_pushresult(&b);
  return 1;
}
#endif

LJLIB_CF(table_concat)		LJLIB_REC(.)
{
#if LJ_54
  /* Official tconcat() order: validate the table-like object and take its
  ** length (running __len) first, then parse separator and range. The
  ** explicit end argument overrides the default but the length operation
  ** still runs like official aux_getn().
  */
  GCstr *sep;
  lua_Integer i, e;
  table_checktab_like54(L, 1, LJ_TABLE_TAB_R|LJ_TABLE_TAB_L,
			"table.concat");
  e = table_len_integer_obj54(L, 1);
  sep = table_optstr_named54(L, 2, "table.concat");
  i = (L->base+2 < L->top && !tvisnil(L->base+2)) ?
      table_checkinteger_named54(L, 3, "table.concat") : 1;
  if (L->base+3 < L->top && !tvisnil(L->base+3))
    e = table_checkinteger_named54(L, 4, "table.concat");
#else
  GCtab *t = lj_lib_checktab(L, 1);
  GCstr *sep = lj_lib_optstr(L, 2);
  int32_t i = lj_lib_optint(L, 3, 1);
  lua_Integer e;
  SBuf *sb, *sbx;
  if (L->base+3 < L->top && !tvisnil(L->base+3)) {
    e = lj_lib_checkint(L, 4);
  } else {
    e = (int32_t)lj_tab_len(t);
  }
#endif
#if LJ_54
  return table_concat54(L, sep, i, e);
#else
  sb = lj_buf_tmp_(L);
  sbx = lj_buf_puttab(sb, t, sep, i, e);
  if (LJ_UNLIKELY(!sbx)) {  /* Error: bad element type. */
    int32_t idx = (int32_t)(intptr_t)sb->w;
    cTValue *o = lj_tab_getint(t, idx);
    lj_err_callerv(L, LJ_ERR_TABCAT,
		   lj_obj_itypename[o ? itypemap(o) : ~LJ_TNIL], idx);
  }
  setstrV(L, L->top-1, lj_buf_str(L, sbx));
  lj_gc_check(L);
  return 1;
#endif
}

/* ------------------------------------------------------------------------ */

static void set2(lua_State *L, int i, int j)
{
#if LJ_54
  /* Lua 5.4 table.sort reads and writes through the public table API so
  ** proxy tables with __index/__newindex are sorted consistently.
  */
  lua_seti(L, 1, i);
  lua_seti(L, 1, j);
#else
  lua_rawseti(L, 1, i);
  lua_rawseti(L, 1, j);
#endif
}

#if LJ_54
#define sort_geti(L, i)	lua_geti((L), 1, (i))
#else
#define sort_geti(L, i)	lua_rawgeti((L), 1, (i))
#endif

static int sort_comp(lua_State *L, int a, int b)
{
  if (!lua_isnil(L, 2)) {  /* function? */
    int res;
    lua_pushvalue(L, 2);
    lua_pushvalue(L, a-1);  /* -1 to compensate function */
    lua_pushvalue(L, b-2);  /* -2 to compensate function and `a' */
    lua_call(L, 2, 1);
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
  } else {  /* a < b? */
#if LJ_54
    /* Plain number comparisons skip the generic metamethod path; numbers
    ** never have ordering metamethods, so this equals lua_lessthan().
    */
    cTValue *oa = L->top + a;
    cTValue *ob = L->top + b;
    if (tvisint(oa) && tvisint(ob))
      return intV(oa) < intV(ob);
    if (tvisnum(oa) && tvisnum(ob))
      return numV(oa) < numV(ob);
#endif
    return lua_lessthan(L, a, b);
  }
}

#if LJ_54 && LJ_TARGET_ARM64
static LJ_AINLINE void sort_raw_int_swap54(TValue *array, int i, int j)
{
  TValue tmp = array[i];
  array[i] = array[j];
  array[j] = tmp;
}

static LJ_AINLINE int sort_raw_int_lt54(TValue *array, int i, int j)
{
  return intV(&array[i]) < intV(&array[j]);
}

static void sort_raw_int_insert54(TValue *array, int l, int u)
{
  int i;
  for (i = l+1; i <= u; i++) {
    TValue tv = array[i];
    int32_t v = intV(&tv);
    int j = i-1;
    while (j >= l && intV(&array[j]) > v) {
      array[j+1] = array[j];
      j--;
    }
    array[j+1] = tv;
  }
}

static void sort_raw_int_quick54(TValue *array, int l, int u)
{
  while (u-l > 15) {
    int p = (l+u) >> 1;
    int i, j;
    TValue pivot;
    int32_t pv;
    if (sort_raw_int_lt54(array, p, l))
      sort_raw_int_swap54(array, p, l);
    if (sort_raw_int_lt54(array, u, p)) {
      sort_raw_int_swap54(array, u, p);
      if (sort_raw_int_lt54(array, p, l))
	sort_raw_int_swap54(array, p, l);
    }
    sort_raw_int_swap54(array, p, u-1);
    pivot = array[u-1];
    pv = intV(&pivot);
    i = l;
    j = u-1;
    for (;;) {
      do { i++; } while (intV(&array[i]) < pv);
      do { j--; } while (pv < intV(&array[j]));
      if (j < i)
	break;
      sort_raw_int_swap54(array, i, j);
    }
    array[u-1] = array[i];
    array[i] = pivot;
    if (i-l < u-i) {
      sort_raw_int_quick54(array, l, i-1);
      l = i+1;
    } else {
      sort_raw_int_quick54(array, i+1, u);
      u = i-1;
    }
  }
  if (l < u)
    sort_raw_int_insert54(array, l, u);
}

static int table_sort_raw_int_array54(lua_State *L, int32_t n)
{
  GCtab *t;
  TValue *array;
  int32_t i;
  if (!tvistab(L->base) || !tvisnil(L->base+1))
    return 0;
  t = tabV(L->base);
  if (tabref(t->metatable) || (MSize)n >= t->asize)
    return 0;
  array = tvref(t->array);
  for (i = 1; i <= n; i++)
    if (!tvisint(&array[i]))
      return 0;
  if (n > 1)
    sort_raw_int_quick54(array, 1, n);
  return 1;
}
#endif

static void auxsort(lua_State *L, int l, int u)
{
  while (l < u) {  /* for tail recursion */
    int i, j;
    /* sort elements a[l], a[(l+u)/2] and a[u] */
    sort_geti(L, l);
    sort_geti(L, u);
    if (sort_comp(L, -1, -2))  /* a[u] < a[l]? */
      set2(L, l, u);  /* swap a[l] - a[u] */
    else
      lua_pop(L, 2);
    if (u-l == 1) break;  /* only 2 elements */
    i = (l+u)/2;
    sort_geti(L, i);
    sort_geti(L, l);
    if (sort_comp(L, -2, -1)) {  /* a[i]<a[l]? */
      set2(L, i, l);
    } else {
      lua_pop(L, 1);  /* remove a[l] */
      sort_geti(L, u);
      if (sort_comp(L, -1, -2))  /* a[u]<a[i]? */
	set2(L, i, u);
      else
	lua_pop(L, 2);
    }
    if (u-l == 2) break;  /* only 3 elements */
    sort_geti(L, i);  /* Pivot */
    lua_pushvalue(L, -1);
    sort_geti(L, u-1);
    set2(L, i, u-1);
    /* a[l] <= P == a[u-1] <= a[u], only need to sort from l+1 to u-2 */
    i = l; j = u-1;
    for (;;) {  /* invariant: a[l..i] <= P <= a[j..u] */
      /* repeat ++i until a[i] >= P */
      while (sort_geti(L, ++i), sort_comp(L, -1, -2)) {
#if LJ_54
	/* Lua 5.4 rejects non-strict comparators once the scan reaches the
	** pivot sentinel itself; otherwise a <= b / a >= b can be accepted.
	*/
	if (i>=u-1)
	  lj_err_caller(L, LJ_ERR_TABSORT);
#else
	if (i>=u) lj_err_caller(L, LJ_ERR_TABSORT);
#endif
	lua_pop(L, 1);  /* remove a[i] */
      }
      /* repeat --j until a[j] <= P */
      while (sort_geti(L, --j), sort_comp(L, -3, -1)) {
#if LJ_54
	if (j<i)
	  lj_err_caller(L, LJ_ERR_TABSORT);
#else
	if (j<=l) lj_err_caller(L, LJ_ERR_TABSORT);
#endif
	lua_pop(L, 1);  /* remove a[j] */
      }
      if (j<i) {
	lua_pop(L, 3);  /* pop pivot, a[i], a[j] */
	break;
      }
      set2(L, i, j);
    }
    sort_geti(L, u-1);
    sort_geti(L, i);
    set2(L, u-1, i);  /* swap pivot (a[u-1]) with a[i] */
    /* a[l..i-1] <= a[i] == P <= a[i+1..u] */
    /* adjust so that smaller half is in [j..i] and larger one in [l..u] */
    if (i-l < u-i) {
      j=l; i=i-1; l=i+2;
    } else {
      j=i+1; i=u; u=j-2;
    }
    auxsort(L, j, i);  /* call recursively the smaller one */
  }  /* repeat the routine for the larger one */
}

LJLIB_CF(table_sort)
{
#if LJ_54
  int32_t n;
  lua_Integer len;
  table_checktab_like54(L, 1, LJ_TABLE_TAB_RW|LJ_TABLE_TAB_L,
			"table.sort");
  len = table_len_integer_obj54(L, 1);
  if (len >= INT32_MAX)
    table_argerror_named54(L, 1, "table.sort", "array too big");
  n = (int32_t)len;
#else
  GCtab *t = lj_lib_checktab(L, 1);
  int32_t n = (int32_t)lj_tab_len(t);
#endif
  lua_settop(L, 2);
#if LJ_54
  /* Lua 5.4 only validates the comparator when the sort will actually
  ** compare elements. Empty and single-element tables accept any 2nd value.
  */
  if (n > 1 && !tvisnil(L->base+1)) {
    table_checkfunc_named54(L, 2, "table.sort");
  }
#if LJ_TARGET_ARM64
  if (table_sort_raw_int_array54(L, n))
    return 0;
#endif
#else
  if (!tvisnil(L->base+1)) {
    lj_lib_checkfunc(L, 2);
  }
#endif
  auxsort(L, 1, n);
  return 0;
}

#if LJ_54
static int lj_cf_table_unpack54(lua_State *L)
{
  int n;
  lua_Integer i = (L->base+1 < L->top && !tvisnil(L->base+1)) ?
		  table_checkinteger_named54(L, 2, "table.unpack") : 1;
  lua_Integer e;
  lua_Unsigned nu;
  if (L->base+2 < L->top && !tvisnil(L->base+2)) {
    e = table_checkinteger_named54(L, 3, "table.unpack");
  } else if (L->base < L->top && tvistab(L->base)) {
    e = table_len_integer_obj54(L, 1);
  } else {
    int ok = 0;
    if (L->base >= L->top)
      setnilV(L->top++);
    lua_len(L, 1);
    /* lua_numbertointeger is the public Lua 5.4 header macro and intentionally
    ** truncates in-range floats. Runtime length checks need exact integers.
    */
    e = lua_tointegerx(L, -1, &ok);
    if (!ok) {
      lua_pop(L, 1);
      luaL_error(L, "object length is not an integer");
    }
    lua_pop(L, 1);
  }
  if (i > e) return 0;
  nu = (lua_Unsigned)e - (lua_Unsigned)i;
  if (nu >= (lua_Unsigned)LUAI_MAXCSTACK)
    lj_err_caller(L, LJ_ERR_UNPACK);
  n = (int)(nu + 1u);
  if (!lua_checkstack(L, n))
    lj_err_caller(L, LJ_ERR_UNPACK);
  do {
    /* Lua 5.4 table.unpack reads through __index, unlike LuaJIT's raw array
    ** helper used by the legacy unpack path.
    */
    lua_geti(L, 1, i);
    if (i >= e) break;
    i = (lua_Integer)((lua_Unsigned)i + (lua_Unsigned)1);
  } while (1);
  return n;
}
#endif

#if LJ_52
LJLIB_PUSH("n")
LJLIB_CF(table_pack)
{
  TValue *array, *base = L->base;
  MSize i, n = (uint32_t)(L->top - base);
  GCtab *t = lj_tab_new(L, n ? n+1 : 0, 1);
  /* NOBARRIER: The table is new (marked white). */
  setintV(lj_tab_setstr(L, t, strV(lj_lib_upvalue(L, 1))), (int32_t)n);
  for (array = tvref(t->array) + 1, i = 0; i < n; i++)
    copyTV(L, &array[i], &base[i]);
  settabV(L, base, t);
  L->top = base+1;
  lj_gc_check(L);
  return 1;
}
#endif

LJLIB_NOREG LJLIB_CF(table_new)		LJLIB_REC(.)
{
  int32_t a = lj_lib_checkint(L, 1);
  int32_t h = lj_lib_checkint(L, 2);
  lua_createtable(L, a, h);
  return 1;
}

LJLIB_NOREG LJLIB_CF(table_clear)	LJLIB_REC(.)
{
  lj_tab_clear(lj_lib_checktab(L, 1));
  return 0;
}

static int luaopen_table_new(lua_State *L)
{
  return lj_lib_postreg(L, lj_cf_table_new, FF_table_new, "new");
}

static int luaopen_table_clear(lua_State *L)
{
  return lj_lib_postreg(L, lj_cf_table_clear, FF_table_clear, "clear");
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_table(lua_State *L)
{
  LJ_LIB_REG(L, LUA_TABLIBNAME, table);
#if LJ_54
  /* Hide Lua 5.1-only table helpers in the Lua 5.4 surface. */
  lua_pushnil(L); lua_setfield(L, -2, "foreach");
  lua_pushnil(L); lua_setfield(L, -2, "foreachi");
  lua_pushnil(L); lua_setfield(L, -2, "getn");
  lua_pushnil(L); lua_setfield(L, -2, "maxn");
  lua_pushcfunction(L, lj_cf_table_remove54);
  lua_setfield(L, -2, "remove");
  lua_pushcfunction(L, lj_cf_table_move54);
  lua_setfield(L, -2, "move");
  lua_pushcfunction(L, lj_cf_table_unpack54);
  lua_setfield(L, -2, "unpack");
#endif
#if LJ_52 && !LJ_54
  lua_getglobal(L, "unpack");
  lua_setfield(L, -2, "unpack");
#endif
  lj_lib_prereg(L, LUA_TABLIBNAME ".new", luaopen_table_new, tabV(L->top-1));
  lj_lib_prereg(L, LUA_TABLIBNAME ".clear", luaopen_table_clear, tabV(L->top-1));
  return 1;
}
