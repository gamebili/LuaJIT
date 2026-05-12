/*
** Debugging and introspection.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_debug_c
#define LUA_CORE

#include <string.h>

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_buf.h"
#include "lj_tab.h"
#include "lj_state.h"
#include "lj_frame.h"
#include "lj_bc.h"
#include "lj_strfmt.h"
#if LJ_HASJIT
#include "lj_jit.h"
#endif

#if LJ_54
/* lj_debug.c is built before generated lj_ffdef.h exists. These IDs match the
** stable base-library order in lj_ffdef.h and are only used for traceback names.
*/
#define LJ_FFID_PCALL	22
#define LJ_FFID_XPCALL	23
#define LJ_FFID_MATH_SIN	46
#endif

/* -- Frames -------------------------------------------------------------- */

/* Get frame corresponding to a level. */
cTValue *lj_debug_frame(lua_State *L, int level, int *size)
{
  cTValue *frame, *nextframe, *bot = tvref(L->stack)+LJ_FR2;
  /* Traverse frames backwards. */
  for (nextframe = frame = L->base-1; frame > bot; ) {
    if (frame_gc(frame) == obj2gco(L))
      level++;  /* Skip dummy frames. See lj_err_optype_call(). */
#if LJ_54
    if (L->close_pcall && frame_ispcall(frame))
      level++;  /* Hide the compiler-internal pcall around __close. */
#endif
    if (level-- == 0) {
      *size = (int)(nextframe - frame);
      return frame;  /* Level found. */
    }
    nextframe = frame;
    if (frame_islua(frame)) {
      frame = frame_prevl(frame);
    } else {
      if (frame_isvarg(frame))
	level++;  /* Skip vararg pseudo-frame. */
      frame = frame_prevd(frame);
    }
  }
  *size = level;
  return NULL;  /* Level not found. */
}

/* Invalid bytecode position. */
#define NO_BCPOS	(~(BCPos)0)

/* Return bytecode position for function/frame or NO_BCPOS. */
static BCPos debug_framepc(lua_State *L, GCfunc *fn, cTValue *nextframe)
{
  const BCIns *ins;
  GCproto *pt;
  BCPos pos;
  lj_assertL(fn->c.gct == ~LJ_TFUNC || fn->c.gct == ~LJ_TTHREAD,
	     "function or frame expected");
  if (!isluafunc(fn)) {  /* Cannot derive a PC for non-Lua functions. */
    return NO_BCPOS;
  } else if (nextframe == NULL) {  /* Lua function on top. */
    void *cf = cframe_raw(L->cframe);
    if (cf == NULL || (char *)cframe_pc(cf) == (char *)cframe_L(cf))
      return NO_BCPOS;
    ins = cframe_pc(cf);  /* Only happens during error/hook handling. */
    if (!ins) return NO_BCPOS;
  } else {
    if (frame_islua(nextframe)) {
      ins = frame_pc(nextframe);
    } else if (frame_iscont(nextframe)) {
      ins = frame_contpc(nextframe);
    } else {
      /* Lua function below errfunc/gc/hook: find cframe to get the PC. */
      void *cf = cframe_raw(L->cframe);
      TValue *f = L->base-1;
      for (;;) {
	if (cf == NULL)
	  return NO_BCPOS;
	while (cframe_nres(cf) < 0) {
	  if (f >= restorestack(L, -cframe_nres(cf)))
	    break;
	  cf = cframe_raw(cframe_prev(cf));
	  if (cf == NULL)
	    return NO_BCPOS;
	}
	if (f < nextframe)
	  break;
	if (frame_islua(f)) {
	  f = frame_prevl(f);
	} else {
	  if (frame_isc(f) || (frame_iscont(f) && frame_iscont_fficb(f)))
	    cf = cframe_raw(cframe_prev(cf));
	  f = frame_prevd(f);
	}
      }
      ins = cframe_pc(cf);
      if (!ins) return NO_BCPOS;
    }
  }
  pt = funcproto(fn);
  pos = proto_bcpos(pt, ins) - 1;
#if LJ_HASJIT
  if (pos == NO_BCPOS) return 1;  /* Pretend it's the first bytecode. */
  if (pos > pt->sizebc) {  /* Undo the effects of lj_trace_exit for JLOOP. */
    if (bc_isret(bc_op(ins[-1]))) {
      GCtrace *T = (GCtrace *)((char *)(ins-1) - offsetof(GCtrace, startins));
      pos = proto_bcpos(pt, mref(T->startpc, const BCIns));
    } else {
      pos = NO_BCPOS;  /* Punt in case of stack overflow for stitched trace. */
    }
  }
#endif
  return pos;
}

BCPos lj_debug_framepc(lua_State *L, GCfunc *fn, cTValue *nextframe)
{
  return debug_framepc(L, fn, nextframe);
}

/* -- Line numbers -------------------------------------------------------- */

/* Get line number for a bytecode position. */
BCLine LJ_FASTCALL lj_debug_line(GCproto *pt, BCPos pc)
{
  const void *lineinfo = proto_lineinfo(pt);
  if (pc <= pt->sizebc && lineinfo) {
    BCLine first = pt->firstline;
    if (pc == pt->sizebc) return first + pt->numline;
    if (pc-- == 0) return first;
    if (pt->numline < 256)
      return first + (BCLine)((const uint8_t *)lineinfo)[pc];
    else if (pt->numline < 65536)
      return first + (BCLine)((const uint16_t *)lineinfo)[pc];
    else
      return first + (BCLine)((const uint32_t *)lineinfo)[pc];
  }
  return 0;
}

