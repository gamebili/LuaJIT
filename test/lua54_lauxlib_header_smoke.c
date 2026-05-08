/*
** Header-surface smoke test for lauxlib.h alone in Lua 5.4 compatibility mode.
*/

#include "lauxlib.h"

#ifdef LUAJIT_EXTERNAL_LUA54
#error "LUAJIT_EXTERNAL_LUA54 is a private header selector and must not leak"
#endif

#ifndef LUA_GNAME
#error "Lua 5.4 lauxlib.h must expose LUA_GNAME"
#endif

#ifndef LUA_FILEHANDLE
#error "Lua 5.4 lauxlib.h must expose LUA_FILEHANDLE"
#endif

#ifndef LUA_LOADED_TABLE
#error "Lua 5.4 lauxlib.h must expose LUA_LOADED_TABLE"
#endif

#ifndef LUA_PRELOAD_TABLE
#error "Lua 5.4 lauxlib.h must expose LUA_PRELOAD_TABLE"
#endif

#ifndef lua_writestring
#error "Lua 5.4 lauxlib.h must expose lua_writestring"
#endif

#ifndef lua_writeline
#error "Lua 5.4 lauxlib.h must expose lua_writeline"
#endif

#ifndef lua_writestringerror
#error "Lua 5.4 lauxlib.h must expose lua_writestringerror"
#endif

#ifndef lua_assert
#error "Lua 5.4 lauxlib.h must expose lua_assert"
#endif

typedef char lua54_lauxlib_errfile[
  LUA_ERRFILE == (LUA_ERRERR + 1) ? 1 : -1
];
typedef char lua54_lauxlib_noref[LUA_NOREF == -2 ? 1 : -1];
typedef char lua54_lauxlib_refnil[LUA_REFNIL == -1 ? 1 : -1];
typedef char lua54_lauxlib_numsizes[
  LUAL_NUMSIZES == (sizeof(lua_Integer) * 16 + sizeof(lua_Number)) ? 1 : -1
];
typedef char lua54_lauxlib_buffersize[
  LUAL_BUFFERSIZE == (int)(16 * sizeof(void *) * sizeof(lua_Number)) ? 1 : -1
];
typedef char lua54_lauxlib_buffer_field_order_b[
  offsetof(luaL_Buffer, b) < offsetof(luaL_Buffer, size) ? 1 : -1
];
typedef char lua54_lauxlib_buffer_field_order_size[
  offsetof(luaL_Buffer, size) < offsetof(luaL_Buffer, n) ? 1 : -1
];
typedef char lua54_lauxlib_buffer_field_order_n[
  offsetof(luaL_Buffer, n) < offsetof(luaL_Buffer, L) ? 1 : -1
];
typedef char lua54_lauxlib_buffer_field_order_L[
  offsetof(luaL_Buffer, L) < offsetof(luaL_Buffer, init) ? 1 : -1
];
typedef char lua54_lauxlib_stream_field_order[
  offsetof(luaL_Stream, f) < offsetof(luaL_Stream, closef) ? 1 : -1
];

#if defined(__GNUC__) || defined(__clang__)
#define LUA54_CHECK_LAUX_SIG(name, sig) \
  typedef char lua54_laux_sig_##name[ \
    __builtin_types_compatible_p(__typeof__(&(name)), sig) ? 1 : -1 \
  ]

