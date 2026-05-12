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

#ifndef lua_open
#error "default header must keep exposing lua_open"
#endif

#ifndef lua_getregistry
#error "default header must keep exposing lua_getregistry"
#endif

#ifndef lua_getgccount
#error "default header must keep exposing lua_getgccount"
#endif

#ifndef lua_Chunkreader
#error "default header must keep exposing lua_Chunkreader"
#endif

#ifndef lua_Chunkwriter
#error "default header must keep exposing lua_Chunkwriter"
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

static int capi51_cpcall(lua_State *L)
{
  lua_pushstring(L, (const char *)lua_touserdata(L, 1));
  lua_setglobal(L, "__lua51_cpcall");
  return 0;
}

typedef struct Capi51ReaderCtx {
  const char *chunk;
  int done;
} Capi51ReaderCtx;

static const char *capi51_reader(lua_State *L, void *data, size_t *size)
{
  Capi51ReaderCtx *ctx = (Capi51ReaderCtx *)data;
  (void)L;
  if (ctx->done) {
    *size = 0;
    return NULL;
  }
  ctx->done = 1;
  *size = strlen(ctx->chunk);
  return ctx->chunk;
}

static const luaL_Reg capi51_reg[] = {
  { "answer", capi51_answer },
  { NULL, NULL }
};

int main(void)
{
  lua_State *L = luaL_newstate();
  lua_State *L2;
  lua_Chunkreader chunkreader = NULL;
  lua_Chunkwriter chunkwriter = NULL;
  check(L, L != NULL, "luaL_newstate");
  check(L, chunkreader == NULL && chunkwriter == NULL, "lua_Chunk aliases");

  L2 = lua_open();
  check(L, L2 != NULL, "lua_open default macro");
  lua_close(L2);

  lua_pushliteral(L, "ok");
  lua_setglobal(L, "__lua51_capi_global");
  lua_getglobal(L, "__lua51_capi_global");
  check(L, lua_tostring(L, -1) != NULL, "lua_getglobal old global path");
  check(L, lua_strlen(L, -1) == 2, "lua_strlen macro");
  check(L, lua_objlen(L, -1) == 2, "lua_objlen default API");
  lua_pop(L, 1);

  lua_pushinteger(L, 1);
  lua_pushinteger(L, 1);
  check(L, lua_equal(L, -1, -2), "lua_equal default API");
  lua_pop(L, 2);

  lua_pushinteger(L, 1);
  lua_pushinteger(L, 2);
  check(L, lua_lessthan(L, -2, -1), "lua_lessthan default API");
  lua_pop(L, 2);

  lua_getregistry(L);
  check(L, lua_istable(L, -1), "lua_getregistry default macro");
  lua_pop(L, 1);
  check(L, lua_getgccount(L) >= 0, "lua_getgccount default macro");

  lua_pushthread(L);
  lua_getfenv(L, -1);
  check(L, lua_istable(L, -1), "lua_getfenv default API");
  lua_pop(L, 2);

  lua_pushthread(L);
  lua_newtable(L);
  lua_pushliteral(L, "threadenv");
  lua_setfield(L, -2, "marker");
  check(L, lua_setfenv(L, -2) == 1, "lua_setfenv default API");
  lua_getfenv(L, -1);
  lua_getfield(L, -1, "marker");
  check(L, strcmp(lua_tostring(L, -1), "threadenv") == 0,
	"lua_setfenv thread environment");
  lua_pop(L, 3);

  check(L, luaL_loadstring(L, "return marker") == LUA_OK,
	"luaL_loadstring default API status");
  lua_newtable(L);
  lua_pushliteral(L, "lua-func-env");
  lua_setfield(L, -2, "marker");
  check(L, lua_setfenv(L, -2) == 1, "lua_setfenv Lua function");
  lua_call(L, 0, 1);
  check(L, strcmp(lua_tostring(L, -1), "lua-func-env") == 0,
	"lua_getfenv Lua function environment");
  lua_pop(L, 1);

  {
    const char marker[] = "cpcall";
    check(L, lua_cpcall(L, capi51_cpcall, (void *)marker) == LUA_OK,
	  "lua_cpcall default API status");
    lua_getglobal(L, "__lua51_cpcall");
    check(L, strcmp(lua_tostring(L, -1), marker) == 0,
	  "lua_cpcall default API userdata");
    lua_pop(L, 1);
  }

  {
    Capi51ReaderCtx ctx = { "return 40 + 2", 0 };
    check(L, lua_loadx(L, capi51_reader, &ctx, "=lua51_loadx", "t") == LUA_OK,
	  "lua_loadx default API status");
    lua_call(L, 0, 1);
    check(L, lua_tointeger(L, -1) == 42, "lua_loadx default API result");
    lua_pop(L, 1);
  }

  lua_newtable(L);
  lua_pushliteral(L, "ref-value");
  {
    int ref = luaL_ref(L, -2);
    check(L, ref > 0, "luaL_ref default API ref");
    lua_rawgeti(L, -1, ref);
    check(L, strcmp(lua_tostring(L, -1), "ref-value") == 0,
	  "luaL_ref default API value");
    lua_pop(L, 1);
    luaL_unref(L, -1, ref);
    lua_rawgeti(L, -1, ref);
    check(L, lua_isnil(L, -1), "luaL_unref default API clears value");
    lua_pop(L, 1);
    lua_rawgeti(L, -1, 0);
    check(L, lua_tointeger(L, -1) == ref,
	  "luaL_unref default API freelist key");
    lua_pop(L, 1);
  }
  lua_pushnil(L);
  check(L, luaL_ref(L, -2) == LUA_REFNIL, "luaL_ref default API nil");
  luaL_unref(L, -1, LUA_NOREF);
  luaL_unref(L, -1, LUA_REFNIL);
  lua_pop(L, 1);

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

  {
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_putchar(&b, '5');
    luaL_putchar(&b, '1');
    luaL_pushresult(&b);
    check(L, strcmp(lua_tostring(L, -1), "51") == 0,
	  "luaL_putchar default compatibility macro");
    lua_pop(L, 1);
  }

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
