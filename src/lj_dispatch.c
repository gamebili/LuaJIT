/*
** Instruction dispatch handling.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_dispatch_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_func.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_debug.h"
#include "lj_state.h"
#include "lj_frame.h"
#include "lj_bc.h"
#include "lj_ff.h"
#include "lj_strfmt.h"
#if LJ_HASJIT
#include "lj_jit.h"
#endif
#if LJ_HASFFI
#include "lj_ccallback.h"
#endif
#include "lj_trace.h"
#include "lj_dispatch.h"
#if LJ_HASPROFILE
#include "lj_profile.h"
#endif
#include "lj_vm.h"
#include "luajit.h"

/* Bump GG_NUM_ASMFF in lj_dispatch.h as needed. Ugly. */
LJ_STATIC_ASSERT(GG_NUM_ASMFF == FF_NUM_ASMFUNC);

/* -- Dispatch table management ------------------------------------------- */

#if LJ_TARGET_MIPS
#include <math.h>
LJ_FUNCA_NORET void LJ_FASTCALL lj_ffh_coroutine_wrap_err(lua_State *L,
							  lua_State *co);
#if !LJ_HASJIT
#define lj_dispatch_stitch	lj_dispatch_ins
#endif
#if !LJ_HASPROFILE
#define lj_dispatch_profile	lj_dispatch_ins
#endif

#define GOTFUNC(name)	(ASMFunction)name,
static const ASMFunction dispatch_got[] = {
  GOTDEF(GOTFUNC)
};
#undef GOTFUNC
#endif

/* Initialize instruction dispatch table and hot counters. */
void lj_dispatch_init(GG_State *GG)
{
  uint32_t i;
  ASMFunction *disp = GG->dispatch;
  for (i = 0; i < GG_LEN_SDISP; i++)
    disp[GG_LEN_DDISP+i] = disp[i] = makeasmfunc(lj_bc_ofs[i]);
  for (i = GG_LEN_SDISP; i < GG_LEN_DDISP; i++)
    disp[i] = makeasmfunc(lj_bc_ofs[i]);
  /* The JIT engine is off by default. luaopen_jit() turns it on. */
  disp[BC_FORL] = disp[BC_IFORL];
  disp[BC_ITERL] = disp[BC_IITERL];
  /* Workaround for stable v2.1 bytecode. TODO: Replace with BC_IITERN. */
  disp[BC_ITERN] = &lj_vm_IITERN;
  disp[BC_LOOP] = disp[BC_ILOOP];
  disp[BC_FUNCF] = disp[BC_IFUNCF];
  disp[BC_FUNCV] = disp[BC_IFUNCV];
  GG->g.bc_cfunc_ext = GG->g.bc_cfunc_int = BCINS_AD(BC_FUNCC, LUA_MINSTACK, 0);
  for (i = 0; i < GG_NUM_ASMFF; i++)
    GG->bcff[i] = BCINS_AD(BC__MAX+i, 0, 0);
#if LJ_TARGET_MIPS
  memcpy(GG->got, dispatch_got, LJ_GOT__MAX*sizeof(ASMFunction *));
#endif
}

#if LJ_HASJIT
/* Initialize hotcount table. */
void lj_dispatch_init_hotcount(global_State *g)
{
  int32_t hotloop = G2J(g)->param[JIT_P_hotloop];
  HotCount start = (HotCount)(hotloop*HOTCOUNT_LOOP - 1);
  HotCount *hotcount = G2GG(g)->hotcount;
  uint32_t i;
  for (i = 0; i < HOTCOUNT_SIZE; i++)
    hotcount[i] = start;
}
#endif

/* Internal dispatch mode bits. */
#define DISPMODE_CALL	0x01	/* Override call dispatch. */
#define DISPMODE_RET	0x02	/* Override return dispatch. */
#define DISPMODE_INS	0x04	/* Override instruction dispatch. */
#define DISPMODE_JIT	0x10	/* JIT compiler on. */
#define DISPMODE_REC	0x20	/* Recording active. */
#define DISPMODE_PROF	0x40	/* Profiling active. */

