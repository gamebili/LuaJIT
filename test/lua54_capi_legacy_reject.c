/*
** Negative compile smoke for Lua 5.4 external headers.
** These Lua 5.1-era lauxlib registration helpers are kept for default
** LuaJIT builds, but must not be declared by the external Lua 5.4 surface.
*/

#include "lua.h"
#include "lauxlib.h"

static const luaL_Reg legacy_funcs[] = {
  { NULL, NULL }
};

int main(void)
{
  lua_State *L = NULL;
  luaL_openlib(L, "legacy", legacy_funcs, 0);
  luaL_register(L, "legacy", legacy_funcs);
  luaL_pushmodule(L, "legacy", 0);
  return 0;
}
