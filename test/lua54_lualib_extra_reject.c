/*
** LuaJIT-specific library open functions are not part of official Lua 5.4's
** lualib.h surface. The symbols may still exist internally/default, but the
** external 5.4 header must not declare them.
*/

#include "lualib.h"

int lua54_reject_lualib_extra(lua_State *L)
{
#if defined(LUA54_REJECT_BIT)
  return luaopen_bit(L);
#elif defined(LUA54_REJECT_JIT)
  return luaopen_jit(L);
#elif defined(LUA54_REJECT_FFI)
  return luaopen_ffi(L);
#elif defined(LUA54_REJECT_STRING_BUFFER)
  return luaopen_string_buffer(L);
#else
#error "select a LuaJIT lualib symbol to reject"
#endif
}
