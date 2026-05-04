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
static int close_call_count = 0;
static int close_nil_error_count = 0;
static int close_body_error_count = 0;
static int hook_call_ftransfer = -1;
static int hook_call_ntransfer = -1;
static int hook_ret_ftransfer = -1;
static int hook_ret_ntransfer = -1;
static int hook_vararg_call_ftransfer = -1;
static int hook_vararg_call_ntransfer = -1;
static int hook_vararg_ret_ftransfer = -1;
static int hook_vararg_ret_ntransfer = -1;
static int hook_c_call_ftransfer = -1;
static int hook_c_call_ntransfer = -1;
static int hook_c_ret_ftransfer = -1;
static int hook_c_ret_ntransfer = -1;
static int hook_tail_ftransfer = -1;
static int hook_tail_ntransfer = -1;
static int hook_tail_istailcall = -1;
static int hook_tail_vararg_call_ftransfer = -1;
static int hook_tail_vararg_call_ntransfer = -1;
static int hook_tail_vararg_ret_ftransfer = -1;
static int hook_tail_vararg_ret_ntransfer = -1;
static int hook_tail_vararg_ret_istailcall = -1;

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
typedef lua_Number (*LuaVersionValueSig)(lua_State *L);

static LuaOpenBaseSig luaopen_base_sig = luaopen_base;
static LuaOpenCoroutineSig luaopen_coroutine_sig = luaopen_coroutine;
static LuaVersionValueSig lua_version_value_sig = lua_version;

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

static int panic_a(lua_State *L)
{
  (void)L;
  return 0;
}

static int panic_b(lua_State *L)
{
  (void)L;
  return 0;
}