/* Update dispatch table depending on various flags. */
void lj_dispatch_update(global_State *g)
{
  uint8_t oldmode = g->dispatchmode;
  uint8_t mode = 0;
#if LJ_HASJIT
  mode |= (G2J(g)->flags & JIT_F_ON
#if LJ_54
	   && !(g->hookmask & HOOK_EVENTMASK)
#endif
	  ) ? DISPMODE_JIT : 0;
  mode |= G2J(g)->state != LJ_TRACE_IDLE ?
	    (DISPMODE_REC|DISPMODE_INS|DISPMODE_CALL) : 0;
#endif
#if LJ_HASPROFILE
  mode |= (g->hookmask & HOOK_PROFILE) ? (DISPMODE_PROF|DISPMODE_INS) : 0;
#endif
  mode |= (g->hookmask & (LUA_MASKLINE|LUA_MASKCOUNT)) ? DISPMODE_INS : 0;
#if LJ_54
  mode |= (g->hookmask & (LUA_MASKCALL|LUA_MASKLINE)) ? DISPMODE_CALL : 0;
#else
  mode |= (g->hookmask & LUA_MASKCALL) ? DISPMODE_CALL : 0;
#endif
  mode |= (g->hookmask & LUA_MASKRET) ? DISPMODE_RET : 0;
  if (oldmode != mode) {  /* Mode changed? */
    ASMFunction *disp = G2GG(g)->dispatch;
    ASMFunction f_forl, f_iterl, f_itern, f_loop, f_funcf, f_funcv;
    g->dispatchmode = mode;

    /* Hotcount if JIT is on, but not while recording. */
    if ((mode & (DISPMODE_JIT|DISPMODE_REC)) == DISPMODE_JIT) {
      f_forl = makeasmfunc(lj_bc_ofs[BC_FORL]);
      f_iterl = makeasmfunc(lj_bc_ofs[BC_ITERL]);
      f_itern = makeasmfunc(lj_bc_ofs[BC_ITERN]);
      f_loop = makeasmfunc(lj_bc_ofs[BC_LOOP]);
      f_funcf = makeasmfunc(lj_bc_ofs[BC_FUNCF]);
      f_funcv = makeasmfunc(lj_bc_ofs[BC_FUNCV]);
    } else {  /* Otherwise use the non-hotcounting instructions. */
      f_forl = disp[GG_LEN_DDISP+BC_IFORL];
      f_iterl = disp[GG_LEN_DDISP+BC_IITERL];
      f_itern = &lj_vm_IITERN;
      f_loop = disp[GG_LEN_DDISP+BC_ILOOP];
      f_funcf = makeasmfunc(lj_bc_ofs[BC_IFUNCF]);
      f_funcv = makeasmfunc(lj_bc_ofs[BC_IFUNCV]);
    }
    /* Init static counting instruction dispatch first (may be copied below). */
    disp[GG_LEN_DDISP+BC_FORL] = f_forl;
    disp[GG_LEN_DDISP+BC_ITERL] = f_iterl;
    disp[GG_LEN_DDISP+BC_ITERN] = f_itern;
    disp[GG_LEN_DDISP+BC_LOOP] = f_loop;

    /* Set dynamic instruction dispatch. */
    if ((oldmode ^ mode) & (DISPMODE_PROF|DISPMODE_REC|DISPMODE_INS)) {
      /* Need to update the whole table. */
      if (!(mode & DISPMODE_INS)) {  /* No ins dispatch? */
	/* Copy static dispatch table to dynamic dispatch table. */
	memcpy(&disp[0], &disp[GG_LEN_DDISP], GG_LEN_SDISP*sizeof(ASMFunction));
	/* Overwrite with dynamic return dispatch. */
	if ((mode & DISPMODE_RET)) {
	  disp[BC_RETM] = lj_vm_rethook;
	  disp[BC_RET] = lj_vm_rethook;
	  disp[BC_RET0] = lj_vm_rethook;
	  disp[BC_RET1] = lj_vm_rethook;
	}
      } else {
	/* The recording dispatch also checks for hooks. */
	ASMFunction f = (mode & DISPMODE_PROF) ? lj_vm_profhook :
			(mode & DISPMODE_REC) ? lj_vm_record : lj_vm_inshook;
	uint32_t i;
	for (i = 0; i < GG_LEN_SDISP; i++)
	  disp[i] = f;
      }
    } else if (!(mode & DISPMODE_INS)) {
      /* Otherwise set dynamic counting ins. */
      disp[BC_FORL] = f_forl;
      disp[BC_ITERL] = f_iterl;
      disp[BC_ITERN] = f_itern;
      disp[BC_LOOP] = f_loop;
      /* Set dynamic return dispatch. */
      if ((mode & DISPMODE_RET)) {
	disp[BC_RETM] = lj_vm_rethook;
	disp[BC_RET] = lj_vm_rethook;
	disp[BC_RET0] = lj_vm_rethook;
	disp[BC_RET1] = lj_vm_rethook;
      } else {
	disp[BC_RETM] = disp[GG_LEN_DDISP+BC_RETM];
	disp[BC_RET] = disp[GG_LEN_DDISP+BC_RET];
	disp[BC_RET0] = disp[GG_LEN_DDISP+BC_RET0];
	disp[BC_RET1] = disp[GG_LEN_DDISP+BC_RET1];
      }
    }

    /* Set dynamic call dispatch. */
    if ((oldmode ^ mode) & DISPMODE_CALL) {  /* Update the whole table? */
      uint32_t i;
      if ((mode & DISPMODE_CALL) == 0) {  /* No call hooks? */
	for (i = GG_LEN_SDISP; i < GG_LEN_DDISP; i++)
	  disp[i] = makeasmfunc(lj_bc_ofs[i]);
      } else {
	for (i = GG_LEN_SDISP; i < GG_LEN_DDISP; i++)
	  disp[i] = lj_vm_callhook;
      }
    }
    if (!(mode & DISPMODE_CALL)) {  /* Overwrite dynamic counting ins. */
      disp[BC_FUNCF] = f_funcf;
      disp[BC_FUNCV] = f_funcv;
    }

#if LJ_HASJIT
    /* Reset hotcounts for JIT off to on transition. */
    if ((mode & DISPMODE_JIT) && !(oldmode & DISPMODE_JIT))
      lj_dispatch_init_hotcount(g);
#endif
  }
}

