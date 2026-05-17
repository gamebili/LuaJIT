/*
** Lua 5.4 to-be-closed value helpers.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_close_c
#define LUA_CORE

#include <string.h>

#include "lua.h"
#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_tab.h"
#include "lj_str.h"
#include "lj_frame.h"
#include "lj_state.h"
#include "lj_err.h"
#include "lj_func.h"
#include "lj_close.h"
#include "lj_strfmt.h"

#if LJ_54
typedef struct CloseState {
  struct CloseState *prev;
  ptrdiff_t slot;
} CloseState;

#define CLOSE_RETURN_NOFRAME	((TValue *)(void *)(uintptr_t)1)

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

int lj_close_canmark(lua_State *L, TValue *slot)
{
  CloseState *cs = (CloseState *)L->closelist;
  return cs == NULL || cs->slot < savestack(L, slot);
}

int lj_close_islast(lua_State *L, TValue *slot)
{
  CloseState *cs = (CloseState *)L->closelist;
  return cs != NULL && cs->slot == savestack(L, slot);
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

static const uint8_t close_raw_pcall_key = 0;
static const uint8_t close_raw_xpcall_key = 0;

void lj_close_setrawpcall(lua_State *L, cTValue *pcall)
{
  GCtab *reg = tabV(registry(L));
  TValue key, *slot;
  setrawlightudV(&key, (void *)&close_raw_pcall_key);
  slot = lj_tab_set(L, reg, &key);
  copyTV(L, slot, pcall);
  lj_gc_anybarriert(L, reg);
}

void lj_close_setrawxpcall(lua_State *L, cTValue *xpcall)
{
  GCtab *reg = tabV(registry(L));
  TValue key, *slot;
  setrawlightudV(&key, (void *)&close_raw_xpcall_key);
  slot = lj_tab_set(L, reg, &key);
  copyTV(L, slot, xpcall);
  lj_gc_anybarriert(L, reg);
}

int lj_close_pushrawxpcall(lua_State *L)
{
  GCtab *reg = tabV(registry(L));
  TValue key;
  cTValue *xpcall;
  setrawlightudV(&key, (void *)&close_raw_xpcall_key);
  xpcall = lj_tab_get(L, reg, &key);
  if (!(xpcall && tvisfunc(xpcall)))
    xpcall = lj_tab_getstr(reg, lj_str_newlit(L, "_LUA54_RAW_XPCALL"));
  if (xpcall && tvisfunc(xpcall)) {
    lj_state_checkstack(L, 1);
    copyTV(L, L->top, xpcall);
    L->top++;
    return 1;
  }
  return 0;
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

int lj_close_hasunwind(lua_State *L, TValue *level)
{
  return close_findunwind(L, savestack(L, level)) != NULL;
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

int lj_close_unwind_pcall(lua_State *L)
{
  ptrdiff_t baseofs = savestack(L, L->base);
  lj_func_closeuv(L, L->base);
  int status = lj_close_unwind(L, L->base);
  TValue *base = restorestack(L, baseofs);
  L->base = base;
  /* Fast pcall returns false plus one error object. The original protected
  ** error is still an error even when every __close succeeds; when a later
  ** __close method replaces it, the newest object is likewise at top-1.
  */
  if (L->top > tvref(L->stack))
    copyTV(L, base, L->top-1);
  else
    setnilV(base);
  L->top = base+1;
  return status;
}

#if LJ_TARGET_ARM || LJ_TARGET_MIPS || LJ_TARGET_MIPS64 || LJ_TARGET_PPC || LJ_TARGET_X86 || LJ_TARGET_X64 || LJ_TARGET_ARM64
static cTValue *close_raw_pcall(lua_State *L)
{
  GCtab *reg = tabV(registry(L));
  TValue key;
  cTValue *pcall;
  setrawlightudV(&key, (void *)&close_raw_pcall_key);
  pcall = lj_tab_get(L, reg, &key);
  if (pcall && tvisfunc(pcall))
    return pcall;
  pcall = lj_tab_getstr(reg, lj_str_newlit(L, "_LUA54_RAW_PCALL"));
  if (pcall && tvisfunc(pcall))
    return pcall;
  pcall = lj_tab_getint(reg, LUA_RIDX_GLOBALS);
  if (pcall && tvistab(pcall)) {
    pcall = lj_tab_getstr(tabV(pcall), lj_str_newlit(L, "pcall"));
    if (pcall && tvisfunc(pcall))
      return pcall;
  }
  return niltv(L);
}