static void capi_transfer_hook(lua_State *L, lua_Debug *ar)
{
  if (!lua_getinfo(L, "nrutS", ar))
    return;
  if (ar->nparams == 2 && !ar->isvararg) {
    if (ar->event == LUA_HOOKCALL) {
      hook_call_ftransfer = ar->ftransfer;
      hook_call_ntransfer = ar->ntransfer;
    } else if (ar->event == LUA_HOOKTAILCALL) {
      hook_tail_ftransfer = ar->ftransfer;
      hook_tail_ntransfer = ar->ntransfer;
      hook_tail_istailcall = ar->istailcall;
    } else if (ar->event == LUA_HOOKRET) {
      hook_ret_ftransfer = ar->ftransfer;
      hook_ret_ntransfer = ar->ntransfer;
    }
  } else if (ar->nparams == 1 && ar->isvararg) {
    /* Lua 5.4 reports only the fixed parameter as call input for varargs. */
    if (ar->event == LUA_HOOKCALL) {
      hook_vararg_call_ftransfer = ar->ftransfer;
      hook_vararg_call_ntransfer = ar->ntransfer;
    } else if (ar->event == LUA_HOOKRET) {
      hook_vararg_ret_ftransfer = ar->ftransfer;
      hook_vararg_ret_ntransfer = ar->ntransfer;
    }
  } else if (ar->nparams == 0 && ar->isvararg && ar->istailcall) {
    if (ar->event == LUA_HOOKTAILCALL) {
      hook_tail_vararg_call_ftransfer = ar->ftransfer;
      hook_tail_vararg_call_ntransfer = ar->ntransfer;
    } else if (ar->event == LUA_HOOKRET) {
      hook_tail_vararg_ret_ftransfer = ar->ftransfer;
      hook_tail_vararg_ret_ntransfer = ar->ntransfer;
      hook_tail_vararg_ret_istailcall = ar->istailcall;
    }
  } else if (ar->event == LUA_HOOKCALL && ar->what != NULL &&
	     strcmp(ar->what, "C") == 0 && hook_c_call_ftransfer < 0) {
    hook_c_call_ftransfer = ar->ftransfer;
    hook_c_call_ntransfer = ar->ntransfer;
  } else if (ar->event == LUA_HOOKRET && ar->what != NULL &&
	     strcmp(ar->what, "C") == 0 && hook_c_ret_ftransfer < 0) {
    hook_c_ret_ftransfer = ar->ftransfer;
    hook_c_ret_ntransfer = ar->ntransfer;
  }
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
  check(L, lua_atpanic(T, panic_a) == NULL, "lua_atpanic initial handler");
  check(L, lua_atpanic(T, panic_b) == panic_a, "lua_atpanic old handler");
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

static int raise_lua_error(lua_State *L)
{
  lua_pushliteral(L, "capi raised error");
  return lua_error(L);
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

static int capi_transfer_cfunc(lua_State *L)
{
  check_integer(L, 1, 1, "capi transfer C arg 1");
  check_integer(L, 2, 2, "capi transfer C arg 2");
  check_integer(L, 3, 3, "capi transfer C arg 3");
  lua_pushinteger(L, 6);
  return 1;
}

static int record_close(lua_State *L)
{
  close_call_count++;
  if (lua_gettop(L) == 2 && lua_isnil(L, 2))
    close_nil_error_count++;
  if (lua_gettop(L) == 2 && lua_tostring(L, 2) != NULL &&
      strstr(lua_tostring(L, 2), "capi body boom") != NULL)
    close_body_error_count++;
  return 0;
}

static int record_close_error(lua_State *L)
{
  close_call_count++;
  if (lua_gettop(L) == 2 && lua_isnil(L, 2))
    close_nil_error_count++;
  lua_pushliteral(L, "capi close boom");
  return lua_error(L);
}

static void push_closeable(lua_State *L, lua_CFunction closef)
{
  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, closef);
  lua_setfield(L, -2, "__close");
  lua_setmetatable(L, -2);
}

static int mark_nonclosable_slot(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_toclose(L, -1);
  return 0;
}

static int mark_false_then_closeslot(lua_State *L)
{
  lua_pushboolean(L, 0);
  lua_toclose(L, -1);
  lua_closeslot(L, -1);
  return 0;
}

static int mark_close_then_pop(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_settop(L, 0);
  return 0;
}

static int mark_close_twice_same_slot(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_toclose(L, -1);
  return 0;
}

static int mark_close_below_active_slot(lua_State *L)
{
  push_closeable(L, record_close);
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_toclose(L, -2);
  return 0;
}

static int mark_nil_below_active_slot(lua_State *L)
{
  lua_pushnil(L);
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_toclose(L, -2);
  return 0;
}

static int closeslot_not_last(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_closeslot(L, -2);
  return 0;
}

static int closeslot_lifo(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_closeslot(L, -1);
  lua_closeslot(L, -2);
  return 0;
}

static int mark_close_then_return(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  return 0;
}

static int mark_close_then_return_self(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  return 1;
}

static int mark_close_error_then_return(lua_State *L)
{
  push_closeable(L, record_close_error);
  lua_toclose(L, -1);
  return 0;
}

static int mark_close_then_error(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  return luaL_error(L, "capi body boom");
}

static int push_upvalue(lua_State *L)
{
  lua_pushvalue(L, lua_upvalueindex(1));
  return 1;
}

static int capi_tostring_meta(lua_State *L)
{
  (void)L;
  lua_pushliteral(L, "meta tostring");
  return 1;
}

static int checkoption_arg(lua_State *L)
{
  static const char *opts[] = { "alpha", "beta", "gamma", NULL };
  lua_pushinteger(L, luaL_checkoption(L, 1, "beta", opts));
  return 1;
}

static int laux_error_arg(lua_State *L)
{
  return luaL_error(L, "laux boom");
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
  static const char light_key;
  lua_Integer iv = 0;
  int okflag;
  int status;
  void **extra;
  const char *ret;
  const char with_nul[] = { 'a', '\0', 'b' };
  lua_State *co;
  void *fullud;

  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  check(L, lua_tothread(L, -1) == L, "LUA_RIDX_MAINTHREAD");
  lua_pop(L, 1);

  check(L, LUA_NUMTAGS == LUA_NUMTYPES, "LUA_NUMTAGS");
  check(L, strcmp(LUA_VERSION_RELEASE, "8") == 0, "LUA_VERSION_RELEASE");
  check(L, strcmp(LUA_RELEASE, "Lua 5.4.8") == 0, "LUA_RELEASE");
  check(L, strstr(LUA_COPYRIGHT, "Lua 5.4.8") != NULL, "LUA_COPYRIGHT release");
  check(L, strchr(LUA_AUTHORS, '&') == NULL, "LUA_AUTHORS separator");
  check(L, LUA_VERSION_RELEASE_NUM == 50408, "LUA_VERSION_RELEASE_NUM");
  check(L, strstr(lua_ident, "LuaVersion: Lua 5.4.8") != NULL, "lua_ident version");
  check(L, strstr(lua_ident, "LuaAuthors:") != NULL, "lua_ident authors");
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  check(L, lua_istable(L, -1), "LUA_RIDX_GLOBALS");
  lua_pop(L, 1);
  check(L, LUA_RIDX_LAST == LUA_RIDX_GLOBALS, "LUA_RIDX_LAST");
  check(L, strcmp(LUA_GNAME, "_G") == 0, "LUA_GNAME");
  check(L, strcmp(LUA_FILEHANDLE, "FILE*") == 0, "LUA_FILEHANDLE");
  check(L, strcmp(LUA_LOADED_TABLE, "_LOADED") == 0, "LUA_LOADED_TABLE");
  check(L, strcmp(LUA_PRELOAD_TABLE, "_PRELOAD") == 0, "LUA_PRELOAD_TABLE");
  check(L, strcmp(LUA_VERSUFFIX, "_5_4") == 0, "LUA_VERSUFFIX");

  check(L, lua_type(L, lua_gettop(L) + 1) == LUA_TNONE,
	"lua_type none");
  check(L, lua_isnone(L, lua_gettop(L) + 1), "lua_isnone none");
  check(L, lua_isnoneornil(L, lua_gettop(L) + 1),
	"lua_isnoneornil none");
  check(L, strcmp(lua_typename(L, LUA_TNONE), "no value") == 0,
	"lua_typename none");
  check(L, strcmp(lua_typename(L, LUA_TNIL), "nil") == 0,
	"lua_typename nil");

  lua_pushnil(L);
  check(L, lua_type(L, -1) == LUA_TNIL, "lua_type nil");
  check(L, lua_isnil(L, -1), "lua_isnil nil");
  check(L, lua_isnoneornil(L, -1), "lua_isnoneornil nil");
  check(L, !lua_toboolean(L, -1), "lua_toboolean nil");
  lua_pop(L, 1);

  lua_pushboolean(L, 0);
  lua_pushboolean(L, 1);
  check(L, lua_isboolean(L, -2) && lua_isboolean(L, -1),
	"lua_isboolean values");
  check(L, !lua_toboolean(L, -2) && lua_toboolean(L, -1),
	"lua_toboolean booleans");
  check(L, !lua_rawequal(L, -2, -1), "lua_rawequal booleans false");
  lua_pop(L, 2);

  lua_pushliteral(L, "same");
  lua_pushliteral(L, "same");
  check(L, lua_rawequal(L, -2, -1), "lua_rawequal strings");
  lua_pop(L, 2);

  lua_pushinteger(L, 123);
  check(L, lua_isstring(L, -1), "lua_isstring number");
  check(L, lua_toboolean(L, -1), "lua_toboolean number");
  lua_pop(L, 1);

  lua_pushcfunction(L, push_answer);
  check(L, lua_iscfunction(L, -1), "lua_iscfunction c closure");
  check(L, lua_tocfunction(L, -1) == push_answer,
	"lua_tocfunction c closure");
  check(L, lua_topointer(L, -1) != NULL, "lua_topointer c closure");
  lua_pop(L, 1);

  status = luaL_loadstring(L, "return 1");
  check(L, status == LUA_OK, "luaL_loadstring lua function setup");
  check(L, lua_isfunction(L, -1), "lua_isfunction lua closure");
  check(L, !lua_iscfunction(L, -1), "lua_iscfunction lua closure false");
  check(L, lua_tocfunction(L, -1) == NULL, "lua_tocfunction lua closure");
  lua_pop(L, 1);

  lua_pushlightuserdata(L, (void *)&light_key);
  check(L, lua_type(L, -1) == LUA_TLIGHTUSERDATA,
	"lua_type lightuserdata");
  check(L, lua_isuserdata(L, -1), "lua_isuserdata lightuserdata");
  check(L, lua_islightuserdata(L, -1), "lua_islightuserdata");
  check(L, lua_touserdata(L, -1) == (void *)&light_key,
	"lua_touserdata lightuserdata");
  check(L, lua_topointer(L, -1) == (void *)&light_key,
	"lua_topointer lightuserdata");
  lua_pop(L, 1);

  fullud = lua_newuserdatauv(L, 8, 0);
  check(L, lua_type(L, -1) == LUA_TUSERDATA, "lua_type userdata");
  check(L, lua_isuserdata(L, -1), "lua_isuserdata full userdata");
  check(L, !lua_islightuserdata(L, -1),
	"lua_islightuserdata full userdata false");
  check(L, lua_touserdata(L, -1) == fullud, "lua_touserdata userdata");
  check(L, lua_topointer(L, -1) != NULL, "lua_topointer userdata");
  lua_pop(L, 1);

  check(L, lua_pushthread(L) == 1, "lua_pushthread main return");
  check(L, lua_tothread(L, -1) == L, "lua_pushthread main value");
  lua_pop(L, 1);

  check(L, lua_checkstack(L, 8), "lua_checkstack grows stack");
  {
    int top = lua_gettop(L);
    lua_pushinteger(L, 77);
    lua_pushvalue(L, -1);
    check_integer(L, -2, 77, "lua_pushvalue source");
    check_integer(L, -1, 77, "lua_pushvalue copy");
    lua_settop(L, top);
    check(L, lua_gettop(L) == top, "lua_settop restore");
  }

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

  lua_pushliteral(L, "Lua");
  lua_pushliteral(L, "5");
  lua_pushliteral(L, ".");
  lua_pushliteral(L, "4");
  lua_concat(L, 4);
  check_string(L, -1, "Lua5.4", "lua_concat strings");
  lua_pop(L, 1);
  lua_concat(L, 0);
  check_string(L, -1, "", "lua_concat zero values");
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

  lua_newtable(L);
  lua_pushinteger(L, 11);
  lua_setfield(L, -2, "a");
  lua_pushinteger(L, 22);
  lua_setfield(L, -2, "b");
  {
    lua_Integer sum = 0;
    int count = 0;
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      int ok = 0;
      sum += lua_tointegerx(L, -1, &ok);
      check(L, ok, "lua_next value integer");
      count++;
      lua_pop(L, 1);
    }
    check(L, count == 2 && sum == 33, "lua_next table traversal");
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

  lua_pushcfunction(L, raise_lua_error);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN, "lua_error status");
  check_string(L, -1, "capi raised error", "lua_error message");
  lua_pop(L, 1);

  close_call_count = 0;
  close_nil_error_count = 0;
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_closeslot(L, -1);
  check(L, lua_isnil(L, -1), "lua_closeslot nils closed slot");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"lua_closeslot calls __close with nil error");
  lua_pop(L, 1);

  lua_pushcfunction(L, mark_false_then_closeslot);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_closeslot rejects ignored false slot");
  check(L, strstr(lua_tostring(L, -1), "no variable to close") != NULL,
	"lua_closeslot ignored false error text");
  lua_pop(L, 1);

  lua_pushcfunction(L, mark_nonclosable_slot);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_toclose rejects non-closable values");
  check(L, strstr(lua_tostring(L, -1), "non-closable") != NULL,
	"lua_toclose non-closable error text");
  lua_pop(L, 1);

  lua_pushcfunction(L, mark_close_twice_same_slot);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_toclose rejects same marked slot");
  check(L, strstr(lua_tostring(L, -1), "below or equal") != NULL,
	"lua_toclose same slot error text");
  lua_pop(L, 1);

  lua_pushcfunction(L, mark_close_below_active_slot);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_toclose rejects below active slot");
  check(L, strstr(lua_tostring(L, -1), "below or equal") != NULL,
	"lua_toclose below active error text");
  lua_pop(L, 1);

  lua_pushcfunction(L, mark_nil_below_active_slot);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_toclose rejects nil below active slot");
  check(L, strstr(lua_tostring(L, -1), "below or equal") != NULL,
	"lua_toclose nil below active error text");
  lua_pop(L, 1);

  lua_pushcfunction(L, closeslot_not_last);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_closeslot rejects non-last slot");
  check(L, strstr(lua_tostring(L, -1), "no variable to close") != NULL,
	"lua_closeslot non-last error text");
  lua_pop(L, 1);

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, closeslot_lifo);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_OK,
	"lua_closeslot accepts last slots in LIFO order");
  check(L, close_call_count == 2 && close_nil_error_count == 2,
	"lua_closeslot LIFO closes both slots");

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, mark_close_then_pop);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_OK,
	"lua_settop closes toclose slot");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"lua_settop close uses nil error");

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, mark_close_then_return);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_OK,
	"C return closes toclose slot");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"C return close uses nil error");

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, mark_close_then_return_self);
  check(L, lua_pcall(L, 0, 1, 0) == LUA_OK,
	"C return keeps closed result slot");
  check(L, lua_istable(L, -1), "C return keeps closed table result");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"C return closes returned slot with nil error");
  lua_pop(L, 1);

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, mark_close_error_then_return);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"C return close error status");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"C return close error uses nil original error");
  check(L, strstr(lua_tostring(L, -1), "capi close boom") != NULL,
	"C return close error text");
  lua_pop(L, 1);

  close_call_count = 0;
  close_body_error_count = 0;
  lua_pushcfunction(L, mark_close_then_error);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua error closes toclose slot");
  check(L, close_call_count == 1 && close_body_error_count == 1,
	"lua error close receives body error");
  check(L, strstr(lua_tostring(L, -1), "capi body boom") != NULL,
	"lua error keeps body error");
  lua_pop(L, 1);

  co = lua_newthread(L);
  check(L, lua_pushthread(co) == 0, "lua_pushthread coroutine return");
  check(L, lua_tothread(co, -1) == co, "lua_pushthread coroutine value");
  lua_pop(co, 1);
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

  lua_pushcfunction(L, record_close);
  lua_setglobal(L, "capi_record_close");
  co = lua_newthread(L);
  close_call_count = 0;
  close_nil_error_count = 0;
  check(L, luaL_loadstring(co,
	"local mt={__close=capi_record_close}; "
	"local x <close> = setmetatable({}, mt); coroutine.yield('paused')") ==
	LUA_OK, "lua_closethread tbc load");
  check(L, lua_resume(co, L, 0, NULL) == LUA_YIELD,
	"lua_closethread tbc yield");
  check(L, lua_closethread(co, L) == LUA_OK,
	"lua_closethread tbc return");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"lua_closethread closes tbc with nil error");
  check(L, lua_status(co) == LUA_OK, "lua_closethread tbc status");
  check(L, lua_gettop(co) == 0, "lua_closethread tbc clears stack");
  lua_pop(L, 1);

  lua_pushcfunction(L, record_close_error);
  lua_setglobal(L, "capi_record_close_error");
  co = lua_newthread(L);
  close_call_count = 0;
  close_nil_error_count = 0;
  check(L, luaL_loadstring(co,
	"local mt={__close=capi_record_close_error}; "
	"local x <close> = setmetatable({}, mt); coroutine.yield('paused')") ==
	LUA_OK, "lua_closethread tbc close-error load");
  check(L, lua_resume(co, L, 0, NULL) == LUA_YIELD,
	"lua_closethread tbc close-error yield");
  check(L, lua_closethread(co, L) == LUA_ERRRUN,
	"lua_closethread tbc close-error return");
  check_string(co, 1, "capi close boom",
	"lua_closethread tbc close-error object");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"lua_closethread reports close error after nil close");
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
  void (*pushresultsize_fn)(luaL_Buffer *, size_t) = luaL_pushresultsize;
  char *(*buffinitsize_fn)(lua_State *, luaL_Buffer *, size_t) =
    luaL_buffinitsize;
  const char *gs;
  int status;
  int ref;
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

  lua_pushcfunction(L, checkoption_arg);
  status = lua_pcall(L, 0, 1, 0);
  check(L, status == LUA_OK, "luaL_checkoption default status");
  check_integer(L, -1, 1, "luaL_checkoption default index");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkoption_arg);
  lua_pushliteral(L, "gamma");
  status = lua_pcall(L, 1, 1, 0);
  check(L, status == LUA_OK, "luaL_checkoption explicit status");
  check_integer(L, -1, 2, "luaL_checkoption explicit index");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkoption_arg);
  lua_pushliteral(L, "delta");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkoption rejects option");
  check(L, strstr(lua_tostring(L, -1), "invalid option") != NULL,
	"luaL_checkoption error");
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

  gs = luaL_gsub(L, "a-b-a", "-", "_");
  check(L, gs != NULL && strcmp(gs, "a_b_a") == 0,
	"luaL_gsub return value");
  check_string(L, -1, "a_b_a", "luaL_gsub pushes result");
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushliteral(L, "ref-value");
  ref = luaL_ref(L, -2);
  check(L, ref > 0, "luaL_ref positive ref");
  lua_rawgeti(L, -1, ref);
  check_string(L, -1, "ref-value", "luaL_ref stored value");
  lua_pop(L, 1);
  luaL_unref(L, -1, ref);
  lua_rawgeti(L, -1, ref);
  check(L, lua_isnil(L, -1), "luaL_unref clears ref");
  lua_pop(L, 1);
  lua_pushnil(L);
  check(L, luaL_ref(L, -2) == LUA_REFNIL, "luaL_ref nil sentinel");
  luaL_unref(L, -1, LUA_NOREF);
  luaL_unref(L, -1, LUA_REFNIL);
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

  lua_newtable(L);
  check(L, strcmp(luaL_typename(L, -1), "table") == 0,
	"luaL_typename table");
  lua_newtable(L);
  lua_pushliteral(L, "CapiMeta");
  lua_setfield(L, -2, "__name");
  lua_pushcfunction(L, capi_tostring_meta);
  lua_setfield(L, -2, "__tostring");
  lua_setmetatable(L, -2);
  check(L, luaL_getmetafield(L, -1, "__name") == 1,
	"luaL_getmetafield returns field");
  check_string(L, -1, "CapiMeta", "luaL_getmetafield value");
  lua_pop(L, 1);
  {
    int top = lua_gettop(L);
    check(L, luaL_getmetafield(L, -1, "__missing") == 0,
	  "luaL_getmetafield missing");
    check(L, lua_gettop(L) == top, "luaL_getmetafield missing stack");
  }
  check(L, luaL_callmeta(L, -1, "__tostring") == 1,
	"luaL_callmeta calls metamethod");
  check_string(L, -1, "meta tostring", "luaL_callmeta result");
  lua_pop(L, 2);

  luaL_where(L, 0);
  check(L, lua_isstring(L, -1), "luaL_where pushes string");
  lua_pop(L, 1);
  lua_pushcfunction(L, laux_error_arg);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_error status");
  check(L, strstr(lua_tostring(L, -1), "laux boom") != NULL,
	"luaL_error message");
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
  pushresultsize_fn(&b, 2);
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
    p = buffinitsize_fn(L, &b, big);
    memset(p, 'q', big);
    pushresultsize_fn(&b, big);
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
  int status;
  lua_Number version;
  lua_Debug ar;

  version = lua_version(L);
  check(L, version == (lua_Number)LUA_VERSION_NUM,
	"lua_version returns numeric Lua 5.4 version");
  version = lua_version_value_sig(L);
  check(L, version == (lua_Number)LUA_VERSION_NUM,
	"lua_version function pointer returns Lua 5.4 version");
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

  hook_call_ftransfer = hook_call_ntransfer = -1;
  hook_ret_ftransfer = hook_ret_ntransfer = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local function capi_transfer_probe(a, b) return a + b, a - b end\n"
    "local x, y = capi_transfer_probe(3, 1); return x, y");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo hook transfer setup");
  check_integer(L, -2, 4, "lua_getinfo hook transfer result 1");
  check_integer(L, -1, 2, "lua_getinfo hook transfer result 2");
  lua_pop(L, 2);
  check(L, hook_call_ftransfer == 1 && hook_call_ntransfer == 2,
	"lua_getinfo call hook transfer fields");
  check(L, hook_ret_ftransfer == 3 && hook_ret_ntransfer == 2,
	"lua_getinfo return hook transfer fields");

  hook_vararg_call_ftransfer = hook_vararg_call_ntransfer = -1;
  hook_vararg_ret_ftransfer = hook_vararg_ret_ntransfer = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local function capi_transfer_vararg(a, ...) return a, ... end\n"
    "local a, b, c = capi_transfer_vararg(1, 2, 3); return a, b, c");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo vararg hook transfer setup");
  check_integer(L, -3, 1, "lua_getinfo vararg transfer result 1");
  check_integer(L, -2, 2, "lua_getinfo vararg transfer result 2");
  check_integer(L, -1, 3, "lua_getinfo vararg transfer result 3");
  lua_pop(L, 3);
  check(L, hook_vararg_call_ftransfer == 1 &&
	hook_vararg_call_ntransfer == 1,
	"lua_getinfo vararg call hook transfer fields");
  check(L, hook_vararg_ret_ftransfer == 2 &&
	hook_vararg_ret_ntransfer == 3,
	"lua_getinfo vararg return hook transfer fields");

  hook_tail_ftransfer = hook_tail_ntransfer = -1;
  hook_tail_istailcall = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local function capi_tail_target(a, b) return a + b end\n"
    "local function capi_tail_caller(a, b) return capi_tail_target(a, b) end\n"
    "local x = capi_tail_caller(2, 3); return x");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo tail hook transfer setup");
  check_integer(L, -1, 5, "lua_getinfo tail hook transfer result");
  lua_pop(L, 1);
  check(L, hook_tail_istailcall == 1 &&
	hook_tail_ftransfer == 1 && hook_tail_ntransfer == 2,
	"lua_getinfo tail call hook transfer fields");

  hook_tail_vararg_call_ftransfer = hook_tail_vararg_call_ntransfer = -1;
  hook_tail_vararg_ret_ftransfer = hook_tail_vararg_ret_ntransfer = -1;
  hook_tail_vararg_ret_istailcall = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local function capi_tail_vararg_target(...) return ... end\n"
    "local function capi_tail_vararg_caller(...) "
      "return capi_tail_vararg_target(...) end\n"
    "local a, b, c = capi_tail_vararg_caller(1, 2, 3); return a, b, c");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo vararg tail hook setup");
  check_integer(L, -3, 1, "lua_getinfo vararg tail result 1");
  check_integer(L, -2, 2, "lua_getinfo vararg tail result 2");
  check_integer(L, -1, 3, "lua_getinfo vararg tail result 3");
  lua_pop(L, 3);
  check(L, hook_tail_vararg_call_ftransfer == 0 &&
	hook_tail_vararg_call_ntransfer == 0,
	"lua_getinfo vararg tail call hook transfer fields");
  check(L, hook_tail_vararg_ret_istailcall == 1 &&
	hook_tail_vararg_ret_ftransfer == 1 &&
	hook_tail_vararg_ret_ntransfer == 3,
	"lua_getinfo vararg tail return hook transfer fields");

  lua_pushcfunction(L, capi_transfer_cfunc);
  lua_setglobal(L, "capi_transfer_cfunc");
  hook_c_call_ftransfer = hook_c_call_ntransfer = -1;
  hook_c_ret_ftransfer = hook_c_ret_ntransfer = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local x = capi_transfer_cfunc(1, 2, 3); return x");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo C hook transfer setup");
  check_integer(L, -1, 6, "lua_getinfo C hook transfer result");
  lua_pop(L, 1);
  check(L, hook_c_call_ftransfer == 1 && hook_c_call_ntransfer == 3,
	"lua_getinfo C call hook transfer fields");
  check(L, hook_c_ret_ftransfer == 4 && hook_c_ret_ntransfer == 1,
	"lua_getinfo C return hook transfer fields");

  hook_c_call_ftransfer = hook_c_call_ntransfer = -1;
  hook_c_ret_ftransfer = hook_c_ret_ntransfer = -1;
  hook_tail_istailcall = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local function capi_tail_c_position(a, b, c) "
      "return capi_transfer_cfunc(a, b, c) end\n"
    "local x = capi_tail_c_position(1, 2, 3); return x");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo tail-position C hook setup");
  check_integer(L, -1, 6, "lua_getinfo tail-position C result");
  lua_pop(L, 1);
  check(L, hook_tail_istailcall < 0,
	"lua_getinfo tail-position C must not report Lua tail hook");
  check(L, hook_c_call_ftransfer == 1 && hook_c_call_ntransfer == 3,
	"lua_getinfo tail-position C call transfer fields");
  check(L, hook_c_ret_ftransfer == 4 && hook_c_ret_ntransfer == 1,
	"lua_getinfo tail-position C return transfer fields");
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