/* Get line number for function/frame. */
static BCLine debug_frameline(lua_State *L, GCfunc *fn, cTValue *nextframe)
{
  BCPos pc = debug_framepc(L, fn, nextframe);
  if (pc != NO_BCPOS) {
    GCproto *pt = funcproto(fn);
    lj_assertL(pc <= pt->sizebc, "PC out of range");
#if LJ_54
    if (proto_lineinfo(pt) == NULL)
      return -1;
#endif
    return lj_debug_line(pt, pc);
  }
  return -1;
}

/* -- Variable names ------------------------------------------------------ */

/* Get name of a local variable from slot number and PC. */
static const char *debug_varname(const GCproto *pt, BCPos pc, BCReg slot)
{
  const char *p = (const char *)proto_varinfo(pt);
  if (p) {
    BCPos lastpc = 0;
    for (;;) {
      const char *name = p;
      uint32_t vn = *(const uint8_t *)p;
      BCPos startpc, endpc;
      if (vn < VARNAME__MAX) {
	if (vn == VARNAME_END) break;  /* End of varinfo. */
      } else {
	do { p++; } while (*(const uint8_t *)p);  /* Skip over variable name. */
      }
      p++;
      lastpc = startpc = lastpc + lj_buf_ruleb128(&p);
      if (startpc > pc) break;
      endpc = startpc + lj_buf_ruleb128(&p);
      if (pc < endpc && slot-- == 0) {
	if (vn < VARNAME__MAX) {
#define VARNAMESTR(name, str)	str "\0"
	  name = VARNAMEDEF(VARNAMESTR);
#undef VARNAMESTR
	  if (--vn) while (*name++ || --vn) ;
	}
	return name;
      }
    }
  }
  return NULL;
}

/* Get name of local variable from 1-based slot number and function/frame. */
static GCfunc *debug_framefunc(lua_State *L, cTValue *frame);

#if LJ_54
static int debug_lua54_isname(const char *name, const char *want)
{
  return name != NULL && strcmp(name, want) == 0;
}

static int debug_lua54_isforgroup(GCproto *pt, BCPos pc, int start)
{
  if (start < 0)
    return 0;
  return debug_lua54_isname(debug_varname(pt, pc, (BCReg)start),
			    "(for state)") &&
	 debug_lua54_isname(debug_varname(pt, pc, (BCReg)(start+1)),
			    "(for generator)") &&
	 debug_lua54_isname(debug_varname(pt, pc, (BCReg)(start+2)),
			    "(for state)") &&
	 debug_lua54_isname(debug_varname(pt, pc, (BCReg)(start+3)),
			    "(for control)");
}

static TValue *debug_lua54_forlocal(GCproto *pt, BCPos pc, TValue *frame,
				    TValue *nextframe, BCReg slot1,
				    const char **name)
{
  int slot0 = (int)slot1 - 1;
  int ofs;
  for (ofs = 0; ofs < 4 && slot0 >= ofs; ofs++) {
    int start = slot0 - ofs;
    if (debug_lua54_isforgroup(pt, pc, start)) {
      /* LuaJIT keeps the generic-for closing value before gen/state/control so
      ** ITERC/ITERL can keep their old slot layout. Lua 5.4 debug APIs expose
      ** the hidden variables in source order: gen, state, control, close.
      */
      TValue *o = ofs < 3 ? frame + slot1 + 1 : frame + slot1 - 3;
      if (o < nextframe) {
	*name = "(for state)";
	return o;
      }
      break;
    }
  }
  return NULL;
}
#endif

static TValue *debug_localname(lua_State *L, const lua_Debug *ar,
			       const char **name, BCReg slot1)
{
  uint32_t ci = (uint32_t)LJ_DEBUG_CI_VALUE(ar->i_ci);
  uint32_t offset = ci & 0xffff;
  uint32_t size = ci >> 16;
  TValue *frame = tvref(L->stack) + offset;
  TValue *nextframe = size ? frame + size : NULL;
  GCfunc *fn = debug_framefunc(L, frame);
  BCPos pc = debug_framepc(L, fn, nextframe);
  if (!nextframe) nextframe = L->top+LJ_FR2;
  if ((int)slot1 < 0) {  /* Negative slot number is for varargs. */
    if (pc != NO_BCPOS) {
      GCproto *pt = funcproto(fn);
      if ((pt->flags & PROTO_VARARG)) {
	slot1 = pt->numparams + (BCReg)(-(int)slot1);
	if (frame_isvarg(frame)) {  /* Vararg frame has been set up? (pc!=0) */
	  nextframe = frame;
	  frame = frame_prevd(frame);
	}
	if (frame + slot1+LJ_FR2 < nextframe) {
#if LJ_54
	  *name = "(vararg)";
#else
	  *name = "(*vararg)";
#endif
	  return frame+slot1;
	}
      }
    }
    return NULL;
  }
#if LJ_54
  if (pc != NO_BCPOS && isluafunc(fn)) {
    TValue *o = debug_lua54_forlocal(funcproto(fn), pc, frame, nextframe,
				     slot1, name);
    if (o != NULL)
      return o;
  }
#endif
  if (pc != NO_BCPOS &&
      (*name = debug_varname(funcproto(fn), pc, slot1-1)) != NULL)
    ;
  else if (slot1 > 0 && frame + slot1+LJ_FR2 < nextframe)
#if LJ_54
    *name = isluafunc(fn) ? "(temporary)" : "(C temporary)";
#else
    *name = "(*temporary)";
#endif
  return frame+slot1;
}

