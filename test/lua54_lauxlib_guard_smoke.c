/*
** Include-guard smoke test for lauxlib.h in Lua 5.4 compatibility mode.
*/

#define lauxlib_h
#include "lauxlib.h"

#ifdef LUA_GNAME
#error "Lua 5.4 lauxlib_h include guard must skip lauxlib.h macros"
#endif

#ifdef LUA_FILEHANDLE
#error "Lua 5.4 lauxlib_h include guard must skip lauxlib.h declarations"
#endif

#ifdef LUA_LOADED_TABLE
#error "Lua 5.4 lauxlib_h include guard must skip lauxlib.h registry macros"
#endif

#ifdef LUA_PRELOAD_TABLE
#error "Lua 5.4 lauxlib_h include guard must skip lauxlib.h preload macros"
#endif

int main(void)
{
  return 0;
}
