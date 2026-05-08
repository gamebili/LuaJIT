/*
** Lua 5.4 C API smoke for lua_setwarnf(NULL, NULL).
**
** The public API replaces the warning function directly.  Passing NULL must
** disable subsequent lua_warning() calls, including control messages such as
** "@on"; otherwise external embedders cannot turn warnings fully off.
*/

#include "lua.h"
#include "lauxlib.h"

int main(void)
{
  lua_State *L = luaL_newstate();
  if (L == NULL) return 2;

  lua_setwarnf(L, NULL, NULL);
  lua_warning(L, "@on", 0);
  lua_warning(L, "must stay silent", 0);

  lua_close(L);
  return 0;
}
