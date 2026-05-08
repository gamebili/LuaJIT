/*
** Negative compile smoke for Lua 5.4 external headers.
** lua_loadx is a LuaJIT/Lua 5.2 compatibility extension; official Lua 5.4
** exposes the mode argument through lua_load itself.
*/

#include "lua.h"

static const char *dummy_reader(lua_State *L, void *ud, size_t *sz)
{
  (void)L;
  (void)ud;
  *sz = 0;
  return NULL;
}

int main(void)
{
  lua_State *L = NULL;
  return lua_loadx(L, dummy_reader, NULL, "chunk", "t");
}