/* -- JIT mode setting ---------------------------------------------------- */

#if LJ_HASJIT
/* Set JIT mode for a single prototype. */
static void setptmode(global_State *g, GCproto *pt, int mode)
{
  if ((mode & LUAJIT_MODE_ON)) {  /* (Re-)enable JIT compilation. */
    pt->flags &= ~PROTO_NOJIT;
    lj_trace_reenableproto(pt);  /* Unpatch all ILOOP etc. bytecodes. */
  } else {  /* Flush and/or disable JIT compilation. */
    if (!(mode & LUAJIT_MODE_FLUSH))
      pt->flags |= PROTO_NOJIT;
    lj_trace_flushproto(g, pt);  /* Flush all traces of prototype. */
  }
}

/* Recursively set the JIT mode for all children of a prototype. */
static void setptmode_all(global_State *g, GCproto *pt, int mode)
{
  ptrdiff_t i;
  if (!(pt->flags & PROTO_CHILD)) return;
  for (i = -(ptrdiff_t)pt->sizekgc; i < 0; i++) {
    GCobj *o = proto_kgc(pt, i);
    if (o->gch.gct == ~LJ_TPROTO) {
      setptmode(g, gco2pt(o), mode);
      setptmode_all(g, gco2pt(o), mode);
    }
  }
}
#endif

/* Public API function: control the JIT engine. */
int luaJIT_setmode(lua_State *L, int idx, int mode)
{
  global_State *g = G(L);
  int mm = mode & LUAJIT_MODE_MASK;
  lj_trace_abort(g);  /* Abort recording on any state change. */
  /* Avoid pulling the rug from under our own feet. */
  if ((g->hookmask & HOOK_GC))
    lj_err_caller(L, LJ_ERR_NOGCMM);
  switch (mm) {
#if LJ_HASJIT
  case LUAJIT_MODE_ENGINE:
    if ((mode & LUAJIT_MODE_FLUSH)) {
      lj_trace_flushall(L);
    } else {
      if (!(mode & LUAJIT_MODE_ON))
	G2J(g)->flags &= ~(uint32_t)JIT_F_ON;
      else
	G2J(g)->flags |= (uint32_t)JIT_F_ON;
      lj_dispatch_update(g);
    }
    break;
  case LUAJIT_MODE_FUNC:
  case LUAJIT_MODE_ALLFUNC:
  case LUAJIT_MODE_ALLSUBFUNC: {
    cTValue *tv = idx == 0 ? frame_prev(L->base-1)-LJ_FR2 :
		  idx > 0 ? L->base + (idx-1) : L->top + idx;
    GCproto *pt;
    if ((idx == 0 || tvisfunc(tv)) && isluafunc(&gcval(tv)->fn))
      pt = funcproto(&gcval(tv)->fn);  /* Cannot use funcV() for frame slot. */
    else if (tvisproto(tv))
      pt = protoV(tv);
    else
      return 0;  /* Failed. */
    if (mm != LUAJIT_MODE_ALLSUBFUNC)
      setptmode(g, pt, mode);
    if (mm != LUAJIT_MODE_FUNC)
      setptmode_all(g, pt, mode);
    break;
    }
  case LUAJIT_MODE_TRACE:
    if (!(mode & LUAJIT_MODE_FLUSH))
      return 0;  /* Failed. */
    lj_trace_flush(G2J(g), idx);
    break;
#else
  case LUAJIT_MODE_ENGINE:
  case LUAJIT_MODE_FUNC:
  case LUAJIT_MODE_ALLFUNC:
  case LUAJIT_MODE_ALLSUBFUNC:
    UNUSED(idx);
    if ((mode & LUAJIT_MODE_ON))
      return 0;  /* Failed. */
    break;
#endif
  case LUAJIT_MODE_WRAPCFUNC:
    if ((mode & LUAJIT_MODE_ON)) {
      if (idx != 0) {
	cTValue *tv = idx > 0 ? L->base + (idx-1) : L->top + idx;
	if (tvislightud(tv))
	  g->wrapf = (lua_CFunction)lightudV(g, tv);
	else
	  return 0;  /* Failed. */
      } else {
	return 0;  /* Failed. */
      }
      setbc_op(&g->bc_cfunc_ext, BC_FUNCCW);
    } else {
      setbc_op(&g->bc_cfunc_ext, BC_FUNCC);
    }
    break;
  default:
    return 0;  /* Failed. */
  }
  return 1;  /* OK. */
}