static TValue *close_setup_cont(lua_State *L, cTValue *mo, TValue *slot,
				cTValue *err, uint32_t contid)
{
  TValue *top = L->top;
  cTValue *pcall = close_raw_pcall(L);
  if (contid == LJ_CONT_CLOSE_RETURN || contid == LJ_CONT_CLOSE_RETURN_HOOK)
    L->close_pcall = 1;
#if LJ_FR2
  (top++)->u64 = contid;
  setnilV(top++);
  copyTV(L, top++, pcall);
  setnilV(top++);
#else
  top->u32.lo = contid;
  top++;
  copyTV(L, top++, pcall);
#endif
  copyTV(L, top, mo);
  copyTV(L, top+1, slot);
  if (err != NULL)
    copyTV(L, top+2, err);
  else
    setnilV(top+2);
  /* Error-unwind close calls go through a real fast pcall frame. That keeps
  ** coroutine.yield() legal inside __close and feeds close errors back to the
  ** continuation as normal pcall(false, err) results.
  */
  L->top = top+3;
  return top;
}

TValue *lj_close_prepare_pcall(lua_State *L)
{
  ptrdiff_t levelofs = savestack(L, L->base);
  cTValue *err = L->top > tvref(L->stack) ? L->top-1 : niltv(L);
  CloseState **pcs;
  int touched = 0;
  lj_func_closeuv(L, L->base);
  while ((pcs = close_findunwind(L, levelofs)) != NULL) {
    CloseState *cs = *pcs;
    TValue *slot = restorestack(L, cs->slot);
    ptrdiff_t slotofs = cs->slot;
    ptrdiff_t errofs = 0;
    int errstack = close_stackvalue(L, err, &errofs);
    cTValue *mo;
    touched = 1;
    *pcs = cs->prev;
    close_freenode(L, cs);
    if (lj_close_isfalse(slot))
      continue;
    mo = lj_close_getmethod(L, slot);
    if (!mo) {
      setstrV(L, L->top, lj_str_newlit(L,
	"attempt to call a nil value (metamethod 'close')"));
      L->top++;
      err = L->top-1;
      continue;
    }
    /* Error-unwind close calls must be yieldable and protected. Prepare a
    ** normal VM call to the active pcall fast function with
    ** (__close, value, err). That keeps coroutine.yield() legal inside
    ** __close and reports later __close errors as pcall results for the
    ** continuation to feed into the remaining outer close methods.
    */
    lj_state_checkstack(L, 8);
    slot = restorestack(L, slotofs);
    if (errstack)
      err = restorestack(L, errofs);
    return close_setup_cont(L, mo, slot, err, LJ_CONT_CLOSE);
  }
  if (L->top > tvref(L->stack))
    copyTV(L, L->base, L->top-1);
  else
    setnilV(L->base);
  if (touched)
    L->top = L->base+1;
  return NULL;
}

TValue *lj_close_continue_pcall(lua_State *L, TValue *mbase, TValue *res,
				int nres1)
{
  TValue *top = mbase - (2+2*LJ_FR2);
  TValue *errslot = top-1;
  /* mbase is the base of the just-returned pcall(__close, value, err). The
  ** current error object is immediately below the continuation frame. If the
  ** protected close failed, replace that error object before continuing.
  */
  if (nres1 >= 2 && tvisfalse(res)) {
    if (nres1 >= 3)
      copyTV(L, errslot, res+1);
    else
      setnilV(errslot);
  }
  L->top = top;
  return lj_close_prepare_pcall(L);
}

