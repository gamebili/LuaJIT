/*
** Smoke tests for the Lua 5.4 compatibility C API surface.
*/

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "lua.h"
#include "lauxlib.h"

#ifdef LUA_GLOBALSINDEX
#error "Lua 5.4 compatibility header must not expose LUA_GLOBALSINDEX"
#endif

#ifdef LUA_ENVIRONINDEX
#error "Lua 5.4 compatibility header must not expose LUA_ENVIRONINDEX"
#endif

#ifdef lua_strlen
#error "Lua 5.4 compatibility header must not expose lua_strlen"
#endif

#ifndef LUA_VERSION_MAJOR
#error "Lua 5.4 compatibility header must expose LUA_VERSION_MAJOR"
#endif

#ifndef LUA_VERSION_MINOR
#error "Lua 5.4 compatibility header must expose LUA_VERSION_MINOR"
#endif

#ifndef LUA_VERSION_RELEASE
#error "Lua 5.4 compatibility header must expose LUA_VERSION_RELEASE"
#endif

#ifndef LUA_VERSION_RELEASE_NUM
#error "Lua 5.4 compatibility header must expose LUA_VERSION_RELEASE_NUM"
#endif

#ifndef LUA_NUMTYPES
#error "Lua 5.4 compatibility header must expose LUA_NUMTYPES"
#endif

#ifndef LUA_NUMTAGS
#error "Lua 5.4 compatibility header must expose LUA_NUMTAGS"
#endif

#ifndef LUA_RIDX_LAST
#error "Lua 5.4 compatibility header must expose LUA_RIDX_LAST"
#endif

#ifndef LUA_EXTRASPACE
#error "Lua 5.4 compatibility header must expose LUA_EXTRASPACE"
#endif

#ifndef LUA_GNAME
#error "Lua 5.4 lauxlib header must expose LUA_GNAME"
#endif

#ifndef LUA_FILEHANDLE
#error "Lua 5.4 lauxlib header must expose LUA_FILEHANDLE"
#endif

#ifndef LUA_LOADED_TABLE
#error "Lua 5.4 lauxlib header must expose LUA_LOADED_TABLE"
#endif

#ifndef LUA_PRELOAD_TABLE
#error "Lua 5.4 lauxlib header must expose LUA_PRELOAD_TABLE"
#endif

#ifndef lua_writestring
#error "Lua 5.4 lauxlib header must expose lua_writestring"
#endif

#ifndef lua_writeline
#error "Lua 5.4 lauxlib header must expose lua_writeline"
#endif

#ifndef lua_writestringerror
#error "Lua 5.4 lauxlib header must expose lua_writestringerror"
#endif

#ifndef luaL_prepbuffer
#error "Lua 5.4 lauxlib header must expose luaL_prepbuffer macro"
#endif

#ifndef luaL_argexpected
#error "Lua 5.4 lauxlib header must expose luaL_argexpected macro"
#endif

#ifndef luaL_pushfail
#error "Lua 5.4 lauxlib header must expose luaL_pushfail macro"
#endif

#ifndef luaL_loadfile
#error "Lua 5.4 lauxlib header must expose luaL_loadfile macro"
#endif

#ifndef luaL_loadbuffer
#error "Lua 5.4 lauxlib header must expose luaL_loadbuffer macro"
#endif

#ifndef lua_newuserdata
#error "Lua 5.4 compatibility header must expose lua_newuserdata alias macro"
#endif

#ifndef lua_getuservalue
#error "Lua 5.4 compatibility header must expose lua_getuservalue alias macro"
#endif

#ifndef lua_setuservalue
#error "Lua 5.4 compatibility header must expose lua_setuservalue alias macro"
#endif

#ifndef lua_insert
#error "Lua 5.4 compatibility header must expose lua_insert macro"
#endif

#ifndef lua_remove
#error "Lua 5.4 compatibility header must expose lua_remove macro"
#endif

#ifndef lua_replace
#error "Lua 5.4 compatibility header must expose lua_replace macro"
#endif

#ifndef lua_tonumber
#error "Lua 5.4 compatibility header must expose lua_tonumber macro"
#endif

#ifndef lua_tointeger
#error "Lua 5.4 compatibility header must expose lua_tointeger macro"
#endif

#ifndef lua_getextraspace
#error "Lua 5.4 compatibility header must expose lua_getextraspace macro"
#endif

#ifndef lua_call
#error "Lua 5.4 compatibility header must expose lua_call macro"
#endif

#ifndef lua_pcall
#error "Lua 5.4 compatibility header must expose lua_pcall macro"
#endif

#ifndef lua_yield
#error "Lua 5.4 compatibility header must expose lua_yield macro"
#endif

#include "lualib.h"

#ifndef LUA_VERSUFFIX
#error "Lua 5.4 lualib header must expose LUA_VERSUFFIX"
#endif

#ifndef LUAMOD_API
#error "Lua 5.4 compatibility header must expose LUAMOD_API"
#endif

static int require_open_count = 0;
static char warning_buf[64];
static int warning_tocont = -1;

static void header_output_macros_compile_only(void)
{
  lua_writestring("", 0);
  lua_writeline();
  lua_writestringerror("%s", "");
}

typedef struct CApiReaderCtx {
  const char *src;
  size_t len;
} CApiReaderCtx;

typedef struct DumpBuffer {
  char data[65536];
  size_t len;
} DumpBuffer;

typedef struct AllocCtx {
  int calls;
  int frees;
} AllocCtx;

