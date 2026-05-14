/*
** Auxiliary library for the Lua/C API.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major parts taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#define lib_aux_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_state.h"
#include "lj_trace.h"
#include "lj_lib.h"
#include "lj_vmevent.h"

#if LJ_TARGET_POSIX
#include <sys/wait.h>
#endif

/* -- I/O error handling -------------------------------------------------- */

LUALIB_API int luaL_fileresult(lua_State *L, int stat, const char *fname)
{
  if (stat) {
    setboolV(L->top++, 1);
    return 1;
  } else {
    int en = errno;  /* Lua API calls may change this value. */
    const char *msg = (LJ_54 && en == 0) ? "(no extra info)" : strerror(en);
    setnilV(L->top++);
    if (fname)
      lua_pushfstring(L, "%s: %s", fname, msg);
    else
      lua_pushfstring(L, "%s", msg);
    setintV(L->top++, en);
    lj_trace_abort(G(L));
    return 3;
  }
}

LUALIB_API int luaL_execresult(lua_State *L, int stat)
{
#if LJ_54
  if (stat != 0 && errno != 0)
    /* Lua 5.4 treats a non-zero process status with errno set as a system
    ** error, not as a normal "exit" result tuple.
    */
    return luaL_fileresult(L, 0, NULL);
#endif
  if (stat != -1) {
#if LJ_TARGET_POSIX
    if (WIFSIGNALED(stat)) {
      stat = WTERMSIG(stat);
      setnilV(L->top++);
      lua_pushliteral(L, "signal");
    } else {
      if (WIFEXITED(stat))
	stat = WEXITSTATUS(stat);
      if (stat == 0)
	setboolV(L->top++, 1);
      else
	setnilV(L->top++);
      lua_pushliteral(L, "exit");
    }
#else
    if (stat == 0)
      setboolV(L->top++, 1);
    else
      setnilV(L->top++);
    lua_pushliteral(L, "exit");
#endif
    setintV(L->top++, stat);
    return 3;
  }
  return luaL_fileresult(L, 0, NULL);
}

/* -- Module registration ------------------------------------------------- */

LUALIB_API const char *luaL_findtable(lua_State *L, int idx,
				      const char *fname, int szhint)
{
  const char *e;
  lua_pushvalue(L, idx);
  do {
    e = strchr(fname, '.');
    if (e == NULL) e = fname + strlen(fname);
    lua_pushlstring(L, fname, (size_t)(e - fname));
    lua_rawget(L, -2);
    if (lua_isnil(L, -1)) {  /* no such field? */
      lua_pop(L, 1);  /* remove this nil */
      lua_createtable(L, 0, (*e == '.' ? 1 : szhint)); /* new table for field */
      lua_pushlstring(L, fname, (size_t)(e - fname));
      lua_pushvalue(L, -2);
      lua_settable(L, -4);  /* set new table into field */
    } else if (!lua_istable(L, -1)) {  /* field has a non-table value? */
      lua_pop(L, 2);  /* remove table and value */
      return fname;  /* return problematic part of the name */
    }
    lua_remove(L, -2);  /* remove previous table */
    fname = e + 1;
  } while (*e == '.');
  return NULL;
}

static int libsize(const luaL_Reg *l)
{
  int size = 0;
  for (; l && l->name; l++) size++;
  return size;
}

LUALIB_API void luaL_pushmodule(lua_State *L, const char *modname, int sizehint)
{
  luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 16);
  lua_getfield(L, -1, modname);
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    if (luaL_findtable(L, LUA_GLOBALSINDEX, modname, sizehint) != NULL)
      lj_err_callerv(L, LJ_ERR_BADMODN, modname);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, modname);  /* _LOADED[modname] = new table. */
  }
  lua_remove(L, -2);  /* Remove _LOADED table. */
}

