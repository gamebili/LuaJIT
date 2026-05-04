/*
** Lua 5.4 to-be-closed value helpers.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_close_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_tab.h"
#include "lj_str.h"
#include "lj_state.h"
#include "lj_close.h"

#if LJ_54
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
#endif
