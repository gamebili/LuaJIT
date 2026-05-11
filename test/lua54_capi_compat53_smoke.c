/*
** Smoke tests for Lua 5.4 headers with the official LUA_COMPAT_5_3 switch.
*/

#include "lua.h"
#include "lauxlib.h"

#ifndef LUA_COMPAT_5_3
#error "this smoke must be compiled with LUA_COMPAT_5_3"
#endif

#ifndef LUA_COMPAT_MATHLIB
#error "LUA_COMPAT_5_3 must define LUA_COMPAT_MATHLIB"
#endif

#ifndef LUA_COMPAT_APIINTCASTS
#error "LUA_COMPAT_5_3 must define LUA_COMPAT_APIINTCASTS"
#endif

#ifndef LUA_COMPAT_LT_LE
#error "LUA_COMPAT_5_3 must define LUA_COMPAT_LT_LE"
#endif

#ifndef lua_strlen
#error "LUA_COMPAT_5_3 must expose lua_strlen"
#endif

#ifndef lua_objlen
#error "LUA_COMPAT_5_3 must expose lua_objlen"
#endif

#ifndef lua_equal
#error "LUA_COMPAT_5_3 must expose lua_equal"
#endif

#ifndef lua_lessthan
#error "LUA_COMPAT_5_3 must expose lua_lessthan"
#endif

#ifndef lua_pushunsigned
#error "LUA_COMPAT_5_3 must expose lua_pushunsigned"
#endif

#ifndef lua_tounsigned
#error "LUA_COMPAT_5_3 must expose lua_tounsigned"
#endif

#ifndef luaL_checkint
#error "LUA_COMPAT_5_3 must expose luaL_checkint"
#endif

#ifndef luaL_optlong
#error "LUA_COMPAT_5_3 must expose luaL_optlong"
#endif

static int fail(lua_State *L, const char *msg)
{
  lua_pushstring(L, msg);
  return lua_error(L);
}

int main(void)
{
  lua_State *L = luaL_newstate();
  if (L == NULL) return 1;

  lua_pushliteral(L, "abc");
  if (lua_strlen(L, -1) != 3) return fail(L, "lua_strlen");
  if (lua_objlen(L, -1) != 3) return fail(L, "lua_objlen");
  lua_pop(L, 1);

  lua_pushinteger(L, 1);
  lua_pushinteger(L, 1);
  if (!lua_equal(L, -1, -2)) return fail(L, "lua_equal");
  lua_pop(L, 2);

  lua_pushinteger(L, 1);
  lua_pushinteger(L, 2);
  if (!lua_lessthan(L, -2, -1)) return fail(L, "lua_lessthan");
  lua_pop(L, 2);

  lua_pushunsigned(L, (lua_Unsigned)42);
  if (lua_tounsigned(L, -1) != (lua_Unsigned)42)
    return fail(L, "lua_tounsigned");
  lua_pop(L, 1);

  lua_close(L);
  return 0;
}
