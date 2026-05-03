/*
** OS library.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#include <errno.h>
#include <string.h>
#include <time.h>

#define lib_os_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_meta.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "lj_lib.h"

#if LJ_TARGET_POSIX
#include <unistd.h>
#else
#include <stdio.h>
#endif

#if !LJ_TARGET_PSVITA
#include <locale.h>
#endif

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_os

#if LJ_54
static void os_argerror_named54(lua_State *L, int narg, const char *fname,
				const char *msg)
{
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      narg, fname, msg));
}

static const char *os_argtypename54(lua_State *L, int narg)
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

static void os_argtype_named54(lua_State *L, int narg, const char *fname,
			       const char *xname)
{
  os_argerror_named54(L, narg, fname,
    lj_strfmt_pushf(L, "%s expected, got %s", xname,
		    os_argtypename54(L, narg)));
}

static GCstr *os_checkstr_named54(lua_State *L, int narg, const char *fname)
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
  os_argtype_named54(L, narg, fname, "string");
  return NULL;  /* unreachable */
}

static GCstr *os_optstr_named54(lua_State *L, int narg, const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o >= L->top || tvisnil(o))
    return NULL;
  return os_checkstr_named54(L, narg, fname);
}

static lua_Number os_checknum_named54(lua_State *L, int narg,
				      const char *fname)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  if (o >= L->top)
    os_argtype_named54(L, narg, fname, "number");
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      os_argtype_named54(L, narg, fname, "number");
    o = &tmp;
  }
  if (tvisint(o))
    return (lua_Number)intV(o);
  if (!tvisnum(o))
    os_argtype_named54(L, narg, fname, "number");
  return numV(o);
}

static lua_Number os_optnum_named54(lua_State *L, int narg, lua_Number def,
				    const char *fname)
{
  cTValue *o = L->base + narg-1;
  return (o < L->top && !tvisnil(o)) ?
	 os_checknum_named54(L, narg, fname) : def;
}

static void os_checktab_named54(lua_State *L, int narg, const char *fname)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && tvistab(o)))
    os_argtype_named54(L, narg, fname, "table");
}

static int os_checkopt_named54(lua_State *L, int narg, int def,
			       const char *lst, const char *fname)
{
  GCstr *s = def >= 0 ? os_optstr_named54(L, narg, fname) :
			 os_checkstr_named54(L, narg, fname);
  if (s) {
    const char *opt = strdata(s);
    MSize len = s->len;
    int i;
    for (i = 0; *(const uint8_t *)lst; i++) {
      if (*(const uint8_t *)lst == len && memcmp(opt, lst+1, len) == 0)
	return i;
      lst += 1+*(const uint8_t *)lst;
    }
    os_argerror_named54(L, narg, fname,
			lj_strfmt_pushf(L, "invalid option '%s'", opt));
  }
  return def;
}
#endif

LJLIB_CF(os_execute)
{
#if LJ_NO_SYSTEM
#if LJ_52
  errno = ENOSYS;
  return luaL_fileresult(L, 0, NULL);
#else
  lua_pushinteger(L, -1);
  return 1;
#endif
#else
#if LJ_54
  GCstr *cmdstr = os_optstr_named54(L, 1, "os.execute");
  const char *cmd = cmdstr ? strdata(cmdstr) : NULL;
#else
  const char *cmd = luaL_optstring(L, 1, NULL);
#endif
  int stat = system(cmd);
#if LJ_52
  if (cmd)
    return luaL_execresult(L, stat);
  setboolV(L->top++, 1);
#else
  setintV(L->top++, stat);
#endif
  return 1;
#endif
}

LJLIB_CF(os_remove)
{
#if LJ_54
  const char *filename = strdata(os_checkstr_named54(L, 1, "os.remove"));
#else
  const char *filename = luaL_checkstring(L, 1);
#endif
  return luaL_fileresult(L, remove(filename) == 0, filename);
}

LJLIB_CF(os_rename)
{
#if LJ_54
  const char *fromname = strdata(os_checkstr_named54(L, 1, "os.rename"));
  const char *toname = strdata(os_checkstr_named54(L, 2, "os.rename"));
#else
  const char *fromname = luaL_checkstring(L, 1);
  const char *toname = luaL_checkstring(L, 2);
#endif
#if LJ_54
  /* Lua 5.4 reports rename failures with the raw system error string; unlike
  ** os.remove, it does not prepend the source file name.
  */
  return luaL_fileresult(L, rename(fromname, toname) == 0, NULL);
#else
  return luaL_fileresult(L, rename(fromname, toname) == 0, fromname);
#endif
}

