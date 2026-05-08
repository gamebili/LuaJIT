/*
** Header-surface smoke test for lualib.h alone in Lua 5.4 compatibility mode.
*/

#include "lualib.h"

#ifdef LUAJIT_EXTERNAL_LUA54
#error "LUAJIT_EXTERNAL_LUA54 is a private header selector and must not leak"
#endif

#ifndef lualib_h
#error "Lua 5.4 lualib.h must use/expose the official lualib_h include guard"
#endif

#ifndef LUA_VERSUFFIX
#error "Lua 5.4 lualib.h must expose LUA_VERSUFFIX"
#endif

#ifndef LUAMOD_API
#error "Lua 5.4 lualib.h must expose LUAMOD_API through lua.h/luaconf.h"
#endif

#ifndef LUA_COLIBNAME
#error "Lua 5.4 lualib.h must expose LUA_COLIBNAME"
#endif

#ifndef LUA_TABLIBNAME
#error "Lua 5.4 lualib.h must expose LUA_TABLIBNAME"
#endif

#ifndef LUA_IOLIBNAME
#error "Lua 5.4 lualib.h must expose LUA_IOLIBNAME"
#endif

#ifndef LUA_OSLIBNAME
#error "Lua 5.4 lualib.h must expose LUA_OSLIBNAME"
#endif

#ifndef LUA_STRLIBNAME
#error "Lua 5.4 lualib.h must expose LUA_STRLIBNAME"
#endif

#ifndef LUA_UTF8LIBNAME
#error "Lua 5.4 lualib.h must expose LUA_UTF8LIBNAME"
#endif

#ifndef LUA_MATHLIBNAME
#error "Lua 5.4 lualib.h must expose LUA_MATHLIBNAME"
#endif

#ifndef LUA_DBLIBNAME
#error "Lua 5.4 lualib.h must expose LUA_DBLIBNAME"
#endif

#ifndef LUA_LOADLIBNAME
#error "Lua 5.4 lualib.h must expose LUA_LOADLIBNAME"
#endif

#ifdef LUA_GNAME
#error "Lua 5.4 lualib.h must not expose lauxlib-only LUA_GNAME"
#endif

#ifdef LUA_FILEHANDLE
#error "Lua 5.4 lualib.h must not expose lauxlib-only LUA_FILEHANDLE"
#endif

#ifdef lua_assert
#error "Lua 5.4 lualib.h must not expose lauxlib-only lua_assert"
#endif

#ifdef LUA_BITLIBNAME
#error "Lua 5.4 lualib.h must not expose LuaJIT bit library name"
#endif

#ifdef LUA_JITLIBNAME
#error "Lua 5.4 lualib.h must not expose LuaJIT jit library name"
#endif

#ifdef LUA_FFILIBNAME
#error "Lua 5.4 lualib.h must not expose LuaJIT ffi library name"
#endif

typedef char lua54_colib_name_len[sizeof(LUA_COLIBNAME) == sizeof("coroutine") ? 1 : -1];
typedef char lua54_tablib_name_len[sizeof(LUA_TABLIBNAME) == sizeof("table") ? 1 : -1];
typedef char lua54_iolib_name_len[sizeof(LUA_IOLIBNAME) == sizeof("io") ? 1 : -1];
typedef char lua54_oslib_name_len[sizeof(LUA_OSLIBNAME) == sizeof("os") ? 1 : -1];
typedef char lua54_strlib_name_len[sizeof(LUA_STRLIBNAME) == sizeof("string") ? 1 : -1];
typedef char lua54_utf8lib_name_len[sizeof(LUA_UTF8LIBNAME) == sizeof("utf8") ? 1 : -1];
typedef char lua54_mathlib_name_len[sizeof(LUA_MATHLIBNAME) == sizeof("math") ? 1 : -1];
typedef char lua54_dblib_name_len[sizeof(LUA_DBLIBNAME) == sizeof("debug") ? 1 : -1];
typedef char lua54_loadlib_name_len[sizeof(LUA_LOADLIBNAME) == sizeof("package") ? 1 : -1];

#if defined(__GNUC__) || defined(__clang__)
typedef int (*lua54_openlib_sig)(lua_State *);
typedef void (*lua54_openlibs_sig)(lua_State *);

typedef char lua54_open_base_sig[
  __builtin_types_compatible_p(__typeof__(&luaopen_base), lua54_openlib_sig) ? 1 : -1
];
typedef char lua54_open_coroutine_sig[
  __builtin_types_compatible_p(__typeof__(&luaopen_coroutine), lua54_openlib_sig) ? 1 : -1
];
typedef char lua54_open_table_sig[
  __builtin_types_compatible_p(__typeof__(&luaopen_table), lua54_openlib_sig) ? 1 : -1
];
typedef char lua54_open_io_sig[
  __builtin_types_compatible_p(__typeof__(&luaopen_io), lua54_openlib_sig) ? 1 : -1
];
typedef char lua54_open_os_sig[
  __builtin_types_compatible_p(__typeof__(&luaopen_os), lua54_openlib_sig) ? 1 : -1
];
typedef char lua54_open_string_sig[
  __builtin_types_compatible_p(__typeof__(&luaopen_string), lua54_openlib_sig) ? 1 : -1
];
typedef char lua54_open_utf8_sig[
  __builtin_types_compatible_p(__typeof__(&luaopen_utf8), lua54_openlib_sig) ? 1 : -1
];
typedef char lua54_open_math_sig[
  __builtin_types_compatible_p(__typeof__(&luaopen_math), lua54_openlib_sig) ? 1 : -1
];
typedef char lua54_open_debug_sig[
  __builtin_types_compatible_p(__typeof__(&luaopen_debug), lua54_openlib_sig) ? 1 : -1
];
typedef char lua54_open_package_sig[
  __builtin_types_compatible_p(__typeof__(&luaopen_package), lua54_openlib_sig) ? 1 : -1
];
typedef char lua54_openlibs_sig_check[
  __builtin_types_compatible_p(__typeof__(&luaL_openlibs), lua54_openlibs_sig) ? 1 : -1
];
#endif

int main(void)
{
  return 0;
}
