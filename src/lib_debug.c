/*
** Debug library.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lib_debug_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_meta.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "lj_lib.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_debug

LJLIB_CF(debug_getregistry)
{
  copyTV(L, L->top++, registry(L));
  return 1;
}

LJLIB_CF(debug_getmetatable)	LJLIB_REC(.)
{
  lj_lib_checkany(L, 1);
  if (!lua_getmetatable(L, 1)) {
    setnilV(L->top-1);
  }
  return 1;
}

LJLIB_CF(debug_setmetatable)
{
  lj_lib_checktabornil(L, 2);
  L->top = L->base+2;
  lua_setmetatable(L, 1);
#if !LJ_52
  setboolV(L->top-1, 1);
#endif
  return 1;
}

LJLIB_CF(debug_getfenv)
{
  lj_lib_checkany(L, 1);
  lua_getfenv(L, 1);
  return 1;
}

LJLIB_CF(debug_setfenv)
{
  lj_lib_checktab(L, 2);
  L->top = L->base+2;
  if (!lua_setfenv(L, 1))
    lj_err_caller(L, LJ_ERR_SETFENV);
  return 1;
}

/* ------------------------------------------------------------------------ */

static void settabss(lua_State *L, const char *i, const char *v)
{
  lua_pushstring(L, v);
  lua_setfield(L, -2, i);
}

static void settabsi(lua_State *L, const char *i, int v)
{
  lua_pushinteger(L, v);
  lua_setfield(L, -2, i);
}

static void settabsb(lua_State *L, const char *i, int v)
{
  lua_pushboolean(L, v);
  lua_setfield(L, -2, i);
}

static lua_State *getthread(lua_State *L, int *arg)
{
  if (L->base < L->top && tvisthread(L->base)) {
    *arg = 1;
    return threadV(L->base);
  } else {
    *arg = 0;
    return L;
  }
}

#if LJ_54
static void debug_argerror_named54(lua_State *L, int narg, const char *fname,
				   const char *msg)
{
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      narg, fname, msg));
}

static const char *debug_argtypename54(lua_State *L, int narg)
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

static void debug_argtype_named54(lua_State *L, int narg, const char *fname,
				  const char *xname)
{
  debug_argerror_named54(L, narg, fname,
    lj_strfmt_pushf(L, "%s expected, got %s", xname,
		    debug_argtypename54(L, narg)));
}

static TValue *debug_checkany_named54(lua_State *L, int narg,
				      const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o >= L->top)
    debug_argerror_named54(L, narg, fname, "value expected");
  return o;
}

static GCfunc *debug_checkfunc_named54(lua_State *L, int narg,
				       const char *fname)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && tvisfunc(o)))
    debug_argtype_named54(L, narg, fname, "function");
  return funcV(o);
}

static GCstr *debug_checkstr_named54(lua_State *L, int narg,
				     const char *fname)
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
  debug_argtype_named54(L, narg, fname, "string");
  return NULL;  /* unreachable */
}

static GCstr *debug_optstr_named54(lua_State *L, int narg,
				   const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o >= L->top || tvisnil(o))
    return NULL;
  return debug_checkstr_named54(L, narg, fname);
}

static int32_t debug_checkint_named54(lua_State *L, int narg,
				      const char *fname)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  lua_Number n;
  int64_t k;
  /* Lua 5.4 debug APIs use luaL_checkinteger semantics: string numerals are
  ** accepted, but fractional numbers must not be silently truncated.
  */
  if (o >= L->top)
    debug_argtype_named54(L, narg, fname, "number");
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      debug_argtype_named54(L, narg, fname, "number");
    o = &tmp;
  }
  if (tvisint(o))
    return intV(o);
  if (!tvisnum(o))
    debug_argtype_named54(L, narg, fname, "number");
  n = numV(o);
  if (!(n >= -2147483648.0 && n <= 2147483647.0))
    debug_argerror_named54(L, narg, fname,
			   "number has no integer representation");
  k = lj_num2i64(n);
  if ((lua_Number)k != n)
    debug_argerror_named54(L, narg, fname,
			   "number has no integer representation");
  return (int32_t)k;
}

static int32_t debug_optint_named54(lua_State *L, int narg, int32_t def,
				    const char *fname)
{
  return lua_isnoneornil(L, narg) ? def :
	 debug_checkint_named54(L, narg, fname);
}
#endif

