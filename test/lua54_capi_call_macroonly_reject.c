/*
** Lua 5.4 exposes lua_call/lua_pcall/lua_yield as macros that route through
** the *k entry points with a NULL continuation. External modules must not be
** able to take the legacy LuaJIT function addresses from the 5.4 headers.
*/

#include "lua.h"

#if defined(LUA54_REJECT_CALL)
void (*lua54_reject_call)(lua_State *, int, int) = lua_call;
#elif defined(LUA54_REJECT_PCALL)
int (*lua54_reject_pcall)(lua_State *, int, int, int) = lua_pcall;
#elif defined(LUA54_REJECT_YIELD)
int (*lua54_reject_yield)(lua_State *, int) = lua_yield;
#else
#error "select a call macro-only symbol to reject"
#endif