typedef int (*RawGetI54Sig)(lua_State *L, int idx, lua_Integer n);
typedef void (*RawSetI54Sig)(lua_State *L, int idx, lua_Integer n);
typedef int (*LuaOpenBaseSig)(lua_State *L);
typedef int (*LuaOpenCoroutineSig)(lua_State *L);

static LuaOpenBaseSig luaopen_base_sig = luaopen_base;
static LuaOpenCoroutineSig luaopen_coroutine_sig = luaopen_coroutine;

static void check(lua_State *L, int cond, const char *msg)
{
  if (!cond) {
    fprintf(stderr, "%s\n", msg);
    luaL_error(L, "lua54 C API smoke failed: %s", msg);
  }
}

static void check_string(lua_State *L, int idx, const char *want,
			 const char *msg)
{
  const char *got = lua_tostring(L, idx);
  check(L, got != NULL && strcmp(got, want) == 0, msg);
}

static void check_integer(lua_State *L, int idx, lua_Integer want,
			  const char *msg)
{
  int ok = 0;
  lua_Integer got = lua_tointegerx(L, idx, &ok);
  check(L, ok && got == want, msg);
}

static void *counting_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
  AllocCtx *ctx = (AllocCtx *)ud;
  (void)osize;
  ctx->calls++;
  if (nsize == 0) {
    ctx->frees++;
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

static void test_state_allocator_api(lua_State *L)
{
  AllocCtx ctx = { 0, 0 };
  void *ud = NULL;
  lua_Alloc allocf;
  lua_State *T = lua_newstate(counting_alloc, &ctx);
  check(L, T != NULL, "lua_newstate custom allocator");
  allocf = lua_getallocf(T, &ud);
  check(L, allocf == counting_alloc && ud == &ctx,
	"lua_getallocf custom allocator");
  lua_setallocf(T, allocf, ud);
  allocf = lua_getallocf(T, &ud);
  check(L, allocf == counting_alloc && ud == &ctx,
	"lua_setallocf preserves allocator");
  lua_close(T);
  check(L, ctx.calls > 0 && ctx.frees > 0,
	"lua_close uses custom allocator");
}

static int checkinteger_fraction(lua_State *L)
{
  luaL_checkinteger(L, 1);
  return 0;
}

static int checknumber_arg(lua_State *L)
{
  luaL_checknumber(L, 1);
  return 0;
}

static int optinteger_fraction(lua_State *L)
{
  luaL_optinteger(L, 1, 0);
  return 0;
}

static int len_meta(lua_State *L)
{
  (void)L;
  lua_pushinteger(L, 77);
  return 1;
}

static int require_open(lua_State *L)
{
  check_string(L, 1, "capi.mod", "luaL_requiref passes module name");
  require_open_count++;
  lua_newtable(L);
  lua_pushliteral(L, "ready");
  lua_setfield(L, -2, "state");
  return 1;
}

static int checkversion_bad_version(lua_State *L)
{
  luaL_checkversion_(L, LUA_VERSION_NUM - 1, LUAL_NUMSIZES);
  return 0;
}

static int checkversion_bad_sizes(lua_State *L)
{
  luaL_checkversion_(L, LUA_VERSION_NUM, LUAL_NUMSIZES + 1);
  return 0;
}

static int push_answer(lua_State *L)
{
  lua_pushinteger(L, 42);
  return 1;
}

static int push_upvalue(lua_State *L)
{
  lua_pushvalue(L, lua_upvalueindex(1));
  return 1;
}

static const luaL_Reg capi_newlib[] = {
  { "answer", push_answer },
  { NULL, NULL }
};

static int capi_newlib_checkversion_count;

static void capi_note_checkversion(lua_State *L)
{
  (void)L;
  capi_newlib_checkversion_count++;
}

/* The Lua 5.4 luaL_newlib macro is required to call luaL_checkversion().
** Redefine that macro only for this helper so the smoke can prove the macro
** shape without perturbing the real luaL_checkversion() runtime tests below.
*/
#undef luaL_checkversion
#define luaL_checkversion(L) capi_note_checkversion((L))
static void test_newlib_macro_checkversion(lua_State *L)
{
  capi_newlib_checkversion_count = 0;
  luaL_newlib(L, capi_newlib);
  check(L, capi_newlib_checkversion_count == 1,
	"luaL_newlib calls luaL_checkversion");
  lua_pop(L, 1);
}
#undef luaL_checkversion
#define luaL_checkversion(L) \
  luaL_checkversion_(L, LUA_VERSION_NUM, LUAL_NUMSIZES)

static const luaL_Reg capi_setfuncs[] = {
  { "upvalue", push_upvalue },
  { NULL, NULL }
};

static int yield_once(lua_State *L)
{
  return lua_yield(L, 0);
}

static int yield_two(lua_State *L)
{
  lua_pushliteral(L, "y1");
  lua_pushliteral(L, "y2");
  return lua_yield(L, 2);
}

static int push_isyieldable(lua_State *L)
{
  /* lua_isyieldable() depends on the currently running C frame, so check it
  ** inside a coroutine resumed through the Lua 5.4 lua_resume() surface.
  */
  lua_pushboolean(L, lua_isyieldable(L));
  return 1;
}

static int return_two(lua_State *L)
{
  lua_pushliteral(L, "r1");
  lua_pushliteral(L, "r2");
  return 2;
}

static void capture_warning(void *ud, const char *msg, int tocont)
{
  (void)ud;
  strncpy(warning_buf, msg, sizeof(warning_buf)-1);
  warning_buf[sizeof(warning_buf)-1] = '\0';
  warning_tocont = tocont;
}

static const char *capi_reader(lua_State *L, void *ud, size_t *sz)
{
  CApiReaderCtx *ctx = (CApiReaderCtx *)ud;
  (void)L;
  if (ctx->src == NULL) {
    *sz = 0;
    return NULL;
  }
  *sz = ctx->len;
  ctx->src = NULL;
  ctx->len = 0;
  return "return 64";
}

static int dump_writer(lua_State *L, const void *p, size_t sz, void *ud)
{
  DumpBuffer *b = (DumpBuffer *)ud;
  (void)L;
  if (b->len + sz > sizeof(b->data))
    return 1;
  memcpy(b->data + b->len, p, sz);
  b->len += sz;
  return 0;
}

static void test_stack_and_number_api(lua_State *L)
{
  lua_Integer iv = 0;
  int okflag;
  void **extra;
  const char *ret;
  const char with_nul[] = { 'a', '\0', 'b' };
  lua_State *co;

  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  check(L, lua_tothread(L, -1) == L, "LUA_RIDX_MAINTHREAD");
  lua_pop(L, 1);

  check(L, LUA_NUMTAGS == LUA_NUMTYPES, "LUA_NUMTAGS");
  check(L, LUA_VERSION_RELEASE_NUM == 50400, "LUA_VERSION_RELEASE_NUM");
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  check(L, lua_istable(L, -1), "LUA_RIDX_GLOBALS");
  lua_pop(L, 1);
  check(L, LUA_RIDX_LAST == LUA_RIDX_GLOBALS, "LUA_RIDX_LAST");
  check(L, strcmp(LUA_GNAME, "_G") == 0, "LUA_GNAME");
  check(L, strcmp(LUA_FILEHANDLE, "FILE*") == 0, "LUA_FILEHANDLE");
  check(L, strcmp(LUA_LOADED_TABLE, "_LOADED") == 0, "LUA_LOADED_TABLE");
  check(L, strcmp(LUA_PRELOAD_TABLE, "_PRELOAD") == 0, "LUA_PRELOAD_TABLE");
  check(L, strcmp(LUA_VERSUFFIX, "_5_4") == 0, "LUA_VERSUFFIX");

  check(L, luaopen_base_sig(L) == 1, "luaopen_base return");
  check(L, lua_istable(L, -1), "luaopen_base table");
  lua_getfield(L, -1, "assert");
  check(L, lua_isfunction(L, -1), "luaopen_base assert");
  lua_pop(L, 2);

  check(L, luaopen_coroutine_sig(L) == 1, "luaopen_coroutine return");
  check(L, lua_istable(L, -1), "luaopen_coroutine table");
  lua_getfield(L, -1, "create");
  check(L, lua_isfunction(L, -1), "luaopen_coroutine create");
  lua_pop(L, 2);

  lua_pushglobaltable(L);
  check(L, lua_istable(L, -1), "lua_pushglobaltable registry path");
  lua_pushliteral(L, "ok");
  lua_setglobal(L, "__capi_global");
  lua_getglobal(L, "__capi_global");
  check_string(L, -1, "ok", "lua_getglobal after lua_setglobal");
  lua_pop(L, 1);
  lua_pushnil(L);
  lua_setglobal(L, "__capi_global");
  lua_pop(L, 1);

  check(L, LUA_EXTRASPACE == sizeof(void *), "LUA_EXTRASPACE");
  extra = (void **)lua_getextraspace(L);
  *extra = L;
  co = lua_newthread(L);
  check(L, *(void **)lua_getextraspace(co) == L, "lua_getextraspace copy");
  lua_pop(L, 1);

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  {
    typedef char lua_rawlen_returns_lua_Unsigned[
      _Generic(lua_rawlen(L, -1), lua_Unsigned: 1, default: -1)];
    (void)sizeof(lua_rawlen_returns_lua_Unsigned);
  }
#endif
  lua_pushliteral(L, "rawlen");
  check(L, lua_rawlen(L, -1) == 6, "lua_rawlen lua_Unsigned result");
  lua_pop(L, 1);

  check(L, lua_stringtonumber(L, "123") == 4, "lua_stringtonumber length");
  check_integer(L, -1, 123, "lua_stringtonumber value");
  lua_pop(L, 1);
  check(L, lua_stringtonumber(L, "nope") == 0, "lua_stringtonumber reject");
  check(L, lua_stringtonumber(L, "inf") == 0, "lua_stringtonumber rejects inf");
  check(L, lua_stringtonumber(L, "NaN") == 0, "lua_stringtonumber rejects nan");
  check(L, lua_stringtonumber(L, "0b10") == 0, "lua_stringtonumber rejects binary prefix");
  lua_pushliteral(L, "inf");
  check(L, !lua_isnumber(L, -1), "lua_isnumber rejects inf string");
  okflag = -1;
  check(L, lua_tonumberx(L, -1, &okflag) == 0 && okflag == 0,
	"lua_tonumberx rejects inf string");
  lua_pop(L, 1);
  lua_pushliteral(L, "0b10");
  check(L, !lua_isnumber(L, -1), "lua_isnumber rejects binary prefix string");
  okflag = -1;
  check(L, lua_tointegerx(L, -1, &okflag) == 0 && okflag == 0,
	"lua_tointegerx rejects binary prefix string");
  lua_pop(L, 1);

  lua_pushnumber(L, (lua_Number)2.5);
  check(L, lua_tonumber(L, -1) == (lua_Number)2.5, "lua_tonumber macro");
  lua_pop(L, 1);

  lua_pushinteger(L, 17);
  check_integer(L, -1, 17, "lua_tointeger macro");
  lua_pop(L, 1);

  ret = lua_pushlstring(L, with_nul, sizeof(with_nul));
  check(L, ret != NULL && memcmp(ret, with_nul, sizeof(with_nul)) == 0,
	"lua_pushlstring return value");
  check(L, lua_rawlen(L, -1) == sizeof(with_nul),
	"lua_pushlstring return length");
  lua_pop(L, 1);

  ret = lua_pushstring(L, "pushstring-return");
  check(L, ret != NULL && strcmp(ret, "pushstring-return") == 0,
	"lua_pushstring return value");
  lua_pop(L, 1);

  ret = lua_pushstring(L, NULL);
  check(L, ret == NULL && lua_isnil(L, -1), "lua_pushstring NULL return");
  lua_pop(L, 1);

  ret = lua_pushliteral(L, "pushliteral-return");
  check(L, ret != NULL && strcmp(ret, "pushliteral-return") == 0,
	"lua_pushliteral return value");
  lua_pop(L, 1);

  check(L, lua_numbertointeger((lua_Number)42, &iv) && iv == 42,
	"lua_numbertointeger integer");
  check(L, !lua_numbertointeger((lua_Number)1.5, &iv),
	"lua_numbertointeger fraction");
  lua_pushnumber(L, (lua_Number)1.5);
  check(L, lua_tointegerx(L, -1, NULL) == 0,
	"lua_tointegerx fraction value");
  {
    int ok = 1;
    lua_tointegerx(L, -1, &ok);
    check(L, !ok, "lua_tointegerx fraction status");
  }
  lua_pop(L, 1);

  lua_pushliteral(L, "a");
  lua_pushliteral(L, "b");
  lua_pushliteral(L, "c");
  lua_pushliteral(L, "d");
  lua_rotate(L, -4, 1);
  check_string(L, -4, "d", "lua_rotate first");
  check_string(L, -3, "a", "lua_rotate second");
  check_string(L, -2, "b", "lua_rotate third");
  check_string(L, -1, "c", "lua_rotate fourth");
  lua_pop(L, 4);

  lua_pushliteral(L, "one");
  lua_pushliteral(L, "two");
  lua_pushliteral(L, "three");
  lua_insert(L, -3);
  check_string(L, -3, "three", "lua_insert macro first");
  check_string(L, -2, "one", "lua_insert macro second");
  check_string(L, -1, "two", "lua_insert macro third");
  lua_remove(L, -2);
  check_string(L, -2, "three", "lua_remove macro first");
  check_string(L, -1, "two", "lua_remove macro second");
  lua_pushliteral(L, "replacement");
  lua_replace(L, -2);
  check_string(L, -2, "three", "lua_replace macro first");
  check_string(L, -1, "replacement", "lua_replace macro target");
  lua_pop(L, 2);

  lua_pushliteral(L, "copy-source");
  lua_pushnil(L);
  lua_copy(L, -2, -1);
  check_string(L, -1, "copy-source", "lua_copy destination");
  lua_pop(L, 2);

  check(L, lua_isyieldable(L) == 0, "lua_isyieldable main C frame");

  lua_pushcfunction(L, push_answer);
  lua_call(L, 0, 1);
  check_integer(L, -1, 42, "lua_call macro");
  lua_pop(L, 1);

  lua_pushcfunction(L, push_answer);
  check(L, lua_pcall(L, 0, 1, 0) == LUA_OK, "lua_pcall macro");
  check_integer(L, -1, 42, "lua_pcall macro result");
  lua_pop(L, 1);

  lua_pushcfunction(L, push_answer);
  lua_callk(L, 0, 1, 0, NULL);
  check_integer(L, -1, 42, "lua_callk macro");
  lua_pop(L, 1);

  lua_pushcfunction(L, push_answer);
  check(L, lua_pcallk(L, 0, 1, 0, 0, NULL) == LUA_OK, "lua_pcallk macro");
  check_integer(L, -1, 42, "lua_pcallk result");
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_once);
  lua_xmove(L, co, 1);
  check(L, lua_resume(co, L, 0, NULL) == LUA_YIELD,
	"lua_resetthread setup yield");
  check(L, lua_status(co) == LUA_YIELD, "lua_resetthread yielded status");
  check(L, lua_resetthread(co) == LUA_OK, "lua_resetthread return");
  check(L, lua_status(co) == LUA_OK, "lua_resetthread status");
  check(L, lua_gettop(co) == 0, "lua_resetthread clears stack");
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_once);
  lua_xmove(L, co, 1);
  check(L, lua_resume(co, L, 0, NULL) == LUA_YIELD,
	"lua_closethread setup yield");
  check(L, lua_closethread(co, L) == LUA_OK,
	"lua_closethread yielded return");
  check(L, lua_status(co) == LUA_OK, "lua_closethread yielded status");
  check(L, lua_gettop(co) == 0, "lua_closethread yielded clears stack");
  lua_pop(L, 1);

  co = lua_newthread(L);
  check(L, lua_closethread(co, L) == LUA_OK,
	"lua_closethread fresh return");
  check(L, lua_status(co) == LUA_OK, "lua_closethread fresh status");
  check(L, lua_gettop(co) == 0, "lua_closethread fresh stack");
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, push_isyieldable);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    check(L, lua_resume(co, L, 0, &nres) == LUA_OK,
	  "lua_isyieldable coroutine resume status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_isyieldable coroutine result count");
    check(L, lua_toboolean(co, 1) == 1,
	  "lua_isyieldable resumed C frame");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_two);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    check(L, lua_resume(co, L, 0, &nres) == LUA_YIELD,
	  "lua_resume54 yield status");
    check(L, nres == 2 && lua_gettop(co) == 2,
	  "lua_resume54 yield result count");
    check_string(co, 1, "y1", "lua_resume54 yield result #1");
    check_string(co, 2, "y2", "lua_resume54 yield result #2");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, return_two);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    check(L, lua_resume(co, L, 0, &nres) == LUA_OK,
	  "lua_resume54 return status");
    check(L, nres == 2 && lua_gettop(co) == 2,
	  "lua_resume54 return result count");
    check_string(co, 1, "r1", "lua_resume54 return result #1");
    check_string(co, 2, "r2", "lua_resume54 return result #2");
  }
  lua_pop(L, 1);
}