static void treatstackoption(lua_State *L, lua_State *L1, const char *fname)
{
  if (L == L1) {
    lua_pushvalue(L, -2);
    lua_remove(L, -3);
  }
  else
    lua_xmove(L1, L, 1);
  lua_setfield(L, -2, fname);
}

LJLIB_CF(debug_getinfo)
{
  lj_Debug ar;
  int arg, opt_f = 0, opt_L = 0;
  lua_State *L1 = getthread(L, &arg);
#if LJ_54
  GCstr *optstr = debug_optstr_named54(L, arg+2, "debug.getinfo");
  const char *options = optstr ? strdata(optstr) : "flnSu";
  if (L->base+arg < L->top && tvisfunc(L->base+arg)) {
    options = lua_pushfstring(L, ">%s", options);
    setfuncV(L1, L1->top++, funcV(L->base+arg));
  } else {
    int32_t level = debug_checkint_named54(L, arg+1, "debug.getinfo");
    if (!lua_getstack(L1, level, (lua_Debug *)&ar)) {
      setnilV(L->top-1);
      return 1;
    }
  }
#else
  const char *options = luaL_optstring(L, arg+2, "flnSu");
  if (lua_isnumber(L, arg+1)) {
    int32_t level;
    level = (int32_t)lua_tointeger(L, arg+1);
    if (!lua_getstack(L1, level, (lua_Debug *)&ar)) {
      setnilV(L->top-1);
      return 1;
    }
  } else if (L->base+arg < L->top && tvisfunc(L->base+arg)) {
    options = lua_pushfstring(L, ">%s", options);
    setfuncV(L1, L1->top++, funcV(L->base+arg));
  } else {
    lj_err_arg(L, arg+1, LJ_ERR_NOFUNCL);
  }
#endif
  if (!lj_debug_getinfo(L1, options, &ar, 1))
#if LJ_54
    debug_argerror_named54(L, arg+2, "debug.getinfo", "invalid option");
#else
    lj_err_arg(L, arg+2, LJ_ERR_INVOPT);
#endif
  lua_createtable(L, 0, 16);  /* Create result table. */
  for (; *options; options++) {
    switch (*options) {
    case 'S':
      settabss(L, "source", ar.source);
      settabss(L, "short_src", ar.short_src);
      settabsi(L, "linedefined", ar.linedefined);
      settabsi(L, "lastlinedefined", ar.lastlinedefined);
      settabss(L, "what", ar.what);
      break;
    case 'l':
      settabsi(L, "currentline", ar.currentline);
      break;
    case 'u':
      settabsi(L, "nups", ar.nups);
      settabsi(L, "nparams", ar.nparams);
      settabsb(L, "isvararg", ar.isvararg);
      break;
    case 'n':
      settabss(L, "name", ar.name);
      settabss(L, "namewhat", ar.namewhat);
      break;
    case 't':
      settabsb(L, "istailcall", ar.istailcall);
      break;
    case 'r':
      settabsi(L, "ftransfer", ar.ftransfer);
      settabsi(L, "ntransfer", ar.ntransfer);
      break;
    case 'f': opt_f = 1; break;
    case 'L': opt_L = 1; break;
    default: break;
    }
  }
  if (opt_L) treatstackoption(L, L1, "activelines");
  if (opt_f) treatstackoption(L, L1, "func");
  return 1;  /* Return result table. */
}

LJLIB_CF(debug_getlocal)
{
  int arg;
  lua_State *L1 = getthread(L, &arg);
  lua_Debug ar;
  const char *name;
#if LJ_54
  int32_t slot = debug_checkint_named54(L, arg+2, "debug.getlocal");
#else
  int slot = lj_lib_checkint(L, arg+2);
#endif
  if (tvisfunc(L->base+arg)) {
    L->top = L->base+arg+1;
    lua_pushstring(L, lua_getlocal(L, NULL, slot));
    return 1;
  }
#if LJ_54
  if (!lua_getstack(L1,
		    debug_checkint_named54(L, arg+1, "debug.getlocal"),
		    &ar))
#else
  if (!lua_getstack(L1, lj_lib_checkint(L, arg+1), &ar))
#endif
    lj_err_arg(L, arg+1, LJ_ERR_LVLRNG);
  name = lua_getlocal(L1, &ar, slot);
  if (name) {
    lua_xmove(L1, L, 1);
    lua_pushstring(L, name);
    lua_pushvalue(L, -2);
    return 2;
  } else {
    setnilV(L->top-1);
    return 1;
  }
}

