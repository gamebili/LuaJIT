/*
** OS library.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#include <errno.h>
#include <limits.h>
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
#include "lj_debug.h"
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
  fname = lj_debug_callname54(L, fname, "os");
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

static time_t os_checktime_named54(lua_State *L, int narg, const char *fname)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  int64_t k;
  if (o >= L->top)
    os_argtype_named54(L, narg, fname, "number");
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      os_argtype_named54(L, narg, fname, "number");
    o = &tmp;
  }
  if (tvisint(o)) {
    k = (int64_t)intV(o);
  } else if (tvisnum(o)) {
    lua_Number n = numV(o);
    if (!(n >= (lua_Number)INT64_MIN && n < -((lua_Number)INT64_MIN)))
      os_argerror_named54(L, narg, fname, "number has no integer representation");
    k = lj_num2i64(n);
    if ((lua_Number)k != n)
      os_argerror_named54(L, narg, fname, "number has no integer representation");
  } else {
    os_argtype_named54(L, narg, fname, "number");
    k = 0;  /* Unreachable. */
  }
  if ((int64_t)(time_t)k != k)
    os_argerror_named54(L, narg, fname, "time out-of-bounds");
  return (time_t)k;
}

static int32_t os_optint_named54(lua_State *L, int narg, int32_t def,
				 const char *fname)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  int64_t k;
  if (o >= L->top || tvisnil(o))
    return def;
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      os_argtype_named54(L, narg, fname, "number");
    o = &tmp;
  }
  if (tvisint(o)) {
    return intV(o);
  } else if (tvisnum(o)) {
    lua_Number n = numV(o);
    if (!(n >= -2147483648.0 && n <= 2147483647.0))
      os_argerror_named54(L, narg, fname,
			  "number has no integer representation");
    k = lj_num2i64(n);
    if ((lua_Number)k != n)
      os_argerror_named54(L, narg, fname,
			  "number has no integer representation");
    return (int32_t)k;
  } else {
    os_argtype_named54(L, narg, fname, "number");
    return 0;  /* Unreachable. */
  }
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
  int stat;
  /* Match Lua 5.4: system() status must not inherit errno from an earlier
  ** failing file operation, or luaL_execresult() reports the wrong tuple.
  */
  errno = 0;
  stat = system(cmd);
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
#if LJ_54
    /* Lua 5.4 reports invalid status arguments against the public os.exit()
    ** name; the second close argument remains a plain truthiness check.
    */
    status = os_optint_named54(L, 1, EXIT_SUCCESS, "os.exit");
#else
    status = lj_lib_optint(L, 1, EXIT_SUCCESS);
#endif
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

static void setallfields(lua_State *L, struct tm *stm)
{
  setfield(L, "sec", stm->tm_sec);
  setfield(L, "min", stm->tm_min);
  setfield(L, "hour", stm->tm_hour);
  setfield(L, "day", stm->tm_mday);
  setfield(L, "month", stm->tm_mon+1);
  setfield(L, "year", stm->tm_year+1900);
  setfield(L, "wday", stm->tm_wday+1);
  setfield(L, "yday", stm->tm_yday+1);
  setboolfield(L, "isdst", stm->tm_isdst);
}

static int getboolfield(lua_State *L, const char *key)
{
  int res;
  lua_getfield(L, -1, key);
  res = lua_isnil(L, -1) ? -1 : lua_toboolean(L, -1);
  lua_pop(L, 1);
  return res;
}

static int getfield(lua_State *L, const char *key, int d
#if LJ_54
		    , int delta
#endif
		   )
{
#if LJ_54
  TValue tmp;
  cTValue *o;
  int64_t res;
  lua_getfield(L, -1, key);
  o = L->top - 1;
  if (tvisnil(o)) {
    lua_pop(L, 1);
    if (d < 0)
      luaL_error(L, "field '%s' missing in date table", key);
    return d;
  }
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      luaL_error(L, "field '%s' is not an integer", key);
    o = &tmp;
  }
  if (tvisint(o)) {
    res = (int64_t)intV(o);
  } else if (tvisnum(o)) {
    lua_Number n = numV(o);
    int64_t k;
    if (!(n >= -2147483648.0 && n <= 2147483647.0))
      luaL_error(L, "field '%s' is not an integer", key);
    k = lj_num2i64(n);
    if ((lua_Number)k != n)
      luaL_error(L, "field '%s' is not an integer", key);
    res = k;
  } else {
    luaL_error(L, "field '%s' is not an integer", key);
    res = 0;  /* Unreachable. */
  }
  if (!(res >= 0 ? res - delta <= INT_MAX : INT_MIN + delta <= res))
    luaL_error(L, "field '%s' is out-of-bound", key);
  lua_pop(L, 1);
  return (int)(res - delta);
#else
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
#endif
}

#if LJ_54
#define OS_DATE_SIZETIMEFMT	250
#if LJ_TARGET_WINDOWS
#define OS_DATE_STRFTIMEOPTIONS  "aAbBcdHIjmMpSUwWxXyYzZ%" \
  "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"
#else
#define OS_DATE_STRFTIMEOPTIONS  "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
  "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"
#endif

