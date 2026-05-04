/*
** Negative compile smoke for Lua 5.4 external headers.
** lua_setlevel is a LuaJIT compatibility hook, not part of Lua 5.4's
** public lua.h surface.
*/

#include "lua.h"

int main(void)
{
  lua_State *L = NULL;
  lua_setlevel(L, L);
  return 0;
}