LJLIB_CF(debug_setlocal)
{
  int arg;
  lua_State *L1 = getthread(L, &arg);
  lua_Debug ar;
  TValue *tv;
#if LJ_54
  if (!lua_getstack(L1,
		    debug_checkint_named54(L, arg+1, "debug.setlocal"),
		    &ar))
#else
  if (!lua_getstack(L1, lj_lib_checkint(L, arg+1), &ar))
#endif
    lj_err_arg(L, arg+1, LJ_ERR_LVLRNG);
#if LJ_54
  tv = debug_checkany_named54(L, arg+3, "debug.setlocal");
#else
  tv = lj_lib_checkany(L, arg+3);
#endif
  copyTV(L1, L1->top++, tv);
  lua_pushstring(L, lua_setlocal(L1, &ar,
#if LJ_54
				 debug_checkint_named54(L, arg+2,
							"debug.setlocal")
#else
				 lj_lib_checkint(L, arg+2)
#endif
				 ));
  return 1;
}

static int debug_getupvalue(lua_State *L, int get, const char *fname)
{
#if LJ_54
  int32_t n = debug_checkint_named54(L, 2, fname);
#else
  int32_t n = lj_lib_checkint(L, 2);
#endif
  const char *name;
#if LJ_54
  debug_checkfunc_named54(L, 1, fname);
#else
  lj_lib_checkfunc(L, 1);
#endif
  name = get ? lua_getupvalue(L, 1, n) : lua_setupvalue(L, 1, n);
  if (name) {
    lua_pushstring(L, name);
    if (!get) return 1;
    copyTV(L, L->top, L->top-2);
    L->top++;
    return 2;
  }
  return 0;
}

LJLIB_CF(debug_getupvalue)
{
  return debug_getupvalue(L, 1, "debug.getupvalue");
}

LJLIB_CF(debug_setupvalue)
{
#if LJ_54
  debug_checkany_named54(L, 3, "debug.setupvalue");
  return debug_getupvalue(L, 0, "debug.setupvalue");
#else
  lj_lib_checkany(L, 3);
  return debug_getupvalue(L, 0, "debug.setupvalue");
#endif
}

LJLIB_CF(debug_upvalueid)
{
#if LJ_54
  GCfunc *fn = debug_checkfunc_named54(L, 1, "debug.upvalueid");
  int32_t n = debug_checkint_named54(L, 2, "debug.upvalueid");
#else
  GCfunc *fn = lj_lib_checkfunc(L, 1);
  int32_t n = lj_lib_checkint(L, 2);
#endif
#if LJ_54
  if (lj_debug_hasenvuv(fn)) {
    if (n == 1) {
      lua_pushlightuserdata(L, (void *)&fn->c.env);
      return 1;
    }
    n--;
  }
#endif
  n--;
  if ((uint32_t)n >= fn->l.nupvalues)
#if LJ_54
    debug_argerror_named54(L, 2, "debug.upvalueid",
			   "index out of range");
#else
    lj_err_arg(L, 2, LJ_ERR_IDXRNG);
#endif
  lua_pushlightuserdata(L, isluafunc(fn) ? (void *)gcref(fn->l.uvptr[n]) :
					   (void *)&fn->c.upvalue[n]);
  return 1;
}

