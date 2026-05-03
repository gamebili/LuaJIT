/*
** Header-surface smoke test for lua.h alone in Lua 5.4 compatibility mode.
*/

#include "lua.h"

#ifdef LUA_LOADED_TABLE
#error "Lua 5.4 lua.h must not expose lauxlib-only LUA_LOADED_TABLE"
#endif

#ifdef LUA_PRELOAD_TABLE
#error "Lua 5.4 lua.h must not expose lauxlib-only LUA_PRELOAD_TABLE"
#endif

int main(void)
{
  return 0;
}
