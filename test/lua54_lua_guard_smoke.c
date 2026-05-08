/*
** Include-guard smoke test for lua.h in Lua 5.4 compatibility mode.
*/

#define lua_h
#include "lua.h"

#ifdef LUA_VERSION
#error "Lua 5.4 lua_h include guard must skip lua.h version macros"
#endif

#ifdef LUA_SIGNATURE
#error "Lua 5.4 lua_h include guard must skip lua.h constants"
#endif

#ifdef LUA_REGISTRYINDEX
#error "Lua 5.4 lua_h include guard must skip lua.h pseudo-index macros"
#endif

#ifdef LUA_TNIL
#error "Lua 5.4 lua_h include guard must skip lua.h type tags"
#endif

int main(void)
{
  return 0;
}
