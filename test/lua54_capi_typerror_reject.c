/*
** Negative compile smoke for Lua 5.4 external headers.
** luaL_typerror is a LuaJIT/Lua 5.1 compatibility helper; Lua 5.4 exposes
** luaL_typeerror instead, so the external 5.4 header must not declare it.
*/

#include "lua.h"
#include "lauxlib.h"

int main(void)
{
  lua_State *L = NULL;
  return luaL_typerror(L, 1, "number");
}