/* Enforce (dynamic) linker error for version mismatches. See luajit.c. */
LUA_API void LUAJIT_VERSION_SYM(void)
{
}

/* -- Hooks --------------------------------------------------------------- */

LUA_API int lua_sethook(lua_State *L, lua_Hook func, int mask, int count)
{
  global_State *g = G(L);
  mask &= HOOK_EVENTMASK;
  if (func == NULL || mask == 0) { mask = 0; func = NULL; }  /* Consistency. */
#if LJ_54
  g->hook_skipline = 0;
  g->hook_skipcount = (uint8_t)((mask & LUA_MASKCOUNT) && count > 0 ?
				(count == 1 ? 8 : 1) : 0);
  g->hook_debug = 0;
#if LJ_HASJIT
  if (mask)
    /* Lua 5.4 hooks must observe interpreted call/return/count boundaries.
    ** Existing traces predate the hook dispatch update, so discard them when
    ** hooks are enabled instead of letting hot loops bypass count hooks.
    */
    lj_trace_flushall(L);
#endif
#endif
  g->hookf = func;
  g->hookcount = g->hookcstart = (int32_t)count;
#if LJ_54
  if ((mask & LUA_MASKCOUNT) && count > 0)
    /* LuaJIT resumes dispatch on the instruction that follows sethook().
    ** Bias only the initial countdown so Lua 5.4 count hooks do not report an
    ** extra event before the first full instruction interval has elapsed.
    */
    g->hookcount++;
#endif
  g->hookmask = (uint8_t)((g->hookmask & ~HOOK_EVENTMASK) | mask);
  lj_trace_abort(g);  /* Abort recording on any hook change. */
#if LJ_54 && LJ_TARGET_ARM64
  if (hook_active(g)) {
    g->hook_needupdate = 1;
    return 1;
  }
#endif
  lj_dispatch_update(g);
  return 1;
}

#if LJ_54
LUA_API void lua_sethook54(lua_State *L, lua_Hook func, int mask, int count)
{
  /* Official Lua 5.4 exposes lua_sethook as void.  The LuaJIT core keeps the
  ** historical int-returning entry point, so external 5.4 headers call here.
  */
  (void)lua_sethook(L, func, mask, count);
}
#endif

LUA_API lua_Hook lua_gethook(lua_State *L)
{
  return G(L)->hookf;
}

LUA_API int lua_gethookmask(lua_State *L)
{
  global_State *g = G(L);
#if LJ_54 && LJ_TARGET_ARM64
  if ((g->hookmask & HOOK_ACTIVE) && g->hook_savemask)
    return g->hook_savemask;
#endif
  return g->hookmask & HOOK_EVENTMASK;
}

LUA_API int lua_gethookcount(lua_State *L)
{
  return (int)G(L)->hookcstart;
}

/* Call a hook. */
#if LJ_54
static int hook_thread_active(lua_State *L, lua_Hook hookfn)
{
  cTValue *tv;
  TValue key;
  if (!G(L)->hook_debug)
    return 1;
  UNUSED(hookfn);
  tv = lj_tab_getstr(tabV(registry(L)), lj_str_newlit(L, "_HOOKKEY"));
  if (!(tv && tvistab(tv)))
    return 0;
  setthreadV(L, &key, L);
  tv = lj_tab_get(L, tabV(tv), &key);
  return tv && tvisfunc(tv);
}
#endif

