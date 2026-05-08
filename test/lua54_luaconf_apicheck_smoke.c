#define LUA_USE_APICHECK
#include "lua.h"

#ifndef luai_apicheck
#error LUA_USE_APICHECK should expose luai_apicheck like official Lua 5.4
#endif

int main(void)
{
  lua_State *L = 0;
  luai_apicheck(L, 1);
  return 0;
}