LJLIB_CF(os_tmpname)
{
#if LJ_TARGET_PS3 || LJ_TARGET_PS4 || LJ_TARGET_PS5 || LJ_TARGET_PSVITA || LJ_TARGET_NX
  lj_err_caller(L, LJ_ERR_OSUNIQF);
  return 0;
#else
#if LJ_TARGET_POSIX
  char buf[15+1];
  int fp;
  strcpy(buf, "/tmp/lua_XXXXXX");
  fp = mkstemp(buf);
  if (fp != -1)
    close(fp);
  else
    lj_err_caller(L, LJ_ERR_OSUNIQF);
#else
  char buf[L_tmpnam];
  if (tmpnam(buf) == NULL)
    lj_err_caller(L, LJ_ERR_OSUNIQF);
#endif
  lua_pushstring(L, buf);
  return 1;
#endif
}

LJLIB_CF(os_getenv)
{
#if LJ_TARGET_CONSOLE
  lua_pushnil(L);
#else
#if LJ_54
  lua_pushstring(L, getenv(strdata(os_checkstr_named54(L, 1, "os.getenv"))));
#else
  lua_pushstring(L, getenv(luaL_checkstring(L, 1)));  /* if NULL push nil */
#endif
#endif
  return 1;
}

LJLIB_CF(os_exit)
{
  int status;
  if (L->base < L->top && tvisbool(L->base))
    status = boolV(L->base) ? EXIT_SUCCESS : EXIT_FAILURE;
  else
    status = lj_lib_optint(L, 1, EXIT_SUCCESS);
  if (L->base+1 < L->top && tvistruecond(L->base+1))
    lua_close(L);
  exit(status);
  return 0;  /* Unreachable. */
}

LJLIB_CF(os_clock)
{
  setnumV(L->top++, ((lua_Number)clock())*(1.0/(lua_Number)CLOCKS_PER_SEC));
  return 1;
}

/* ------------------------------------------------------------------------ */

static void setfield(lua_State *L, const char *key, int value)
{
  lua_pushinteger(L, value);
  lua_setfield(L, -2, key);
}

static void setboolfield(lua_State *L, const char *key, int value)
{
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  lua_pushboolean(L, value);
  lua_setfield(L, -2, key);
}

static int getboolfield(lua_State *L, const char *key)
{
  int res;
  lua_getfield(L, -1, key);
  res = lua_isnil(L, -1) ? -1 : lua_toboolean(L, -1);
  lua_pop(L, 1);
  return res;
}

static int getfield(lua_State *L, const char *key, int d)
{
  int res;
  lua_getfield(L, -1, key);
  if (lua_isnumber(L, -1)) {
    res = (int)lua_tointeger(L, -1);
  } else {
    if (d < 0)
      lj_err_callerv(L, LJ_ERR_OSDATEF, key);
    res = d;
  }
  lua_pop(L, 1);
  return res;
}