static void test_compare_len_arith(lua_State *L)
{
  static const char pointer_key;
  int rtype;
  RawGetI54Sig rawgeti_sig = lua_rawgeti54;
  RawSetI54Sig rawseti_sig = lua_rawseti54;

  (void)rawgeti_sig;
  (void)rawseti_sig;

  lua_pushinteger(L, 2);
  lua_pushinteger(L, 3);
  check(L, lua_compare(L, -2, -1, LUA_OPLT), "lua_compare lt");
  check(L, lua_compare(L, -2, -1, LUA_OPLE), "lua_compare le");
  check(L, !lua_compare(L, -2, -1, LUA_OPEQ), "lua_compare eq false");
  lua_pop(L, 2);

  lua_newtable(L);
  lua_pushliteral(L, "x");
  lua_seti(L, -2, 1);
  lua_pushliteral(L, "y");
  lua_seti(L, -2, 2);
  lua_len(L, -1);
  check_integer(L, -1, 2, "lua_len table raw length");
  lua_pop(L, 2);

  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, len_meta);
  lua_setfield(L, -2, "__len");
  lua_setmetatable(L, -2);
  lua_len(L, -1);
  check_integer(L, -1, 77, "lua_len __len metamethod");
  lua_pop(L, 2);

  lua_pushinteger(L, 5);
  lua_pushinteger(L, 2);
  lua_arith(L, LUA_OPADD);
  check_integer(L, -1, 7, "lua_arith add");
  lua_pop(L, 1);

  lua_pushinteger(L, 7);
  lua_pushinteger(L, 3);
  lua_arith(L, LUA_OPIDIV);
  check_integer(L, -1, 2, "lua_arith idiv");
  lua_pop(L, 1);

  lua_pushinteger(L, 6);
  lua_pushinteger(L, 3);
  lua_arith(L, LUA_OPBAND);
  check_integer(L, -1, 2, "lua_arith band");
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushliteral(L, "field-value");
  lua_setfield(L, -2, "field");
  lua_pushliteral(L, "index-value");
  lua_seti(L, -2, 7);
  lua_pushliteral(L, "raw-value");
  lua_rawseti(L, -2, 8);
  lua_pushliteral(L, "ptr-value");
  lua_rawsetp(L, -2, &pointer_key);

  rtype = lua_getfield(L, -1, "field");
  check(L, rtype == LUA_TSTRING, "lua_getfield return type");
  check_string(L, -1, "field-value", "lua_getfield return value");
  lua_pop(L, 1);

  rtype = lua_geti(L, -1, 7);
  check(L, rtype == LUA_TSTRING, "lua_geti return type");
  check_string(L, -1, "index-value", "lua_geti return value");
  lua_pop(L, 1);

  lua_pushliteral(L, "field");
  rtype = lua_gettable(L, -2);
  check(L, rtype == LUA_TSTRING, "lua_gettable return type");
  check_string(L, -1, "field-value", "lua_gettable return value");
  lua_pop(L, 1);

  lua_pushinteger(L, 8);
  rtype = lua_rawget(L, -2);
  check(L, rtype == LUA_TSTRING, "lua_rawget return type");
  check_string(L, -1, "raw-value", "lua_rawget return value");
  lua_pop(L, 1);

  rtype = lua_rawgeti(L, -1, 8);
  check(L, rtype == LUA_TSTRING, "lua_rawgeti return type");
  check_string(L, -1, "raw-value", "lua_rawgeti return value");
  lua_pop(L, 1);

  rtype = lua_rawgetp(L, -1, &pointer_key);
  check(L, rtype == LUA_TSTRING, "lua_rawgetp return type");
  check_string(L, -1, "ptr-value", "lua_rawgetp return value");
  lua_pop(L, 1);

  rtype = lua_getglobal(L, "debug");
  check(L, rtype == LUA_TTABLE, "lua_getglobal return type");
  lua_pop(L, 2);
}