typedef void (*lua54_checkversion_sig)(lua_State *, lua_Number, size_t);
typedef int (*lua54_meta_sig)(lua_State *, int, const char *);
typedef const char *(*lua54_tolstring_sig)(lua_State *, int, size_t *);
typedef int (*lua54_argerror_sig)(lua_State *, int, const char *);
typedef const char *(*lua54_checklstring_sig)(lua_State *, int, size_t *);
typedef const char *(*lua54_optlstring_sig)(lua_State *, int, const char *, size_t *);
typedef lua_Number (*lua54_checknumber_sig)(lua_State *, int);
typedef lua_Number (*lua54_optnumber_sig)(lua_State *, int, lua_Number);
typedef lua_Integer (*lua54_checkinteger_sig)(lua_State *, int);
typedef lua_Integer (*lua54_optinteger_sig)(lua_State *, int, lua_Integer);
typedef void (*lua54_checkstack_sig)(lua_State *, int, const char *);
typedef void (*lua54_checktype_sig)(lua_State *, int, int);
typedef void (*lua54_checkany_sig)(lua_State *, int);
typedef int (*lua54_newmetatable_sig)(lua_State *, const char *);
typedef void (*lua54_setmetatable_sig)(lua_State *, const char *);
typedef void *(*lua54_udata_sig)(lua_State *, int, const char *);
typedef void (*lua54_where_sig)(lua_State *, int);
typedef int (*lua54_error_sig)(lua_State *, const char *, ...);
typedef int (*lua54_checkoption_sig)(lua_State *, int, const char *, const char *const[]);
typedef int (*lua54_fileresult_sig)(lua_State *, int, const char *);
typedef int (*lua54_execresult_sig)(lua_State *, int);
typedef int (*lua54_ref_sig)(lua_State *, int);
typedef void (*lua54_unref_sig)(lua_State *, int, int);
typedef int (*lua54_loadfilex_sig)(lua_State *, const char *, const char *);
typedef int (*lua54_loadbufferx_sig)(lua_State *, const char *, size_t, const char *, const char *);
typedef int (*lua54_loadstring_sig)(lua_State *, const char *);
typedef lua_State *(*lua54_newstate_sig)(void);
typedef lua_Integer (*lua54_len_sig)(lua_State *, int);
typedef void (*lua54_addgsub_sig)(luaL_Buffer *, const char *, const char *, const char *);
typedef const char *(*lua54_gsub_sig)(lua_State *, const char *, const char *, const char *);
typedef void (*lua54_setfuncs_sig)(lua_State *, const luaL_Reg *, int);
typedef int (*lua54_getsubtable_sig)(lua_State *, int, const char *);
typedef void (*lua54_traceback_sig)(lua_State *, lua_State *, const char *, int);
typedef void (*lua54_requiref_sig)(lua_State *, const char *, lua_CFunction, int);
typedef void (*lua54_buffinit_sig)(lua_State *, luaL_Buffer *);
typedef char *(*lua54_prepbuffsize_sig)(luaL_Buffer *, size_t);
typedef void (*lua54_addlstring_sig)(luaL_Buffer *, const char *, size_t);
typedef void (*lua54_addstring_sig)(luaL_Buffer *, const char *);
typedef void (*lua54_addvalue_sig)(luaL_Buffer *);
typedef void (*lua54_pushresult_sig)(luaL_Buffer *);
typedef void (*lua54_pushresultsize_sig)(luaL_Buffer *, size_t);
typedef char *(*lua54_buffinitsize_sig)(lua_State *, luaL_Buffer *, size_t);

typedef char lua54_buffer_b_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Buffer *)0)->b), char *) ? 1 : -1
];
typedef char lua54_buffer_size_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Buffer *)0)->size), size_t) ? 1 : -1
];
typedef char lua54_buffer_n_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Buffer *)0)->n), size_t) ? 1 : -1
];
typedef char lua54_buffer_L_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Buffer *)0)->L), lua_State *) ? 1 : -1
];
typedef char lua54_buffer_init_n_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Buffer *)0)->init.n), lua_Number) ? 1 : -1
];
typedef char lua54_buffer_init_u_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Buffer *)0)->init.u), double) ? 1 : -1
];
typedef char lua54_buffer_init_s_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Buffer *)0)->init.s), void *) ? 1 : -1
];
typedef char lua54_buffer_init_i_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Buffer *)0)->init.i), lua_Integer) ? 1 : -1
];
typedef char lua54_buffer_init_l_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Buffer *)0)->init.l), long) ? 1 : -1
];
typedef char lua54_buffer_init_b_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Buffer *)0)->init.b),
                               char [LUAL_BUFFERSIZE]) ? 1 : -1
];
typedef char lua54_stream_f_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Stream *)0)->f), FILE *) ? 1 : -1
];
typedef char lua54_stream_closef_type[
  __builtin_types_compatible_p(__typeof__(((luaL_Stream *)0)->closef),
                               lua_CFunction) ? 1 : -1
];

