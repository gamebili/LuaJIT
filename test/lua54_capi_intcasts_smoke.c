/*
** Smoke tests for Lua 5.4 LUA_COMPAT_APIINTCASTS compatibility macros.
*/

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#ifndef LUA_COMPAT_APIINTCASTS
#error "this smoke must be compiled with LUA_COMPAT_APIINTCASTS"
#endif

#ifndef lua_pushunsigned
#error "LUA_COMPAT_APIINTCASTS must expose lua_pushunsigned"
#endif

#ifndef lua_tounsignedx
#error "LUA_COMPAT_APIINTCASTS must expose lua_tounsignedx"
#endif

#ifndef lua_tounsigned
#error "LUA_COMPAT_APIINTCASTS must expose lua_tounsigned"
#endif

#ifndef luaL_checkunsigned
#error "LUA_COMPAT_APIINTCASTS must expose luaL_checkunsigned"
#endif

#ifndef luaL_optunsigned
#error "LUA_COMPAT_APIINTCASTS must expose luaL_optunsigned"
#endif

static void check(lua_State *L, int cond, const char *msg)
{
  if (!cond) {
    fprintf(stderr, "%s\n", msg);
    luaL_error(L, "lua54 C API intcasts smoke failed: %s", msg);
  }
}

static int checkunsigned_arg(lua_State *L)
{
  luaL_checkunsigned(L, 1);
  return 0;
}

static int optunsigned_arg(lua_State *L)
{
  luaL_optunsigned(L, 1, (lua_Unsigned)77u);
  return 0;
}

int main(void)
{
  lua_State *L = luaL_newstate();
  int ok = 0;
  int status;
  lua_Unsigned wide = (lua_Unsigned)0xffffffffu + (lua_Unsigned)1u;

  check(L, L != NULL, "luaL_newstate");

  lua_pushunsigned(L, (lua_Unsigned)123u);
  check(L, lua_tounsigned(L, -1) == (lua_Unsigned)123u, "lua_tounsigned");
  check(L, lua_tounsignedx(L, -1, &ok) == (lua_Unsigned)123u && ok,
	"lua_tounsignedx");
  lua_pop(L, 1);

  lua_pushinteger(L, (lua_Integer)-1);
  check(L, lua_tounsigned(L, -1) == LUA_MAXUNSIGNED,
	"lua_tounsigned negative wrap");
  check(L, lua_tounsignedx(L, -1, &ok) == LUA_MAXUNSIGNED && ok,
	"lua_tounsignedx negative wrap");
  check(L, luaL_checkunsigned(L, 1) == LUA_MAXUNSIGNED,
	"luaL_checkunsigned negative wrap");
  lua_pop(L, 1);

  lua_pushliteral(L, "-1");
  check(L, lua_tounsignedx(L, -1, &ok) == LUA_MAXUNSIGNED && ok,
	"lua_tounsignedx negative string wrap");
  check(L, luaL_checkunsigned(L, 1) == LUA_MAXUNSIGNED,
	"luaL_checkunsigned negative string wrap");
  lua_pop(L, 1);

  lua_pushinteger(L, 42);
  check(L, luaL_checkunsigned(L, 1) == (lua_Unsigned)42u,
	"luaL_checkunsigned");
  check(L, luaL_checkint(L, 1) == 42, "luaL_checkint");
  check(L, luaL_checklong(L, 1) == 42L, "luaL_checklong");
  lua_pop(L, 1);

  check(L, luaL_optunsigned(L, 1, (lua_Unsigned)77u) == (lua_Unsigned)77u,
	"luaL_optunsigned default");
  check(L, luaL_optint(L, 1, 78) == 78, "luaL_optint default");
  check(L, luaL_optlong(L, 1, 79L) == 79L, "luaL_optlong default");

  lua_pushnumber(L, (lua_Number)1.5);
  ok = 1;
  check(L, lua_tounsignedx(L, -1, &ok) == 0 && !ok,
	"lua_tounsignedx rejects fraction");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkunsigned_arg);
  lua_pushnumber(L, (lua_Number)1.5);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkunsigned rejects fraction");
  check(L, strstr(lua_tostring(L, -1),
		  "number has no integer representation") != NULL,
	"luaL_checkunsigned fraction error");
  lua_pop(L, 1);

  lua_pushcfunction(L, optunsigned_arg);
  lua_pushnumber(L, (lua_Number)1.5);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_optunsigned rejects fraction");
  check(L, strstr(lua_tostring(L, -1),
		  "number has no integer representation") != NULL,
	"luaL_optunsigned fraction error");
  lua_pop(L, 1);

  if (wide > (lua_Unsigned)0xffffffffu) {
    lua_pushunsigned(L, wide);
    check(L, lua_tounsigned(L, -1) == wide,
	  "lua_tounsigned 64-bit value");
    check(L, lua_tounsignedx(L, -1, &ok) == wide && ok,
	  "lua_tounsignedx 64-bit value");
    lua_pop(L, 1);

    lua_pushliteral(L, "4294967296");
    check(L, luaL_checkunsigned(L, 1) == wide,
	  "luaL_checkunsigned 64-bit string");
    lua_pop(L, 1);

    check(L, luaL_optunsigned(L, 1, wide) == wide,
	  "luaL_optunsigned 64-bit default");

    lua_pushunsigned(L, LUA_MAXUNSIGNED);
    check(L, lua_tounsigned(L, -1) == LUA_MAXUNSIGNED,
	  "lua_tounsigned max unsigned wrap");
    lua_pop(L, 1);
    check(L, luaL_optunsigned(L, 1, LUA_MAXUNSIGNED) == LUA_MAXUNSIGNED,
	  "luaL_optunsigned max unsigned default");
  }

  lua_close(L);
  return 0;
}
