/*
** Smoke tests for the default LuaJIT 5.1-compatible C API surface.
*/

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#ifdef LUAJIT_ENABLE_LUA54COMPAT
#error "default C API smoke must not be compiled with Lua 5.4 compatibility"
#endif

#if LUA_VERSION_NUM != 501
#error "default header must keep reporting Lua 5.1"
#endif

#ifndef LUA_GLOBALSINDEX
#error "default header must keep exposing LUA_GLOBALSINDEX"
#endif

#ifndef LUA_ENVIRONINDEX
#error "default header must keep exposing LUA_ENVIRONINDEX"
#endif

#ifndef lua_strlen
#error "default header must keep exposing lua_strlen"
#endif

#ifndef lua_open
#error "default header must keep exposing lua_open"
#endif

#ifndef lua_getregistry
#error "default header must keep exposing lua_getregistry"
#endif

#ifndef lua_getgccount
#error "default header must keep exposing lua_getgccount"
#endif

#ifndef lua_Chunkreader
#error "default header must keep exposing lua_Chunkreader"
#endif

#ifndef lua_Chunkwriter
#error "default header must keep exposing lua_Chunkwriter"
#endif

#ifndef luaL_checkstring
#error "default lauxlib header must keep exposing luaL_checkstring"
#endif

#ifndef luaL_optstring
#error "default lauxlib header must keep exposing luaL_optstring"
#endif

#ifndef luaL_typename
#error "default lauxlib header must keep exposing luaL_typename"
#endif

#ifndef luaL_getmetatable
#error "default lauxlib header must keep exposing luaL_getmetatable"
#endif

#ifndef luaL_newlibtable
#error "default lauxlib header must keep exposing luaL_newlibtable"
#endif

#ifndef luaL_newlib
#error "default lauxlib header must keep exposing luaL_newlib"
#endif

static void check(lua_State *L, int cond, const char *msg)
{
  if (!cond) {
    fprintf(stderr, "%s\n", msg);
    luaL_error(L, "lua51 C API smoke failed: %s", msg);
  }
}

static int capi51_answer(lua_State *L)
{
  lua_pushinteger(L, 51);
  return 1;
}

static int capi51_yield_once(lua_State *L)
{
  return lua_yield(L, 0);
}

static int capi51_return_upvalue(lua_State *L)
{
  lua_pushvalue(L, lua_upvalueindex(1));
  return 1;
}

static int capi51_require_open_count;

static int capi51_require_open(lua_State *L)
{
  check(L, strcmp(lua_tostring(L, 1), "capi51.req") == 0,
	"luaL_requiref passes module name");
  capi51_require_open_count++;
  lua_newtable(L);
  lua_pushliteral(L, "ready");
  lua_setfield(L, -2, "status");
  return 1;
}

static int capi51_typerror(lua_State *L)
{
  return luaL_typerror(L, 1, "number");
}

static int capi51_typerror_null_name(lua_State *L)
{
  lua_pushnil(L);
  return luaL_typerror(L, 1, NULL);
}

static int capi51_typeerror_null_name(lua_State *L)
{
  lua_pushnil(L);
  return luaL_typeerror(L, 1, NULL);
}

static int capi51_argerror_null_message(lua_State *L)
{
  return luaL_argerror(L, 1, NULL);
}

static int capi51_laux_error_null_format(lua_State *L)
{
  return luaL_error(L, NULL);
}

static int capi51_cpcall(lua_State *L)
{
  lua_pushstring(L, (const char *)lua_touserdata(L, 1));
  lua_setglobal(L, "__lua51_cpcall");
  return 0;
}

static int capi51_cpcall_null_function(lua_State *L)
{
  lua_cpcall(L, NULL, NULL);
  return 0;
}

static int capi51_createtable_large_array_hint(lua_State *L)
{
  lua_createtable(L, INT_MAX, 0);
  return 1;
}

static int capi51_getinfo_null_what(lua_State *L)
{
  lua_Debug ar;
  lua_getinfo(L, NULL, &ar);
  return 0;
}

static int capi51_getinfo_null_debug(lua_State *L)
{
  lua_getinfo(L, "S", NULL);
  return 0;
}

static int capi51_getstack_null_debug(lua_State *L)
{
  lua_getstack(L, 0, NULL);
  return 0;
}

static int capi51_setlocal_null_debug(lua_State *L)
{
  lua_pushnil(L);
  lua_setlocal(L, NULL, 1);
  return 0;
}

static int capi51_setlocal_missing_name_preserves_stack(lua_State *L)
{
  lua_Debug ar;
  int top;
  check(L, lua_getstack(L, 0, &ar), "lua_setlocal missing-name setup");
  lua_pushliteral(L, "sentinel");
  top = lua_gettop(L);
  check(L, lua_setlocal(L, &ar, 9999) == NULL,
	"lua_setlocal missing name returns NULL");
  check(L, lua_gettop(L) == top, "lua_setlocal missing name preserves stack");
  lua_pop(L, 1);
  return 0;
}

static int capi51_checkoption_null_list(lua_State *L)
{
  lua_pushliteral(L, "alpha");
  luaL_checkoption(L, 1, NULL, NULL);
  return 0;
}

static int capi51_traceback_null_thread(lua_State *L)
{
  luaL_traceback(L, NULL, "trace", 0);
  return 0;
}

static int capi51_checkstack_negative(lua_State *L)
{
  luaL_checkstack(L, -1, "negative stack size");
  return 0;
}

static int capi51_dump_writer(lua_State *L, const void *p, size_t sz, void *ud)
{
  (void)L; (void)p; (void)sz; (void)ud;
  return 0;
}

static int capi51_dump_empty_stack(lua_State *L)
{
  lua_dump(L, capi51_dump_writer, NULL);
  return 0;
}

static int capi51_dump_null_writer(lua_State *L)
{
  check(L, luaL_loadstring(L, "return 51") == LUA_OK,
	"lua_dump NULL writer setup");
  lua_dump(L, NULL, NULL);
  return 0;
}

static int capi51_load_null_reader(lua_State *L)
{
  lua_load(L, NULL, NULL, "=null-reader");
  return 0;
}

static int capi51_concat_negative_count(lua_State *L)
{
  lua_concat(L, -1);
  return 0;
}

static int capi51_rotate_large_count(lua_State *L)
{
  lua_pushnil(L);
  lua_pushnil(L);
  lua_rotate(L, -2, 3);
  return 0;
}

static int capi51_rotate_min_count(lua_State *L)
{
  lua_pushnil(L);
  lua_rotate(L, -1, INT_MIN);
  return 0;
}

static int capi51_typename_too_low(lua_State *L)
{
  (void)lua_typename(L, LUA_TNONE - 1);
  return 0;
}

static int capi51_typename_too_high(lua_State *L)
{
  (void)lua_typename(L, LUA_NUMTYPES + 3);
  return 0;
}

static int capi51_absindex_zero_index(lua_State *L)
{
  (void)lua_absindex(L, 0);
  return 0;
}

static int capi51_absindex_too_negative(lua_State *L)
{
  (void)lua_absindex(L, -1);
  return 0;
}

