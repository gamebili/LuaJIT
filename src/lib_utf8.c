/*
** UTF-8 library for Lua 5.4 compatibility mode.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#include <stdint.h>
#include <string.h>

#define lib_utf8_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_meta.h"
#include "lj_str.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"

/* ------------------------------------------------------------------------ */

static void utf8_argerror_named(lua_State *L, int narg, const char *fname,
				const char *msg)
{
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      narg, fname, msg));
}

static const char *utf8_argtypename(lua_State *L, int narg)
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

static void utf8_argtype_named(lua_State *L, int narg, const char *fname,
			       const char *xname)
{
  utf8_argerror_named(L, narg, fname,
    lj_strfmt_pushf(L, "%s expected, got %s", xname,
		    utf8_argtypename(L, narg)));
}

static const char *utf8_checklstring_named(lua_State *L, int narg,
					   size_t *len, const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o < L->top) {
    GCstr *s;
    if (tvisstr(o)) {
      s = strV(o);
    } else if (tvisnumber(o)) {
      s = lj_strfmt_number(L, o);
      setstrV(L, o, s);
    } else {
      utf8_argtype_named(L, narg, fname, "string");
      return NULL;  /* unreachable */
    }
    if (len)
      *len = s->len;
    return strdata(s);
  }
  utf8_argtype_named(L, narg, fname, "string");
  return NULL;  /* unreachable */
}

static lua_Integer utf8_checkinteger_named(lua_State *L, int narg,
					   const char *fname)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  lua_Number n;
  int64_t k;
  if (o >= L->top)
    utf8_argtype_named(L, narg, fname, "number");
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      utf8_argtype_named(L, narg, fname, "number");
    o = &tmp;
  }
  if (tvisint(o))
    return (lua_Integer)intV(o);
  if (!tvisnum(o))
    utf8_argtype_named(L, narg, fname, "number");
  n = numV(o);
  if (!(n >= (lua_Number)LUA_MININTEGER && n <= (lua_Number)LUA_MAXINTEGER))
    utf8_argerror_named(L, narg, fname,
			"number has no integer representation");
  k = lj_num2i64(n);
  if ((lua_Number)k != n)
    utf8_argerror_named(L, narg, fname,
			"number has no integer representation");
  return (lua_Integer)k;
}

static lua_Integer utf8_optinteger_named(lua_State *L, int narg,
					 lua_Integer def, const char *fname)
{
  return lua_isnoneornil(L, narg) ? def :
	 utf8_checkinteger_named(L, narg, fname);
}

static int utf8_iscont(unsigned char c)
{
  return (c & 0xc0) == 0x80;
}

static lua_Integer utf8_posrelat(lua_Integer pos, size_t len)
{
  return pos >= 0 ? pos : (lua_Integer)len + pos + 1;
}

static int utf8_decode(const unsigned char *s, size_t len, size_t pos,
		       uint32_t *cp, size_t *next, int strict)
{
  uint32_t c, res, min;
  size_t n, i;
  if (pos >= len)
    return 0;
  c = s[pos];
  if (c < 0x80) {
    *cp = c;
    *next = pos + 1;
    return 1;
  } else if (c >= 0xc2 && c <= 0xdf) {
    res = c & 0x1f;
    min = 0x80;
    n = 2;
  } else if (c >= 0xe0 && c <= 0xef) {
    res = c & 0x0f;
    min = 0x800;
    n = 3;
  } else if (c >= 0xf0 && (strict ? c <= 0xf4 : c <= 0xf7)) {
    res = c & 0x07;
    min = 0x10000;
    n = 4;
  } else if (!strict && c >= 0xf8 && c <= 0xfb) {
    res = c & 0x03;
    min = 0x200000;
    n = 5;
  } else if (!strict && c >= 0xfc && c <= 0xfd) {
    res = c & 0x01;
    min = 0x4000000;
    n = 6;
  } else {
    return 0;
  }
  if (pos + n > len)
    return 0;
  for (i = 1; i < n; i++) {
    c = s[pos+i];
    if (!utf8_iscont((unsigned char)c))
      return 0;
    res = (res << 6) | (c & 0x3f);
  }
  /* Strict mode follows valid Unicode; lax mode follows Lua 5.4's original
  ** UTF-8 range, which intentionally accepts extended code points.
  */
  if (res < min || (strict &&
      (res > 0x10ffff || (res >= 0xd800 && res <= 0xdfff))) ||
      (!strict && res > 0x7fffffff))
    return 0;
  *cp = res;
  *next = pos + n;
  return 1;
}