LUA54_CHECK_LAUX_SIG(luaL_checkversion_, lua54_checkversion_sig);
LUA54_CHECK_LAUX_SIG(luaL_getmetafield, lua54_meta_sig);
LUA54_CHECK_LAUX_SIG(luaL_callmeta, lua54_meta_sig);
LUA54_CHECK_LAUX_SIG(luaL_tolstring, lua54_tolstring_sig);
LUA54_CHECK_LAUX_SIG(luaL_argerror, lua54_argerror_sig);
LUA54_CHECK_LAUX_SIG(luaL_typeerror, lua54_argerror_sig);
LUA54_CHECK_LAUX_SIG(luaL_checklstring, lua54_checklstring_sig);
LUA54_CHECK_LAUX_SIG(luaL_optlstring, lua54_optlstring_sig);
LUA54_CHECK_LAUX_SIG(luaL_checknumber, lua54_checknumber_sig);
LUA54_CHECK_LAUX_SIG(luaL_optnumber, lua54_optnumber_sig);
LUA54_CHECK_LAUX_SIG(luaL_checkinteger, lua54_checkinteger_sig);
LUA54_CHECK_LAUX_SIG(luaL_optinteger, lua54_optinteger_sig);
LUA54_CHECK_LAUX_SIG(luaL_checkstack, lua54_checkstack_sig);
LUA54_CHECK_LAUX_SIG(luaL_checktype, lua54_checktype_sig);
LUA54_CHECK_LAUX_SIG(luaL_checkany, lua54_checkany_sig);
LUA54_CHECK_LAUX_SIG(luaL_newmetatable, lua54_newmetatable_sig);
LUA54_CHECK_LAUX_SIG(luaL_setmetatable, lua54_setmetatable_sig);
LUA54_CHECK_LAUX_SIG(luaL_testudata, lua54_udata_sig);
LUA54_CHECK_LAUX_SIG(luaL_checkudata, lua54_udata_sig);
LUA54_CHECK_LAUX_SIG(luaL_where, lua54_where_sig);
LUA54_CHECK_LAUX_SIG(luaL_error, lua54_error_sig);
LUA54_CHECK_LAUX_SIG(luaL_checkoption, lua54_checkoption_sig);
LUA54_CHECK_LAUX_SIG(luaL_fileresult, lua54_fileresult_sig);
LUA54_CHECK_LAUX_SIG(luaL_execresult, lua54_execresult_sig);
LUA54_CHECK_LAUX_SIG(luaL_ref, lua54_ref_sig);
LUA54_CHECK_LAUX_SIG(luaL_unref, lua54_unref_sig);
LUA54_CHECK_LAUX_SIG(luaL_loadfilex, lua54_loadfilex_sig);
LUA54_CHECK_LAUX_SIG(luaL_loadbufferx, lua54_loadbufferx_sig);
LUA54_CHECK_LAUX_SIG(luaL_loadstring, lua54_loadstring_sig);
LUA54_CHECK_LAUX_SIG(luaL_newstate, lua54_newstate_sig);
LUA54_CHECK_LAUX_SIG(luaL_len, lua54_len_sig);
LUA54_CHECK_LAUX_SIG(luaL_addgsub, lua54_addgsub_sig);
LUA54_CHECK_LAUX_SIG(luaL_gsub, lua54_gsub_sig);
LUA54_CHECK_LAUX_SIG(luaL_setfuncs, lua54_setfuncs_sig);
LUA54_CHECK_LAUX_SIG(luaL_getsubtable, lua54_getsubtable_sig);
LUA54_CHECK_LAUX_SIG(luaL_traceback, lua54_traceback_sig);
LUA54_CHECK_LAUX_SIG(luaL_requiref, lua54_requiref_sig);
LUA54_CHECK_LAUX_SIG(luaL_buffinit, lua54_buffinit_sig);
LUA54_CHECK_LAUX_SIG(luaL_prepbuffsize, lua54_prepbuffsize_sig);
LUA54_CHECK_LAUX_SIG(luaL_addlstring, lua54_addlstring_sig);
LUA54_CHECK_LAUX_SIG(luaL_addstring, lua54_addstring_sig);
LUA54_CHECK_LAUX_SIG(luaL_addvalue, lua54_addvalue_sig);
LUA54_CHECK_LAUX_SIG(luaL_pushresult, lua54_pushresult_sig);
LUA54_CHECK_LAUX_SIG(luaL_pushresultsize, lua54_pushresultsize_sig);
LUA54_CHECK_LAUX_SIG(luaL_buffinitsize, lua54_buffinitsize_sig);
#endif

int main(void)
{
  return 0;
}
