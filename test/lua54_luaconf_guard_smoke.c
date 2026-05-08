/*
** Include-guard smoke test for luaconf.h in Lua 5.4 compatibility mode.
*/

#define luaconf_h
#include "luaconf.h"

#ifdef LUA_INT_TYPE
#error "Lua 5.4 luaconf_h include guard must skip numeric config macros"
#endif

#ifdef LUA_FLOAT_TYPE
#error "Lua 5.4 luaconf_h include guard must skip float config macros"
#endif

#ifdef LUA_PATH_SEP
#error "Lua 5.4 luaconf_h include guard must skip path config macros"
#endif

#ifdef LUA_USE_WINDOWS
#error "Lua 5.4 luaconf_h include guard must skip platform config macros"
#endif

#ifdef LUAI_MAXSTACK
#error "Lua 5.4 luaconf_h include guard must skip internal limit macros"
#endif

int main(void)
{
  return 0;
}