static int capi51_type_zero_index(lua_State *L)
{
  (void)lua_type(L, 0);
  return 0;
}

static int capi51_type_too_negative(lua_State *L)
{
  (void)lua_type(L, -1);
  return 0;
}

static int capi51_objlen_zero_index(lua_State *L)
{
  (void)lua_objlen(L, 0);
  return 0;
}

static int capi51_objlen_too_negative(lua_State *L)
{
  (void)lua_objlen(L, -1);
  return 0;
}

static int capi51_topointer_zero_index(lua_State *L)
{
  (void)lua_topointer(L, 0);
  return 0;
}

static int capi51_rawequal_too_negative(lua_State *L)
{
  (void)lua_rawequal(L, -1, 1);
  return 0;
}

static int capi51_error_missing_object(lua_State *L)
{
  return lua_error(L);
}

static int capi51_pcall_nonfunction_errfunc(lua_State *L)
{
  lua_pushboolean(L, 1);
  lua_pushcfunction(L, capi51_answer);
  lua_pcall(L, 0, 0, 1);
  return 0;
}

static int capi51_checktype_invalid_expected_low(lua_State *L)
{
  lua_pushnil(L);
  luaL_checktype(L, 1, LUA_TNONE - 1);
  return 0;
}

static int capi51_checktype_invalid_expected_high(lua_State *L)
{
  lua_pushnil(L);
  luaL_checktype(L, 1, LUA_NUMTYPES + 3);
  return 0;
}

static int capi51_ref_missing_value(lua_State *L)
{
  (void)luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

static int capi51_pushcclosure_null_function(lua_State *L)
{
  lua_pushcclosure(L, NULL, 0);
  return 0;
}

static int capi51_pushlstring_null_nonzero(lua_State *L)
{
  lua_pushlstring(L, NULL, 1);
  return 0;
}

static int capi51_pushfstring_null_format(lua_State *L)
{
  lua_pushfstring(L, NULL);
  return 0;
}

static const char *capi51_pushvfstring_wrap(lua_State *L, const char *fmt, ...)
{
  const char *ret;
  va_list argp;
  va_start(argp, fmt);
  ret = lua_pushvfstring(L, fmt, argp);
  va_end(argp);
  return ret;
}

static int capi51_pushvfstring_null_format(lua_State *L)
{
  (void)capi51_pushvfstring_wrap(L, NULL);
  return 0;
}

static int capi51_setallocf_null_function(lua_State *L)
{
  lua_setallocf(L, NULL, NULL);
  return 0;
}

static int capi51_stringtonumber_null_string(lua_State *L)
{
  lua_stringtonumber(L, NULL);
  return 0;
}

static int capi51_loadbuffer_null_nonzero(lua_State *L)
{
  luaL_loadbuffer(L, NULL, 1, "=null-buffer");
  return 0;
}

static int capi51_loadstring_null_string(lua_State *L)
{
  luaL_loadstring(L, NULL);
  return 0;
}

static int capi51_addlstring_null_nonzero(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addlstring(&b, NULL, 1);
  return 0;
}

static int capi51_addstring_null_string(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addstring(&b, NULL);
  return 0;
}

static int capi51_addvalue_missing_value(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addvalue(&b);
  return 0;
}

static int capi51_addvalue_bad_value(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  lua_pushboolean(L, 1);
  luaL_addvalue(&b);
  return 0;
}

static int capi51_addgsub_null_subject(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addgsub(&b, NULL, "x", "y");
  return 0;
}

static int capi51_addgsub_null_pattern(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addgsub(&b, "x", NULL, "y");
  return 0;
}

static int capi51_addgsub_null_replacement(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addgsub(&b, "x", "x", NULL);
  return 0;
}

static int capi51_getfield_null_name(lua_State *L)
{
  lua_newtable(L);
  lua_getfield(L, -1, NULL);
  return 0;
}

static int capi51_geti_invalid_index(lua_State *L)
{
  lua_geti(L, 1, 1);
  return 0;
}

static int capi51_setfield_null_name(lua_State *L)
{
  lua_newtable(L);
  lua_pushnil(L);
  lua_setfield(L, -2, NULL);
  return 0;
}

static int capi51_seti_invalid_index(lua_State *L)
{
  lua_pushliteral(L, "value");
  lua_seti(L, 2, 1);
  return 0;
}

static int capi51_rawgeti_invalid_index(lua_State *L)
{
  lua_rawgeti(L, 1, 1);
  return 0;
}

static int capi51_rawgetp_invalid_index(lua_State *L)
{
  lua_rawgetp(L, 1, (const void *)capi51_rawgetp_invalid_index);
  return 0;
}

static int capi51_rawseti_invalid_index(lua_State *L)
{
  lua_pushliteral(L, "value");
  lua_rawseti(L, 2, 1);
  return 0;
}

static int capi51_rawsetp_invalid_index(lua_State *L)
{
  lua_pushliteral(L, "value");
  lua_rawsetp(L, 2, (const void *)capi51_rawsetp_invalid_index);
  return 0;
}

static int capi51_getglobal_null_name(lua_State *L)
{
  lua_getglobal(L, NULL);
  return 0;
}

static int capi51_setglobal_null_name(lua_State *L)
{
  lua_pushnil(L);
  lua_setglobal(L, NULL);
  return 0;
}

static int capi51_newmetatable_null_name(lua_State *L)
{
  luaL_newmetatable(L, NULL);
  return 0;
}

static int capi51_getmetafield_null_name(lua_State *L)
{
  lua_newtable(L);
  luaL_getmetafield(L, -1, NULL);
  return 0;
}

static int capi51_callmeta_invalid_index(lua_State *L)
{
  luaL_callmeta(L, 1, "__tostring");
  return 0;
}

static int capi51_checkudata_null_name(lua_State *L)
{
  lua_newuserdata(L, 1);
  luaL_checkudata(L, -1, NULL);
  return 0;
}

static int capi51_checkudata_invalid_index(lua_State *L)
{
  luaL_checkudata(L, 1, "capi51.ud");
  return 0;
}

static int capi51_findtable_null_name(lua_State *L)
{
  luaL_findtable(L, LUA_REGISTRYINDEX, NULL, 1);
  return 0;
}

static int capi51_pushmodule_null_name(lua_State *L)
{
  luaL_pushmodule(L, NULL, 1);
  return 0;
}

static int capi51_getsubtable_null_name(lua_State *L)
{
  lua_newtable(L);
  luaL_getsubtable(L, -1, NULL);
  return 0;
}

static int capi51_requiref_null_name(lua_State *L)
{
  luaL_requiref(L, NULL, capi51_answer, 0);
  return 0;
}

static int capi51_requiref_null_openf(lua_State *L)
{
  luaL_requiref(L, "capi51.nullopen", NULL, 0);
  return 0;
}

static int capi51_setmetatable_null_name(lua_State *L)
{
  lua_newtable(L);
  luaL_setmetatable(L, NULL);
  return 0;
}

static int capi51_getmetatable_invalid_index(lua_State *L)
{
  (void)lua_getmetatable(L, 1);
  return 0;
}

static int capi51_setfenv_missing_value(lua_State *L)
{
  lua_setfenv(L, LUA_REGISTRYINDEX);
  return 0;
}

static int capi51_setfenv_nontable_value(lua_State *L)
{
  lua_pushthread(L);
  lua_pushboolean(L, 1);
  lua_setfenv(L, -2);
  return 0;
}

static int capi51_getfenv_invalid_index(lua_State *L)
{
  lua_getfenv(L, 1);
  return 0;
}

static int capi51_setfenv_invalid_index(lua_State *L)
{
  lua_newtable(L);
  lua_setfenv(L, -2);
  return 0;
}

static int capi51_replace_globals_nontable(lua_State *L)
{
  lua_pushboolean(L, 1);
  lua_replace(L, LUA_GLOBALSINDEX);
  return 0;
}

static int capi51_replace_env_nontable(lua_State *L)
{
  lua_pushboolean(L, 1);
  lua_replace(L, LUA_ENVIRONINDEX);
  return 0;
}

typedef struct Capi51ReaderCtx {
  const char *chunk;
  int done;
} Capi51ReaderCtx;

static const char *capi51_reader(lua_State *L, void *data, size_t *size)
{
  Capi51ReaderCtx *ctx = (Capi51ReaderCtx *)data;
  (void)L;
  if (ctx->done) {
    *size = 0;
    return NULL;
  }
  ctx->done = 1;
  *size = strlen(ctx->chunk);
  return ctx->chunk;
}

static const luaL_Reg capi51_reg[] = {
  { "answer", capi51_answer },
  { NULL, NULL }
};

static const luaL_Reg capi51_upvalue_reg[] = {
  { "upvalue", capi51_return_upvalue },
  { NULL, NULL }
};

static int capi51_setfuncs_null_list(lua_State *L)
{
  lua_newtable(L);
  luaL_setfuncs(L, NULL, 0);
  return 0;
}

static int capi51_setfuncs_negative_upvalues(lua_State *L)
{
  lua_newtable(L);
  luaL_setfuncs(L, capi51_reg, -1);
  return 0;
}

static int capi51_setfuncs_missing_target(lua_State *L)
{
  luaL_setfuncs(L, capi51_reg, 0);
  return 0;
}

static int capi51_setfuncs_missing_upvalue(lua_State *L)
{
  lua_newtable(L);
  luaL_setfuncs(L, capi51_reg, 1);
  return 0;
}

static int capi51_openlib_negative_upvalues(lua_State *L)
{
  luaL_openlib(L, NULL, capi51_reg, -1);
  return 0;
}

static int capi51_openlib_missing_upvalue(lua_State *L)
{
  luaL_openlib(L, "capi51.partialopen", capi51_reg, 1);
  return 0;
}

static int capi51_getupvalue_invalid_index(lua_State *L)
{
  (void)lua_getupvalue(L, 1, 1);
  return 0;
}

static int capi51_setupvalue_invalid_index(lua_State *L)
{
  lua_pushnil(L);
  (void)lua_setupvalue(L, -2, 1);
  return 0;
}

static int capi51_hook_pseudoindex;

static void capi51_invalid_pseudoindex_hook(lua_State *L, lua_Debug *ar)
{
  int idx;
  (void)ar;
  lua_sethook(L, NULL, 0, 0);
  idx = capi51_hook_pseudoindex ? LUA_ENVIRONINDEX : lua_upvalueindex(1);
  (void)lua_type(L, idx);
}

static int capi51_run_pseudoindex_hook(lua_State *L, int use_env_index)
{
  check(L, luaL_loadstring(L, "local x = 1\nx = x + 1\n") == LUA_OK,
	"lua51 pseudo-index hook load");
  capi51_hook_pseudoindex = use_env_index;
  lua_sethook(L, capi51_invalid_pseudoindex_hook, LUA_MASKLINE, 0);
  lua_call(L, 0, 0);
  lua_sethook(L, NULL, 0, 0);
  return 0;
}

static int capi51_upvalueindex_from_lua_hook(lua_State *L)
{
  return capi51_run_pseudoindex_hook(L, 0);
}

static int capi51_environindex_from_lua_hook(lua_State *L)
{
  return capi51_run_pseudoindex_hook(L, 1);
}

static void capi51_check_invalid_value(lua_State *L, lua_CFunction fn,
				       const char *statusmsg,
				       const char *errmsg)
{
  int status;
  lua_pushcfunction(L, fn);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, statusmsg);
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL, errmsg);
  lua_pop(L, 1);
}