static const char *os_date_checkoption54(lua_State *L, const char *conv,
					 MSize convlen, char *buff)
{
  const char *option = OS_DATE_STRFTIMEOPTIONS;
  int oplen = 1;
  for (; *option != '\0' && oplen <= (int)convlen; option += oplen) {
    if (*option == '|') {
      oplen++;
    } else if (memcmp(conv, option, (size_t)oplen) == 0) {
      memcpy(buff, conv, (size_t)oplen);
      buff[oplen] = '\0';
      return conv + oplen;
    }
  }
  os_argerror_named54(L, 1, "os.date",
    lua_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* Unreachable. */
}
#endif

LJLIB_CF(os_date)
{
#if LJ_54
  GCstr *fmt = os_optstr_named54(L, 1, "os.date");
  const char *s = fmt ? strdata(fmt) : "%c";
  MSize slen = fmt ? fmt->len : 2;
  time_t t = lua_isnoneornil(L, 2) ? time(NULL) :
	     os_checktime_named54(L, 2, "os.date");
#else
  const char *s = luaL_optstring(L, 1, "%c");
  time_t t = lua_isnoneornil(L, 2) ? time(NULL) :
	     lj_num2int_type(luaL_checknumber(L, 2), time_t);
#endif
  struct tm *stm;
#if LJ_TARGET_POSIX
  struct tm rtm;
#endif
  if (
#if LJ_54
      slen != 0 &&
#endif
      *s == '!') {  /* UTC? */
    s++;  /* Skip '!' */
#if LJ_54
    slen--;
#endif
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
#if LJ_54
    return luaL_error(L, "date result cannot be represented in this installation");
#else
    setnilV(L->top++);
#endif
  } else if (
#if LJ_54
	     slen == 2 && s[0] == '*' && s[1] == 't'
#else
	     strcmp(s, "*t") == 0
#endif
	     ) {
    lua_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
#if LJ_54
  } else {
    SBuf *sb = &G(L)->tmpbuf;
    const char *se = s + slen;
    char cc[4];
    cc[0] = '%';
    setsbufL(sb, L);
    lj_buf_reset(sb);
    while (s < se) {
      if (*s != '%') {
	lj_buf_putb(sb, (uint8_t)*s++);
      } else {
	size_t len;
	char *buf;
	s++;
	s = os_date_checkoption54(L, s, (MSize)(se - s), cc + 1);
	/* strftime is C-string based, so feed it one validated conversion at a
	** time and append literal bytes, including embedded NULs, separately.
	*/
	buf = lj_buf_more(sb, OS_DATE_SIZETIMEFMT);
#if LJ_TARGET_WINDOWS
	/* The official Windows Lua 5.4 binary reports %c through the same
	** short-date/short-time form as %x %X. Normalize this conversion so the
	** compat build is not exposed to the host CRT's alternate %c spelling.
	*/
	len = strftime(buf, OS_DATE_SIZETIMEFMT,
		       (cc[1] == 'c' && cc[2] == '\0') ? "%x %X" : cc, stm);
#else
	len = strftime(buf, OS_DATE_SIZETIMEFMT, cc, stm);
#endif
	sb->w = buf + len;
      }
    }
    setstrV(L, L->top++, lj_buf_str(L, sb));
    lj_gc_check(L);
#else
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
#endif
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
#if LJ_54
    ts.tm_sec = getfield(L, "sec", 0, 0);
    ts.tm_min = getfield(L, "min", 0, 0);
    ts.tm_hour = getfield(L, "hour", 12, 0);
    ts.tm_mday = getfield(L, "day", -1, 0);
    ts.tm_mon = getfield(L, "month", -1, 1);
    ts.tm_year = getfield(L, "year", -1, 1900);
#else
    ts.tm_sec = getfield(L, "sec", 0);
    ts.tm_min = getfield(L, "min", 0);
    ts.tm_hour = getfield(L, "hour", 12);
    ts.tm_mday = getfield(L, "day", -1);
    ts.tm_mon = getfield(L, "month", -1) - 1;
    ts.tm_year = getfield(L, "year", -1) - 1900;
#endif
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
#if LJ_54
    /* Lua 5.4 exposes mktime normalization by updating the input date table. */
    setallfields(L, &ts);
#endif
  }
  if (t == (time_t)(-1))
#if LJ_54
    return luaL_error(L, "time result cannot be represented in this installation");
#else
    lua_pushnil(L);
#endif
#if LJ_54
  /* Lua 5.4 returns os.time() as an integer.  The current compat runtime still
  ** has a 32-bit integer subtype, so setint64V keeps in-range timestamps as
  ** integers and falls back to number for values that need the larger surface.
  */
  else
    setint64V(L->top++, (int64_t)t);
#else
  else
    lua_pushnumber(L, (lua_Number)t);
#endif
  return 1;
}

LJLIB_CF(os_difftime)
{
#if LJ_54
  time_t t1, t2;
  /* Do not pass checks directly to difftime(): C does not define argument
  ** evaluation order, while Lua 5.4 reports a missing first time as #1.
  */
  t1 = os_checktime_named54(L, 1, "os.difftime");
  t2 = os_checktime_named54(L, 2, "os.difftime");
  lua_pushnumber(L, difftime(t1, t2));
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