LJLIB_CF(debug_upvaluejoin)
{
  GCfunc *fn[2];
  GCRef *p[2];
#if LJ_54
  int envuv[2] = { 0, 0 };
#endif
  int i;
  for (i = 0; i < 2; i++) {
    int32_t n;
#if LJ_54
    fn[i] = debug_checkfunc_named54(L, 2*i+1, "debug.upvaluejoin");
#else
    fn[i] = lj_lib_checkfunc(L, 2*i+1);
#endif
    if (!isluafunc(fn[i]))
#if LJ_54
      debug_argerror_named54(L, 2*i+1, "debug.upvaluejoin",
			     "Lua function expected");
#else
      lj_err_arg(L, 2*i+1, LJ_ERR_NOLFUNC);
#endif
#if LJ_54
    n = debug_checkint_named54(L, 2*i+2, "debug.upvaluejoin");
#else
    n = lj_lib_checkint(L, 2*i+2);
#endif
#if LJ_54
    if (lj_debug_hasenvuv(fn[i])) {
      if (n == 1) {
	envuv[i] = 1;
	continue;
      }
      n--;
    }
#endif
    n--;
    if ((uint32_t)n >= fn[i]->l.nupvalues)
#if LJ_54
      debug_argerror_named54(L, 2*i+2, "debug.upvaluejoin",
			     "index out of range");
#else
      lj_err_arg(L, 2*i+2, LJ_ERR_IDXRNG);
#endif
    p[i] = &fn[i]->l.uvptr[n];
  }
#if LJ_54
  if (envuv[0]) {
    GCtab *t;
    if (envuv[1]) {
      t = tabref(fn[1]->c.env);
    } else {
      GCupval *uv = &gcref(*p[1])->uv;
      TValue *tv = uvval(uv);
      if (!tvistab(tv))
#if LJ_54
	debug_argerror_named54(L, 4, "debug.upvaluejoin",
			       "index out of range");
#else
	lj_err_arg(L, 4, LJ_ERR_IDXRNG);
#endif
      t = tabV(tv);
    }
    setgcref(fn[0]->c.env, obj2gco(t));
    lj_gc_objbarrier(L, fn[0], t);
    return 0;
  } else if (envuv[1]) {
    GCupval *uv = &gcref(*p[0])->uv;
    TValue *tv = uvval(uv);
    settabV(L, tv, tabref(fn[1]->c.env));
    lj_gc_barrier(L, obj2gco(uv), tv);
    return 0;
  }
#endif
  setgcrefr(*p[0], *p[1]);
  lj_gc_objbarrier(L, fn[0], gcref(*p[1]));
  return 0;
}

#if LJ_52
LJLIB_CF(debug_getuservalue)
{
  TValue *o = L->base;
#if LJ_54
  int32_t n;
  int tp;
  if (!(o < L->top && tvisudata(o)))
    debug_argtype_named54(L, 1, "debug.getuservalue", "userdata");
  n = debug_optint_named54(L, 2, 1, "debug.getuservalue");
  tp = lua_getiuservalue(L, 1, n);
  if (tp == LUA_TNONE) {
    setnilV(o);
  } else {
    copyTV(L, o, L->top-1);
  }
  L->top = o+1;
  return 1;
#else
  if (o < L->top && tvisudata(o))
    settabV(L, o, tabref(udataV(o)->env));
  else
    setnilV(o);
  L->top = o+1;
  return 1;
#endif
}

LJLIB_CF(debug_setuservalue)
{
  TValue *o = L->base;
#if LJ_54
  int32_t n;
  int ok;
  if (!(o < L->top && tvisudata(o)))
    debug_argtype_named54(L, 1, "debug.setuservalue", "userdata");
  debug_checkany_named54(L, 2, "debug.setuservalue");
  n = debug_optint_named54(L, 3, 1, "debug.setuservalue");
  L->top = o+2;  /* lua_setiuservalue consumes the value from the top. */
  ok = lua_setiuservalue(L, 1, n);
  if (!ok)
    setnilV(o);
  L->top = o+1;
  return 1;
#else
  if (!(o < L->top && tvisudata(o)))
    lj_err_argt(L, 1, LUA_TUSERDATA);
  if (!(o+1 < L->top && tvistab(o+1)))
    lj_err_argt(L, 2, LUA_TTABLE);
  L->top = o+2;
  lua_setfenv(L, 1);
  return 1;
#endif
}
#endif

/* ------------------------------------------------------------------------ */

#define KEY_HOOK	(U64x(81000000,00000000)|'h')

static void hookf(lua_State *L, lua_Debug *ar)
{
  static const char *const hooknames[] =
    {"call", "return", "line", "count",
#if LJ_54
     "tail call"
#else
     "tail return"
#endif
    };
  (L->top++)->u64 = KEY_HOOK;
  lua_rawget(L, LUA_REGISTRYINDEX);
  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, hooknames[(int)ar->event]);
    if (ar->currentline >= 0)
      lua_pushinteger(L, ar->currentline);
    else lua_pushnil(L);
    lua_call(L, 2, 0);
  }
}

