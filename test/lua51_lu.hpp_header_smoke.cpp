/*
** Header-surface smoke test for lua.hpp in default LuaJIT mode.
*/

#include "lua.hpp"

#ifndef LUAJIT_VERSION
#error "Default LuaJIT lua.hpp must continue to expose luajit.h"
#endif

typedef char lua51_lu_hpp_version[LUA_VERSION_NUM == 501 ? 1 : -1];

int main()
{
  return LUAJIT_VERSION_NUM > 0 ? 0 : 1;
}