LUALIB_API void luaL_openlib(lua_State *L, const char *libname,
			     const luaL_Reg *l, int nup)
{
  lj_lib_checkfpu(L);
  if (libname) {
    luaL_pushmodule(L, libname, libsize(l));
    lua_insert(L, -(nup + 1));  /* Move module table below upvalues. */
  }
  if (l)
    luaL_setfuncs(L, l, nup);
  else
    lua_pop(L, nup);  /* Remove upvalues. */
}

LUALIB_API void luaL_register(lua_State *L, const char *libname,
			      const luaL_Reg *l)
{
  luaL_openlib(L, libname, l, 0);
}

LUALIB_API void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup)
{
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name; l++) {
    if (LJ_54 && l->func == NULL) {
      /* Lua 5.4 uses NULL luaL_Reg entries as false placeholders; do not
      ** consume or copy the shared upvalues for those non-functions.
      */
      lua_pushboolean(L, 0);
    } else {
      int i;
      for (i = 0; i < nup; i++)  /* Copy upvalues to the top. */
	lua_pushvalue(L, -nup);
      lua_pushcclosure(L, l->func, nup);
    }
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* Remove upvalues. */
}

LUALIB_API int luaL_getsubtable(lua_State *L, int idx, const char *fname)
{
  idx = lua_absindex(L, idx);
  lua_getfield(L, idx, fname);
  if (lua_istable(L, -1))
    return 1;
  lua_pop(L, 1);
  lua_newtable(L);
  lua_pushvalue(L, -1);
  lua_setfield(L, idx, fname);
  return 0;
}

LUALIB_API void luaL_requiref(lua_State *L, const char *modname,
			      lua_CFunction openf, int glb)
{
  luaL_getsubtable(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, modname);
  if (!lua_toboolean(L, -1)) {
    lua_pop(L, 1);
    lua_pushcfunction(L, openf);
    lua_pushstring(L, modname);
    lua_call(L, 1, 1);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, modname);
  }
  lua_remove(L, -2);
  if (glb) {
    lua_pushvalue(L, -1);
    lua_setglobal(L, modname);
  }
}

