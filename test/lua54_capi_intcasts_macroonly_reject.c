/*
** Negative compile smoke for Lua 5.4 LUA_COMPAT_APIINTCASTS headers.
** These deprecated compatibility entries are macros, so external modules can
** call them but must not be able to take old LuaJIT function addresses.
*/

#include "lua.h"
#include "lauxlib.h"

#ifndef LUA_COMPAT_APIINTCASTS
#error "this negative smoke must be compiled with LUA_COMPAT_APIINTCASTS"
#endif

#if defined(LUA54_REJECT_PUSHUNSIGNED)
void (*lua54_reject_pushunsigned)(lua_State *, lua_Unsigned) =
  lua_pushunsigned;
#elif defined(LUA54_REJECT_TOUNSIGNEDX)
lua_Unsigned (*lua54_reject_tounsignedx)(lua_State *, int, int *) =
  lua_tounsignedx;
#elif defined(LUA54_REJECT_TOUNSIGNED)
lua_Unsigned (*lua54_reject_tounsigned)(lua_State *, int) =
  lua_tounsigned;
#elif defined(LUA54_REJECT_CHECKUNSIGNED)
lua_Unsigned (*lua54_reject_checkunsigned)(lua_State *, int) =
  luaL_checkunsigned;
#elif defined(LUA54_REJECT_OPTUNSIGNED)
lua_Unsigned (*lua54_reject_optunsigned)(lua_State *, int, lua_Unsigned) =
  luaL_optunsigned;
#elif defined(LUA54_REJECT_CHECKINT)
int (*lua54_reject_checkint)(lua_State *, int) = luaL_checkint;
#elif defined(LUA54_REJECT_OPTINT)
int (*lua54_reject_optint)(lua_State *, int, int) = luaL_optint;
#elif defined(LUA54_REJECT_CHECKLONG)
long (*lua54_reject_checklong)(lua_State *, int) = luaL_checklong;
#elif defined(LUA54_REJECT_OPTLONG)
long (*lua54_reject_optlong)(lua_State *, int, long) = luaL_optlong;
#else
#error "select a LUA_COMPAT_APIINTCASTS macro-only symbol to reject"
#endif