void LJ_FASTCALL lj_dispatch_clear_dead_debug_hook(lua_State *L)
{
#if LJ_54
  global_State *g = G(L);
  if (g->hook_debug && L->status != LUA_YIELD &&
      hook_thread_active(L, g->hookf)) {
    g->hookmask &= (uint8_t)~HOOK_EVENTMASK;
    g->hook_debug = 0;
    lj_dispatch_update(g);
  }
#else
  UNUSED(L);
#endif
}

static void callhook(lua_State *L, int event, BCLine line,
		     uint16_t ftransfer, uint16_t ntransfer)
{
  global_State *g = G(L);
  lua_Hook hf = g->hookf;
  if (hf && !hook_active(g)) {
    lua_Debug ar;
#if LJ_54
#if !LJ_HASPROFILE || LJ_PROFILE_SIGPROF
    uint8_t oldevents = g->hookmask & HOOK_EVENTMASK;
#endif
#endif
    lj_trace_abort(g);  /* Abort recording on any hook call. */
    ar.event = event;
    ar.currentline = line;
    /* Top frame, nextframe = NULL. */
    ar.i_ci = LJ_DEBUG_CI_ENCODE((L->base-1) - tvref(L->stack));
    ar.ftransfer = ftransfer;
    ar.ntransfer = ntransfer;
    g->hook_L = L;
    g->hook_ci = (int)LJ_DEBUG_CI_VALUE(ar.i_ci);
    g->hook_ftransfer = ftransfer;
    g->hook_ntransfer = ntransfer;
    lj_state_checkstack(L, 1+LUA_MINSTACK);
#if LJ_54
    if (!hook_thread_active(L, hf))
      return;
#endif
#if LJ_HASPROFILE && !LJ_PROFILE_SIGPROF
    lj_profile_hook_enter(g);
#else
    hook_enter(g);
#if LJ_54 && LJ_TARGET_ARM64
    /* Lua 5.4 keeps hooks disabled while a hook callback is running, but
    ** debug.gethook() from inside the callback still reports the public mask.
    */
    g->hook_savemask = oldevents;
    g->hookmask &= (uint8_t)~HOOK_EVENTMASK;
#endif
#endif
    hf(L, &ar);
    lj_assertG(hook_active(g), "active hook flag removed");
    setgcref(g->cur_L, obj2gco(L));
#if LJ_HASPROFILE && !LJ_PROFILE_SIGPROF
    lj_profile_hook_leave(g);
#else
#if LJ_54 && LJ_TARGET_ARM64
    if ((g->hookmask & HOOK_EVENTMASK) == 0 && g->hookf == hf)
      g->hookmask |= oldevents;
    g->hook_savemask = 0;
#endif
    hook_leave(g);
#if LJ_54 && LJ_TARGET_ARM64
    if (g->hook_needupdate) {
      g->hook_needupdate = 0;
      lj_dispatch_update(g);
    }
#endif
#endif
  }
}

#if LJ_54 && LJ_TARGET_ARM64
static int arm64_lua54_close_return_hook(lua_State *L)
{
  const char *name = NULL;
  int size;
  cTValue *frame;
  const char *kind;
  if (!L->close_pcall)
    return 0;
  frame = lj_debug_frame(L, 0, &size);
  kind = frame ? lj_debug_funcname(L, frame, &name) : NULL;
  return kind && name &&
	 kind[0] == 'm' && kind[1] == 'e' &&
	 name[0] == 'c' && name[1] == 'l' && name[2] == 'o' &&
	 name[3] == 's' && name[4] == 'e' && name[5] == '\0';
}
#endif

/* C function return hook dispatch.
**
** The assembler C-call return path already knows the first returned slot and
** result count before it folds results back into the caller frame. Keep that
** architecture-specific stack arithmetic there, but route the hook state and
** public debug metadata through the same callhook() path as Lua returns.
*/
uint32_t LJ_FASTCALL lj_dispatch_ceret(lua_State *L, uint32_t ftransfer,
				       uint32_t ntransfer)
{
  ERRNO_SAVE
  uint32_t nres1 = ntransfer + 1;
  if (ftransfer > 65535u) ftransfer = 65535u;
  if (ntransfer > 65535u) ntransfer = 65535u;
#if LJ_54
  if (G(L)->hook_skipret) {
    G(L)->hook_skipret--;
    ERRNO_RESTORE
    return nres1;
  }
#endif
  callhook(L, LUA_HOOKRET, -1, (uint16_t)ftransfer, (uint16_t)ntransfer);
  ERRNO_RESTORE
  return nres1;
}