static TValue *close_prepare_cframe_pcall(lua_State *L, TValue *errslot)
{
  ptrdiff_t levelofs = savestack(L, L->base);
  TValue *nresslot = errslot-1;
  cTValue *err = tvisnil(errslot) ? niltv(L) : errslot;
  CloseState **pcs;
  lj_func_closeuv(L, L->base);
  while ((pcs = close_findunwind(L, levelofs)) != NULL) {
    CloseState *cs = *pcs;
    TValue *slot = restorestack(L, cs->slot);
    ptrdiff_t slotofs = cs->slot;
    ptrdiff_t errofs = 0;
    int errstack = close_stackvalue(L, err, &errofs);
    cTValue *mo;
    *pcs = cs->prev;
    close_freenode(L, cs);
    if (lj_close_isfalse(slot))
      continue;
    mo = lj_close_getmethod(L, slot);
    if (!mo) {
      setstrV(L, errslot, lj_str_newlit(L,
	"attempt to call a nil value (metamethod 'close')"));
      err = errslot;
      continue;
    }
    /* C API return closing runs before result relocation. Keep a private
    ** error marker above the still-unmoved C results, so yieldable __close
    ** calls can resume without clobbering the values that must be returned
    ** after the final close method succeeds.
    */
    {
      ptrdiff_t errslotofs = savestack(L, errslot);
      lj_state_checkstack(L, 8);
      errslot = restorestack(L, errslotofs);
    }
    slot = restorestack(L, slotofs);
    if (errstack)
      err = restorestack(L, errofs);
    L->top = errslot+1;
    return close_setup_cont(L, mo, slot, err, LJ_CONT_CLOSE_CFRAME);
  }
  if (tvisnil(errslot)) {
    /* Nested pcall(__close, ...) returns also pass through vm_returnc while
    ** the outer C frame still owns close slots. Keep the original C return
    ** count in a hidden stack slot so those nested returns can use the shared
    ** state field without corrupting the outer continuation.
    */
    L->close_cframe_nres1 = (int32_t)intV(nresslot);
    L->top = nresslot;
    return NULL;
  }
  /* The C function's return window is still below errslot. Once a close
  ** error wins, Lua 5.4 exposes only that error to the resumed coroutine;
  ** drop the abandoned return values before entering the normal error path.
  */
  copyTV(L, L->base, errslot);
  L->top = L->base+1;
  L->close_cframe_nres1 = 0;
  lj_err_run(L);
}

TValue *lj_close_prepare_cframe_pcall(lua_State *L, uint32_t nres1)
{
  TValue *nresslot;
  TValue *errslot;
  if (close_findunwind(L, savestack(L, L->base)) == NULL) {
    L->close_cframe_nres1 = (int32_t)nres1;
    return NULL;
  }
  lj_state_checkstack(L, 2);
  nresslot = L->top++;
  setintV(nresslot, (int32_t)nres1);
  errslot = L->top++;
  setnilV(errslot);
  return close_prepare_cframe_pcall(L, errslot);
}

TValue *lj_close_continue_cframe_pcall(lua_State *L, TValue *mbase,
				       TValue *res, int nres1)
{
  TValue *top = mbase - (2+2*LJ_FR2);
  TValue *errslot = top-1;
  if (nres1 >= 2 && tvisfalse(res)) {
    if (nres1 >= 3)
      copyTV(L, errslot, res+1);
    else
      setnilV(errslot);
  }
  L->top = top;
  return close_prepare_cframe_pcall(L, errslot);
}

static void close_restore_return(lua_State *L, TValue *errslot)
{
  TValue *nresslot = errslot-1;
  TValue *multresslot = errslot-2;
  TValue *raslot = errslot-3;
  int32_t nres1 = (int32_t)intV(nresslot);
  uint32_t nres = (uint32_t)(nres1 - 1);
  TValue *src = raslot - nres;
  TValue *dst = L->base + intV(raslot);
  uint32_t i;
  for (i = 0; i < nres; i++)
    copyTV(L, dst+i, src+i);
  L->close_cframe_nres1 = nres1;
  L->close_multres = (int32_t)intV(multresslot);
  L->close_pcall = 0;
  L->top = dst + nres;
}

static void close_store_error(lua_State *L, TValue *errslot, cTValue *err)
{
  if (tvisstr(err)) {
    GCstr *s = strV(err);
    const char *msg = strdata(s);
    if (msg[0] == '@' || strchr(msg, ' ') == NULL) {
      ptrdiff_t errslotofs = savestack(L, errslot);
      L->top = errslot+1;
      lj_strfmt_pushf(L, "close.lua:0: %s", msg);
      errslot = restorestack(L, errslotofs);
      copyTV(L, errslot, L->top-1);
      L->top = errslot+1;
      return;
    }
  }
  copyTV(L, errslot, err);
}

