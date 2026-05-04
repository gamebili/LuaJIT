/*
** Negative compile smoke for Lua 5.4 external headers.
** luaL_findtable is a LuaJIT auxiliary helper, not part of Lua 5.4's
** public lauxlib.h surface.
*/

#include "lua.h"
#include "lauxlib.h"

int main(void)
{
  lua_State *L = NULL;
  const char *err = luaL_findtable(L, LUA_REGISTRYINDEX, "legacy", 1);
  return err != NULL;
}