/* Get name of upvalue. */
const char *lj_debug_uvname(GCproto *pt, uint32_t idx)
{
  const uint8_t *p = proto_uvinfo(pt);
  lj_assertX(idx < pt->sizeuv, "bad upvalue index");
  if (!p) return "";
  if (idx) while (*p++ || --idx) ;
  return (const char *)p;
}

int lj_debug_hasenvuv(GCfunc *fn)
{
#if LJ_54
  GCproto *pt;
  BCIns *bc;
  MSize i;
  const char *uvname;
  if (!isluafunc(fn))
    return 0;
  pt = funcproto(fn);
  uvname = pt->sizeuv > 0 ? lj_debug_uvname(pt, 0) : "";
  if (uvname[0] == '_' && uvname[1] == 'E' && uvname[2] == 'N' &&
      uvname[3] == 'V' && uvname[4] == '\0')
    return 0;  /* A lexical _ENV already exists as a real upvalue. */
  bc = proto_bc(pt);
  for (i = 1; i < pt->sizebc; i++) {
    BCOp op = bc_op(bc[i]);
    if (op == BC_GGET || op == BC_GSET)
      return 1;  /* Free names still use LuaJIT's function env internally. */
  }
  if (pt->firstline == 0 && proto_uvinfo(pt) != NULL)
    return 1;  /* Source main chunks in Lua 5.4 always expose _ENV. */
#else
  UNUSED(fn);
#endif
  return 0;
}

/* Get name and value of upvalue. */
const char *lj_debug_uvnamev(cTValue *o, uint32_t idx, TValue **tvp, GCobj **op)
{
  if (tvisfunc(o)) {
    GCfunc *fn = funcV(o);
    if (isluafunc(fn)) {
      GCproto *pt = funcproto(fn);
      if (idx < pt->sizeuv) {
	GCobj *uvo = gcref(fn->l.uvptr[idx]);
	const char *name = lj_debug_uvname(pt, idx);
	*tvp = uvval(&uvo->uv);
	*op = uvo;
#if LJ_54
	return *name ? name : "(no name)";
#else
	return name;
#endif
      }
    } else {
      if (idx < fn->c.nupvalues) {
	*tvp = &fn->c.upvalue[idx];
	*op = obj2gco(fn);
	return "";
      }
    }
  }
  return NULL;
}

#if LJ_54
static int debug_is_lua54_env_source(const char *kind, const char *name)
{
  return name != NULL && kind != NULL &&
	 (strcmp(kind, "local") == 0 || strcmp(kind, "upvalue") == 0) &&
	 strcmp(name, "_ENV") == 0;
}

static int debug_lua54_keyslot_is_temp(GCproto *pt, const BCIns *ip,
				       BCReg slot)
{
  return debug_varname(pt, proto_bcpos(pt, ip), slot) == NULL;
}

static int debug_lua54_name_skipped_by_jmp(GCproto *pt, const BCIns *origin,
					   const BCIns *candidate)
{
  const BCIns *bc = proto_bc(pt);
  const BCIns *ip;
  for (ip = candidate; --ip > bc; ) {
    if (bc_op(*ip) == BC_JMP) {
      const BCIns *target = ip + bc_j(*ip) + 1;
      /* A short-circuit join can execute with a value from before the jump,
      ** so names from the skipped arm must not be reported as operand names.
      */
      if (ip < candidate && candidate < target && target <= origin)
	return 1;
    }
  }
  return 0;
}
#endif

