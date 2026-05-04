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

#ifdef LUA_LOADED_TABLE
#error "Lua 5.4 lua.h must not expose lauxlib-only LUA_LOADED_TABLE"
#endif

#ifdef LUA_PRELOAD_TABLE
#error "Lua 5.4 lua.h must not expose lauxlib-only LUA_PRELOAD_TABLE"
#endif

int main(void)
{
  return 0;
}