static void test_uservalue_api(lua_State *L)
{
  int top;
  void *alias_ud;
  void *ud = lua_newuserdatauv(L, 4, 2);
  check(L, ud != NULL, "lua_newuserdatauv");
  lua_pushliteral(L, "uv1");
  check(L, lua_setiuservalue(L, -2, 1) == 1, "lua_setiuservalue #1");
  check(L, lua_getiuservalue(L, -1, 1) == LUA_TSTRING,
	"lua_getiuservalue #1 type");
  check_string(L, -1, "uv1", "lua_getiuservalue #1 value");
  lua_pop(L, 1);
  check(L, lua_getiuservalue(L, -1, 2) == LUA_TNIL,
	"lua_getiuservalue declared nil");
  lua_pop(L, 1);
  top = lua_gettop(L);
  check(L, lua_getiuservalue(L, -1, 3) == LUA_TNONE,
	"lua_getiuservalue out of range");
  check(L, lua_gettop(L) == top, "lua_getiuservalue none pushes nothing");
  lua_pushnil(L);
  check(L, lua_setiuservalue(L, -2, 3) == 0,
	"lua_setiuservalue out of range");
  check(L, lua_gettop(L) == top, "lua_setiuservalue invalid pops value");

  alias_ud = lua_newuserdata(L, 4);
  check(L, alias_ud != NULL, "lua_newuserdata alias");
  lua_pushliteral(L, "uv-alias");
  check(L, lua_setuservalue(L, -2) == 1, "lua_setuservalue alias");
  check(L, lua_getuservalue(L, -1) == LUA_TSTRING,
	"lua_getuservalue alias type");
  check_string(L, -1, "uv-alias", "lua_getuservalue alias value");
  lua_pop(L, 2);

  lua_getglobal(L, "debug");
  lua_getfield(L, -1, "getuservalue");
  lua_pushvalue(L, -3);
  lua_pushinteger(L, 1);
  lua_call(L, 2, 1);
  check_string(L, -1, "uv1", "debug.getuservalue reads declared slot");
  lua_pop(L, 1);

  lua_getfield(L, -1, "setuservalue");
  lua_pushvalue(L, -3);
  lua_pushliteral(L, "uv2");
  lua_pushinteger(L, 2);
  lua_call(L, 3, 1);
  check(L, lua_touserdata(L, -1) == ud, "debug.setuservalue returns userdata");
  lua_pop(L, 1);

  lua_getfield(L, -1, "getuservalue");
  lua_pushvalue(L, -3);
  lua_pushinteger(L, 2);
  lua_call(L, 2, 1);
  check_string(L, -1, "uv2", "debug.setuservalue updates declared slot");
  lua_pop(L, 1);

  lua_getfield(L, -1, "setuservalue");
  lua_pushvalue(L, -3);
  lua_pushliteral(L, "bad");
  lua_pushinteger(L, 3);
  lua_call(L, 3, 1);
  check(L, lua_isnil(L, -1), "debug.setuservalue rejects undeclared slot");
  lua_pop(L, 2);
}