LUALIB_API void luaL_addgsub(luaL_Buffer *B, const char *s,
			     const char *p, const char *r)
{
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    luaL_addlstring(B, s, (size_t)(wild - s));  /* push prefix */
    luaL_addstring(B, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after `p' */
  }
  luaL_addstring(B, s);  /* push last suffix */
}

LUALIB_API const char *luaL_gsub(lua_State *L, const char *s,
				 const char *p, const char *r)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addgsub(&b, s, p, r);
  luaL_pushresult(&b);
  return lua_tostring(L, -1);
}

/* -- Buffer handling ----------------------------------------------------- */

#if LJ_54

#define bufffree(B)	((B)->size - (B)->n)

typedef struct UBox {
  void *box;
  size_t bsize;
} UBox;

static void *resizebox(lua_State *L, int idx, size_t newsize)
{
  void *ud;
  lua_Alloc allocf = lua_getallocf(L, &ud);
  UBox *box = (UBox *)lua_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (temp == NULL && newsize > 0)
    lj_err_mem(L);
  box->box = temp;
  box->bsize = newsize;
  return temp;
}

static int boxgc(lua_State *L)
{
  resizebox(L, 1, 0);
  return 0;
}

static const luaL_Reg boxmt[] = {
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};

static void newbox(lua_State *L)
{
  UBox *box = (UBox *)lua_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (luaL_newmetatable(L, "_UBOX*"))
    luaL_setfuncs(L, boxmt, 0);
  lua_setmetatable(L, -2);
}

#define buffonstack(B)	((B)->b != (B)->init.b)

static size_t newbuffsize(luaL_Buffer *B, size_t sz)
{
  size_t newsize = (B->size / 2) * 3;
  if (((size_t)~(size_t)0) - sz < B->n)
    lj_err_mem(B->L);
  if (newsize < B->n + sz)
    newsize = B->n + sz;
  return newsize;
}

static char *prepbuffsize(luaL_Buffer *B, size_t sz, int boxidx)
{
  if (bufffree(B) >= sz) {
    return B->b + B->n;
  } else {
    lua_State *L = B->L;
    char *newbuf;
    size_t newsize = newbuffsize(B, sz);
    if (buffonstack(B)) {
      newbuf = (char *)resizebox(L, boxidx, newsize);
    } else {
      /* Lua 5.4 keeps a growable buffer inside a to-be-closed userdata box.
      ** If a C API caller errors before luaL_pushresult(), normal unwind or
      ** GC still releases the side allocation instead of leaking it.
      */
      lua_remove(L, boxidx);
      newbox(L);
      lua_insert(L, boxidx);
      lua_toclose(L, boxidx);
      newbuf = (char *)resizebox(L, boxidx, newsize);
      if (B->n)
	memcpy(newbuf, B->b, B->n);
    }
    B->b = newbuf;
    B->size = newsize;
    return newbuf + B->n;
  }
}

LUALIB_API char *luaL_prepbuffsize(luaL_Buffer *B, size_t sz)
{
  return prepbuffsize(B, sz, -1);
}

LUALIB_API char *luaL_prepbuffer(luaL_Buffer *B)
{
  return luaL_prepbuffsize(B, LUAL_BUFFERSIZE);
}

LUALIB_API void luaL_addlstring(luaL_Buffer *B, const char *s, size_t l)
{
  if (l > 0) {
    char *p = prepbuffsize(B, l, -1);
    memcpy(p, s, l);
    B->n += l;
  }
}

LUALIB_API void luaL_addstring(luaL_Buffer *B, const char *s)
{
  luaL_addlstring(B, s, strlen(s));
}

LUALIB_API void luaL_pushresult(luaL_Buffer *B)
{
  lua_State *L = B->L;
  lua_pushlstring(L, B->b, B->n);
  if (B->b != B->init.b)
    lua_closeslot(L, -2);
  lua_remove(L, -2);
}

LUALIB_API void luaL_pushresultsize(luaL_Buffer *B, size_t sz)
{
  luaL_addsize(B, sz);
  luaL_pushresult(B);
}

LUALIB_API void luaL_addvalue(luaL_Buffer *B)
{
  lua_State *L = B->L;
  size_t vl;
  const char *s = lua_tolstring(L, -1, &vl);
  char *p = prepbuffsize(B, vl, -2);
  memcpy(p, s, vl);
  B->n += vl;
  lua_pop(L, 1);
}

LUALIB_API void luaL_buffinit(lua_State *L, luaL_Buffer *B)
{
  B->L = L;
  B->b = B->init.b;
  B->size = LUAL_BUFFERSIZE;
  B->n = 0;
  lua_pushlightuserdata(L, (void *)B);
}

LUALIB_API char *luaL_buffinitsize(lua_State *L, luaL_Buffer *B, size_t sz)
{
  luaL_buffinit(L, B);
  return luaL_prepbuffsize(B, sz);
}

#else

#define bufflen(B)	((size_t)((B)->p - (B)->buffer))
#define bufffree(B)	((size_t)(LUAL_BUFFERSIZE - bufflen(B)))

static int emptybuffer(luaL_Buffer *B)
{
  size_t l = bufflen(B);
  if (l == 0)
    return 0;  /* put nothing on stack */
  lua_pushlstring(B->L, B->buffer, l);
  B->p = B->buffer;
  B->lvl++;
  return 1;
}

static void adjuststack(luaL_Buffer *B)
{
  if (B->lvl > 1) {
    lua_State *L = B->L;
    int toget = 1;  /* number of levels to concat */
    size_t toplen = lua_strlen(L, -1);
    do {
      size_t l = lua_strlen(L, -(toget+1));
      if (!(B->lvl - toget + 1 >= LUA_MINSTACK/2 || toplen > l))
	break;
      toplen += l;
      toget++;
    } while (toget < B->lvl);
    lua_concat(L, toget);
    B->lvl = B->lvl - toget + 1;
  }
}

LUALIB_API char *luaL_prepbuffer(luaL_Buffer *B)
{
  if (emptybuffer(B))
    adjuststack(B);
  return B->buffer;
}

LUALIB_API void luaL_addlstring(luaL_Buffer *B, const char *s, size_t l)
{
  if (l <= bufffree(B)) {
    memcpy(B->p, s, l);
    B->p += l;
  } else {
    emptybuffer(B);
    lua_pushlstring(B->L, s, l);
    B->lvl++;
    adjuststack(B);
  }
}

LUALIB_API void luaL_addstring(luaL_Buffer *B, const char *s)
{
  luaL_addlstring(B, s, strlen(s));
}

LUALIB_API void luaL_pushresult(luaL_Buffer *B)
{
  emptybuffer(B);
  lua_concat(B->L, B->lvl);
  B->lvl = 1;
}

LUALIB_API void luaL_addvalue(luaL_Buffer *B)
{
  lua_State *L = B->L;
  size_t vl;
  const char *s = lua_tolstring(L, -1, &vl);
  if (vl <= bufffree(B)) {  /* fit into buffer? */
    memcpy(B->p, s, vl);  /* put it there */
    B->p += vl;
    lua_pop(L, 1);  /* remove from stack */
  } else {
    if (emptybuffer(B))
      lua_insert(L, -2);  /* put buffer before new value */
    B->lvl++;  /* add new value into B stack */
    adjuststack(B);
  }
}

LUALIB_API void luaL_buffinit(lua_State *L, luaL_Buffer *B)
{
  B->L = L;
  B->p = B->buffer;
  B->lvl = 0;
}

#endif

/* -- Lua 5.4 auxiliary compatibility ------------------------------------ */

#if LJ_54
static void luaL_fmtversion54(char *buf, size_t sz, lua_Number n)
{
  size_t i;
  if (sz == 0)
    return;
  snprintf(buf, sz, "%.14g", (double)n);
  buf[sz-1] = '\0';
  for (i = 0; buf[i] != '\0'; i++)
    if (buf[i] == '.' || buf[i] == 'e' || buf[i] == 'E')
      return;
  if (i + 2 < sz) {
    /* Lua 5.4 formats version numbers through lua_Number tostring, so an
    ** integral float is still reported as "504.0", not as integer "504".
    */
    buf[i++] = '.';
    buf[i++] = '0';
    buf[i] = '\0';
  }
}
#endif

LUALIB_API void luaL_checkversion_(lua_State *L, lua_Number ver, size_t sz)
{
  lua_Number v = *lua_version(L);
  /* Lua 5.4 modules call this at open time; fail loudly instead of letting
  ** mismatched headers or numeric ABIs corrupt values later.
  */
  if (sz != LUAL_NUMSIZES)
    luaL_error(L, "core and library have incompatible numeric types");
  if (v != ver) {
#if LJ_54
    char need[64], have[64];
    luaL_fmtversion54(need, sizeof(need), ver);
    luaL_fmtversion54(have, sizeof(have), v);
    luaL_error(L, "version mismatch: app. needs %s, Lua core provides %s",
	       need, have);
#else
    luaL_error(L, "version mismatch: app. needs %d, Lua core provides %d",
	       (int)ver, (int)v);
#endif
  }
}

LUALIB_API void luaL_pushfail(lua_State *L)
{
  lua_pushnil(L);
}

LUALIB_API lua_Integer luaL_len(lua_State *L, int idx)
{
  int isnum = 0;
  lua_Integer len;
  lua_len(L, idx);
  len = lua_tointegerx(L, -1, &isnum);
  if (!isnum)
    luaL_error(L, "object length is not an integer");
  lua_pop(L, 1);
  return len;
}

LUALIB_API int luaL_typeerror(lua_State *L, int narg, const char *tname)
{
  int idx = lua_absindex(L, narg);
  const char *typearg;
  if (luaL_getmetafield(L, idx, "__name")) {
    typearg = lua_tostring(L, -1);
    if (typearg == NULL) {
      lua_pop(L, 1);
      typearg = luaL_typename(L, idx);
    }
#if LJ_54
  } else if (lua_type(L, idx) == LUA_TLIGHTUSERDATA) {
    /* Lua 5.4 keeps lua_typename() at "userdata", but argument errors make
    ** the light/full distinction explicit when no string __name overrides it.
    */
    typearg = "light userdata";
#endif
  } else {
    typearg = luaL_typename(L, idx);
  }
  return luaL_argerror(L, narg,
		       lua_pushfstring(L, "%s expected, got %s",
				       tname, typearg));
}

LUALIB_API void luaL_argexpected(lua_State *L, int cond, int arg,
				 const char *tname)
{
  if (!cond)
    luaL_typeerror(L, arg, tname);
}

LUALIB_API const char *luaL_tolstring(lua_State *L, int idx, size_t *len)
{
  idx = lua_absindex(L, idx);
  if (luaL_callmeta(L, idx, "__tostring")) {
    if (!lua_isstring(L, -1))
      luaL_error(L, "'__tostring' must return a string");
  } else {
    int t = lua_type(L, idx);
    switch (t) {
    case LUA_TNUMBER:
    case LUA_TSTRING:
      lua_pushvalue(L, idx);
      break;
    case LUA_TBOOLEAN:
      lua_pushstring(L, lua_toboolean(L, idx) ? "true" : "false");
      break;
    case LUA_TNIL:
      lua_pushliteral(L, "nil");
      break;
    default: {
      const char *kind = luaL_typename(L, idx);
#if LJ_54
      int tt = luaL_getmetafield(L, idx, "__name");
      if (tt) {
	if (tt == LUA_TSTRING)
	  kind = lua_tostring(L, -1);
	lua_pop(L, 1);
      }
#else
      if (luaL_getmetafield(L, idx, "__name")) {
	const char *name = lua_tostring(L, -1);
	if (name)
	  kind = name;
	lua_pop(L, 1);
      }
#endif
      lua_pushfstring(L, "%s: %p", kind, lua_topointer(L, idx));
      break;
    }
    }
  }
  return lua_tolstring(L, -1, len);
}

/* -- Reference management ------------------------------------------------ */

#if LJ_54
#define FREELIST_REF	(LUA_RIDX_LAST + 1)
#else
#define FREELIST_REF	0
#endif

/* Convert a stack index to an absolute index. */
#define abs_index(L, i) \
  ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : lua_gettop(L) + (i) + 1)

LUALIB_API int luaL_ref(lua_State *L, int t)
{
  int ref;
  t = abs_index(L, t);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);  /* remove from stack */
    return LUA_REFNIL;  /* `nil' has a unique fixed reference */
  }
  lua_rawgeti(L, t, FREELIST_REF);  /* get first free element */
#if LJ_54
  if (lua_isnil(L, -1)) {
    /* Lua 5.4 keeps the auxiliary freelist after the registry constants.
    ** Initialize it there so arbitrary ref tables can safely use key 0.
    */
    ref = 0;
    lua_pushinteger(L, 0);
    lua_rawseti(L, t, FREELIST_REF);
  } else
#endif
  ref = (int)lua_tointeger(L, -1);  /* ref = t[FREELIST_REF] */
  lua_pop(L, 1);  /* remove it from stack */
  if (ref != 0) {  /* any free element? */
    lua_rawgeti(L, t, ref);  /* remove it from list */
    lua_rawseti(L, t, FREELIST_REF);  /* (t[FREELIST_REF] = t[ref]) */
  } else {  /* no free elements */
    ref = (int)lua_objlen(L, t);
    ref++;  /* create new reference */
  }
  lua_rawseti(L, t, ref);
  return ref;
}