static void utf8_addchar(lua_State *L, luaL_Buffer *b, lua_Integer cp,
			 const char *fname)
{
  char buf[6];
  size_t n;
  if (cp < 0 || cp > 0x7fffffff)
    utf8_argerror_named(L, 1, fname, "value out of range");
  if (cp <= 0x7f) {
    buf[0] = (char)cp;
    n = 1;
  } else if (cp <= 0x7ff) {
    buf[0] = (char)(0xc0 | (cp >> 6));
    buf[1] = (char)(0x80 | (cp & 0x3f));
    n = 2;
  } else if (cp <= 0xffff) {
    buf[0] = (char)(0xe0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
    buf[2] = (char)(0x80 | (cp & 0x3f));
    n = 3;
  } else if (cp <= 0x1fffff) {
    buf[0] = (char)(0xf0 | (cp >> 18));
    buf[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
    buf[3] = (char)(0x80 | (cp & 0x3f));
    n = 4;
  } else if (cp <= 0x3ffffff) {
    /* Lua 5.4's utf8.char intentionally allows extended 5-byte forms. */
    buf[0] = (char)(0xf8 | (cp >> 24));
    buf[1] = (char)(0x80 | ((cp >> 18) & 0x3f));
    buf[2] = (char)(0x80 | ((cp >> 12) & 0x3f));
    buf[3] = (char)(0x80 | ((cp >> 6) & 0x3f));
    buf[4] = (char)(0x80 | (cp & 0x3f));
    n = 5;
  } else {
    /* 6-byte forms complete the range accepted by Lua 5.4: 0..0x7fffffff. */
    buf[0] = (char)(0xfc | (cp >> 30));
    buf[1] = (char)(0x80 | ((cp >> 24) & 0x3f));
    buf[2] = (char)(0x80 | ((cp >> 18) & 0x3f));
    buf[3] = (char)(0x80 | ((cp >> 12) & 0x3f));
    buf[4] = (char)(0x80 | ((cp >> 6) & 0x3f));
    buf[5] = (char)(0x80 | (cp & 0x3f));
    n = 6;
  }
  luaL_addlstring(b, buf, n);
}

static int utf8_char(lua_State *L)
{
  luaL_Buffer b;
  int i, n = lua_gettop(L);
  luaL_buffinit(L, &b);
  for (i = 1; i <= n; i++)
    utf8_addchar(L, &b, utf8_checkinteger_named(L, i, "utf8.char"),
		 "utf8.char");
  luaL_pushresult(&b);
  return 1;
}

static int utf8_codepoint(lua_State *L)
{
  size_t len, pos, end, next;
  const unsigned char *s = (const unsigned char *)
    utf8_checklstring_named(L, 1, &len, "utf8.codepoint");
  lua_Integer i = utf8_posrelat(utf8_optinteger_named(L, 2, 1,
						      "utf8.codepoint"), len);
  lua_Integer j = utf8_posrelat(utf8_optinteger_named(L, 3, i,
						      "utf8.codepoint"), len);
  int strict = !lua_toboolean(L, 4);
  int n = 0;
  uint32_t cp;
  if (i < 1 || j < 0 || i > (lua_Integer)len + 1)
    utf8_argerror_named(L, 2, "utf8.codepoint", "position out of range");
  if (j > (lua_Integer)len)
    utf8_argerror_named(L, 3, "utf8.codepoint", "position out of range");
  if (j < i)
    return 0;
  pos = (size_t)i - 1;
  end = (size_t)j;
  while (pos < end) {
    if (!utf8_decode(s, len, pos, &cp, &next, strict))
      luaL_error(L, "invalid UTF-8 code");
    lua_pushinteger(L, (lua_Integer)cp);
    n++;
    pos = next;
  }
  return n;
}

static int utf8_len(lua_State *L)
{
  size_t len, pos, end, next;
  const unsigned char *s = (const unsigned char *)
    utf8_checklstring_named(L, 1, &len, "utf8.len");
  lua_Integer i = utf8_posrelat(utf8_optinteger_named(L, 2, 1,
						      "utf8.len"), len);
  lua_Integer j = utf8_posrelat(utf8_optinteger_named(L, 3, -1,
						      "utf8.len"), len);
  lua_Integer n = 0;
  int strict = !lua_toboolean(L, 4);
  uint32_t cp;
  if (i < 1 || i > (lua_Integer)len + 1)
    utf8_argerror_named(L, 2, "utf8.len", "position out of range");
  if (j < 0 || j > (lua_Integer)len)
    utf8_argerror_named(L, 3, "utf8.len", "position out of range");
  if (j < i) {
    lua_pushinteger(L, 0);
    return 1;
  }
  pos = (size_t)i - 1;
  end = (size_t)j;
  while (pos < end) {
    if (!utf8_decode(s, len, pos, &cp, &next, strict)) {
      lua_pushnil(L);
      lua_pushinteger(L, (lua_Integer)pos + 1);
      return 2;
    }
    n++;
    pos = next;
  }
  lua_pushinteger(L, n);
  return 1;
}

static size_t utf8_charstart(const unsigned char *s, size_t len, size_t pos)
{
  if (pos >= len)
    pos = len;
  while (pos > 0 && utf8_iscont(s[pos]))
    pos--;
  return pos;
}

static int utf8_offset(lua_State *L)
{
  size_t len, pos, next;
  const unsigned char *s = (const unsigned char *)
    utf8_checklstring_named(L, 1, &len, "utf8.offset");
  lua_Integer n = utf8_checkinteger_named(L, 2, "utf8.offset");
  lua_Integer ipos = lua_isnoneornil(L, 3) ?
    (n >= 0 ? 1 : (lua_Integer)len + 1) :
    utf8_posrelat(utf8_checkinteger_named(L, 3, "utf8.offset"), len);
  uint32_t cp;
  if (ipos < 1 || ipos > (lua_Integer)len + 1)
    utf8_argerror_named(L, 3, "utf8.offset", "position out of range");
  pos = (size_t)ipos - 1;
  if (n == 0) {
    pos = utf8_charstart(s, len, pos);
    lua_pushinteger(L, (lua_Integer)pos + 1);
    return 1;
  }
  if (pos < len && utf8_iscont(s[pos]))
    return luaL_error(L, "initial position is a continuation byte");
  if (n > 0) {
    n--;
    while (n > 0) {
      if (pos >= len) {
	lua_pushnil(L);
	return 1;
      }
      if (!utf8_decode(s, len, pos, &cp, &next, 1))
	return luaL_error(L, "invalid UTF-8 code");
      pos = next;
      n--;
    }
    if (pos <= len)
      lua_pushinteger(L, (lua_Integer)pos + 1);
    else
      lua_pushnil(L);
  } else {
    while (n++ < 0) {
      if (pos == 0) {
	lua_pushnil(L);
	return 1;
      }
      pos--;
      while (pos > 0 && utf8_iscont(s[pos]))
	pos--;
    }
    lua_pushinteger(L, (lua_Integer)pos + 1);
  }
  return 1;
}

static int utf8_codes_iter(lua_State *L)
{
  size_t len, pos, next;
  const unsigned char *s = (const unsigned char *)
    utf8_checklstring_named(L, 1, &len, "utf8.codes");
  lua_Integer last = utf8_checkinteger_named(L, 2, "utf8.codes");
  uint32_t cp;
  if (last < 0 || last > (lua_Integer)len)
    return luaL_error(L, "invalid UTF-8 position");
  pos = (size_t)last;
  if (last > 0) {
    if (!utf8_decode(s, len, (size_t)last - 1, &cp, &pos,
		     !lua_toboolean(L, lua_upvalueindex(1))))
      return luaL_error(L, "invalid UTF-8 code");
  }
  if (pos >= len)
    return 0;
  if (!utf8_decode(s, len, pos, &cp, &next,
		   !lua_toboolean(L, lua_upvalueindex(1))))
    return luaL_error(L, "invalid UTF-8 code");
  lua_pushinteger(L, (lua_Integer)pos + 1);
  lua_pushinteger(L, (lua_Integer)cp);
  return 2;
}

static int utf8_codes(lua_State *L)
{
  utf8_checklstring_named(L, 1, NULL, "utf8.codes");
  lua_pushboolean(L, lua_toboolean(L, 2));
  lua_pushcclosure(L, utf8_codes_iter, 1);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, 0);
  return 3;
}

static const luaL_Reg utf8_lib[] = {
  { "char",		utf8_char },
  { "codepoint",	utf8_codepoint },
  { "codes",		utf8_codes },
  { "len",		utf8_len },
  { "offset",		utf8_offset },
  { NULL,		NULL }
};

LUALIB_API int luaopen_utf8(lua_State *L)
{
  static const char charpattern[] = "[\0-\x7f\xc2-\xfd][\x80-\xbf]*";
  luaL_register(L, LUA_UTF8LIBNAME, utf8_lib);
  lua_pushlstring(L, charpattern, sizeof(charpattern)-1);
  lua_setfield(L, -2, "charpattern");
  return 1;
}
