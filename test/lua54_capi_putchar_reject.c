/*
** luaL_putchar is a LuaJIT/Lua 5.1 compatibility macro, not part of the
** official Lua 5.4 lauxlib surface.
*/

#include "lua.h"
#include "lauxlib.h"

static void reject_putchar(luaL_Buffer *B)
{
  luaL_putchar(B, 'x');
}

int main(void)
{
  (void)reject_putchar;
  return 0;
}