LUALIB_API void luaL_unref(lua_State *L, int t, int ref)
{
  if (ref >= 0) {
    t = abs_index(L, t);
    lua_rawgeti(L, t, FREELIST_REF);
    lua_rawseti(L, t, ref);  /* t[ref] = t[FREELIST_REF] */
    lua_pushinteger(L, ref);
    lua_rawseti(L, t, FREELIST_REF);  /* t[FREELIST_REF] = ref */
  }
}

/* -- Default allocator and panic function -------------------------------- */

static int panic(lua_State *L)
{
  const char *s = lua_tostring(L, -1);
  fputs("PANIC: unprotected error in call to Lua API (", stderr);
  fputs(s ? s : "?", stderr);
  fputc(')', stderr); fputc('\n', stderr);
  fflush(stderr);
  return 0;
}

#ifndef LUAJIT_DISABLE_VMEVENT
static int error_finalizer(lua_State *L)
{
  const char *s = lua_tostring(L, -1);
#if LJ_54
  /* Lua 5.4 reports finalizer errors through the warning channel, so warning
  ** state and custom warning callbacks decide whether anything is emitted.
  */
  lua_pushfstring(L, "error in __gc (%s)", s ? s : "?");
  lua_warning(L, lua_tostring(L, -1), 0);
  lua_pop(L, 1);
#else
  fputs("ERROR in finalizer: ", stderr);
  fputs(s ? s : "?", stderr);
  fputc('\n', stderr);
  fflush(stderr);
#endif
  return 0;
}
#endif

