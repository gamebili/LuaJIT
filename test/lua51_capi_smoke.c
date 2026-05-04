/*
** Smoke tests for the default LuaJIT 5.1-compatible C API surface.
*/

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#ifdef LUAJIT_ENABLE_LUA54COMPAT
#error "default C API smoke must not be compiled with Lua 5.4 compatibility"
#endif

#if LUA_VERSION_NUM != 501
#error "default header must keep reporting Lua 5.1"
#endif

#ifndef LUA_GLOBALSINDEX
#error "default header must keep exposing LUA_GLOBALSINDEX"
#endif

#ifndef LUA_ENVIRONINDEX
#error "default header must keep exposing LUA_ENVIRONINDEX"
#endif

#ifndef lua_strlen
#error "default header must keep exposing lua_strlen"
#endif

static void check(lua_State *L, int cond, const char *msg)
{
  if (!cond) {
    fprintf(stderr, "%s\n", msg);
    luaL_error(L, "lua51 C API smoke failed: %s", msg);
  }
}

static int capi51_answer(lua_State *L)
{
  lua_pushinteger(L, 51);
  return 1;
}

static int capi51_typerror(lua_State *L)
{
  return luaL_typerror(L, 1, "number");
}

static const luaL_Reg capi51_reg[] = {
  { "answer", capi51_answer },
  { NULL, NULL }
};

int main(void)
{
  lua_State *L = luaL_newstate();
  check(L, L != NULL, "luaL_newstate");

  lua_pushliteral(L, "ok");
  lua_setglobal(L, "__lua51_capi_global");
  lua_getglobal(L, "__lua51_capi_global");
  check(L, lua_tostring(L, -1) != NULL, "lua_getglobal old global path");
  check(L, lua_strlen(L, -1) == 2, "lua_strlen macro");
  check(L, lua_objlen(L, -1) == 2, "lua_objlen default API");
  lua_pop(L, 1);

  lua_pushthread(L);
  lua_getfenv(L, -1);
  check(L, lua_istable(L, -1), "lua_getfenv default API");
  lua_pop(L, 2);

  luaL_register(L, "capi51", capi51_reg);
  lua_getfield(L, -1, "answer");
  lua_call(L, 0, 1);
  check(L, lua_tointeger(L, -1) == 51, "luaL_register default API");
  lua_pop(L, 2);

  lua_newtable(L);
  luaL_openlib(L, NULL, capi51_reg, 0);
  lua_getfield(L, -1, "answer");
  lua_call(L, 0, 1);
  check(L, lua_tointeger(L, -1) == 51, "luaL_openlib default API");
  lua_pop(L, 2);

  luaL_pushmodule(L, "capi51.push", 1);
  check(L, lua_istable(L, -1), "luaL_pushmodule default API");
  lua_pop(L, 1);

  check(L, luaL_findtable(L, LUA_REGISTRYINDEX, "__lua51.findtable", 1) == NULL,
	"luaL_findtable default API");
  check(L, lua_istable(L, -1), "luaL_findtable table");
  lua_pushliteral(L, "value");
  lua_setfield(L, -2, "key");
  lua_getfield(L, -1, "key");
  check(L, strcmp(lua_tostring(L, -1), "value") == 0,
	"luaL_findtable table value");
  lua_pop(L, 2);

  lua_pushcfunction(L, capi51_typerror);
  lua_pushliteral(L, "bad");
  check(L, lua_pcall(L, 1, 0, 0) == LUA_ERRRUN,
	"luaL_typerror default API status");
  check(L, strstr(lua_tostring(L, -1), "number expected") != NULL,
	"luaL_typerror default API message");
  lua_pop(L, 1);

  lua_close(L);
  return 0;
}
