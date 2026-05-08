/*
** Header-surface smoke test for lua.hpp in Lua 5.4 compatibility mode.
*/

#include "lua.hpp"

#ifdef LUAJIT_VERSION
#error "Lua 5.4 lua.hpp must not expose luajit.h-only LUAJIT_VERSION"
#endif

#ifdef LUAJIT_EXTERNAL_LUA54
#error "LUAJIT_EXTERNAL_LUA54 is a private header selector and must not leak"
#endif

typedef char lua54_lu_hpp_version[LUA_VERSION_NUM == 504 ? 1 : -1];
typedef char lua54_lu_hpp_release[LUA_VERSION_RELEASE_NUM == 50408 ? 1 : -1];
typedef char lua54_lu_hpp_registry[
  LUA_REGISTRYINDEX == (-LUAI_MAXSTACK - 1000) ? 1 : -1
];

static int lua54_lu_hpp_openlibs_type(lua_State *L)
{
  luaL_openlibs(L);
  return luaopen_utf8(L);
}

int main()
{
  lua_State *L = 0;
  lua54_lu_hpp_openlibs_type(L);
  return 0;
}