static void test_lauxlib_api(lua_State *L)
{
  luaL_Buffer b;
  luaL_Stream stream;
  char *p;
  int status;
  int rtype;
  void *ud;
  CApiReaderCtx reader;

  stream.f = NULL;
  stream.closef = NULL;
  check(L, stream.f == NULL && stream.closef == NULL, "luaL_Stream fields");

  luaL_checkversion(L);
  luaL_argexpected(L, 1, 1, "truthy condition");

  lua_pushcfunction(L, checkversion_bad_version);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkversion_ rejects wrong version");
  check(L, strstr(lua_tostring(L, -1), "version mismatch") != NULL,
	"luaL_checkversion_ version error");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkversion_bad_sizes);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkversion_ rejects wrong sizes");
  check(L, strstr(lua_tostring(L, -1), "incompatible numeric types") != NULL,
	"luaL_checkversion_ sizes error");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkinteger_fraction);
  lua_pushnumber(L, (lua_Number)1.5);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkinteger rejects fraction");
  check(L, strstr(lua_tostring(L, -1),
		  "number has no integer representation") != NULL,
	"luaL_checkinteger fraction error");
  lua_pop(L, 1);

  lua_pushcfunction(L, optinteger_fraction);
  lua_pushnumber(L, (lua_Number)1.5);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_optinteger rejects fraction");
  check(L, strstr(lua_tostring(L, -1),
		  "number has no integer representation") != NULL,
	"luaL_optinteger fraction error");
  lua_pop(L, 1);

  lua_pushcfunction(L, checknumber_arg);
  lua_pushliteral(L, "nan");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checknumber rejects nan string");
  check(L, strstr(lua_tostring(L, -1), "number expected") != NULL,
	"luaL_checknumber nan error");
  lua_pop(L, 1);

  luaL_pushfail(L);
  check(L, lua_isnil(L, -1), "luaL_pushfail pushes nil");
  lua_pop(L, 1);

  check(L, luaL_intop(+, LUA_MAXINTEGER, 1) == LUA_MININTEGER,
	"luaL_intop add wrap");
  check(L, luaL_intop(-, LUA_MININTEGER, 1) == LUA_MAXINTEGER,
	"luaL_intop sub wrap");
  check(L, luaL_intop(&, (lua_Integer)0x33, (lua_Integer)0x55) == 0x11,
	"luaL_intop bit and");

  lua_newtable(L);
  check(L, luaL_getsubtable(L, -1, "child") == 0,
	"luaL_getsubtable creates");
  check(L, lua_istable(L, -1), "luaL_getsubtable created table");
  lua_pop(L, 1);
  check(L, luaL_getsubtable(L, -1, "child") == 1,
	"luaL_getsubtable reuses");
  lua_pop(L, 2);

  check(L, luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE) == 1,
	"LUA_LOADED_TABLE");
  lua_pop(L, 1);

  luaL_requiref(L, "capi.mod", require_open, 1);
  check(L, require_open_count == 1, "luaL_requiref calls opener once");
  lua_getfield(L, -1, "state");
  check_string(L, -1, "ready", "luaL_requiref module result");
  lua_pop(L, 2);
  luaL_requiref(L, "capi.mod", require_open, 1);
  check(L, require_open_count == 1, "luaL_requiref reuses loaded module");
  lua_pop(L, 1);

  check(L, luaL_fileresult(L, 1, NULL) == 1, "luaL_fileresult success arity");
  check(L, lua_toboolean(L, -1), "luaL_fileresult success value");
  lua_pop(L, 1);

  errno = ENOENT;
  check(L, luaL_fileresult(L, 0, "missing.lua") == 3,
	"luaL_fileresult failure arity");
  check(L, lua_isnil(L, -3), "luaL_fileresult failure nil");
  check(L, strstr(lua_tostring(L, -2), "missing.lua") != NULL,
	"luaL_fileresult failure filename");
  check_integer(L, -1, ENOENT, "luaL_fileresult errno");
  lua_pop(L, 3);

  check(L, luaL_execresult(L, 0) == 3, "luaL_execresult success arity");
  check(L, lua_toboolean(L, -3), "luaL_execresult success bool");
  check_string(L, -2, "exit", "luaL_execresult success kind");
  check_integer(L, -1, 0, "luaL_execresult success code");
  lua_pop(L, 3);

  luaL_newlib(L, capi_newlib);
  lua_getfield(L, -1, "answer");
  lua_call(L, 0, 1);
  check_integer(L, -1, 42, "luaL_newlib function");
  lua_pop(L, 2);
  test_newlib_macro_checkversion(L);

  lua_newtable(L);
  lua_pushliteral(L, "captured-upvalue");
  luaL_setfuncs(L, capi_setfuncs, 1);
  lua_getfield(L, -1, "upvalue");
  lua_call(L, 0, 1);
  check_string(L, -1, "captured-upvalue", "luaL_setfuncs upvalue");
  lua_pop(L, 2);

  check(L, luaL_newmetatable(L, "capi.ud") == 1, "luaL_newmetatable creates");
  rtype = lua_getfield(L, -1, "__name");
  check(L, rtype == LUA_TSTRING, "luaL_newmetatable __name type");
  check_string(L, -1, "capi.ud", "luaL_newmetatable __name value");
  lua_pop(L, 2);
  check(L, luaL_getmetatable(L, "capi.ud") == LUA_TTABLE,
	"luaL_getmetatable type");
  lua_pop(L, 1);
  ud = lua_newuserdatauv(L, 1, 0);
  luaL_setmetatable(L, "capi.ud");
  check(L, luaL_testudata(L, -1, "capi.ud") == ud, "luaL_testudata match");
  check(L, luaL_testudata(L, -1, "capi.other") == NULL,
	"luaL_testudata mismatch");
  check(L, luaL_checkudata(L, -1, "capi.ud") == ud, "luaL_checkudata match");
  lua_pop(L, 1);

  luaL_traceback(L, L, "trace-msg", 0);
  check(L, strstr(lua_tostring(L, -1), "trace-msg") != NULL,
	"luaL_traceback message");
  check(L, strstr(lua_tostring(L, -1), "stack traceback") != NULL,
	"luaL_traceback stack");
  lua_pop(L, 1);

  status = luaL_dostring(L, "return 12");
  check(L, status == LUA_OK, "luaL_dostring status");
  check_integer(L, -1, 12, "luaL_dostring result");
  lua_pop(L, 1);

  lua_pushboolean(L, 1);
  luaL_tolstring(L, -1, NULL);
  check_string(L, -1, "true", "luaL_tolstring boolean");
  lua_pop(L, 2);

  luaL_buffinit(L, &b);
  p = luaL_prepbuffer(&b);
  check(L, p == luaL_buffaddr(&b), "luaL_buffaddr");
  memcpy(p, "abcd", 4);
  luaL_addsize(&b, 4);
  check(L, luaL_bufflen(&b) == 4, "luaL_bufflen");
  luaL_buffsub(&b, 1);
  luaL_pushresult(&b);
  check_string(L, -1, "abc", "luaL_buffsub result");
  lua_pop(L, 1);

  luaL_buffinit(L, &b);
  p = luaL_prepbuffer(&b);
  memcpy(p, "xy", 2);
  luaL_pushresultsize(&b, 2);
  check_string(L, -1, "xy", "luaL_pushresultsize");
  lua_pop(L, 1);

  luaL_buffinit(L, &b);
  {
    size_t big = LUAL_BUFFERSIZE + 32;
    p = luaL_prepbuffsize(&b, big);
    memset(p, 'z', big);
    luaL_addsize(&b, big);
    check(L, luaL_bufflen(&b) == big, "luaL_prepbuffsize big length");
    luaL_pushresult(&b);
    check(L, lua_rawlen(L, -1) == big, "luaL_prepbuffsize big result size");
    check(L, lua_tostring(L, -1)[0] == 'z' &&
	     lua_tostring(L, -1)[big - 1] == 'z',
	  "luaL_prepbuffsize big result bytes");
    lua_pop(L, 1);
  }

  {
    size_t big = LUAL_BUFFERSIZE + 17;
    p = luaL_buffinitsize(L, &b, big);
    memset(p, 'q', big);
    luaL_pushresultsize(&b, big);
    check(L, lua_rawlen(L, -1) == big, "luaL_buffinitsize big result size");
    check(L, lua_tostring(L, -1)[0] == 'q' &&
	     lua_tostring(L, -1)[big - 1] == 'q',
	  "luaL_buffinitsize big result bytes");
    lua_pop(L, 1);
  }

  luaL_buffinit(L, &b);
  luaL_addgsub(&b, "a?$?", "?", "Lua54");
  luaL_pushresult(&b);
  check_string(L, -1, "aLua54$Lua54", "luaL_addgsub result");
  lua_pop(L, 1);

  status = luaL_loadbufferx(L, "return 54", 9, "=capi-buffer", "t");
  check(L, status == LUA_OK, "luaL_loadbufferx text mode");
  lua_call(L, 0, 1);
  check_integer(L, -1, 54, "luaL_loadbufferx loaded function");
  lua_pop(L, 1);

  status = luaL_loadbuffer(L, "return 55", 9, "=capi-loadbuffer");
  check(L, status == LUA_OK, "luaL_loadbuffer macro");
  lua_call(L, 0, 1);
  check_integer(L, -1, 55, "luaL_loadbuffer macro result");
  lua_pop(L, 1);

  status = luaL_loadbufferx(L, "return 54", 9, "=capi-buffer", "b");
  check(L, status == LUA_ERRSYNTAX, "luaL_loadbufferx binary mode rejects text");
  check(L, strstr(lua_tostring(L, -1),
		  "attempt to load a text chunk (mode is 'b')") != NULL,
	"luaL_loadbufferx wrong mode error");
  lua_pop(L, 1);

  status = luaL_loadfile(L, "test/smoke.lua");
  check(L, status == LUA_OK, "luaL_loadfile macro");
  lua_pop(L, 1);

  status = luaL_loadfilex(L, "test/smoke.lua", "t");
  check(L, status == LUA_OK, "luaL_loadfilex text mode");
  lua_pop(L, 1);

  status = luaL_loadfilex(L, "test/smoke.lua", "b");
  check(L, status == LUA_ERRSYNTAX, "luaL_loadfilex binary mode rejects text");
  check(L, strstr(lua_tostring(L, -1),
		  "attempt to load a text chunk (mode is 'b')") != NULL,
	"luaL_loadfilex wrong mode error");
  lua_pop(L, 1);

  reader.src = "return 64";
  reader.len = 9;
  status = lua_load(L, capi_reader, &reader, "=capi-reader", "t");
  check(L, status == LUA_OK, "lua_load text mode");
  lua_call(L, 0, 1);
  check_integer(L, -1, 64, "lua_load loaded function");
  lua_pop(L, 1);

  reader.src = "return 64";
  reader.len = 9;
  status = lua_load(L, capi_reader, &reader, "=capi-reader", "b");
  check(L, status == LUA_ERRSYNTAX, "lua_load binary mode rejects text");
  check(L, strstr(lua_tostring(L, -1),
		  "attempt to load a text chunk (mode is 'b')") != NULL,
	"lua_load wrong mode error");
  lua_pop(L, 1);
}