static TValue *close_prepare_return_pcall(lua_State *L, TValue *errslot,
					  uint32_t contid)
{
  ptrdiff_t levelofs = savestack(L, L->base);
  cTValue *err = tvisnil(errslot) ? niltv(L) : errslot;
  CloseState **pcs;
  lj_func_closeuv(L, L->base);
  while ((pcs = close_findunwind(L, levelofs)) != NULL) {
    CloseState *cs = *pcs;
    TValue *slot = restorestack(L, cs->slot);
    ptrdiff_t slotofs = cs->slot;
    ptrdiff_t errofs = 0;
    int errstack = close_stackvalue(L, err, &errofs);
    cTValue *mo;
    *pcs = cs->prev;
    close_freenode(L, cs);
    if (lj_close_isfalse(slot))
      continue;
    mo = lj_close_getmethod(L, slot);
    if (!mo) {
      setstrV(L, errslot, lj_str_newlit(L,
	"attempt to call a nil value (metamethod 'close')"));
      err = errslot;
      continue;
    }
    {
      ptrdiff_t errslotofs = savestack(L, errslot);
      lj_state_checkstack(L, 8);
      errslot = restorestack(L, errslotofs);
    }
    slot = restorestack(L, slotofs);
    if (errstack)
      err = restorestack(L, errofs);
    L->top = errslot+1;
    return close_setup_cont(L, mo, slot, err, contid);
  }
  if (tvisnil(errslot)) {
    close_restore_return(L, errslot);
    return NULL;
  }
  copyTV(L, L->base, errslot);
  L->top = L->base+1;
  L->close_cframe_nres1 = 0;
  L->close_multres = 0;
  L->close_pcall = 0;
  lj_err_run(L);
}

static TValue *close_prepare_return_start(lua_State *L, TValue *res,
					  uint32_t nres1, uint32_t multres,
					  uint32_t contid)
{
  uint32_t nres = nres1 - 1;
  ptrdiff_t resofs = savestack(L, res);
  TValue *frame_top = curr_topL(L);
  TValue *resend = res + nres;
  TValue *save;
  TValue *raslot;
  TValue *multresslot;
  TValue *nresslot;
  TValue *errslot;
  uint32_t i;
  if (close_findunwind(L, savestack(L, L->base)) == NULL) {
    L->close_cframe_nres1 = (int32_t)nres1;
    L->close_multres = (int32_t)multres;
    L->top = res + nres;
    return CLOSE_RETURN_NOFRAME;
  }
  L->top = frame_top > resend ? frame_top : resend;
  lj_state_checkstack(L, nres + 8);
  res = restorestack(L, resofs);
  frame_top = curr_topL(L);
  resend = res + nres;
  L->top = frame_top > resend ? frame_top : resend;
  save = L->top;
  for (i = 0; i < nres; i++)
    copyTV(L, save+i, res+i);
  raslot = save+nres;
  setintV(raslot, (int32_t)(res - L->base));
  multresslot = raslot+1;
  setintV(multresslot, (int32_t)multres);
  nresslot = multresslot+1;
  setintV(nresslot, (int32_t)nres1);
  errslot = nresslot+1;
  setnilV(errslot);
  L->top = errslot+1;
  return close_prepare_return_pcall(L, errslot, contid);
}

TValue *lj_close_prepare_return_pcall(lua_State *L, TValue *res,
				      uint32_t nres1, uint32_t multres)
{
  return close_prepare_return_start(L, res, nres1, multres,
				   LJ_CONT_CLOSE_RETURN);
}

TValue *lj_close_prepare_return_hook_pcall(lua_State *L, TValue *res,
					   uint32_t nres1, uint32_t multres)
{
  return close_prepare_return_start(L, res, nres1, multres,
				   LJ_CONT_CLOSE_RETURN_HOOK);
}

static TValue *close_continue_return_pcall(lua_State *L, TValue *mbase,
					   TValue *res, int nres1,
					   uint32_t contid)
{
  TValue *top = mbase - (2+2*LJ_FR2);
  TValue *errslot = top-1;
  if (nres1 >= 2 && tvisfalse(res)) {
    if (nres1 >= 3)
      close_store_error(L, errslot, res+1);
    else
      setnilV(errslot);
  }
  L->top = top;
  return close_prepare_return_pcall(L, errslot, contid);
}

TValue *lj_close_continue_return_pcall(lua_State *L, TValue *mbase,
				       TValue *res, int nres1)
{
  return close_continue_return_pcall(L, mbase, res, nres1,
				     LJ_CONT_CLOSE_RETURN);
}

TValue *lj_close_continue_return_hook_pcall(lua_State *L, TValue *mbase,
					    TValue *res, int nres1)
{
  return close_continue_return_pcall(L, mbase, res, nres1,
				     LJ_CONT_CLOSE_RETURN_HOOK);
}
#endif

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