static void capi51_check_resume_invalid_result(lua_State *L, lua_State *co,
					       int status,
					       const char *statusmsg,
					       const char *errmsg)
{
  check(L, status == LUA_ERRRUN, statusmsg);
  check(L, lua_gettop(co) == 1, statusmsg);
  check(L, strstr(lua_tostring(co, -1), "invalid value") != NULL, errmsg);
}

static void capi51_check_yielded_thread_call_state(lua_State *L)
{
  lua_State *co = lua_newthread(L);
  int status;
  lua_pushcfunction(co, capi51_yield_once);
  check(L, lua_resume(co, 0) == LUA_YIELD, "lua_pcall yield setup");
  check(L, lua_gettop(co) == 0, "lua_pcall yield setup stack");
  lua_pushcfunction(co, capi51_answer);
  status = lua_pcall(co, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pcall rejects yielded thread state");
  check(L, lua_gettop(co) == 1 &&
	   strstr(lua_tostring(co, -1), "invalid value") != NULL,
	"lua_pcall yielded thread state error");
  lua_settop(co, 0);
  status = lua_cpcall(co, capi51_answer, NULL);
  check(L, status == LUA_ERRRUN, "lua_cpcall rejects yielded thread state");
  check(L, lua_gettop(co) == 1 &&
	   strstr(lua_tostring(co, -1), "invalid value") != NULL,
	"lua_cpcall yielded thread state error");
  lua_pop(L, 1);
}

int main(void)
{
  lua_State *L = luaL_newstate();
  lua_State *L2;
  lua_Chunkreader chunkreader = NULL;
  lua_Chunkwriter chunkwriter = NULL;
  int status;
  check(L, L != NULL, "luaL_newstate");
  check(L, chunkreader == NULL && chunkwriter == NULL, "lua_Chunk aliases");
  check(L, lua_newstate(NULL, NULL) == NULL,
	"lua_newstate rejects NULL allocator");

  lua_pushcfunction(L, capi51_setallocf_null_function);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_setallocf rejects NULL allocator");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_setallocf NULL allocator error");
  lua_pop(L, 1);

  L2 = lua_open();
  check(L, L2 != NULL, "lua_open default macro");
  lua_close(L2);

  lua_pushliteral(L, "ok");
  lua_setglobal(L, "__lua51_capi_global");
  lua_getglobal(L, "__lua51_capi_global");
  check(L, lua_tostring(L, -1) != NULL, "lua_getglobal old global path");
  check(L, lua_strlen(L, -1) == 2, "lua_strlen macro");
  check(L, lua_objlen(L, -1) == 2, "lua_objlen default API");
  lua_pop(L, 1);
  lua_pushlstring(L, NULL, 0);
  check(L, lua_objlen(L, -1) == 0, "lua_pushlstring NULL zero length");
  lua_pop(L, 1);
  lua_pushcfunction(L, capi51_createtable_large_array_hint);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN,
	"lua_createtable rejects large array hint");
  check(L, strstr(lua_tostring(L, -1), "table overflow") != NULL,
	"lua_createtable large array hint error");
  lua_pop(L, 1);
  status = luaL_loadbuffer(L, NULL, 0, "=empty-null-buffer");
  check(L, status == LUA_OK, "luaL_loadbuffer accepts NULL zero length");
  lua_pop(L, 1);
  {
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addlstring(&b, NULL, 0);
    luaL_pushresult(&b);
    check(L, lua_objlen(L, -1) == 0,
	  "luaL_addlstring accepts NULL zero length");
    lua_pop(L, 1);
  }

  capi51_check_invalid_value(L, capi51_getfield_null_name,
			     "lua_getfield rejects NULL name",
			     "lua_getfield NULL name error");
  capi51_check_invalid_value(L, capi51_setfield_null_name,
			     "lua_setfield rejects NULL name",
			     "lua_setfield NULL name error");
  capi51_check_invalid_value(L, capi51_geti_invalid_index,
			     "lua_geti rejects invalid table index",
			     "lua_geti invalid table index error");
  capi51_check_invalid_value(L, capi51_seti_invalid_index,
			     "lua_seti rejects invalid table index",
			     "lua_seti invalid table index error");
  capi51_check_invalid_value(L, capi51_rawgeti_invalid_index,
			     "lua_rawgeti rejects invalid table index",
			     "lua_rawgeti invalid table index error");
  capi51_check_invalid_value(L, capi51_rawgetp_invalid_index,
			     "lua_rawgetp rejects invalid table index",
			     "lua_rawgetp invalid table index error");
  capi51_check_invalid_value(L, capi51_rawseti_invalid_index,
			     "lua_rawseti rejects invalid table index",
			     "lua_rawseti invalid table index error");
  capi51_check_invalid_value(L, capi51_rawsetp_invalid_index,
			     "lua_rawsetp rejects invalid table index",
			     "lua_rawsetp invalid table index error");
  capi51_check_invalid_value(L, capi51_getglobal_null_name,
			     "lua_getglobal rejects NULL name",
			     "lua_getglobal NULL name error");
  capi51_check_invalid_value(L, capi51_setglobal_null_name,
			     "lua_setglobal rejects NULL name",
			     "lua_setglobal NULL name error");
  capi51_check_invalid_value(L, capi51_stringtonumber_null_string,
			     "lua_stringtonumber rejects NULL string",
			     "lua_stringtonumber NULL string error");
  capi51_check_invalid_value(L, capi51_loadbuffer_null_nonzero,
			     "luaL_loadbuffer rejects NULL nonzero buffer",
			     "luaL_loadbuffer NULL nonzero buffer error");
  capi51_check_invalid_value(L, capi51_loadstring_null_string,
			     "luaL_loadstring rejects NULL string",
			     "luaL_loadstring NULL string error");
  capi51_check_invalid_value(L, capi51_addlstring_null_nonzero,
			     "luaL_addlstring rejects NULL nonzero string",
			     "luaL_addlstring NULL nonzero string error");
  capi51_check_invalid_value(L, capi51_addstring_null_string,
			     "luaL_addstring rejects NULL string",
			     "luaL_addstring NULL string error");
  capi51_check_invalid_value(L, capi51_addvalue_missing_value,
			     "luaL_addvalue rejects missing value",
			     "luaL_addvalue missing value error");
  capi51_check_invalid_value(L, capi51_addvalue_bad_value,
			     "luaL_addvalue rejects non-string value",
			     "luaL_addvalue non-string value error");
  capi51_check_invalid_value(L, capi51_addgsub_null_subject,
			     "luaL_addgsub rejects NULL subject",
			     "luaL_addgsub NULL subject error");
  capi51_check_invalid_value(L, capi51_addgsub_null_pattern,
			     "luaL_addgsub rejects NULL pattern",
			     "luaL_addgsub NULL pattern error");
  capi51_check_invalid_value(L, capi51_addgsub_null_replacement,
			     "luaL_addgsub rejects NULL replacement",
			     "luaL_addgsub NULL replacement error");
  capi51_check_invalid_value(L, capi51_newmetatable_null_name,
			     "luaL_newmetatable rejects NULL name",
			     "luaL_newmetatable NULL name error");
  capi51_check_invalid_value(L, capi51_getmetafield_null_name,
			     "luaL_getmetafield rejects NULL name",
			     "luaL_getmetafield NULL name error");
  capi51_check_invalid_value(L, capi51_callmeta_invalid_index,
			     "luaL_callmeta rejects invalid object index",
			     "luaL_callmeta invalid object index error");
  capi51_check_invalid_value(L, capi51_checkudata_null_name,
			     "luaL_checkudata rejects NULL name",
			     "luaL_checkudata NULL name error");
  capi51_check_invalid_value(L, capi51_checkudata_invalid_index,
			     "luaL_checkudata rejects invalid object index",
			     "luaL_checkudata invalid object index error");
  capi51_check_invalid_value(L, capi51_findtable_null_name,
			     "luaL_findtable rejects NULL name",
			     "luaL_findtable NULL name error");
  capi51_check_invalid_value(L, capi51_pushmodule_null_name,
			     "luaL_pushmodule rejects NULL name",
			     "luaL_pushmodule NULL name error");
  capi51_check_invalid_value(L, capi51_getsubtable_null_name,
			     "luaL_getsubtable rejects NULL name",
			     "luaL_getsubtable NULL name error");
  capi51_check_invalid_value(L, capi51_requiref_null_name,
			     "luaL_requiref rejects NULL module name",
			     "luaL_requiref NULL module name error");
  capi51_check_invalid_value(L, capi51_requiref_null_openf,
			     "luaL_requiref rejects NULL opener",
			     "luaL_requiref NULL opener error");
  capi51_check_invalid_value(L, capi51_setmetatable_null_name,
			     "luaL_setmetatable rejects NULL name",
			     "luaL_setmetatable NULL name error");
  capi51_check_invalid_value(L, capi51_getmetatable_invalid_index,
			     "lua_getmetatable rejects invalid object index",
			     "lua_getmetatable invalid object index error");
  capi51_check_invalid_value(L, capi51_setfuncs_null_list,
			     "luaL_setfuncs rejects NULL list",
			     "luaL_setfuncs NULL list error");
  capi51_check_invalid_value(L, capi51_typerror_null_name,
			     "luaL_typerror rejects NULL expected name",
			     "luaL_typerror NULL expected name error");
  capi51_check_invalid_value(L, capi51_typeerror_null_name,
			     "luaL_typeerror rejects NULL expected name",
			     "luaL_typeerror NULL expected name error");
  capi51_check_invalid_value(L, capi51_argerror_null_message,
			     "luaL_argerror rejects NULL message",
			     "luaL_argerror NULL message error");
  capi51_check_invalid_value(L, capi51_laux_error_null_format,
			     "luaL_error rejects NULL format",
			     "luaL_error NULL format error");
  capi51_check_invalid_value(L, capi51_setfuncs_negative_upvalues,
			     "luaL_setfuncs rejects negative upvalues",
			     "luaL_setfuncs negative upvalues error");
  capi51_check_invalid_value(L, capi51_setfuncs_missing_target,
			     "luaL_setfuncs rejects missing target",
			     "luaL_setfuncs missing target error");
  capi51_check_invalid_value(L, capi51_setfuncs_missing_upvalue,
			     "luaL_setfuncs rejects missing upvalue",
			     "luaL_setfuncs missing upvalue error");
  capi51_check_invalid_value(L, capi51_openlib_negative_upvalues,
			     "luaL_openlib rejects negative upvalues",
			     "luaL_openlib negative upvalues error");
  capi51_check_invalid_value(L, capi51_openlib_missing_upvalue,
			     "luaL_openlib rejects missing upvalue",
			     "luaL_openlib missing upvalue error");
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "capi51.partialopen");
    check(L, lua_isnil(L, -1), "luaL_openlib failure is not published");
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  capi51_check_invalid_value(L, capi51_getupvalue_invalid_index,
			     "lua_getupvalue rejects invalid function index",
			     "lua_getupvalue invalid function index error");
  capi51_check_invalid_value(L, capi51_setupvalue_invalid_index,
			     "lua_setupvalue rejects invalid function index",
			     "lua_setupvalue invalid function index error");
  capi51_check_invalid_value(L, capi51_upvalueindex_from_lua_hook,
			     "lua_upvalueindex rejects non-C current frame",
			     "lua_upvalueindex non-C frame error");
  capi51_check_invalid_value(L, capi51_environindex_from_lua_hook,
			     "LUA_ENVIRONINDEX rejects non-C current frame",
			     "LUA_ENVIRONINDEX non-C frame error");

  lua_pushinteger(L, 1);
  lua_pushinteger(L, 1);
  check(L, lua_equal(L, -1, -2), "lua_equal default API");
  lua_pop(L, 2);

  lua_pushinteger(L, 1);
  lua_pushinteger(L, 2);
  check(L, lua_lessthan(L, -2, -1), "lua_lessthan default API");
  lua_pop(L, 2);

  lua_getregistry(L);
  check(L, lua_istable(L, -1), "lua_getregistry default macro");
  lua_pop(L, 1);
  check(L, lua_getgccount(L) >= 0, "lua_getgccount default macro");
  lua_setlevel(L, L);
  check(L, lua_getlocal(L, NULL, 1) == NULL,
	"lua_getlocal NULL debug without function returns NULL");

  capi51_check_invalid_value(L, capi51_cpcall_null_function,
			     "lua_cpcall rejects NULL function",
			     "lua_cpcall NULL function error");
  capi51_check_invalid_value(L, capi51_getinfo_null_what,
			     "lua_getinfo rejects NULL what",
			     "lua_getinfo NULL what error");
  capi51_check_invalid_value(L, capi51_getinfo_null_debug,
			     "lua_getinfo rejects NULL debug record",
			     "lua_getinfo NULL debug record error");
  capi51_check_invalid_value(L, capi51_getstack_null_debug,
			     "lua_getstack rejects NULL debug record",
			     "lua_getstack NULL debug record error");
  capi51_check_invalid_value(L, capi51_setlocal_null_debug,
			     "lua_setlocal rejects NULL debug record",
			     "lua_setlocal NULL debug record error");
  capi51_check_invalid_value(L, capi51_checkoption_null_list,
			     "luaL_checkoption rejects NULL option list",
			     "luaL_checkoption NULL option list error");
  capi51_check_invalid_value(L, capi51_traceback_null_thread,
			     "luaL_traceback rejects NULL target thread",
			     "luaL_traceback NULL target thread error");
  capi51_check_invalid_value(L, capi51_checkstack_negative,
			     "luaL_checkstack rejects negative size",
			     "luaL_checkstack negative size error");

  lua_pushcfunction(L, capi51_setlocal_missing_name_preserves_stack);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_OK, "lua_setlocal missing name preserves stack");

  lua_pushcfunction(L, capi51_setfenv_missing_value);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_setfenv rejects missing value");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_setfenv missing value error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_setfenv_nontable_value);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_setfenv rejects non-table value");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_setfenv non-table value error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_getfenv_invalid_index);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_getfenv rejects invalid index");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_getfenv invalid index error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_setfenv_invalid_index);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_setfenv rejects invalid index");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_setfenv invalid index error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_replace_globals_nontable);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_replace rejects non-table globals");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_replace non-table globals error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_replace_env_nontable);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_replace rejects non-table env");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_replace non-table env error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_dump_empty_stack);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_dump rejects empty stack");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_dump empty stack error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_dump_null_writer);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_dump rejects NULL writer");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_dump NULL writer error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_load_null_reader);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_load rejects NULL reader");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_load NULL reader error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_concat_negative_count);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_concat rejects negative count");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_concat negative count error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_rotate_large_count);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_rotate rejects too-large count");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_rotate too-large count error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_rotate_min_count);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_rotate rejects INT_MIN count");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_rotate INT_MIN count error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_typename_too_low);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_typename rejects below LUA_TNONE");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_typename below LUA_TNONE error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_typename_too_high);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_typename rejects too-large type code");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_typename too-large type code error");
  lua_pop(L, 1);

  capi51_check_invalid_value(L, capi51_absindex_zero_index,
			     "lua_absindex rejects zero index",
			     "lua_absindex zero index error");
  capi51_check_invalid_value(L, capi51_absindex_too_negative,
			     "lua_absindex rejects too-negative index",
			     "lua_absindex too-negative index error");
  capi51_check_invalid_value(L, capi51_type_zero_index,
			     "lua_type rejects zero index",
			     "lua_type zero index error");
  capi51_check_invalid_value(L, capi51_type_too_negative,
			     "lua_type rejects too-negative index",
			     "lua_type too-negative index error");
  capi51_check_invalid_value(L, capi51_objlen_zero_index,
			     "lua_objlen rejects zero index",
			     "lua_objlen zero index error");
  capi51_check_invalid_value(L, capi51_objlen_too_negative,
			     "lua_objlen rejects too-negative index",
			     "lua_objlen too-negative index error");
  capi51_check_invalid_value(L, capi51_topointer_zero_index,
			     "lua_topointer rejects zero index",
			     "lua_topointer zero index error");
  capi51_check_invalid_value(L, capi51_rawequal_too_negative,
			     "lua_rawequal rejects too-negative index",
			     "lua_rawequal too-negative index error");

  lua_pushcfunction(L, capi51_error_missing_object);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_error rejects missing object");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_error missing object error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_pcall_nonfunction_errfunc);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pcall rejects non-function errfunc");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pcall non-function errfunc error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_checktype_invalid_expected_low);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checktype rejects below LUA_TNONE");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"luaL_checktype below LUA_TNONE error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_checktype_invalid_expected_high);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN,
	"luaL_checktype rejects too-large expected type");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"luaL_checktype too-large expected type error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_ref_missing_value);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_ref rejects missing value");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"luaL_ref missing value error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_pushcclosure_null_function);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushcclosure rejects NULL function");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushcclosure NULL function error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_pushlstring_null_nonzero);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushlstring rejects NULL nonzero");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushlstring NULL nonzero error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_pushfstring_null_format);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushfstring rejects NULL format");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushfstring NULL format error");
  lua_pop(L, 1);

  lua_pushcfunction(L, capi51_pushvfstring_null_format);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushvfstring rejects NULL format");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushvfstring NULL format error");
  lua_pop(L, 1);

  lua_pushthread(L);
  lua_getfenv(L, -1);
  check(L, lua_istable(L, -1), "lua_getfenv default API");
  lua_pop(L, 2);

  lua_pushliteral(L, "c-upvalue");
  lua_pushcclosure(L, capi51_return_upvalue, 1);
  check(L, lua_upvalueid(L, -1, 1) != NULL,
	"lua_upvalueid C closure upvalue");
  check(L, lua_upvalueid(L, -1, 2) == NULL,
	"lua_upvalueid C closure invalid index");
  lua_call(L, 0, 1);
  check(L, strcmp(lua_tostring(L, -1), "c-upvalue") == 0,
	"lua_upvalueid C closure preserves callable");
  lua_pop(L, 1);

  check(L, luaL_loadstring(L,
    "local x = 'lua-upvalue'\n"
    "return function() return x end") == LUA_OK,
    "lua_upvalueid Lua closure load");
  lua_call(L, 0, 1);
  check(L, lua_upvalueid(L, -1, 1) != NULL,
	"lua_upvalueid Lua closure upvalue");
  check(L, lua_upvalueid(L, -1, 2) == NULL,
	"lua_upvalueid Lua closure invalid index");
  lua_pop(L, 1);

  lua_pushthread(L);
  lua_newtable(L);
  lua_pushliteral(L, "threadenv");
  lua_setfield(L, -2, "marker");
  check(L, lua_setfenv(L, -2) == 1, "lua_setfenv default API");
  lua_getfenv(L, -1);
  lua_getfield(L, -1, "marker");
  check(L, strcmp(lua_tostring(L, -1), "threadenv") == 0,
	"lua_setfenv thread environment");
  lua_pop(L, 3);

  check(L, luaL_loadstring(L, "return marker") == LUA_OK,
	"luaL_loadstring default API status");
  lua_newtable(L);
  lua_pushliteral(L, "lua-func-env");
  lua_setfield(L, -2, "marker");
  check(L, lua_setfenv(L, -2) == 1, "lua_setfenv Lua function");
  lua_call(L, 0, 1);
  check(L, strcmp(lua_tostring(L, -1), "lua-func-env") == 0,
	"lua_getfenv Lua function environment");
  lua_pop(L, 1);

  {
    const char marker[] = "cpcall";
    check(L, lua_cpcall(L, capi51_cpcall, (void *)marker) == LUA_OK,
	  "lua_cpcall default API status");
    lua_getglobal(L, "__lua51_cpcall");
    check(L, strcmp(lua_tostring(L, -1), marker) == 0,
	  "lua_cpcall default API userdata");
    lua_pop(L, 1);
  }

  L2 = lua_newthread(L);
  lua_pushcfunction(L2, capi51_answer);
  capi51_check_resume_invalid_result(L, L2, lua_resume(L2, -1),
				     "lua_resume rejects negative nargs",
				     "lua_resume negative nargs error");
  lua_pop(L, 1);

  L2 = lua_newthread(L);
  lua_pushcfunction(L2, capi51_answer);
  capi51_check_resume_invalid_result(L, L2, lua_resume(L2, 1),
				     "lua_resume rejects too many initial args",
				     "lua_resume too many initial args error");
  lua_pop(L, 1);

  L2 = lua_newthread(L);
  lua_pushcfunction(L2, capi51_yield_once);
  check(L, lua_resume(L2, 0) == LUA_YIELD, "lua_resume yield setup");
  check(L, lua_gettop(L2) == 0, "lua_resume yield setup stack");
  capi51_check_resume_invalid_result(L, L2, lua_resume(L2, 1),
				     "lua_resume rejects too many resume args",
				     "lua_resume too many resume args error");
  lua_pop(L, 1);
  capi51_check_yielded_thread_call_state(L);

  {
    Capi51ReaderCtx ctx = { "return 40 + 2", 0 };
    check(L, lua_loadx(L, capi51_reader, &ctx, "=lua51_loadx", "t") == LUA_OK,
	  "lua_loadx default API status");
    lua_call(L, 0, 1);
    check(L, lua_tointeger(L, -1) == 42, "lua_loadx default API result");
    lua_pop(L, 1);
  }

  {
    const char fname[] = "lua51_capi_loadfile.tmp";
    FILE *fp = fopen(fname, "wb");
    check(L, fp != NULL, "luaL_loadfile fixture open");
    check(L, fputs("return 43", fp) >= 0, "luaL_loadfile fixture write");
    check(L, fclose(fp) == 0, "luaL_loadfile fixture close");
    check(L, luaL_loadfile(L, fname) == LUA_OK,
	  "luaL_loadfile default API status");
    lua_call(L, 0, 1);
    check(L, lua_tointeger(L, -1) == 43, "luaL_loadfile default API result");
    lua_pop(L, 1);
    check(L, luaL_dofile(L, fname) == LUA_OK,
	  "luaL_dofile default macro status");
    check(L, lua_tointeger(L, -1) == 43, "luaL_dofile default macro result");
    lua_pop(L, 1);
    check(L, remove(fname) == 0, "luaL_loadfile fixture remove");
  }

  check(L, luaL_loadbuffer(L, "return 44", strlen("return 44"),
			   "=lua51_loadbuffer") == LUA_OK,
	"luaL_loadbuffer default API status");
  lua_call(L, 0, 1);
  check(L, lua_tointeger(L, -1) == 44, "luaL_loadbuffer default API result");
  lua_pop(L, 1);

  check(L, luaL_dostring(L, "return 45") == LUA_OK,
	"luaL_dostring default macro status");
  check(L, lua_tointeger(L, -1) == 45, "luaL_dostring default macro result");
  lua_pop(L, 1);

  lua_pushinteger(L, 42);
  check(L, luaL_checkint(L, -1) == 42, "luaL_checkint default macro");
  check(L, luaL_checklong(L, -1) == 42L, "luaL_checklong default macro");
  check(L, luaL_checkinteger(L, -1) == 42,
	"luaL_checkinteger default API");
  check(L, luaL_opt(L, luaL_checkinteger, -1, 77) == 42,
	"luaL_opt explicit default macro");
  check(L, luaL_optint(L, 2, 77) == 77, "luaL_optint default macro");
  check(L, luaL_optlong(L, 2, 78L) == 78L, "luaL_optlong default macro");
  check(L, luaL_optinteger(L, 2, 79) == 79,
	"luaL_optinteger default API");
  check(L, luaL_opt(L, luaL_checkinteger, 2, 80) == 80,
	"luaL_opt fallback default macro");
  lua_pop(L, 1);

  lua_pushliteral(L, "checked-string");
  check(L, strcmp(luaL_checkstring(L, -1), "checked-string") == 0,
	"luaL_checkstring default macro");
  check(L, strcmp(luaL_optstring(L, 2, "fallback-string"),
		  "fallback-string") == 0,
	"luaL_optstring default macro");
  check(L, strcmp(luaL_typename(L, -1), "string") == 0,
	"luaL_typename default macro");
  lua_pop(L, 1);

  lua_pushnumber(L, 3.5);
  check(L, luaL_checknumber(L, -1) == (lua_Number)3.5,
	"luaL_checknumber default API");
  check(L, luaL_optnumber(L, 2, (lua_Number)4.5) == (lua_Number)4.5,
	"luaL_optnumber default API");
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushliteral(L, "ref-value");
  {
    int ref = luaL_ref(L, -2);
    check(L, ref > 0, "luaL_ref default API ref");
    lua_rawgeti(L, -1, ref);
    check(L, strcmp(lua_tostring(L, -1), "ref-value") == 0,
	  "luaL_ref default API value");
    lua_pop(L, 1);
    luaL_unref(L, -1, ref);
    lua_rawgeti(L, -1, ref);
    check(L, lua_isnil(L, -1), "luaL_unref default API clears value");
    lua_pop(L, 1);
    lua_rawgeti(L, -1, 0);
    check(L, lua_tointeger(L, -1) == ref,
	  "luaL_unref default API freelist key");
    lua_pop(L, 1);
  }
  lua_pushnil(L);
  check(L, luaL_ref(L, -2) == LUA_REFNIL, "luaL_ref default API nil");
  luaL_unref(L, -1, LUA_NOREF);
  luaL_unref(L, -1, LUA_REFNIL);
  lua_pop(L, 1);

  luaL_register(L, "capi51", capi51_reg);
  lua_getfield(L, -1, "answer");
  lua_call(L, 0, 1);
  check(L, lua_tointeger(L, -1) == 51, "luaL_register default API");
  lua_pop(L, 2);

  lua_newtable(L);
  luaL_openlib(L, NULL, capi51_reg, 0);
  lua_getfield(L, -1, "answer");
  lua_call(L, 0, 1);
  check(L, lua_tointeger(L, -1) == 51, "luaL_openlib default API");
  lua_pop(L, 2);

  luaL_newlibtable(L, capi51_reg);
  check(L, lua_istable(L, -1), "luaL_newlibtable default macro");
  lua_getfield(L, -1, "answer");
  check(L, lua_isnil(L, -1), "luaL_newlibtable leaves fields unset");
  lua_pop(L, 2);

  luaL_newlib(L, capi51_reg);
  lua_getfield(L, -1, "answer");
  lua_call(L, 0, 1);
  check(L, lua_tointeger(L, -1) == 51, "luaL_newlib default macro");
  lua_pop(L, 2);

  lua_newtable(L);
  lua_pushliteral(L, "shared-upvalue");
  luaL_openlib(L, NULL, capi51_upvalue_reg, 1);
  lua_getfield(L, -1, "upvalue");
  lua_call(L, 0, 1);
  check(L, strcmp(lua_tostring(L, -1), "shared-upvalue") == 0,
	"luaL_openlib default API upvalue");
  lua_pop(L, 2);

  lua_pushliteral(L, "module-upvalue");
  luaL_openlib(L, "capi51.upmodule", capi51_upvalue_reg, 1);
  lua_getfield(L, -1, "upvalue");
  lua_call(L, 0, 1);
  check(L, strcmp(lua_tostring(L, -1), "module-upvalue") == 0,
	"luaL_openlib named module upvalue");
  lua_pop(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, "capi51.upmodule");
  check(L, lua_rawequal(L, -1, -3), "luaL_openlib publishes _LOADED");
  lua_pop(L, 2);
  lua_getglobal(L, "capi51");
  lua_getfield(L, -1, "upmodule");
  check(L, lua_rawequal(L, -1, -3), "luaL_openlib publishes global module");
  lua_pop(L, 3);

  {
    int top = lua_gettop(L);
    luaL_pushmodule(L, "capi51.push", 1);
    check(L, lua_istable(L, -1), "luaL_pushmodule default API");
    lua_pushliteral(L, "push-marker");
    lua_setfield(L, -2, "marker");
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, -1, "capi51.push");
    check(L, lua_rawequal(L, -1, -3), "luaL_pushmodule stores _LOADED");
    lua_pop(L, 2);
    lua_getglobal(L, "capi51");
    lua_getfield(L, -1, "push");
    check(L, lua_rawequal(L, -1, -3), "luaL_pushmodule stores global table");
    lua_pop(L, 2);
    lua_pop(L, 1);
    luaL_pushmodule(L, "capi51.push", 1);
    lua_getfield(L, -1, "marker");
    check(L, strcmp(lua_tostring(L, -1), "push-marker") == 0,
	  "luaL_pushmodule reuses loaded table");
    lua_pop(L, 2);
    check(L, lua_gettop(L) == top, "luaL_pushmodule stack balanced");
  }

  {
    int top = lua_gettop(L);
    lua_newtable(L);
    check(L, luaL_getsubtable(L, -1, "child") == 0,
	  "luaL_getsubtable creates default table");
    lua_pushliteral(L, "sub-marker");
    lua_setfield(L, -2, "marker");
    lua_pop(L, 1);
    check(L, luaL_getsubtable(L, -1, "child") == 1,
	  "luaL_getsubtable reuses default table");
    lua_getfield(L, -1, "marker");
    check(L, strcmp(lua_tostring(L, -1), "sub-marker") == 0,
	  "luaL_getsubtable default table value");
    lua_pop(L, 3);
    check(L, lua_gettop(L) == top, "luaL_getsubtable stack balanced");
  }

  capi51_require_open_count = 0;
  luaL_requiref(L, "capi51.req", capi51_require_open, 1);
  check(L, capi51_require_open_count == 1,
	"luaL_requiref default API calls opener");
  lua_getfield(L, -1, "status");
  check(L, strcmp(lua_tostring(L, -1), "ready") == 0,
	"luaL_requiref default API result");
  lua_pop(L, 2);
  lua_pushnil(L);
  lua_setglobal(L, "capi51.req");
  luaL_requiref(L, "capi51.req", capi51_require_open, 1);
  check(L, capi51_require_open_count == 1,
	"luaL_requiref default API reuses loaded module");
  lua_getglobal(L, "capi51.req");
  check(L, lua_rawequal(L, -1, -2),
	"luaL_requiref default API republishes global");
  lua_pop(L, 2);

  luaL_pushfail(L);
  check(L, lua_isnil(L, -1), "luaL_pushfail default macro");
  lua_pop(L, 1);

  luaopen_string_buffer(L);
  check(L, lua_istable(L, -1), "luaopen_string_buffer default API");
  lua_pop(L, 1);

  check(L, luaL_findtable(L, LUA_REGISTRYINDEX, "__lua51.findtable", 1) == NULL,
	"luaL_findtable default API");
  check(L, lua_istable(L, -1), "luaL_findtable table");
  lua_pushliteral(L, "value");
  lua_setfield(L, -2, "key");
  lua_getfield(L, -1, "key");
  check(L, strcmp(lua_tostring(L, -1), "value") == 0,
	"luaL_findtable table value");
  lua_pop(L, 2);

  {
    const char *part;
    int top = lua_gettop(L);
    lua_newtable(L);
    lua_pushliteral(L, "not-table");
    lua_setfield(L, -2, "conflict");
    lua_setfield(L, LUA_REGISTRYINDEX, "__lua51_findtable_conflict");
    part = luaL_findtable(L, LUA_REGISTRYINDEX,
			  "__lua51_findtable_conflict.conflict.child", 1);
    check(L, part != NULL && strcmp(part, "conflict.child") == 0,
	  "luaL_findtable conflict part");
    check(L, lua_gettop(L) == top, "luaL_findtable conflict stack");
  }

  {
    const char *gs;
    gs = luaL_gsub(L, "a-b-a", "-", "_");
    check(L, gs == lua_tostring(L, -1),
	  "luaL_gsub default API return value");
    check(L, strcmp(gs, "a_b_a") == 0,
	  "luaL_gsub default API replacement");
    lua_pop(L, 1);
    gs = luaL_gsub(L, "plain", "z", "_");
    check(L, gs == lua_tostring(L, -1),
	  "luaL_gsub default API no-match return value");
    check(L, strcmp(gs, "plain") == 0,
	  "luaL_gsub default API no-match result");
    lua_pop(L, 1);
    gs = luaL_gsub(L, "aaaa", "aa", "b");
    check(L, gs == lua_tostring(L, -1),
	  "luaL_gsub default API non-overlap return value");
    check(L, strcmp(gs, "bb") == 0,
	  "luaL_gsub default API non-overlap result");
    lua_pop(L, 1);
  }

  {
    int top = lua_gettop(L);
    luaL_where(L, 0);
    check(L, lua_gettop(L) == top + 1, "luaL_where default API pushes");
    check(L, lua_isstring(L, -1), "luaL_where default API string");
    lua_pop(L, 1);
  }

  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, capi51_answer);
  lua_setfield(L, -2, "__capi51");
  lua_setmetatable(L, -2);
  check(L, luaL_getmetafield(L, -1, "__capi51") == 1,
	"luaL_getmetafield default API");
  lua_call(L, 0, 1);
  check(L, lua_tointeger(L, -1) == 51,
	"luaL_getmetafield default API value");
  lua_pop(L, 1);
  check(L, luaL_callmeta(L, -1, "__capi51") == 1,
	"luaL_callmeta default API");
  check(L, lua_tointeger(L, -1) == 51, "luaL_callmeta default API value");
  lua_pop(L, 1);
  check(L, luaL_callmeta(L, -1, "__missing") == 0,
	"luaL_callmeta missing default API");
  lua_pop(L, 1);

  {
    void *ud = lua_newuserdata(L, sizeof(int));
    check(L, luaL_newmetatable(L, "capi51.ud") == 1,
	  "luaL_newmetatable default API creates");
    lua_pushliteral(L, "ud-meta");
    lua_setfield(L, -2, "marker");
    lua_setmetatable(L, -2);
    check(L, luaL_newmetatable(L, "capi51.ud") == 0,
	  "luaL_newmetatable default API reuses");
    lua_getfield(L, -1, "marker");
    check(L, strcmp(lua_tostring(L, -1), "ud-meta") == 0,
	  "luaL_newmetatable default API value");
    lua_pop(L, 2);
    luaL_getmetatable(L, "capi51.ud");
    lua_getfield(L, -1, "marker");
    check(L, strcmp(lua_tostring(L, -1), "ud-meta") == 0,
	  "luaL_getmetatable default macro");
    lua_pop(L, 2);
    luaL_getmetatable(L, "capi51.missing");
    check(L, lua_isnil(L, -1), "luaL_getmetatable missing default macro");
    lua_pop(L, 1);
    check(L, luaL_checkudata(L, -1, "capi51.ud") == ud,
	  "luaL_checkudata default API");
    lua_pop(L, 1);

    ud = lua_newuserdata(L, sizeof(int));
    luaL_setmetatable(L, "capi51.ud");
    check(L, luaL_testudata(L, -1, "capi51.ud") == ud,
	  "luaL_testudata default API match");
    check(L, luaL_testudata(L, -1, "capi51.missing") == NULL,
	  "luaL_testudata default API mismatch");
    luaL_setmetatable(L, "capi51.missing");
    check(L, luaL_testudata(L, -1, "capi51.ud") == NULL,
	  "luaL_setmetatable missing clears default metatable");
    lua_pop(L, 1);
  }

  {
    luaL_Buffer b;
    char *p;
    luaL_buffinit(L, &b);
    p = luaL_prepbuffer(&b);
    memcpy(p, "abc", 3);
    luaL_addsize(&b, 3);
    check(L, luaL_bufflen(&b) == 3, "luaL_bufflen default macro");
    check(L, memcmp(luaL_buffaddr(&b), "abc", 3) == 0,
	  "luaL_buffaddr default macro");
    luaL_buffsub(&b, 1);
    check(L, luaL_bufflen(&b) == 2, "luaL_buffsub default macro");
    luaL_addchar(&b, 'd');
    luaL_addlstring(&b, "ef", 2);
    luaL_addstring(&b, "g");
    lua_pushinteger(L, 51);
    luaL_addvalue(&b);
    luaL_pushresult(&b);
    check(L, strcmp(lua_tostring(L, -1), "abdefg51") == 0,
	  "luaL_Buffer default compatibility macros");
    lua_pop(L, 1);

    luaL_buffinitsize(L, &b, 4);
    memcpy(luaL_buffaddr(&b), "xy", 2);
    luaL_pushresultsize(&b, 2);
    check(L, strcmp(lua_tostring(L, -1), "xy") == 0,
	  "luaL_buffinitsize/luaL_pushresultsize default macros");
    lua_pop(L, 1);

    luaL_buffinit(L, &b);
    luaL_putchar(&b, '5');
    luaL_putchar(&b, '1');
    luaL_pushresult(&b);
    check(L, strcmp(lua_tostring(L, -1), "51") == 0,
	  "luaL_putchar default compatibility macro");
    lua_pop(L, 1);
  }

  lua_pushcfunction(L, capi51_typerror);
  lua_pushliteral(L, "bad");
  check(L, lua_pcall(L, 1, 0, 0) == LUA_ERRRUN,
	"luaL_typerror default API status");
  check(L, strstr(lua_tostring(L, -1), "number expected") != NULL,
	"luaL_typerror default API message");
  lua_pop(L, 1);

  lua_close(L);
  return 0;
}