/* Deduce name of an object from slot number and PC. */
const char *lj_debug_slotname(GCproto *pt, const BCIns *ip, BCReg slot,
			      const char **name)
{
  const char *lname;
#if LJ_54
  const BCIns *origin = ip;
  const char *kind;
  if (bc_op(*origin) == BC_ITERC && slot == bc_a(*origin)) {
    *name = "for iterator";
    return "for iterator";
  }
#endif
restart:
  lname = debug_varname(pt, proto_bcpos(pt, ip), slot);
  if (lname != NULL) { *name = lname; return "local"; }
  while (--ip > proto_bc(pt)) {
    BCIns ins = *ip;
    BCOp op = bc_op(ins);
    BCReg ra = bc_a(ins);
    if (bcmode_a(op) == BCMbase) {
      if (slot >= ra && (op != BC_KNIL || slot <= bc_d(ins)))
	return NULL;
    } else if (bcmode_a(op) == BCMdst && ra == slot) {
      switch (bc_op(ins)) {
      case BC_MOV:
	if (ra == slot) { slot = bc_d(ins); goto restart; }
	break;
      case BC_GGET:
#if LJ_54
	if (debug_lua54_name_skipped_by_jmp(pt, origin, ip))
	  return NULL;
#endif
	*name = strdata(gco2str(proto_kgc(pt, ~(ptrdiff_t)bc_d(ins))));
	return "global";
      case BC_TGETS:
#if LJ_54
	if (debug_lua54_name_skipped_by_jmp(pt, origin, ip))
	  return NULL;
#endif
	*name = strdata(gco2str(proto_kgc(pt, ~(ptrdiff_t)bc_c(ins))));
#if LJ_54
	kind = lj_debug_slotname(pt, ip, bc_b(ins), &lname);
	if (debug_is_lua54_env_source(kind, lname))
	  return "global";
#endif
	if (ip > proto_bc(pt)) {
	  BCIns insp = ip[-1];
	  if (bc_op(insp) == BC_MOV && bc_a(insp) == ra+1+LJ_FR2 &&
	      bc_d(insp) == bc_b(ins))
	    return "method";
	}
	return "field";
#if LJ_54
      case BC_TGETV:
#if LJ_54
	if (debug_lua54_name_skipped_by_jmp(pt, origin, ip))
	  return NULL;
#endif
	if (ip > proto_bc(pt)) {
	  BCIns insp = ip[-1];
	  /* Lua 5.4 global access can lower _ENV.name to KSTR + TGETV when a
	  ** large chunk pushes the field name outside TGETS' 8-bit constant slot.
	  ** Preserve debug.getinfo(..., "n") names for hooks and errors.
	  */
	  if (bc_op(insp) == BC_KSTR && bc_a(insp) == bc_c(ins) &&
	      debug_lua54_keyslot_is_temp(pt, ip, bc_c(ins))) {
	    *name = strdata(gco2str(proto_kgc(pt, ~(ptrdiff_t)bc_d(insp))));
	    kind = lj_debug_slotname(pt, ip, bc_b(ins), &lname);
	    if (debug_is_lua54_env_source(kind, lname))
	      return "global";
	    if (ip > proto_bc(pt)+1) {
	      BCIns inspm = ip[-2];
	      if (bc_op(inspm) == BC_MOV && bc_a(inspm) == ra+1+LJ_FR2 &&
		  bc_d(inspm) == bc_b(ins))
		return "method";
	    }
	    return "field";
	  }
	}
	*name = "?";
	kind = lj_debug_slotname(pt, ip, bc_b(ins), &lname);
	return debug_is_lua54_env_source(kind, lname) ? "global" : "field";
#endif
      case BC_UGET:
#if LJ_54
	if (debug_lua54_name_skipped_by_jmp(pt, origin, ip))
	  return NULL;
#endif
	*name = lj_debug_uvname(pt, bc_d(ins));
	return "upvalue";
      default:
	return NULL;
      }
    }
  }
  return NULL;
}

/* Deduce function name from caller of a frame. */
#if LJ_54
static int debug_is_lua54_closecall(GCproto *pt, const BCIns *ip, BCReg slot)
{
  const BCIns *bc = proto_bc(pt);
  int nscan = 0;
  while (ip > bc && nscan++ < 10) {
    BCIns ins = *--ip;
    BCOp op = bc_op(ins);
    if (op == BC_TGETS && bc_a(ins) == slot) {
      GCstr *field = gco2str(proto_kgc(pt, ~(ptrdiff_t)bc_c(ins)));
      if (field->len == 17 &&
	  memcmp(strdata(field), "_lua54_closevalue", 17) == 0)
	return 1;
    }
  }
  return 0;
}

static const char *debug_lua54_tail_ffname(GCfunc *fn)
{
  /* Tail calls replace the Lua caller frame, so argument errors from fast
  ** functions cannot recover names from caller bytecode. Keep the official
  ** Lua 5.4-visible name for covered fast functions here.
  */
  if (isffunc(fn) && fn->c.ffid == LJ_FFID_MATH_SIN)
    return "sin";
  return NULL;
}
#endif

const char *lj_debug_funcname(lua_State *L, cTValue *frame, const char **name)
{
  cTValue *pframe;
  GCfunc *fn;
  BCPos pc;
  if (frame <= tvref(L->stack)+LJ_FR2)
    return NULL;
  if (frame_isvarg(frame))
    frame = frame_prevd(frame);
#if LJ_54
  if (G(L)->debug_mmname && G(L)->debug_mmfunc &&
      frame_func(frame) == G(L)->debug_mmfunc) {
    *name = G(L)->debug_mmname;
    return "metamethod";
  }
#endif
  pframe = frame_prev(frame);
#if LJ_54
  if (hook_active(G(L)) && G(L)->hook_L == L &&
      (int)(pframe - tvref(L->stack)) == G(L)->hook_ci) {
    *name = "?";
    return "hook";
  }
#endif
  fn = frame_func(pframe);
  pc = debug_framepc(L, fn, frame);
  if (pc != NO_BCPOS) {
    GCproto *pt = funcproto(fn);
    const BCIns *ip = &proto_bc(pt)[check_exp(pc < pt->sizebc, pc)];
    MMS mm = bcmode_mm(bc_op(*ip));
#if LJ_54
    if (bc_op(*ip) == BC_ITERC || bc_op(*ip) == BC_ITERN) {
      *name = "for iterator";
      return "for iterator";
    }
#endif
    if (mm == MM_call) {
      BCReg slot = bc_a(*ip);
      if (bc_op(*ip) == BC_ITERC) slot -= 3;
#if LJ_54
      /* Parser-emitted to-be-closed calls are ordinary Lua calls so __close can
      ** yield. Recognize the preceding close helper and present the frame like
      ** Lua 5.4's "metamethod 'close'" without adding a non-yieldable wrapper.
      */
      if (debug_is_lua54_closecall(pt, ip, slot)) {
	*name = "close";
	return "metamethod";
      }
#endif
      return lj_debug_slotname(pt, ip, slot, name);
    } else if (mm != MM__MAX) {
      *name = strdata(mmname_str(G(L), mm));
#if LJ_54
      if ((*name)[0] == '_' && (*name)[1] == '_')
	*name += 2;
#endif
      return "metamethod";
    }
  }
#if LJ_54
  {
    const char *ffname = debug_lua54_tail_ffname(frame_func(frame));
    if (ffname) {
      *name = ffname;
      return "function";
    }
  }
#endif
  return NULL;
}

