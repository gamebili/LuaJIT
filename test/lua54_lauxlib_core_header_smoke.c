/*
** Internal header-surface smoke test for lauxlib.h in Lua 5.4 compatibility
** mode. Core/library users must see the same luaL_intop width as public 5.4
** headers, even though they keep LuaJIT's non-external ABI selectors.
*/

#ifndef LUA_CORE
#define LUA_CORE
#endif

#include "lauxlib.h"

#ifdef LUAJIT_EXTERNAL_LUA54
#error "LUAJIT_EXTERNAL_LUA54 is a private header selector and must not leak"
#endif

#if defined(LUAJIT_ENABLE_LUA54COMPAT) && \
    (defined(_WIN64) || defined(__LP64__) || defined(__LLP64__) || \
     defined(__x86_64__) || defined(_M_X64) || \
     defined(__aarch64__) || defined(_M_ARM64))
typedef char lua54_lauxlib_core_intop_add64[
  luaL_intop(+, (lua_Integer)0x100000000LL, (lua_Integer)1) ==
  (lua_Integer)0x100000001LL ? 1 : -1
];
typedef char lua54_lauxlib_core_intop_wrap64[
  luaL_intop(+, LUA_MAXINTEGER, (lua_Integer)1) == LUA_MININTEGER ? 1 : -1
];
#endif

int main(void)
{
  return 0;
}