LJLIB_CF(os_date)
{
#if LJ_54
  GCstr *fmt = os_optstr_named54(L, 1, "os.date");
  const char *s = fmt ? strdata(fmt) : "%c";
  time_t t = lua_isnoneornil(L, 2) ? time(NULL) :
	     lj_num2int_type(os_checknum_named54(L, 2, "os.date"), time_t);
#else
  const char *s = luaL_optstring(L, 1, "%c");
  time_t t = lua_isnoneornil(L, 2) ? time(NULL) :
	     lj_num2int_type(luaL_checknumber(L, 2), time_t);
#endif
  struct tm *stm;
#if LJ_TARGET_POSIX
  struct tm rtm;
#endif
  if (*s == '!') {  /* UTC? */
    s++;  /* Skip '!' */
#if LJ_TARGET_POSIX
    stm = gmtime_r(&t, &rtm);
#else
    stm = gmtime(&t);
#endif
  } else {
#if LJ_TARGET_POSIX
    stm = localtime_r(&t, &rtm);
#else
    stm = localtime(&t);
#endif
  }
  if (stm == NULL) {  /* Invalid date? */
    setnilV(L->top++);
  } else if (strcmp(s, "*t") == 0) {
    lua_createtable(L, 0, 9);  /* 9 = number of fields */
    setfield(L, "sec", stm->tm_sec);
    setfield(L, "min", stm->tm_min);
    setfield(L, "hour", stm->tm_hour);
    setfield(L, "day", stm->tm_mday);
    setfield(L, "month", stm->tm_mon+1);
    setfield(L, "year", stm->tm_year+1900);
    setfield(L, "wday", stm->tm_wday+1);
    setfield(L, "yday", stm->tm_yday+1);
    setboolfield(L, "isdst", stm->tm_isdst);
  } else if (*s) {
    SBuf *sb = &G(L)->tmpbuf;
    MSize sz = 0, retry = 4;
    const char *q;
    for (q = s; *q; q++)
      sz += (*q == '%') ? 30 : 1;  /* Overflow doesn't matter. */
    setsbufL(sb, L);
    while (retry--) {  /* Limit growth for invalid format or empty result. */
      char *buf = lj_buf_need(sb, sz);
      size_t len = strftime(buf, sbufsz(sb), s, stm);
      if (len) {
	setstrV(L, L->top++, lj_str_new(L, buf, len));
	lj_gc_check(L);
	break;
      }
      sz += (sz|1);
    }
  } else {
    setstrV(L, L->top++, &G(L)->strempty);
  }
  return 1;
}

LJLIB_CF(os_time)
{
  time_t t;
  if (lua_isnoneornil(L, 1)) {  /* called without args? */
    t = time(NULL);  /* get current time */
  } else {
    struct tm ts;
#if LJ_54
    os_checktab_named54(L, 1, "os.time");
#else
    luaL_checktype(L, 1, LUA_TTABLE);
#endif
    lua_settop(L, 1);  /* make sure table is at the top */
    ts.tm_sec = getfield(L, "sec", 0);
    ts.tm_min = getfield(L, "min", 0);
    ts.tm_hour = getfield(L, "hour", 12);
    ts.tm_mday = getfield(L, "day", -1);
    ts.tm_mon = getfield(L, "month", -1) - 1;
    ts.tm_year = getfield(L, "year", -1) - 1900;
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
  }
  if (t == (time_t)(-1))
    lua_pushnil(L);
  else
    lua_pushnumber(L, (lua_Number)t);
  return 1;
}

LJLIB_CF(os_difftime)
{
#if LJ_54
  lua_pushnumber(L,
    difftime(lj_num2int_type(os_checknum_named54(L, 1, "os.difftime"), time_t),
	     lj_num2int_type(os_optnum_named54(L, 2, (lua_Number)0,
					       "os.difftime"), time_t)));
#else
  lua_pushnumber(L,
    difftime(lj_num2int_type(luaL_checknumber(L, 1), time_t),
	     lj_num2int_type(luaL_optnumber(L, 2, (lua_Number)0), time_t)));
#endif
  return 1;
}

/* ------------------------------------------------------------------------ */

LJLIB_CF(os_setlocale)
{
#if LJ_TARGET_PSVITA
  lua_pushliteral(L, "C");
#else
#if LJ_54
  GCstr *s = os_optstr_named54(L, 1, "os.setlocale");
#else
  GCstr *s = lj_lib_optstr(L, 1);
#endif
  const char *str = s ? strdata(s) : NULL;
#if LJ_54
  int opt = os_checkopt_named54(L, 2, 6,
    "\5ctype\7numeric\4time\7collate\10monetary\1\377\3all",
    "os.setlocale");
#else
  int opt = lj_lib_checkopt(L, 2, 6,
    "\5ctype\7numeric\4time\7collate\10monetary\1\377\3all");
#endif
  if (opt == 0) opt = LC_CTYPE;
  else if (opt == 1) opt = LC_NUMERIC;
  else if (opt == 2) opt = LC_TIME;
  else if (opt == 3) opt = LC_COLLATE;
  else if (opt == 4) opt = LC_MONETARY;
  else if (opt == 6) opt = LC_ALL;
  lua_pushstring(L, setlocale(opt, str));
#endif
  return 1;
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_os(lua_State *L)
{
  LJ_LIB_REG(L, LUA_OSLIBNAME, os);
  return 1;
}