#if LJ_54
static int debug_call_from_pcall54(lua_State *L)
{
  cTValue *frame = L->base-1;
  cTValue *pframe;
  if (frame <= tvref(L->stack)+LJ_FR2)
    return 0;
  if (frame_isvarg(frame))
    frame = frame_prevd(frame);
  if (frame <= tvref(L->stack)+LJ_FR2)
    return 0;
  pframe = frame_prev(frame);
  if (frame_ispcall(frame))
    return 1;  /* pcall(function, ...) has no source call slot to name. */
  return frame_ispcall(pframe);
}

const char *lj_debug_callname54(lua_State *L, const char *fallback,
				const char *prefix)
{
  const char *name = "?";
  const char *kind = lj_debug_funcname(L, L->base-1, &name);
  int direct_pcall = debug_call_from_pcall54(L);
  size_t prefixlen = prefix ? strlen(prefix) : 0;
  int hasprefix = prefix && strncmp(fallback, prefix, prefixlen) == 0 &&
		  fallback[prefixlen] == '.';
  if (kind && name) {
    /* Direct pcall(lib.fn, ...) has no bytecode field/local call site. Keep the
    ** full fallback name there, but use source-level names for real calls.
    */
    if (direct_pcall && strcmp(kind, "function") == 0)
      return fallback;
    if (name[0] == '?' && name[1] == '\0')
      return name;
    if (!direct_pcall && hasprefix &&
	strncmp(name, fallback, strlen(fallback)+1) == 0)
      return fallback + prefixlen + 1;
    return name;
  }
  if (direct_pcall)
    return fallback;
  return hasprefix ? fallback + prefixlen + 1 : fallback;
}
#endif

/* -- Source code locations ----------------------------------------------- */

/* Generate shortened source name. */
void lj_debug_shortname(char *out, GCstr *str, BCLine line)
{
  const char *src = strdata(str);
  if (*src == '=') {
    strncpy(out, src+1, LUA_IDSIZE);  /* Remove first char. */
    out[LUA_IDSIZE-1] = '\0';  /* Ensures null termination. */
  } else if (*src == '@') {  /* Output "source", or "...source". */
    size_t len = str->len-1;
    src++;  /* Skip the `@' */
    if (len >= LUA_IDSIZE) {
      src += len-(LUA_IDSIZE-4);  /* Get last part of file name. */
      *out++ = '.'; *out++ = '.'; *out++ = '.';
    }
    strcpy(out, src);
  } else {  /* Output [string "string"] or [builtin:name]. */
    size_t len;  /* Length, up to first control char. */
    for (len = 0; len < LUA_IDSIZE-12; len++)
      if (((const unsigned char *)src)[len] < ' ') break;
    strcpy(out, line == ~(BCLine)0 ? "[builtin:" : "[string \""); out += 9;
    if (src[len] != '\0') {  /* Must truncate? */
      if (len > LUA_IDSIZE-15) len = LUA_IDSIZE-15;
      strncpy(out, src, len); out += len;
      strcpy(out, "..."); out += 3;
    } else {
      strcpy(out, src); out += len;
    }
    strcpy(out, line == ~(BCLine)0 ? "]" : "\"]");
  }
}

static GCfunc *debug_framefunc(lua_State *L, cTValue *frame)
{
  UNUSED(L);
  return frame_func(frame);
}

