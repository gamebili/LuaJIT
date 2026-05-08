/*
** Negative compile smoke for Lua 5.4 external headers.
** The deprecated int/long/unsigned conversion macros are official only when
** LUA_COMPAT_APIINTCASTS is requested. Default LuaJIT headers keep them, but
** the plain external Lua 5.4 surface must not expose them.
*/

#include "lua.h"
#include "lauxlib.h"

int main(void)
{
  lua_State *L = NULL;
  (void)luaL_checkint(L, 1);
  (void)luaL_optint(L, 1, 0);
  (void)luaL_checklong(L, 1);
  (void)luaL_optlong(L, 1, 0L);
  lua_pushunsigned(L, (lua_Unsigned)1);
  (void)lua_tounsignedx(L, 1, NULL);
  (void)lua_tounsigned(L, 1);
  (void)luaL_checkunsigned(L, 1);
  (void)luaL_optunsigned(L, 1, (lua_Unsigned)0);
  return 0;
}
