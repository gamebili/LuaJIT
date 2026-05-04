/*
** Header-surface smoke test for lua.h alone in Lua 5.4 compatibility mode.
*/

#include "lua.h"

#if defined(__GNUC__) || defined(__clang__)
typedef void (*lua54_sethook_type)(lua_State *, lua_Hook, int, int);
typedef char lua54_sethook_must_return_void[
  __builtin_types_compatible_p(__typeof__(&lua_sethook), lua54_sethook_type) ? 1 : -1
];
typedef char lua54_pushglobaltable_must_return_void[
  __builtin_types_compatible_p(
    __typeof__(lua_pushglobaltable((lua_State *)0)), void) ? 1 : -1
];
#endif

#ifndef LUAI_IS32INT
#error "Lua 5.4 luaconf.h must expose LUAI_IS32INT"
#endif

typedef char lua54_luai_maxstack_formula[
  LUAI_MAXSTACK == (LUAI_IS32INT ? 1000000 : 15000) ? 1 : -1
];

typedef char lua54_registryindex_formula[
  LUA_REGISTRYINDEX == (-LUAI_MAXSTACK - 1000) ? 1 : -1
];

typedef char lua54_upvalueindex_formula[
  lua_upvalueindex(1) == (LUA_REGISTRYINDEX - 1) ? 1 : -1
];

#ifdef LUA_LOADED_TABLE
#error "Lua 5.4 lua.h must not expose lauxlib-only LUA_LOADED_TABLE"
#endif

#ifdef LUA_PRELOAD_TABLE
#error "Lua 5.4 lua.h must not expose lauxlib-only LUA_PRELOAD_TABLE"
#endif

#ifdef LUA_HOOKTAILRET
#error "Lua 5.4 lua.h must not expose LuaJIT/Lua 5.1 LUA_HOOKTAILRET"
#endif

#ifdef lua_open
#error "Lua 5.4 lua.h must not expose LuaJIT lua_open"
#endif

#ifdef lua_getregistry
#error "Lua 5.4 lua.h must not expose LuaJIT lua_getregistry"
#endif

#ifdef lua_getgccount
#error "Lua 5.4 lua.h must not expose LuaJIT lua_getgccount"
#endif

#ifdef lua_Chunkreader
#error "Lua 5.4 lua.h must not expose LuaJIT lua_Chunkreader"
#endif

#ifdef lua_Chunkwriter
#error "Lua 5.4 lua.h must not expose LuaJIT lua_Chunkwriter"
#endif

int main(void)
{
  return 0;
}
