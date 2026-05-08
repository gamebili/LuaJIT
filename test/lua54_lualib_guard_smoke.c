/*
** Include-guard smoke test for lualib.h in Lua 5.4 compatibility mode.
*/

#define lualib_h
#include "lualib.h"

#ifdef LUA_COLIBNAME
#error "Lua 5.4 lualib_h include guard must skip lualib.h declarations"
#endif

#ifdef LUA_VERSUFFIX
#error "Lua 5.4 lualib_h include guard must skip lualib.h macros"
#endif

int main(void)
{
  return 0;
}