static int makemask(const char *smask, int count)
{
  int mask = 0;
  if (strchr(smask, 'c')) mask |= LUA_MASKCALL;
  if (strchr(smask, 'r')) mask |= LUA_MASKRET;
  if (strchr(smask, 'l')) mask |= LUA_MASKLINE;
  if (count > 0) mask |= LUA_MASKCOUNT;
  return mask;
}

static char *unmakemask(int mask, char *smask)
{
  int i = 0;
  if (mask & LUA_MASKCALL) smask[i++] = 'c';
  if (mask & LUA_MASKRET) smask[i++] = 'r';
  if (mask & LUA_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}

LJLIB_CF(debug_sethook)
{
  int arg, mask, count;
  lua_Hook func;
  (void)getthread(L, &arg);
  if (lua_isnoneornil(L, arg+1)) {
    lua_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  } else {
#if LJ_54
    const char *smask = strdata(debug_checkstr_named54(L, arg+2,
						       "debug.sethook"));
    debug_checkfunc_named54(L, arg+1, "debug.sethook");
    count = debug_optint_named54(L, arg+3, 0, "debug.sethook");
#else
    const char *smask = luaL_checkstring(L, arg+2);
    luaL_checktype(L, arg+1, LUA_TFUNCTION);
    count = luaL_optint(L, arg+3, 0);
#endif
    func = hookf; mask = makemask(smask, count);
  }
  (L->top++)->u64 = KEY_HOOK;
  lua_pushvalue(L, arg+1);
  lua_rawset(L, LUA_REGISTRYINDEX);
  lua_sethook(L, func, mask, count);
  return 0;
}

LJLIB_CF(debug_gethook)
{
  char buff[5];
  int mask = lua_gethookmask(L);
  lua_Hook hook = lua_gethook(L);
  if (hook != NULL && hook != hookf) {  /* external hook? */
    lua_pushliteral(L, "external hook");
  } else {
    (L->top++)->u64 = KEY_HOOK;
    lua_rawget(L, LUA_REGISTRYINDEX);   /* get hook */
  }
  lua_pushstring(L, unmakemask(mask, buff));
  lua_pushinteger(L, lua_gethookcount(L));
  return 3;
}

/* ------------------------------------------------------------------------ */

LJLIB_CF(debug_debug)
{
  for (;;) {
    char buffer[250];
    fputs("lua_debug> ", stderr);
    if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
	strcmp(buffer, "cont\n") == 0)
      return 0;
    if (luaL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
	lua_pcall(L, 0, 0, 0)) {
      const char *s = lua_tostring(L, -1);
      fputs(s ? s : "(error object is not a string)", stderr);
      fputs("\n", stderr);
    }
    lua_settop(L, 0);  /* remove eventual returns */
  }
}

/* ------------------------------------------------------------------------ */

#define LEVELS1	12	/* size of the first part of the stack */
#define LEVELS2	10	/* size of the second part of the stack */

LJLIB_CF(debug_traceback)
{
  int arg;
  lua_State *L1 = getthread(L, &arg);
  const char *msg = lua_tostring(L, arg+1);
  if (msg == NULL && L->top > L->base+arg)
    L->top = L->base+arg+1;
  else
    luaL_traceback(L, L1, msg,
#if LJ_54
		   debug_optint_named54(L, arg+2, (L == L1),
					"debug.traceback")
#else
		   lj_lib_optint(L, arg+2, (L == L1))
#endif
		   );
  return 1;
}

/* ------------------------------------------------------------------------ */

#if LJ_54
static int lj_cf_debug_setcstacklimit(lua_State *L)
{
  int32_t limit = debug_checkint_named54(L, 1, "debug.setcstacklimit");
  UNUSED(limit);
  /* LuaJIT has its own C stack checks; this Lua 5.4 entrypoint is a no-op
  ** compatibility shim and returns the stable effective limit value.
  */
  lua_pushinteger(L, 200);
  return 1;
}
#endif

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_debug(lua_State *L)
{
  LJ_LIB_REG(L, LUA_DBLIBNAME, debug);
#if LJ_54
  /* Lua 5.4 removed environment APIs; hide them from the compat surface. */
  lua_pushnil(L); lua_setfield(L, -2, "getfenv");
  lua_pushnil(L); lua_setfield(L, -2, "setfenv");
  lua_pushcfunction(L, lj_cf_debug_setcstacklimit);
  lua_setfield(L, -2, "setcstacklimit");
#endif
  return 1;
}