static void test_dump_api(lua_State *L)
{
  const char *src = "return function(a) return a + 1 end";
  DumpBuffer full;
  DumpBuffer stripped;
  int status;

  memset(&full, 0, sizeof(full));
  memset(&stripped, 0, sizeof(stripped));

  status = luaL_loadbufferx(L, src, strlen(src), "=dump-source", "t");
  check(L, status == LUA_OK, "lua_dump setup load");
  lua_call(L, 0, 1);
  check(L, lua_dump(L, dump_writer, &full, 0) == 0, "lua_dump full");
  check(L, full.len > 0, "lua_dump full length");
  check(L, lua_dump(L, dump_writer, &stripped, 1) == 0, "lua_dump stripped");
  check(L, stripped.len > 0 && stripped.len <= full.len,
	"lua_dump stripped length");
  lua_pop(L, 1);

  status = luaL_loadbufferx(L, stripped.data, stripped.len, "=dumped", "b");
  check(L, status == LUA_OK, "lua_dump stripped reload");
  lua_pushinteger(L, 41);
  lua_call(L, 1, 1);
  check_integer(L, -1, 42, "lua_dump stripped roundtrip");
  lua_pop(L, 1);
}

static void test_warning_and_gc_api(lua_State *L)
{
  int oldmode;
  int countb;
  lua_Number version;
  lua_Debug ar;

  version = lua_version(L);
  check(L, version == (lua_Number)LUA_VERSION_NUM,
	"lua_version returns numeric Lua 5.4 version");
  check(L, lua_setcstacklimit(L, 0) == 200,
	"lua_setcstacklimit query shim");
  check(L, lua_setcstacklimit(L, 200) == 200,
	"lua_setcstacklimit set shim");

  lua_setwarnf(L, capture_warning, NULL);
  lua_warning(L, "captured", 0);
  check(L, strcmp(warning_buf, "captured") == 0 && warning_tocont == 0,
	"lua_warning callback");

  check(L, lua_gc(L, LUA_GCCOLLECT) == 0, "lua_gc vararg collect");
  (void)lua_gc(L, LUA_GCCOUNT);
  countb = lua_gc(L, LUA_GCCOUNTB);
  check(L, countb >= 0 && countb < 1024, "LUA_GCCOUNTB byte remainder");

  oldmode = lua_gc(L, LUA_GCGEN, 0, 0);
  check(L, oldmode == LUA_GCGEN || oldmode == LUA_GCINC, "LUA_GCGEN");
  oldmode = lua_gc(L, LUA_GCINC, 0, 0, 0);
  check(L, oldmode == LUA_GCGEN, "LUA_GCINC previous mode");
  oldmode = lua_gc(L, LUA_GCGEN, 0, 0);
  check(L, oldmode == LUA_GCINC, "LUA_GCGEN previous mode");

  memset(&ar, 0, sizeof(ar));
  lua_pushcfunction(L, push_answer);
  check(L, lua_getinfo(L, ">utr", &ar), "lua_getinfo >utr");
  check(L, ar.nparams == 0 && ar.isvararg == 1, "lua_Debug u fields");
  check(L, ar.istailcall == 0 && ar.ftransfer == 0 && ar.ntransfer == 0,
	"lua_Debug t/transfer fields");
}

int main(void)
{
  lua_State *L = luaL_newstate();
  if (L == NULL)
    return 2;
  luaL_openlibs(L);
  test_state_allocator_api(L);
  test_stack_and_number_api(L);
  test_compare_len_arith(L);
  test_uservalue_api(L);
  test_lauxlib_api(L);
  test_dump_api(L);
  test_warning_and_gc_api(L);
  lua_close(L);
  return 0;
}