#ifdef LUAJIT_USE_SYSMALLOC

#if LJ_64 && !LJ_GC64 && !defined(LUAJIT_USE_VALGRIND)
#error "Must use builtin allocator for 64 bit target"
#endif

static void *mem_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  } else {
    return realloc(ptr, nsize);
  }
}

LUALIB_API lua_State *luaL_newstate(void)
{
  lua_State *L = lua_newstate(mem_alloc, NULL);
  if (L) {
    G(L)->panic = panic;
#ifndef LUAJIT_DISABLE_VMEVENT
    luaL_findtable(L, LUA_REGISTRYINDEX, LJ_VMEVENTS_REGKEY, LJ_VMEVENTS_HSIZE);
    lua_pushcfunction(L, error_finalizer);
    lua_rawseti(L, -2, VMEVENT_HASH(LJ_VMEVENT_ERRFIN));
    G(L)->vmevmask = VMEVENT_MASK(LJ_VMEVENT_ERRFIN);
    L->top--;
#endif
  }
  return L;
}

#else

LUALIB_API lua_State *luaL_newstate(void)
{
  lua_State *L;
#if LJ_64 && !LJ_GC64
  L = lj_state_newstate(LJ_ALLOCF_INTERNAL, NULL);
#else
  L = lua_newstate(LJ_ALLOCF_INTERNAL, NULL);
#endif
  if (L) {
    G(L)->panic = panic;
#ifndef LUAJIT_DISABLE_VMEVENT
    luaL_findtable(L, LUA_REGISTRYINDEX, LJ_VMEVENTS_REGKEY, LJ_VMEVENTS_HSIZE);
    lua_pushcfunction(L, error_finalizer);
    lua_rawseti(L, -2, VMEVENT_HASH(LJ_VMEVENT_ERRFIN));
    G(L)->vmevmask = VMEVENT_MASK(LJ_VMEVENT_ERRFIN);
    L->top--;
#endif
  }
  return L;
}

#if LJ_64 && !LJ_GC64
LUA_API lua_State *lua_newstate(lua_Alloc f, void *ud)
{
  UNUSED(f); UNUSED(ud);
  fputs("Must use luaL_newstate() for 64 bit target\n", stderr);
  return NULL;
}
#endif

#endif