/* Add current location of a frame to error message. */
void lj_debug_addloc(lua_State *L, const char *msg,
		     cTValue *frame, cTValue *nextframe)
{
  if (frame) {
    GCfunc *fn = debug_framefunc(L, frame);
    if (isluafunc(fn)) {
      BCLine line = debug_frameline(L, fn, nextframe);
#if LJ_54
      /* Lua 5.4 runtime errors still report stripped chunks as "?:-1:",
      ** but luaL_where() itself must suppress non-positive line locations.
      */
      if ((line >= 0 || line == -1) && (*msg != '\0' || line > 0)) {
#else
      if (line >= 0) {
#endif
	GCproto *pt = funcproto(fn);
	char buf[LUA_IDSIZE];
	lj_debug_shortname(buf, proto_chunkname(pt), pt->firstline);
	lj_strfmt_pushf(L, "%s:%d: %s", buf, line, msg);
	return;
      }
    }
  }
  lj_strfmt_pushf(L, "%s", msg);
}

/* Push location string for a bytecode position to Lua stack. */
void lj_debug_pushloc(lua_State *L, GCproto *pt, BCPos pc)
{
  GCstr *name = proto_chunkname(pt);
  const char *s = strdata(name);
  MSize i, len = name->len;
  BCLine line = lj_debug_line(pt, pc);
  if (pt->firstline == ~(BCLine)0) {
    lj_strfmt_pushf(L, "builtin:%s", s);
  } else if (*s == '@') {
    s++; len--;
    for (i = len; i > 0; i--)
      if (s[i] == '/' || s[i] == '\\') {
	s += i+1;
	break;
      }
    lj_strfmt_pushf(L, "%s:%d", s, line);
  } else if (len > 40) {
    lj_strfmt_pushf(L, "%p:%d", pt, line);
  } else if (*s == '=') {
    lj_strfmt_pushf(L, "%s:%d", s+1, line);
  } else {
    lj_strfmt_pushf(L, "\"%s\":%d", s, line);
  }
}

/* -- Public debug API ---------------------------------------------------- */

/* lua_getupvalue() and lua_setupvalue() are in lj_api.c. */

#if LJ_54
static int debug_istailcall(lua_State *L, cTValue *frame)
{
  if (frame == NULL)
    return 0;
  if (frame_isvarg(frame))
    frame = frame_prevd(frame);
  /* Vararg pseudo-frames sit above the real Lua frame. The VM stores the
  ** marker on the real frame, so normalize before exposing Lua 5.4 metadata.
  */
  if (frame_islua(frame)) {
    int32_t ci = (int32_t)(frame - tvref(L->stack));
    return L->tailcall_ci == ci || L->tailcall_ci2 == ci;
  }
  return 0;
}
#endif

LUA_API const char *lua_getlocal(lua_State *L, const lua_Debug *ar, int n)
{
  const char *name = NULL;
  if (ar) {
    TValue *o = debug_localname(L, ar, &name, (BCReg)n);
    if (name) {
      copyTV(L, L->top, o);
      incr_top(L);
    }
  } else if (tvisfunc(L->top-1) && isluafunc(funcV(L->top-1))) {
    name = debug_varname(funcproto(funcV(L->top-1)), 0, (BCReg)n-1);
  }
  return name;
}

LUA_API const char *lua_setlocal(lua_State *L, const lua_Debug *ar, int n)
{
  const char *name = NULL;
  TValue *o = debug_localname(L, ar, &name, (BCReg)n);
  if (name)
    copyTV(L, o, L->top-1);
  L->top--;
  return name;
}

int lj_debug_getinfo(lua_State *L, const char *what, lj_Debug *ar, int ext)
{
  int opt_f = 0, opt_L = 0;
  TValue *frame = NULL;
  TValue *nextframe = NULL;
  GCfunc *fn;
  if (ext) {
    ar->nparams = 0;
    ar->isvararg = 0;
    ar->istailcall = 0;
    ar->ftransfer = 0;
    ar->ntransfer = 0;
  }
  if (*what == '>') {
    TValue *func = L->top - 1;
    if (!tvisfunc(func)) return 0;
    fn = funcV(func);
    L->top--;
    what++;
  } else {
    uint32_t ci = (uint32_t)LJ_DEBUG_CI_VALUE(ar->i_ci);
    uint32_t offset = ci & 0xffff;
    uint32_t size = ci >> 16;
    lj_assertL(offset != 0, "bad frame offset");
    frame = tvref(L->stack) + offset;
    if (size) nextframe = frame + size;
    lj_assertL(frame <= tvref(L->maxstack) &&
	       (!nextframe || nextframe <= tvref(L->maxstack)),
	       "broken frame chain");
    fn = debug_framefunc(L, frame);
    lj_assertL(fn->c.gct == ~LJ_TFUNC, "bad frame function");
  }
  for (; *what; what++) {
    if (*what == 'S') {
      if (isluafunc(fn)) {
	GCproto *pt = funcproto(fn);
	BCLine firstline = pt->firstline;
	GCstr *name = proto_chunkname(pt);
#if LJ_54
	if (proto_lineinfo(pt) == NULL && firstline == 0 && pt->numline == 0)
	  firstline = 1;
#endif
	ar->source = strdata(name);
#if LJ_54
	ar->srclen = name->len;
#endif
	lj_debug_shortname(ar->short_src, name, pt->firstline);
	ar->linedefined = (int)firstline;
	ar->lastlinedefined = (int)(firstline + pt->numline);
	ar->what = (firstline || !pt->numline) ? "Lua" : "main";
      } else {
	ar->source = "=[C]";
#if LJ_54
	ar->srclen = 4;
#endif
	ar->short_src[0] = '[';
	ar->short_src[1] = 'C';
	ar->short_src[2] = ']';
	ar->short_src[3] = '\0';
	ar->linedefined = -1;
	ar->lastlinedefined = -1;
	ar->what = "C";
      }
    } else if (*what == 'l') {
      ar->currentline = frame ? debug_frameline(L, fn, nextframe) : -1;
    } else if (*what == 'u') {
      ar->nups = (isffunc(fn) && fn->c.nupvalues <= 1) ? 0 : fn->c.nupvalues;
#if LJ_54
      if (lj_debug_hasenvuv(fn))
	ar->nups++;
#endif
      if (ext) {
	if (isluafunc(fn)) {
	  GCproto *pt = funcproto(fn);
	  ar->nparams = pt->numparams;
	  ar->isvararg = !!(pt->flags & PROTO_VARARG);
	} else {
	  ar->nparams = 0;
	  ar->isvararg = 1;
	}
      }
    } else if (*what == 'n') {
      ar->namewhat = frame ? lj_debug_funcname(L, frame, &ar->name) : NULL;
      if (ar->namewhat == NULL) {
	ar->namewhat = "";
	ar->name = NULL;
      }
    } else if (*what == 'f') {
      opt_f = 1;
    } else if (*what == 'L') {
      opt_L = 1;
    } else if (*what == 't') {
      if (ext)
	ar->istailcall =
#if LJ_54
	  debug_istailcall(L, frame);
#else
	  0;
#endif
      continue;
    } else if (*what == 'r') {
      /* Lua 5.4 exposes transferred argument/result ranges while a hook is
      ** running. Outside that exact hooked frame, the conservative answer is
      ** still an empty range.
      */
      if (ext) {
	global_State *g = G(L);
	if (frame && hook_active(g) && g->hook_L == L &&
	    ((int)(frame - tvref(L->stack)) == g->hook_ci)) {
	  ar->ftransfer = g->hook_ftransfer;
	  ar->ntransfer = g->hook_ntransfer;
	} else {
	  ar->ftransfer = 0;
	  ar->ntransfer = 0;
	}
      }
      continue;
    } else {
      return 0;  /* Bad option. */
    }
  }
  if (opt_f) {
    setfuncV(L, L->top, fn);
    incr_top(L);
  }
  if (opt_L) {
    if (isluafunc(fn)) {
      GCtab *t = lj_tab_new(L, 0, 0);
      GCproto *pt = funcproto(fn);
      const void *lineinfo = proto_lineinfo(pt);
      if (lineinfo) {
	BCLine first = pt->firstline;
	int sz = pt->numline < 256 ? 1 : pt->numline < 65536 ? 2 : 4;
	MSize i, szl = pt->sizebc-1;
	for (i = 0; i < szl; i++) {
	  BCLine line = first +
	    (sz == 1 ? (BCLine)((const uint8_t *)lineinfo)[i] :
	     sz == 2 ? (BCLine)((const uint16_t *)lineinfo)[i] :
	     (BCLine)((const uint32_t *)lineinfo)[i]);
	  setboolV(lj_tab_setint(L, t, line), 1);
	}
#if LJ_54
	setboolV(lj_tab_setint(L, t, first + pt->numline), 1);
#endif
      }
      settabV(L, L->top, t);
    } else {
      setnilV(L->top);
    }
    incr_top(L);
  }
  return 1;  /* Ok. */
}

LUA_API int lua_getinfo(lua_State *L, const char *what, lua_Debug *ar)
{
  return lj_debug_getinfo(L, what, (lj_Debug *)ar, 1);
}

LUA_API int lua_getstack(lua_State *L, int level, lua_Debug *ar)
{
  int size;
  cTValue *frame = lj_debug_frame(L, level, &size);
  if (frame) {
    ar->i_ci = LJ_DEBUG_CI_ENCODE((size << 16) + (int)(frame - tvref(L->stack)));
    return 1;
  } else {
    ar->i_ci = LJ_DEBUG_CI_ENCODE(level - size);
    return 0;
  }
}

#if LJ_HASPROFILE
/* Put the chunkname into a buffer. */
static int debug_putchunkname(SBuf *sb, GCproto *pt, int pathstrip)
{
  GCstr *name = proto_chunkname(pt);
  const char *p = strdata(name);
  if (pt->firstline == ~(BCLine)0) {
    lj_buf_putmem(sb, "[builtin:", 9);
    lj_buf_putstr(sb, name);
    lj_buf_putb(sb, ']');
    return 0;
  }
  if (*p == '=' || *p == '@') {
    MSize len = name->len-1;
    p++;
    if (pathstrip) {
      int i;
      for (i = len-1; i >= 0; i--)
	if (p[i] == '/' || p[i] == '\\') {
	  len -= i+1;
	  p = p+i+1;
	  break;
	}
    }
    lj_buf_putmem(sb, p, len);
  } else {
    lj_buf_putmem(sb, "[string]", 8);
  }
  return 1;
}

/* Put a compact stack dump into a buffer. */
void lj_debug_dumpstack(lua_State *L, SBuf *sb, const char *fmt, int depth)
{
  int level = 0, dir = 1, pathstrip = 1;
  MSize lastlen = 0;
  if (depth < 0) { level = ~depth; depth = dir = -1; }  /* Reverse frames. */
  while (level != depth) {  /* Loop through all frame. */
    int size;
    cTValue *frame = lj_debug_frame(L, level, &size);
    if (frame) {
      cTValue *nextframe = size ? frame+size : NULL;
      GCfunc *fn = debug_framefunc(L, frame);
      const uint8_t *p = (const uint8_t *)fmt;
      int c;
      while ((c = *p++)) {
	switch (c) {
	case 'p':  /* Preserve full path. */
	  pathstrip = 0;
	  break;
	case 'F': case 'f': {  /* Dump function name. */
	  const char *name;
	  const char *what = lj_debug_funcname(L, frame, &name);
	  if (what) {
	    if (c == 'F' && isluafunc(fn)) {  /* Dump module:name for 'F'. */
	      GCproto *pt = funcproto(fn);
	      if (pt->firstline != ~(BCLine)0) {  /* Not a bytecode builtin. */
		debug_putchunkname(sb, pt, pathstrip);
		lj_buf_putb(sb, ':');
	      }
	    }
	    lj_buf_putmem(sb, name, (MSize)strlen(name));
	    break;
	  }  /* else: can't derive a name, dump module:line. */
	  }
	  /* fallthrough */
	case 'l':  /* Dump module:line. */
	  if (isluafunc(fn)) {
	    GCproto *pt = funcproto(fn);
	    if (debug_putchunkname(sb, pt, pathstrip)) {
	      /* Regular Lua function. */
	      BCLine line = c == 'l' ? debug_frameline(L, fn, nextframe) :
				       pt->firstline;
	      lj_buf_putb(sb, ':');
	      lj_strfmt_putint(sb, line >= 0 ? line : pt->firstline);
	    }
	  } else if (isffunc(fn)) {  /* Dump numbered builtins. */
	    lj_buf_putmem(sb, "[builtin#", 9);
	    lj_strfmt_putint(sb, fn->c.ffid);
	    lj_buf_putb(sb, ']');
	  } else {  /* Dump C function address. */
	    lj_buf_putb(sb, '@');
	    lj_strfmt_putptr(sb, fn->c.f);
	  }
	  break;
	case 'Z':  /* Zap trailing separator. */
	  lastlen = sbuflen(sb);
	  break;
	default:
	  lj_buf_putb(sb, c);
	  break;
	}
      }
    } else if (dir == 1) {
      break;
    } else {
      level -= size;  /* Reverse frame order: quickly skip missing level. */
    }
    level += dir;
  }
  if (lastlen)
    sb->w = sb->b + lastlen;  /* Zap trailing separator. */
}
#endif

/* Number of frames for the leading and trailing part of a traceback. */
#if LJ_54
#define TRACEBACK_LEVELS1	11
#define TRACEBACK_LEVELS2	11
#else
#define TRACEBACK_LEVELS1	12
#define TRACEBACK_LEVELS2	10
#endif

LUALIB_API void luaL_traceback (lua_State *L, lua_State *L1, const char *msg,
				int level)
{
  int top = (int)(L->top - L->base);
#if LJ_54
  int lim = level + TRACEBACK_LEVELS1 - 1;
#else
  int lim = TRACEBACK_LEVELS1;
#endif
  lua_Debug ar;
  if (msg) lua_pushfstring(L, "%s\n", msg);
  lua_pushliteral(L, "stack traceback:");
  while (lua_getstack(L1, level++, &ar)) {
    GCfunc *fn;
    if (level > lim) {
      if (!lua_getstack(L1, level + TRACEBACK_LEVELS2, &ar)) {
	level--;
      } else {
#if LJ_54
	int oldlevel = level;
#endif
	lua_getstack(L1, -10, &ar);
	level = (int)LJ_DEBUG_CI_VALUE(ar.i_ci) - TRACEBACK_LEVELS2;
#if LJ_54
	lua_pushfstring(L, "\n\t...\t(skipping %d levels)", level - oldlevel);
#else
	lua_pushliteral(L, "\n\t...");
#endif
      }
      lim = 2147483647;
      continue;
    }
    lua_getinfo(L1, "Snlf", &ar);
    fn = funcV(L1->top-1); L1->top--;
#if LJ_54
    if (isffunc(fn) && !*ar.namewhat &&
	(fn->c.ffid == LJ_FFID_PCALL || fn->c.ffid == LJ_FFID_XPCALL))
      lua_pushfstring(L, "\n\t[C]: in function " LUA_QS,
		      fn->c.ffid == LJ_FFID_PCALL ? "pcall" : "xpcall");
    else
#endif
    if (isffunc(fn) && !*ar.namewhat)
      lua_pushfstring(L, "\n\t[builtin#%d]:", fn->c.ffid);
    else
      lua_pushfstring(L, "\n\t%s:", ar.short_src);
    if (ar.currentline > 0)
      lua_pushfstring(L, "%d:", ar.currentline);
    if (*ar.namewhat) {
#if LJ_54
      if (strcmp(ar.namewhat, "hook") == 0)
	lua_pushfstring(L, " in hook " LUA_QS, ar.name);
      else if (strcmp(ar.namewhat, "metamethod") == 0)
	lua_pushfstring(L, " in metamethod " LUA_QS, ar.name);
      else if (*ar.what == 'C' && ar.name &&
	       strcmp(ar.name, "traceback") == 0)
	lua_pushliteral(L, " in function 'debug.traceback'");
      else if (*ar.what == 'C' && ar.name && strcmp(ar.name, "yield") == 0)
	lua_pushliteral(L, " in function 'coroutine.yield'");
      else
#endif
      lua_pushfstring(L, " in function " LUA_QS, ar.name);
    } else {
      if (*ar.what == 'm') {
	lua_pushliteral(L, " in main chunk");
      } else if (*ar.what == 'C') {
#if LJ_54
	/* Lua 5.4 tracebacks do not expose raw C function pointers for
	** unnamed C frames; they use the same unknown-name marker as PUC Lua.
	*/
	lua_pushliteral(L, " in ?");
#else
	lua_pushfstring(L, " at %p", fn->c.f);
#endif
      } else {
	lua_pushfstring(L, " in function <%s:%d>",
			ar.short_src, ar.linedefined);
      }
    }
    if ((int)(L->top - L->base) - top >= 15)
      lua_concat(L, (int)(L->top - L->base) - top);
  }
  lua_concat(L, (int)(L->top - L->base) - top);
}

