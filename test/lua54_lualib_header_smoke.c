/*
** Header-surface smoke test for lualib.h alone in Lua 5.4 compatibility mode.
*/

#include "lualib.h"

#ifndef LUA_VERSUFFIX
#error "Lua 5.4 lualib.h must expose LUA_VERSUFFIX"
#endif

#ifdef LUA_GNAME
#error "Lua 5.4 lualib.h must not expose lauxlib-only LUA_GNAME"
#endif

#ifdef LUA_FILEHANDLE
#error "Lua 5.4 lualib.h must not expose lauxlib-only LUA_FILEHANDLE"
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