#if LJ_54
/* Fast-function return hook dispatch.
**
** Assembler fast functions initially place their results over the frame slots
** (base-2, base-1, ...), which is perfect for the old fast return path but not
** for Lua 5.4 debug hooks: debug.getlocal() must see returned values in the
** public transfer range after the original arguments, while debug.getinfo()
** still needs an intact C frame.  Build that temporary window here and then
** copy any hook-modified result values back to the fast return slots.
*/
uint32_t LJ_FASTCALL lj_dispatch_fferet(lua_State *L, uint32_t ftransfer,
					uint32_t ntransfer)
{
  global_State *g = G(L);
  TValue *frame = L->base - 1;
  TValue *res = frame - LJ_FR2;
  TValue *pub;
  const BCIns *pc = cframe_pc(cframe_raw(L->cframe));
  GCfunc *fn = g->hook_cfunc;
  uint32_t i, nres1;
  if (ntransfer == 0)
    ftransfer = 0;
  pub = frame + ftransfer;
  L->top = pub + ntransfer;
  lj_state_checkstack(L, LUA_MINSTACK);
  frame = L->base - 1;
  res = frame - LJ_FR2;
  pub = frame + ftransfer;
  if (pub > res && pub < res + ntransfer) {
    for (i = ntransfer; i > 0; i--)
      copyTV(L, pub + i - 1, res + i - 1);
  } else {
    for (i = 0; i < ntransfer; i++)
      copyTV(L, pub + i, res + i);
  }
  if (fn)
    setfuncV(L, res, fn);
  setframe_pc(frame, pc);
  L->top = pub + ntransfer;
  nres1 = lj_dispatch_ceret(L, ftransfer, ntransfer);
  frame = L->base - 1;
  res = frame - LJ_FR2;
  pub = frame + ftransfer;
  for (i = 0; i < ntransfer; i++)
    copyTV(L, res + i, pub + i);
  return nres1;
}
#endif

/* -- Dispatch callbacks -------------------------------------------------- */

/* Calculate number of used stack slots in the current frame. */
static BCReg cur_topslot(GCproto *pt, const BCIns *pc, uint32_t nres)
{
  BCIns ins = pc[-1];
  if (bc_op(ins) == BC_UCLO)
    ins = pc[bc_j(ins)];
  switch (bc_op(ins)) {
  case BC_CALLM: case BC_CALLMT: return bc_a(ins) + bc_c(ins) + nres-1+1+LJ_FR2;
#if LJ_TARGET_ARM64
  case BC_CALL:
    if (bc_b(ins) != 0) {
      BCReg slots = bc_a(ins) + bc_b(ins) - 1 + LJ_FR2;
      return slots > pt->framesize ? slots : pt->framesize;
    }
    return pt->framesize;
#endif
  case BC_RETM: return bc_a(ins) + bc_d(ins) + nres-1;
  case BC_TSETM: return bc_a(ins) + nres-1;
  default: return pt->framesize;
  }
}

