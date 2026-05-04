/*
** Lua 5.4 to-be-closed value helpers.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_close_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_tab.h"
#include "lj_str.h"
#include "lj_state.h"
#include "lj_err.h"
#include "lj_close.h"

#if LJ_54
typedef struct CloseState {
  struct CloseState *prev;
  ptrdiff_t slot;
} CloseState;

int lj_close_isfalse(cTValue *o)
{
  return tvisnil(o) || tvisfalse(o);
}

cTValue *lj_close_getmethod(lua_State *L, cTValue *o)
{
  GCtab *mt;
  if (tvistab(o))
    mt = tabref(tabV(o)->metatable);
  else if (tvisudata(o))
    mt = tabref(udataV(o)->metatable);
  else
    mt = tabref(basemt_obj(G(L), o));
  if (mt) {
    cTValue *mo = lj_tab_getstr(mt, lj_str_newlit(L, "__close"));
    if (mo && !tvisnil(mo))
      return mo;
  }
  return NULL;
}

int lj_close_check(lua_State *L, cTValue *o)
{
  return lj_close_isfalse(o) || lj_close_getmethod(L, o) != NULL;
}

static int close_stackvalue(lua_State *L, cTValue *o, ptrdiff_t *ofs)
{
  TValue *stack = tvref(L->stack);
  if (o >= stack && o < tvref(L->maxstack)) {
    *ofs = savestack(L, o);
    return 1;
  }
  return 0;
}

int lj_close_call(lua_State *L, TValue *slot, cTValue *err, int clear)
{
  ptrdiff_t slotofs = savestack(L, slot);
  ptrdiff_t errofs = 0;
  int errstack = 0;
  cTValue *mo;
  TValue *top;
  if (lj_close_isfalse(slot)) {
    if (clear)
      setnilV(slot);
    return 1;
  }
  mo = lj_close_getmethod(L, slot);
  if (!mo)
    return 0;
  /* Save stack-relative pointers before growing the stack. This keeps the
  ** helper usable for the later VM unwind path, where the error object may be
  ** another stack value instead of the current nil-only compatibility bridge.
  */
  if (err != NULL)
    errstack = close_stackvalue(L, err, &errofs);
  lj_state_checkstack(L, 3);
  slot = restorestack(L, slotofs);
  if (errstack)
    err = restorestack(L, errofs);
  top = L->top;
  copyTV(L, top, mo);
  copyTV(L, top+1, slot);
  if (err != NULL)
    copyTV(L, top+2, err);
  else
    setnilV(top+2);
  L->top = top+3;
  lua_call(L, 2, 0);
  slot = restorestack(L, slotofs);
  if (clear)
    setnilV(slot);
  return 1;
}

static int close_pcall(lua_State *L, TValue *slot, cTValue *err, int clear)
{
  ptrdiff_t slotofs = savestack(L, slot);
  ptrdiff_t errofs = 0;
  int errstack = 0;
  cTValue *mo;
  TValue *top;
  int status;
  if (lj_close_isfalse(slot)) {
    if (clear)
      setnilV(slot);
    return LUA_OK;
  }
  mo = lj_close_getmethod(L, slot);
  if (!mo) {
    setstrV(L, L->top, lj_str_newlit(L, "attempt to close non-closable value"));
    L->top++;
    return LUA_ERRRUN;
  }
  if (err != NULL)
    errstack = close_stackvalue(L, err, &errofs);
  lj_state_checkstack(L, 3);
  slot = restorestack(L, slotofs);
  if (errstack)
    err = restorestack(L, errofs);
  top = L->top;
  copyTV(L, top, mo);
  copyTV(L, top+1, slot);
  if (err != NULL)
    copyTV(L, top+2, err);
  else
    setnilV(top+2);
  L->top = top+3;
  status = lua_pcall(L, 2, 0, 0);
  slot = restorestack(L, slotofs);
  if (clear)
    setnilV(slot);
  return status;
}

static void close_freenode(lua_State *L, CloseState *cs)
{
  lj_mem_freet(G(L), cs);
}

void lj_close_unmark(lua_State *L, TValue *slot)
{
  ptrdiff_t slotofs = savestack(L, slot);
  CloseState **pcs = (CloseState **)&L->closelist;
  while (*pcs) {
    CloseState *cs = *pcs;
    if (cs->slot == slotofs) {
      *pcs = cs->prev;
      close_freenode(L, cs);
      return;
    }
    pcs = &cs->prev;
  }
}

void lj_close_mark(lua_State *L, TValue *slot)
{
  CloseState *cs;
  if (lj_close_isfalse(slot))
    return;
  /* Stack slots are recycled. Drop a stale mark for the same slot before
  ** installing the new lifetime record.
  */
  lj_close_unmark(L, slot);
  cs = lj_mem_newt(L, sizeof(CloseState), CloseState);
  cs->slot = savestack(L, slot);
  cs->prev = (CloseState *)L->closelist;
  L->closelist = cs;
}

static CloseState **close_findunwind(lua_State *L, ptrdiff_t levelofs)
{
  CloseState **pcs = (CloseState **)&L->closelist;
  while (*pcs) {
    if ((*pcs)->slot >= levelofs)
      return pcs;
    pcs = &(*pcs)->prev;
  }
  return NULL;
}

static int close_unwind(lua_State *L, TValue *level, cTValue *err, int clear)
{
  ptrdiff_t levelofs = savestack(L, level);
  CloseState **pcs;
  int status = LUA_OK;
  if (err == NULL)
    err = niltv(L);
  while ((pcs = close_findunwind(L, levelofs)) != NULL) {
    CloseState *cs = *pcs;
    TValue *slot = restorestack(L, cs->slot);
    *pcs = cs->prev;
    close_freenode(L, cs);
    /* Remove the lifetime record before calling __close. If __close throws,
    ** the replacement error will unwind again and close any remaining outer
    ** records with the new error object, matching Lua 5.4's error replacement
    ** rule.
    */
    {
      int closestatus = close_pcall(L, slot, err, clear);
      if (closestatus != LUA_OK) {
        status = closestatus;
        err = L->top > tvref(L->stack) ? L->top-1 : niltv(L);
      }
    }
  }
  return status;
}

int lj_close_unwind(lua_State *L, TValue *level)
{
  cTValue *err = L->top > tvref(L->stack) ? L->top-1 : niltv(L);
  return close_unwind(L, level, err, 1);
}

int lj_close_unwind_status(lua_State *L, TValue *level, int status)
{
  cTValue *err = niltv(L);
  if (status != LUA_OK && status != LUA_YIELD && L->top > tvref(L->stack))
    err = L->top-1;
  return close_unwind(L, level, err, 1);
}

uint32_t lj_close_cframe(lua_State *L, uint32_t nres1)
{
  if (L->closelist != NULL) {
    /* Lua 5.4 closes C API to-be-closed slots before moving return values
    ** down and before running the return hook. Do not clear the slots here:
    ** an active slot may itself be one of the returned values.
    */
    int status = close_unwind(L, L->base, niltv(L), 0);
    if (status != LUA_OK)
      lua_error(L);
  }
  return nres1;
}

void lj_close_freeall(lua_State *L)
{
  CloseState *cs = (CloseState *)L->closelist;
  while (cs) {
    CloseState *next = cs->prev;
    close_freenode(L, cs);
    cs = next;
  }
  L->closelist = NULL;
}
#endif
