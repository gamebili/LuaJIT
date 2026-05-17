/*
** Negative compile smoke for Lua 5.4 external headers.
** These Lua 5.1-era C API entry points are kept for default LuaJIT builds, but
** must not be declared by the external Lua 5.4 lua.h surface.
*/

#include "lua.h"

static int dummy_cfunc(lua_State *L)
{
  (void)L;
  return 0;
}

int main(void)
{
  lua_State *L = NULL;
  lua_State *T;
  lua_Chunkreader reader = NULL;
  lua_Chunkwriter writer = NULL;
  (void)lua_equal(L, 1, 2);
  (void)lua_lessthan(L, 1, 2);
  (void)lua_strlen(L, 1);
  (void)lua_objlen(L, 1);
  (void)lua_cpcall(L, dummy_cfunc, NULL);
  T = lua_open();
  (void)T;
  lua_getregistry(L);
  (void)lua_getgccount(L);
  lua_getfenv(L, 1);
  (void)lua_setfenv(L, 1);
  (void)reader;
  (void)writer;
  return 0;
}