/* Instruction dispatch. Used by instr/line/return hooks or when recording. */
void LJ_FASTCALL lj_dispatch_ins(lua_State *L, const BCIns *pc)
{
  ERRNO_SAVE
  GCfunc *fn = curr_func(L);
  GCproto *pt = funcproto(fn);
  void *cf = cframe_raw(L->cframe);
  const BCIns *oldpc = cframe_pc(cf);
  global_State *g = G(L);
  BCReg slots;
  setcframe_pc(cf, pc);
  slots = cur_topslot(pt, pc, cframe_multres_n(cf));
  L->top = L->base + slots;  /* Fix top. */
#if LJ_HASJIT
  {
    jit_State *J = G2J(g);
    if (J->state != LJ_TRACE_IDLE
#if LJ_54
	&& L->closelist == NULL
#endif
       ) {
#ifdef LUA_USE_ASSERT
      ptrdiff_t delta = L->top - L->base;
#endif
      J->L = L;
      lj_trace_ins(J, pc-1);  /* The interpreter bytecode PC is offset by 1. */
      lj_assertG(L->top - L->base == delta,
		 "unbalanced stack after tracing of instruction");
    }
  }
#endif
  if ((g->hookmask & LUA_MASKCOUNT) && g->hookcount == 0) {
    g->hookcount = g->hookcstart;
#if LJ_54
    if (g->hook_skipcount) {
      g->hook_skipcount--;
    } else
#endif
    callhook(L, LUA_HOOKCOUNT, -1, 0, 0);
    L->top = L->base + slots;  /* Fix top again. */
  }
  if ((g->hookmask & LUA_MASKLINE)) {
    BCPos npc = proto_bcpos(pt, pc) - 1;
    BCPos opc = proto_bcpos(pt, oldpc) - 1;
#if LJ_54
    int hasline = proto_lineinfo(pt) != NULL;
    /* Stripped Lua 5.4 chunks still emit a line hook for the first executed
    ** instruction, but the hook argument must be nil instead of LuaJIT's
    ** synthetic line 0.
    */
    BCLine line = hasline ? lj_debug_line(pt, npc) : -1;
    int skipline = 0;
    int sameline_cont = (hasline && npc > 0 && line == lj_debug_line(pt, npc-1));
    if (g->hook_skipline) {
      int32_t ci = (int32_t)((L->base-1) - tvref(L->stack));
      if (ci == g->hook_skipline_ci) {
	g->hook_skipline = 0;
	if (hasline && line == g->hook_skipline_line)
	  skipline = 1;
      } else if (ci < g->hook_skipline_ci) {
	/* The enabling frame has returned; do not carry same-line
	** suppression into unrelated lower frames.
	*/
	g->hook_skipline = 0;
      }
    }
    if (!skipline && !(opc >= pt->sizebc && sameline_cont) &&
	(pc <= oldpc || opc >= pt->sizebc ||
	 !hasline || line != lj_debug_line(pt, opc))) {
#else
    BCLine line = lj_debug_line(pt, npc);
    if (pc <= oldpc || opc >= pt->sizebc || line != lj_debug_line(pt, opc)) {
#endif
      callhook(L, LUA_HOOKLINE, line, 0, 0);
      L->top = L->base + slots;  /* Fix top again. */
    }
  }
  if ((g->hookmask & LUA_MASKRET) && bc_isret(bc_op(pc[-1]))) {
#if LJ_54
    if (g->hook_skipret) {
      g->hook_skipret--;
    } else
#endif
    {
      BCIns ins = pc[-1];
      BCReg first = bc_a(ins);
      uint32_t nres = 0;
#if LJ_54 && LJ_TARGET_ARM64
      int close_return_hook = arm64_lua54_close_return_hook(L);
#endif
      switch (bc_op(ins)) {
      case BC_RET1:
	nres = 1;
	break;
      case BC_RET:
	nres = bc_d(ins) - 1;
	break;
      case BC_RETM:
	nres = bc_d(ins) + cframe_multres_n(cf) - 1;
	break;
      default:
	break;
      }
      callhook(L, LUA_HOOKRET, -1, nres ? (uint16_t)(first + 1) : 0,
	       (uint16_t)nres);
#if LJ_54 && LJ_TARGET_ARM64
      if (close_return_hook && (g->hookmask & LUA_MASKRET))
	g->hook_skipret++;
#endif
    }
  }
  ERRNO_RESTORE
}

/* Initialize call. Ensure stack space and return # of missing parameters. */
static int call_init(lua_State *L, GCfunc *fn)
{
  if (isluafunc(fn)) {
    GCproto *pt = funcproto(fn);
    int numparams = pt->numparams;
    int gotparams = (int)(L->top - L->base);
    int need = pt->framesize;
    if ((pt->flags & PROTO_VARARG)) need += 1+LJ_FR2+gotparams;
    lj_state_checkstack(L, (MSize)need);
    numparams -= gotparams;
    return numparams >= 0 ? numparams : 0;
  } else {
    lj_state_checkstack(L, LUA_MINSTACK);
    return 0;
  }
}

/* Call dispatch. Used by call hooks, hot calls or when recording. */
ASMFunction LJ_FASTCALL lj_dispatch_call(lua_State *L, const BCIns *pc)
{
  ERRNO_SAVE
  GCfunc *fn = curr_func(L);
  BCOp op;
  global_State *g = G(L);
#if LJ_HASJIT
  jit_State *J = G2J(g);
#endif
  int missing = call_init(L, fn);
#if LJ_HASJIT
  J->L = L;
  if ((uintptr_t)pc & 1) {  /* Marker for hot call. */
#if LJ_54
    if (L->closelist != NULL)
      goto out;
#endif
#ifdef LUA_USE_ASSERT
    ptrdiff_t delta = L->top - L->base;
#endif
    pc = (const BCIns *)((uintptr_t)pc & ~(uintptr_t)1);
    lj_trace_hot(J, pc);
    lj_assertG(L->top - L->base == delta,
	       "unbalanced stack after hot call");
    goto out;
  } else if (J->state != LJ_TRACE_IDLE &&
#if LJ_54
	     L->closelist == NULL &&
#endif
	     !(g->hookmask & (HOOK_GC|HOOK_VMEVENT))) {
#ifdef LUA_USE_ASSERT
    ptrdiff_t delta = L->top - L->base;
#endif
    /* Record the FUNC* bytecodes, too. */
    lj_trace_ins(J, pc-1);  /* The interpreter bytecode PC is offset by 1. */
    lj_assertG(L->top - L->base == delta,
	       "unbalanced stack after hot instruction");
  }
#endif
  if ((g->hookmask & LUA_MASKCALL) && !hook_active(g)) {
    int i;
    int event = LUA_HOOKCALL;
    uint16_t nparams = 0;
    if (isluafunc(fn))
      nparams = funcproto(fn)->numparams;
    else {
      ptrdiff_t nargs = L->top - L->base;
      /* C functions are vararg in debug metadata; Lua 5.4 still reports the
      ** actual argument range transferred by this concrete call.
      */
      nparams = nargs > 65535 ? 65535u : (uint16_t)nargs;
    }
#if LJ_54
    if (isluafunc(fn) &&
	((int32_t)((L->base-1) - tvref(L->stack)) == L->tailcall_ci ||
	 (int32_t)((L->base-1) - tvref(L->stack)) == L->tailcall_ci2))
      event = LUA_HOOKTAILCALL;
#endif
    for (i = 0; i < missing; i++)  /* Add missing parameters. */
      setnilV(L->top++);
    callhook(L, event, -1, nparams ? 1 : 0, nparams);
    /* Preserve modifications of missing parameters by lua_setlocal(). */
    while (missing-- > 0 && tvisnil(L->top - 1))
      L->top--;
  }
#if LJ_54
  if ((g->hookmask & LUA_MASKLINE) && isluafunc(fn) && !hook_active(g)) {
    GCproto *pt = funcproto(fn);
    int hasline = proto_lineinfo(pt) != NULL;
    BCLine firstline = hasline ? lj_debug_line(pt, pt->sizebc > 1 ? 1 : 0) : -1;
    if (!hasline || firstline == pt->firstline) {
      int slots = (int)(L->top - L->base);
      /* Lua 5.4 reports a line event when entering a one-line function body.
      ** LuaJIT's normal line-dispatch path only sees later line changes, so
      ** synthesize the entry event only when it would otherwise be invisible.
      ** Stripped chunks use the same event, but expose nil for the line.
      */
      callhook(L, LUA_HOOKLINE, firstline, 0, 0);
      L->top = L->base + slots;
    }
  }
#endif
#if LJ_HASJIT
out:
#endif
  op = bc_op(pc[-1]);  /* Get FUNC* op. */
#if LJ_HASJIT
  /* Use the non-hotcounting variants if JIT is off or while recording. */
  if ((!(J->flags & JIT_F_ON) || J->state != LJ_TRACE_IDLE) &&
      (op == BC_FUNCF || op == BC_FUNCV))
    op = (BCOp)((int)op+(int)BC_IFUNCF-(int)BC_FUNCF);
#endif
  ERRNO_RESTORE
  return makeasmfunc(lj_bc_ofs[op]);  /* Return static dispatch target. */
}

#if LJ_HASJIT
/* Stitch a new trace. */
void LJ_FASTCALL lj_dispatch_stitch(jit_State *J, const BCIns *pc)
{
  if (!(J2G(J)->hookmask & HOOK_VMEVENT)) {
    ERRNO_SAVE
    lua_State *L = J->L;
    void *cf = cframe_raw(L->cframe);
    const BCIns *oldpc = cframe_pc(cf);
    setcframe_pc(cf, pc);
    /* Before dispatch, have to bias PC by 1. */
    L->top = L->base + cur_topslot(curr_proto(L), pc+1, cframe_multres_n(cf));
    lj_trace_stitch(J, pc-1);  /* Point to the CALL instruction. */
    setcframe_pc(cf, oldpc);
    ERRNO_RESTORE
  }
}
#endif

#if LJ_HASPROFILE
/* Profile dispatch. */
void LJ_FASTCALL lj_dispatch_profile(lua_State *L, const BCIns *pc)
{
  ERRNO_SAVE
  GCfunc *fn = curr_func(L);
  GCproto *pt = funcproto(fn);
  void *cf = cframe_raw(L->cframe);
  const BCIns *oldpc = cframe_pc(cf);
  global_State *g;
  setcframe_pc(cf, pc);
  L->top = L->base + cur_topslot(pt, pc, cframe_multres_n(cf));
  lj_profile_interpreter(L);
  setcframe_pc(cf, oldpc);
  g = G(L);
  setgcref(g->cur_L, obj2gco(L));
  setvmstate(g, INTERP);
  ERRNO_RESTORE
}
#endif
