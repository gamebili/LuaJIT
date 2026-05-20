/*
** Garbage collector.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_gc_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_func.h"
#include "lj_udata.h"
#include "lj_meta.h"
#include "lj_state.h"
#include "lj_frame.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#include "lj_cdata.h"
#endif
#include "lj_trace.h"
#include "lj_dispatch.h"
#include "lj_vm.h"
#include "lj_vmevent.h"

#define GCSTEPSIZE	1024u
#define GCSWEEPMAX	40
#define GCSWEEPCOST	10
#define GCFINALIZECOST	100

#if LJ_54
static size_t gc_propagate_gray(global_State *g);

static size_t gc_work_add54(size_t a, size_t b)
{
  return b > ~(size_t)0 - a ? ~(size_t)0 : a + b;
}

static GCSize gc_stepsize54(global_State *g)
{
  MSize bits = g->gc_stepsize54;
  if (bits >= sizeof(GCSize) * 8)
    return LJ_MAX_MEM;
  return ((GCSize)1) << bits;
}

static GCSize gc_gen_threshold54(global_State *g)
{
  GCSize minor = (g->gc.total / 100) * g->gc_genminormul54;
  if (minor > LJ_MAX_MEM - g->gc.total)
    return LJ_MAX_MEM;
  return g->gc.total + minor;
}

static GCSize gc_gen_majorinc54(global_State *g, GCSize majorbase)
{
  GCSize unit = majorbase / 100;
  GCSize majorinc;
  if (g->gc_genmajormul54 != 0 &&
      unit > LJ_MAX_MEM / g->gc_genmajormul54)
    return LJ_MAX_MEM;
  majorinc = unit * g->gc_genmajormul54;
  if (majorinc > LJ_MAX_MEM - majorbase)
    return LJ_MAX_MEM;
  return majorinc;
}

static int gc_gen_needmajor54(global_State *g)
{
  GCSize majorbase = g->gc.estimate;
  GCSize majorinc = gc_gen_majorinc54(g, majorbase);
  return majorinc != LJ_MAX_MEM && g->gc.total > majorbase + majorinc;
}

static void gc_gen_setpause54(global_State *g)
{
  GCSize threshold;
  if (g->gc.pause != 0 && g->gc.estimate <= LJ_MAX_MEM / g->gc.pause)
    threshold = (g->gc.estimate / 100) * g->gc.pause;
  else
    threshold = LJ_MAX_MEM;
  if (threshold < g->gc.total)
    threshold = g->gc.total;
  g->gc.threshold = threshold;
}

static GCSize gc_gen_atomicwork54(global_State *g)
{
  return g->gc_genatomicwork54 != 0 ? g->gc_genatomicwork54 : 1;
}

static int gc_gen_atomicwork_good54(GCSize newatomic, GCSize lastatomic)
{
  GCSize margin = lastatomic >> 3;
  GCSize limit = margin > LJ_MAX_MEM - lastatomic ?
		 LJ_MAX_MEM : lastatomic + margin;
  return newatomic < limit;
}

static void gc_clear_weak_lists54(global_State *g)
{
  setgcrefnull(g->gc.weak);
  setgcrefnull(g->gc.ephemeron);
  setgcrefnull(g->gc.allweak);
}

static void gc_link_gray_list54(global_State *g, GCRef *list)
{
  GCobj *o = gcref(*list);
  GCobj *tail = o;
  if (o == NULL)
    return;
  while (gcref(tail->gch.gclist) != NULL)
    tail = gcref(tail->gch.gclist);
  setgcrefr(tail->gch.gclist, g->gc.gray);
  setgcrefr(g->gc.gray, *list);
  setgcrefnull(*list);
}

static void gc_link_weak_lists_gray54(global_State *g)
{
  gc_link_gray_list54(g, &g->gc.weak);
  gc_link_gray_list54(g, &g->gc.ephemeron);
  gc_link_gray_list54(g, &g->gc.allweak);
}

static void gc_gen_blacken_old54(global_State *g, GCobj *o)
{
  setgcage(o, LJ_GC_AGE_OLD);
  if (o->gch.gct == ~LJ_TUPVAL && !gco2uv(o)->closed) {
    o->gch.marked &= (uint8_t)~LJ_GC_COLORS;
    return;
  }
  if (o->gch.gct == ~LJ_TTHREAD) {
    o->gch.marked &= (uint8_t)~LJ_GC_COLORS;
    setgcrefr(o->gch.gclist, g->gc.grayagain);
    setgcref(g->gc.grayagain, o);
  } else {
    o->gch.marked = (uint8_t)((o->gch.marked & (uint8_t)~LJ_GC_COLORS) |
			      LJ_GC_BLACK);
  }
}

static void gc_gen_blacken_chain54(global_State *g, GCobj *o)
{
  while (o) {
    if (o->gch.gct == ~LJ_TTHREAD)
      gc_gen_blacken_chain54(g, gcref(gco2th(o)->openupval));
    gc_gen_blacken_old54(g, o);
    o = gcnext(o);
  }
}

static void gc_gen_blacken_mmudata54(global_State *g)
{
  GCobj *root = gcref(g->gc.mmudata);
  GCobj *o = root;
  if (o) {
    do {
      gc_gen_blacken_old54(g, o);
      o = gcnext(o);
    } while (o != root);
  }
}

static void gc_gen_enter54(global_State *g)
{
  MSize i;
  setgcrefnull(g->gc.gray);
  setgcrefnull(g->gc.grayagain);
  gc_clear_weak_lists54(g);
  gc_gen_blacken_chain54(g, gcref(g->gc.root));
  gc_gen_blacken_mmudata54(g);
  if (g->str.tab) {
    for (i = g->str.mask; i != ~(MSize)0; i--) {
      GCobj *o = (GCobj *)(gcrefu(g->str.tab[i]) & ~(uintptr_t)1);
      gc_gen_blacken_chain54(g, o);
    }
  }
  gc_gen_blacken_old54(g, obj2gco(&g->strempty));
  g->gc.state = GCSpropagate;
  g->gc_genactive54 = 1;
  g->gc_genlastatomic54 = 0;
}

static void gc_whitelist_chain54(global_State *g, GCobj *o)
{
  while (o) {
    if (o->gch.gct == ~LJ_TTHREAD)
      gc_whitelist_chain54(g, gcref(gco2th(o)->openupval));
    setgcage(o, LJ_GC_AGE_NEW);
    makewhite(g, o);
    o = gcnext(o);
  }
}

static void gc_whitelist_mmudata54(global_State *g)
{
  GCobj *root = gcref(g->gc.mmudata);
  GCobj *o = root;
  if (o) {
    do {
      setgcage(o, LJ_GC_AGE_NEW);
      makewhite(g, o);
      o = gcnext(o);
    } while (o != root);
  }
}

void lj_gc_gen_whitelist54(global_State *g)
{
  MSize i;
  setgcrefnull(g->gc.gray);
  setgcrefnull(g->gc.grayagain);
  gc_clear_weak_lists54(g);
  gc_whitelist_chain54(g, gcref(g->gc.root));
  gc_whitelist_mmudata54(g);
  if (g->str.tab) {
    for (i = g->str.mask; i != ~(MSize)0; i--) {
      GCobj *o = (GCobj *)(gcrefu(g->str.tab[i]) & ~(uintptr_t)1);
      gc_whitelist_chain54(g, o);
    }
  }
  setgcage(obj2gco(&g->strempty), LJ_GC_AGE_NEW);
  makewhite(g, obj2gco(&g->strempty));
  g->gc.state = GCSpause;
  g->gc_genactive54 = 0;
  g->gc_genlastatomic54 = 0;
}
#endif

/* Macros to set GCobj colors and flags. */
#define white2gray(x)		((x)->gch.marked &= (uint8_t)~LJ_GC_WHITES)
#define gray2black(x)		((x)->gch.marked |= LJ_GC_BLACK)
#define isfinalized(u)		((u)->marked & LJ_GC_FINALIZED)

/* -- Mark phase ---------------------------------------------------------- */

/* Mark a TValue (if needed). */
#define gc_marktv(g, tv) \
  { lj_assertG(!tvisgcv(tv) || (~itype(tv) == gcval(tv)->gch.gct), \
	       "TValue and GC type mismatch"); \
    if (tviswhite(tv)) gc_mark(g, gcV(tv)); }

/* Mark a GCobj (if needed). */
#define gc_markobj(g, o) \
  { if (iswhite(obj2gco(o))) gc_mark(g, obj2gco(o)); }

/* Mark a string object. */
#define gc_mark_str(s)		((s)->marked &= (uint8_t)~LJ_GC_WHITES)

/* Mark a white GCobj. */
static void gc_mark(global_State *g, GCobj *o)
{
  int gct = o->gch.gct;
  lj_assertG(iswhite(o), "mark of non-white object");
  lj_assertG(!isdead(g, o), "mark of dead object");
  white2gray(o);
  if (LJ_UNLIKELY(gct == ~LJ_TUDATA)) {
    GCtab *mt = tabref(gco2ud(o)->metatable);
    gray2black(o);  /* Userdata are never gray. */
    if (mt) gc_markobj(g, mt);
    gc_markobj(g, tabref(gco2ud(o)->env));
    if (LJ_HASBUFFER && gco2ud(o)->udtype == UDTYPE_BUFFER) {
      SBufExt *sbx = (SBufExt *)uddata(gco2ud(o));
      if (sbufiscow(sbx) && gcref(sbx->cowref))
	gc_markobj(g, gcref(sbx->cowref));
      if (gcref(sbx->dict_str))
	gc_markobj(g, gcref(sbx->dict_str));
      if (gcref(sbx->dict_mt))
	gc_markobj(g, gcref(sbx->dict_mt));
    }
  } else if (LJ_UNLIKELY(gct == ~LJ_TUPVAL)) {
    GCupval *uv = gco2uv(o);
    gc_marktv(g, uvval(uv));
    if (uv->closed)
      gray2black(o);  /* Closed upvalues are never gray. */
  } else if (gct != ~LJ_TSTR && gct != ~LJ_TCDATA &&
	     gct != ~LJ_TINT64) {
    lj_assertG(gct == ~LJ_TFUNC || gct == ~LJ_TTAB ||
	       gct == ~LJ_TTHREAD || gct == ~LJ_TPROTO || gct == ~LJ_TTRACE,
	       "bad GC type %d", gct);
    setgcrefr(o->gch.gclist, g->gc.gray);
    setgcref(g->gc.gray, o);
  }
}

/* Mark GC roots. */
static void gc_mark_gcroot(global_State *g)
{
  ptrdiff_t i;
  for (i = 0; i < GCROOT_MAX; i++)
    if (gcref(g->gcroot[i]) != NULL)
      gc_markobj(g, gcref(g->gcroot[i]));
}

/* Start a GC cycle and mark the root set. */
static void gc_mark_start(global_State *g)
{
  setgcrefnull(g->gc.gray);
  setgcrefnull(g->gc.grayagain);
#if LJ_54
  gc_clear_weak_lists54(g);
#else
  setgcrefnull(g->gc.weak);
#endif
  gc_markobj(g, mainthread(g));
  gc_markobj(g, tabref(mainthread(g)->env));
  gc_markobj(g, vmthread(g));
  gc_marktv(g, &g->registrytv);
  gc_mark_gcroot(g);
  g->gc.state = GCSpropagate;
}

/* Mark open upvalues. */
static void gc_mark_uv(global_State *g)
{
  GCupval *uv;
  for (uv = uvnext(&g->uvhead); uv != &g->uvhead; uv = uvnext(uv)) {
    lj_assertG(uvprev(uvnext(uv)) == uv && uvnext(uvprev(uv)) == uv,
	       "broken upvalue chain");
    if (isgray(obj2gco(uv)))
      gc_marktv(g, uvval(uv));
  }
}

/* Mark objects in the finalizer list. */
static void gc_mark_mmudata(global_State *g)
{
  GCobj *root = gcref(g->gc.mmudata);
  GCobj *u = root;
  if (u) {
    do {
      u = gcnext(u);
      makewhite(g, u);  /* Could be from previous GC. */
      gc_mark(g, u);
    } while (u != root);
  }
}

static void gc_link_mmudata(global_State *g, GCobj *o)
{
  if (gcref(g->gc.mmudata)) {  /* Link to end of finalizer list. */
    GCobj *root = gcref(g->gc.mmudata);
    setgcrefr(o->gch.nextgc, root->gch.nextgc);
    setgcref(root->gch.nextgc, o);
    setgcref(g->gc.mmudata, o);
  } else {  /* Create circular list. */
    setgcref(o->gch.nextgc, o);
    setgcref(g->gc.mmudata, o);
  }
}

#if LJ_54
static int gc_isweakkey(cTValue *o)
{
  /* Lua weak keys only apply to collectable keys except strings and int64
  ** values. Strings are interned and int64 values are numeric values; both are
  ** treated as strong keys, matching the existing weak clear path.
  */
  return tvisgcv(o) && !tvisstr(o) && !tvisi64(o);
}

static int gc_isreachable_weakkey(cTValue *o)
{
  return !gc_isweakkey(o) || !iswhite(gcV(o));
}

static size_t gc_sizetab(GCtab *t)
{
  size_t sz = 0;
  if (t->hmask > 0)
    sz += sizeof(Node) * (t->hmask + 1);
  if (t->asize > 0 && LJ_MAX_COLOSIZE != 0 && t->colo <= 0)
    sz += sizeof(TValue) * t->asize;
  if (LJ_MAX_COLOSIZE != 0 && t->colo)
    sz += sizetabcolo((uint32_t)t->colo & 0x7f);
  else
    sz += sizeof(GCtab);
  return sz;
}
#endif

/* Separate objects to be finalized to mmudata list. */
size_t lj_gc_separateudata(global_State *g, int all)
{
  size_t m = 0;
  GCRef *p = &mainthread(g)->nextgc;
  GCobj *o;
  while ((o = gcref(*p)) != NULL) {
    if (!(iswhite(o) || all) || isfinalized(gco2ud(o))) {
      p = &o->gch.nextgc;  /* Nothing to do. */
    } else if (!lj_meta_fastg(g, tabref(gco2ud(o)->metatable), MM_gc)) {
      markfinalized(o);  /* Done, as there's no __gc metamethod. */
      p = &o->gch.nextgc;
    } else {  /* Otherwise move userdata to be finalized to mmudata list. */
      m += sizeudata(gco2ud(o));
      markfinalized(o);
      *p = o->gch.nextgc;
      gc_link_mmudata(g, o);
    }
  }
#if LJ_54
  p = &g->gc.root;
  while ((o = gcref(*p)) != NULL) {
    if (o->gch.gct == ~LJ_TTAB) {
      GCtab *t = gco2tab(o);
      if ((iswhite(o) || all) && (t->flags54 & LJ_TAB_HAS_GC)) {
	/* Lua 5.4 table finalizers are armed when the metatable is assigned,
	** not when __gc is added later to the existing metatable.
	*/
	m += gc_sizetab(t);
	t->flags54 = (uint8_t)((t->flags54 & (uint8_t)~LJ_TAB_HAS_GC) |
			       LJ_TAB_GC_PENDING);
	*p = o->gch.nextgc;
	gc_link_mmudata(g, o);
	continue;
      }
    }
    p = &o->gch.nextgc;
  }
#endif
  return m;
}

/* -- Propagation phase --------------------------------------------------- */

/* Traverse a table. */
static int gc_traverse_tab(global_State *g, GCtab *t)
{
  int weak = 0;
  cTValue *mode;
  GCtab *mt = tabref(t->metatable);
  if (mt)
    gc_markobj(g, mt);
  mode = lj_meta_fastg(g, mt, MM_mode);
  if (mode && tvisstr(mode)) {  /* Valid __mode field? */
    const char *modestr = strVdata(mode);
    int c;
    while ((c = *modestr++)) {
      if (c == 'k') weak |= LJ_GC_WEAKKEY;
      else if (c == 'v') weak |= LJ_GC_WEAKVAL;
    }
    if (weak) {  /* Weak tables are cleared in the atomic phase. */
#if LJ_HASFFI
      if (gcref(g->gcroot[GCROOT_FFI_FIN]) == obj2gco(t)) {
	weak = (int)(~0u & ~LJ_GC_WEAKVAL);
      } else
#endif
      {
#if LJ_54
	GCRef *list = (weak & LJ_GC_WEAKKEY) ?
		      ((weak & LJ_GC_WEAKVAL) ? &g->gc.allweak :
						&g->gc.ephemeron) :
		      &g->gc.weak;
	t->marked = (uint8_t)((t->marked & ~LJ_GC_WEAK) | weak);
	setgcrefr(t->gclist, *list);
	setgcref(*list, obj2gco(t));
#else
	t->marked = (uint8_t)((t->marked & ~LJ_GC_WEAK) | weak);
	setgcrefr(t->gclist, g->gc.weak);
	setgcref(g->gc.weak, obj2gco(t));
#endif
      }
    }
  }
  if (weak == LJ_GC_WEAK)  /* Nothing to mark if both keys/values are weak. */
    return 1;
  if (!(weak & LJ_GC_WEAKVAL)) {  /* Mark array part. */
    MSize i, asize = t->asize;
    for (i = 0; i < asize; i++)
      gc_marktv(g, arrayslot(t, i));
  }
  if (t->hmask > 0) {  /* Mark hash part. */
    Node *node = noderef(t->node);
    MSize i, hmask = t->hmask;
    for (i = 0; i <= hmask; i++) {
      Node *n = &node[i];
      if (!tvisnil(&n->val)) {  /* Mark non-empty slot. */
	lj_assertG(!tvisnil(&n->key), "mark of nil key in non-empty slot");
	if (!(weak & LJ_GC_WEAKKEY)) gc_marktv(g, &n->key);
	if (!(weak & LJ_GC_WEAKVAL)) {
#if LJ_54
	  if (!(weak & LJ_GC_WEAKKEY) || !gc_isweakkey(&n->key) ||
	      !iswhite(gcV(&n->key))) {
	    /* For weak-key tables, the value is ephemeron-reachable only after
	    ** the collectable key is reachable from outside this table.
	    */
	    gc_marktv(g, &n->val);
	  }
#else
	  gc_marktv(g, &n->val);
#endif
	}
      }
    }
  }
  return weak;
}

#if LJ_54
/* Complete Lua 5.4 ephemeron marking for weak-key/strong-value tables.
** A value is only kept after its key is reachable from outside the table, and
** marking that value can in turn make more ephemeron keys reachable.
*/
static int gc_mark_ephemeron(global_State *g, GCobj *o)
{
  int marked = 0;
  while (o) {
    GCtab *t = gco2tab(o);
    if ((t->marked & LJ_GC_WEAKKEY) && !(t->marked & LJ_GC_WEAKVAL) &&
	t->hmask > 0) {
      Node *node = noderef(t->node);
      MSize i, hmask = t->hmask;
      for (i = 0; i <= hmask; i++) {
	Node *n = &node[i];
	if (!tvisnil(&n->val) && gc_isreachable_weakkey(&n->key)) {
	  if (tviswhite(&n->val))
	    marked = 1;
	  gc_marktv(g, &n->val);
	}
      }
    }
    o = gcref(t->gclist);
  }
  return marked;
}
#endif

/* Traverse a function. */
static void gc_traverse_func(global_State *g, GCfunc *fn)
{
  gc_markobj(g, tabref(fn->c.env));
  if (isluafunc(fn)) {
    uint32_t i;
    lj_assertG(fn->l.nupvalues <= funcproto(fn)->sizeuv,
	       "function upvalues out of range");
    gc_markobj(g, funcproto(fn));
    for (i = 0; i < fn->l.nupvalues; i++)  /* Mark Lua function upvalues. */
      gc_markobj(g, &gcref(fn->l.uvptr[i])->uv);
  } else {
    uint32_t i;
    for (i = 0; i < fn->c.nupvalues; i++)  /* Mark C function upvalues. */
      gc_marktv(g, &fn->c.upvalue[i]);
  }
}

#if LJ_HASJIT
/* Mark a trace. */
static void gc_marktrace(global_State *g, TraceNo traceno)
{
  GCobj *o = obj2gco(traceref(G2J(g), traceno));
  lj_assertG(traceno != G2J(g)->cur.traceno, "active trace escaped");
  if (iswhite(o)) {
    white2gray(o);
    setgcrefr(o->gch.gclist, g->gc.gray);
    setgcref(g->gc.gray, o);
  }
}

/* Traverse a trace. */
static void gc_traverse_trace(global_State *g, GCtrace *T)
{
  IRRef ref;
  if (T->traceno == 0) return;
  for (ref = T->nk; ref < REF_TRUE; ref++) {
    IRIns *ir = &T->ir[ref];
    if (ir->o == IR_KGC)
      gc_markobj(g, ir_kgc(ir));
    if (irt_is64(ir->t) && ir->o != IR_KNULL)
      ref++;
  }
  if (T->link) gc_marktrace(g, T->link);
  if (T->nextroot) gc_marktrace(g, T->nextroot);
  if (T->nextside) gc_marktrace(g, T->nextside);
  gc_markobj(g, gcref(T->startpt));
}

/* The current trace is a GC root while not anchored in the prototype (yet). */
#define gc_traverse_curtrace(g)	gc_traverse_trace(g, &G2J(g)->cur)
#else
#define gc_traverse_curtrace(g)	UNUSED(g)
#endif

/* Traverse a prototype. */
static void gc_traverse_proto(global_State *g, GCproto *pt)
{
  ptrdiff_t i;
  gc_mark_str(proto_chunkname(pt));
  for (i = -(ptrdiff_t)pt->sizekgc; i < 0; i++)  /* Mark collectable consts. */
    gc_markobj(g, proto_kgc(pt, i));
#if LJ_54
  {
    TValue *kn = mref(pt->k, TValue);
    for (i = 0; i < (ptrdiff_t)pt->sizekn; i++)  /* Mark boxed integer consts. */
      if (tvisi64(&kn[i]))
	gc_markobj(g, gcV(&kn[i]));
  }
#endif
#if LJ_HASJIT
  if (pt->trace) gc_marktrace(g, pt->trace);
#endif
}

/* Traverse the frame structure of a stack. */
static MSize gc_traverse_frames(global_State *g, lua_State *th)
{
  TValue *frame, *top = th->top-1, *bot = tvref(th->stack);
  /* Note: extra vararg frame not skipped, marks function twice (harmless). */
  for (frame = th->base-1; frame > bot+LJ_FR2; frame = frame_prev(frame)) {
    GCfunc *fn = frame_func(frame);
    TValue *ftop = frame;
    if (isluafunc(fn)) ftop += funcproto(fn)->framesize;
    if (ftop > top) top = ftop;
    if (!LJ_FR2) gc_markobj(g, fn);  /* Need to mark hidden function (or L). */
  }
  top++;  /* Correct bias of -1 (frame == base-1). */
  if (top > tvref(th->maxstack)) top = tvref(th->maxstack);
  return (MSize)(top - bot);  /* Return minimum needed stack size. */
}

/* Traverse a thread object. */
static void gc_traverse_thread(global_State *g, lua_State *th)
{
  TValue *o, *top = th->top;
  for (o = tvref(th->stack)+1+LJ_FR2; o < top; o++)
    gc_marktv(g, o);
  if (g->gc.state == GCSatomic) {
    top = tvref(th->stack) + th->stacksize;
    for (; o < top; o++)  /* Clear unmarked slots. */
      setnilV(o);
  }
  gc_markobj(g, tabref(th->env));
  lj_state_shrinkstack(th, gc_traverse_frames(g, th));
}

/* Propagate one gray object. Traverse it and turn it black. */
static size_t propagatemark(global_State *g)
{
  GCobj *o = gcref(g->gc.gray);
  int gct = o->gch.gct;
  lj_assertG(isgray(o), "propagation of non-gray object");
  gray2black(o);
  setgcrefr(g->gc.gray, o->gch.gclist);  /* Remove from gray list. */
  if (LJ_LIKELY(gct == ~LJ_TTAB)) {
    GCtab *t = gco2tab(o);
    if (gc_traverse_tab(g, t) > 0)
      black2gray(o);  /* Keep weak tables gray. */
    return sizeof(GCtab) + sizeof(TValue) * t->asize +
			   (t->hmask ? sizeof(Node) * (t->hmask + 1) : 0);
  } else if (LJ_LIKELY(gct == ~LJ_TFUNC)) {
    GCfunc *fn = gco2func(o);
    gc_traverse_func(g, fn);
    return isluafunc(fn) ? sizeLfunc((MSize)funcproto(fn)->sizeuv) :
			   sizeCfunc((MSize)fn->c.nupvalues);
  } else if (LJ_LIKELY(gct == ~LJ_TPROTO)) {
    GCproto *pt = gco2pt(o);
    gc_traverse_proto(g, pt);
    return pt->sizept;
  } else if (LJ_LIKELY(gct == ~LJ_TTHREAD)) {
    lua_State *th = gco2th(o);
    setgcrefr(th->gclist, g->gc.grayagain);
    setgcref(g->gc.grayagain, o);
    black2gray(o);  /* Threads are never black. */
    gc_traverse_thread(g, th);
    return sizeof(lua_State) + sizeof(TValue) * th->stacksize;
  } else {
#if LJ_HASJIT
    GCtrace *T = gco2trace(o);
    gc_traverse_trace(g, T);
    return ((sizeof(GCtrace)+7)&~7) + (T->nins-T->nk)*sizeof(IRIns) +
	   T->nsnap*sizeof(SnapShot) + T->nsnapmap*sizeof(SnapEntry);
#else
    lj_assertG(0, "bad GC type %d", gct);
    return 0;
#endif
  }
}

/* Propagate all gray objects. */
static size_t gc_propagate_gray(global_State *g)
{
  size_t m = 0;
  while (gcref(g->gc.gray) != NULL)
    m += propagatemark(g);
  return m;
}

/* -- Sweep phase --------------------------------------------------------- */

/* Type of GC free functions. */
typedef void (LJ_FASTCALL *GCFreeFunc)(global_State *g, GCobj *o);

/* GC free functions for LJ_TSTR .. LJ_TUDATA. ORDER LJ_T */
static const GCFreeFunc gc_freefunc[] = {
  (GCFreeFunc)lj_str_free,
  (GCFreeFunc)lj_func_freeuv,
  (GCFreeFunc)lj_state_free,
  (GCFreeFunc)lj_func_freeproto,
  (GCFreeFunc)lj_func_free,
#if LJ_HASJIT
  (GCFreeFunc)lj_trace_free,
#else
  (GCFreeFunc)0,
#endif
#if LJ_HASFFI
  (GCFreeFunc)lj_cdata_free,
#else
  (GCFreeFunc)0,
#endif
  (GCFreeFunc)lj_obj_freeint64,
  (GCFreeFunc)lj_tab_free,
  (GCFreeFunc)lj_udata_free
};

/* Full sweep of a GC list. */
#define gc_fullsweep(g, p)	gc_sweep(g, (p), ~(uint32_t)0)

/* Partial sweep of a GC list. */
static GCRef *gc_sweep(global_State *g, GCRef *p, uint32_t lim)
{
  /* Mask with other white and LJ_GC_FIXED. Or LJ_GC_SFIXED on shutdown. */
  int ow = otherwhite(g);
  GCobj *o;
  while ((o = gcref(*p)) != NULL && lim-- > 0) {
    if (o->gch.gct == ~LJ_TTHREAD)  /* Need to sweep open upvalues, too. */
      gc_fullsweep(g, &gco2th(o)->openupval);
    if (((o->gch.marked ^ LJ_GC_WHITES) & ow)) {  /* Black or current white? */
      lj_assertG(!isdead(g, o) || (o->gch.marked & LJ_GC_FIXED),
		 "sweep of undead object");
      makewhite(g, o);  /* Value is alive, change to the current white. */
      p = &o->gch.nextgc;
    } else {  /* Otherwise value is dead, free it. */
      lj_assertG(isdead(g, o) || ow == LJ_GC_SFIXED,
		 "sweep of unlive object");
      setgcrefr(*p, o->gch.nextgc);
      if (o == gcref(g->gc.root))
	setgcrefr(g->gc.root, o->gch.nextgc);  /* Adjust list anchor. */
      gc_freefunc[o->gch.gct - ~LJ_TSTR](g, o);
    }
  }
  return p;
}

/* Sweep one string interning table chain. Preserves hashalg bit. */
static void gc_sweepstr(global_State *g, GCRef *chain)
{
  /* Mask with other white and LJ_GC_FIXED. Or LJ_GC_SFIXED on shutdown. */
  int ow = otherwhite(g);
  uintptr_t u = gcrefu(*chain);
  GCRef q;
  GCRef *p = &q;
  GCobj *o;
  setgcrefp(q, (u & ~(uintptr_t)1));
  while ((o = gcref(*p)) != NULL) {
    if (((o->gch.marked ^ LJ_GC_WHITES) & ow)) {  /* Black or current white? */
      lj_assertG(!isdead(g, o) || (o->gch.marked & LJ_GC_FIXED),
		 "sweep of undead string");
      makewhite(g, o);  /* String is alive, change to the current white. */
      p = &o->gch.nextgc;
    } else {  /* Otherwise string is dead, free it. */
      lj_assertG(isdead(g, o) || ow == LJ_GC_SFIXED,
		 "sweep of unlive string");
      setgcrefr(*p, o->gch.nextgc);
      lj_str_free(g, gco2str(o));
    }
  }
  setgcrefp(*chain, (gcrefu(q) | (u & 1)));
}

#if LJ_54
static int gc_gen_revisit_age54(GCobj *o)
{
  uint8_t age = gcage(o);
  return age == LJ_GC_AGE_OLD1 || age == LJ_GC_AGE_TOUCHED1 ||
	 age == LJ_GC_AGE_TOUCHED2;
}

static int gc_gen_has_gclist54(GCobj *o)
{
  int gct = o->gch.gct;
  return gct == ~LJ_TTAB || gct == ~LJ_TFUNC || gct == ~LJ_TTHREAD ||
	 gct == ~LJ_TPROTO ||
#if LJ_HASJIT
	 gct == ~LJ_TTRACE ||
#endif
	 0;
}

static void gc_gen_revisit_obj54(global_State *g, GCobj *o)
{
  if (gc_gen_revisit_age54(o) && gc_gen_has_gclist54(o) && isblack(o)) {
    o->gch.marked &= (uint8_t)~LJ_GC_BLACK;
    setgcrefr(o->gch.gclist, g->gc.gray);
    setgcref(g->gc.gray, o);
  }
}

static void gc_gen_revisit_chain54(global_State *g, GCobj *o)
{
  while (o) {
    gc_gen_revisit_obj54(g, o);
    o = gcnext(o);
  }
}

static void gc_gen_revisit_mmudata54(global_State *g)
{
  GCobj *root = gcref(g->gc.mmudata);
  GCobj *o = root;
  if (o) {
    do {
      gc_gen_revisit_obj54(g, o);
      o = gcnext(o);
    } while (o != root);
  }
}

static void gc_gen_revisit_old54(global_State *g)
{
  if (gcref(g->gc.gray) != NULL)
    gc_propagate_gray(g);
  setgcrefnull(g->gc.gray);
  gc_gen_revisit_chain54(g, gcref(g->gc.root));
  gc_gen_revisit_mmudata54(g);
}

static int gc_gen_hastrace54(global_State *g)
{
#if LJ_HASJIT
  jit_State *J = G2J(g);
  MSize i;
  for (i = 1; i < J->sizetrace; i++)
    if (gcref(J->trace[i]) != NULL)
      return 1;
#else
  UNUSED(g);
#endif
  return 0;
}

static int gc_gen_canminor54(global_State *g)
{
#if LJ_HASJIT
  if (G2J(g)->flags & JIT_F_ON)
    return 0;
#endif
  return !gc_gen_hastrace54(g);
}

static void gc_gen_keepblack54(GCobj *o)
{
  if (o->gch.gct == ~LJ_TUPVAL && !gco2uv(o)->closed) {
    o->gch.marked &= (uint8_t)~LJ_GC_COLORS;
    return;
  }
  if (o->gch.gct != ~LJ_TTHREAD)
    o->gch.marked = (uint8_t)((o->gch.marked & (uint8_t)~LJ_GC_COLORS) |
			      LJ_GC_BLACK);
}

static void gc_gen_age_survivor54(global_State *g, GCobj *o)
{
  switch (gcage(o)) {
  case LJ_GC_AGE_NEW:
    setgcage(o, LJ_GC_AGE_SURVIVAL);
    makewhite(g, o);
    break;
  case LJ_GC_AGE_SURVIVAL:
  case LJ_GC_AGE_OLD0:
    setgcage(o, LJ_GC_AGE_OLD1);
    gc_gen_keepblack54(o);
    break;
  case LJ_GC_AGE_OLD1:
    setgcage(o, LJ_GC_AGE_OLD);
    gc_gen_keepblack54(o);
    break;
  case LJ_GC_AGE_TOUCHED1:
    setgcage(o, LJ_GC_AGE_TOUCHED2);
    gc_gen_keepblack54(o);
    break;
  case LJ_GC_AGE_TOUCHED2:
    setgcage(o, LJ_GC_AGE_OLD);
    gc_gen_keepblack54(o);
    break;
  default:
    gc_gen_keepblack54(o);
    break;
  }
}

static GCRef *gc_sweepgen54(lua_State *L, global_State *g, GCRef *p)
{
  GCobj *o;
  while ((o = gcref(*p)) != NULL) {
    if (o->gch.gct == ~LJ_TTHREAD)
      gc_sweepgen54(L, g, &gco2th(o)->openupval);
    if (iswhite(o) && !isoldgc(o) && !(o->gch.marked & LJ_GC_FIXED)) {
      setgcrefr(*p, o->gch.nextgc);
      if (o == gcref(g->gc.root))
	setgcrefr(g->gc.root, o->gch.nextgc);
      gc_freefunc[o->gch.gct - ~LJ_TSTR](g, o);
    } else {
      gc_gen_age_survivor54(g, o);
      p = &o->gch.nextgc;
    }
  }
  return p;
}

static void gc_sweepstrgen54(global_State *g, GCRef *chain)
{
  uintptr_t u = gcrefu(*chain);
  GCRef q;
  GCRef *p = &q;
  GCobj *o;
  setgcrefp(q, (u & ~(uintptr_t)1));
  while ((o = gcref(*p)) != NULL) {
    if (iswhite(o) && !isoldgc(o) && !(o->gch.marked & LJ_GC_FIXED)) {
      setgcrefr(*p, o->gch.nextgc);
      lj_str_free(g, gco2str(o));
    } else {
      gc_gen_age_survivor54(g, o);
      p = &o->gch.nextgc;
    }
  }
  setgcrefp(*chain, (gcrefu(q) | (u & 1)));
}

static void gc_correctgraygen54(global_State *g)
{
  GCRef *p = &g->gc.grayagain;
  GCobj *o;
  while ((o = gcref(*p)) != NULL) {
    if (iswhite(o))
      setgcrefr(*p, o->gch.gclist);
    else
      p = &o->gch.gclist;
  }
}
#endif

/* Check whether we can clear a key or a value slot from a table. */
static int gc_mayclear(global_State *g, cTValue *o, int val)
{
  if (tvisgcv(o)) {  /* Only collectable objects can be weak references. */
    if (tvisstr(o)) {  /* But strings cannot be used as weak references. */
      gc_mark_str(strV(o));  /* And need to be marked. */
      return 0;
    }
#if LJ_54
    if (tvisi64(o)) {  /* Lua 5.4 integers are numeric values, too. */
      gc_markobj(g, gcV(o));
      return 0;
    }
#endif
    if (iswhite(gcV(o)))
      return 1;  /* Object is about to be collected. */
    if (tvisudata(o) && val && isfinalized(udataV(o)))
      return 1;  /* Finalized userdata is dropped only from values. */
#if LJ_54
    if (tvistab(o) && val && (tabV(o)->flags54 & LJ_TAB_GC_PENDING))
      return 1;  /* Tables pending finalization are dropped from values. */
#endif
  }
  return 0;  /* Cannot clear. */
}

/* Clear collected values from weak-value tables. */
static void gc_clearweakvalues(global_State *g, GCobj *o)
{
  while (o) {
    GCtab *t = gco2tab(o);
    lj_assertG((t->marked & LJ_GC_WEAK), "clear of non-weak table");
    if ((t->marked & LJ_GC_WEAKVAL)) {
      MSize i, asize = t->asize;
      for (i = 0; i < asize; i++) {
	/* Clear array slot when value is about to be collected. */
	TValue *tv = arrayslot(t, i);
	if (gc_mayclear(g, tv, 1))
	  setnilV(tv);
      }
    }
    if (t->hmask > 0) {
      Node *node = noderef(t->node);
      MSize i, hmask = t->hmask;
      for (i = 0; i <= hmask; i++) {
	Node *n = &node[i];
	if (!tvisnil(&n->val) && gc_mayclear(g, &n->val, 1))
	  setnilV(&n->val);
      }
    }
    o = gcref(t->gclist);
  }
}

/* Clear collected keys from weak-key tables. */
static void gc_clearweakkeys(global_State *g, GCobj *o)
{
  while (o) {
    GCtab *t = gco2tab(o);
    lj_assertG((t->marked & LJ_GC_WEAK), "clear of non-weak table");
    if ((t->marked & LJ_GC_WEAKKEY) && t->hmask > 0) {
      Node *node = noderef(t->node);
      MSize i, hmask = t->hmask;
      for (i = 0; i <= hmask; i++) {
	Node *n = &node[i];
	if (!tvisnil(&n->val) && gc_mayclear(g, &n->key, 0))
	  setnilV(&n->val);
      }
    }
    o = gcref(t->gclist);
  }
}

/* Call a userdata, table or cdata finalizer. */
static void gc_call_finalizer(global_State *g, lua_State *L,
			      cTValue *mo, GCobj *o)
{
  /* Save and restore lots of state around the __gc callback. */
  uint8_t oldh = hook_save(g);
  GCSize oldt = g->gc.threshold;
  int errcode;
  lua_State *VL = vmthread(g);
  TValue *top;
#if LJ_54
  const char *oldmm = g->debug_mmname;
  GCfunc *oldmmfunc = g->debug_mmfunc;
#endif
  lj_trace_abort(g);
  hook_entergc(g);  /* Disable hooks and new traces during __gc. */
  if (LJ_HASPROFILE && (oldh & HOOK_PROFILE)) lj_dispatch_update(g);
  g->gc.threshold = LJ_MAX_MEM;  /* Prevent GC steps. */
  top = VL->top;
  copyTV(VL, top++, mo);
  if (LJ_FR2) setnilV(top++);
  setgcV(VL, top, o, ~o->gch.gct);
  VL->top = top+1;
#if LJ_54
  /* Finalizers are invoked from the collector, not from a bytecode MM site.
  ** Bind the active callback explicitly so debug.getinfo(1) reports __gc as a
  ** Lua 5.4 metamethod while the finalizer is running.
  */
  g->debug_mmname = "__gc";
  g->debug_mmfunc = tvisfunc(mo) ? funcV(mo) : NULL;
#endif
  errcode = lj_vm_pcall(VL, top, 1+0, -1);  /* Stack: |mo|o| -> | */
#if LJ_54
  g->debug_mmfunc = oldmmfunc;
  g->debug_mmname = oldmm;
#endif
  setgcref(g->cur_L, obj2gco(L));
  hook_restore(g, oldh);
  if (LJ_HASPROFILE && (oldh & HOOK_PROFILE)) lj_dispatch_update(g);
  g->gc.threshold = oldt;  /* Restore GC threshold. */
  if (errcode) {
    TValue tmp;
    copyTV(VL, &tmp, VL->top-1);
    VL->top--;
    lj_vmevent_send(g, ERRFIN,
      copyTV(V, V->top++, &tmp);
    );
  }
}

/* Finalize one object from the mmudata list. */
static void gc_finalize(lua_State *L)
{
  global_State *g = G(L);
  GCobj *o = gcnext(gcref(g->gc.mmudata));
  cTValue *mo;
  lj_assertG(tvref(g->jit_base) == NULL, "finalizer called on trace");
  /* Unchain from list of userdata to be finalized. */
  if (o == gcref(g->gc.mmudata))
    setgcrefnull(g->gc.mmudata);
  else
    setgcrefr(gcref(g->gc.mmudata)->gch.nextgc, o->gch.nextgc);
#if LJ_HASFFI
  if (o->gch.gct == ~LJ_TCDATA) {
    TValue tmp, *tv;
    /* Add cdata back to the GC list and make it white. */
    setgcrefr(o->gch.nextgc, g->gc.root);
    setgcref(g->gc.root, o);
    makewhite(g, o);
    o->gch.marked &= (uint8_t)~LJ_GC_CDATA_FIN;
    /* Resolve finalizer. */
    setcdataV(L, &tmp, gco2cd(o));
    tv = lj_tab_set(L, tabref(g->gcroot[GCROOT_FFI_FIN]), &tmp);
    if (!tvisnil(tv)) {
      copyTV(L, &tmp, tv);
      setnilV(tv);  /* Clear entry in finalizer table. */
      gc_call_finalizer(g, L, &tmp, o);
    }
    return;
  }
#endif
#if LJ_54
  if (o->gch.gct == ~LJ_TTAB) {
    /* Reinsert before calling __gc so a resurrected table stays valid.
    ** The arming flag was cleared during separation, so it runs at most once
    ** unless user code assigns another __gc-bearing metatable later.
    */
    GCtab *t = gco2tab(o);
    t->flags54 &= (uint8_t)~LJ_TAB_GC_PENDING;
    setgcrefr(o->gch.nextgc, g->gc.root);
    setgcref(g->gc.root, o);
    makewhite(g, o);
    {
      GCtab *mt = tabref(t->metatable);
      mo = mt ? lj_tab_getstr(mt, mmname_str(g, MM_gc)) : NULL;
      if (mo && !tvisnil(mo))
	gc_call_finalizer(g, L, mo, o);
    }
    return;
  }
#endif
  /* Add userdata back to the main userdata list and make it white. */
  setgcrefr(o->gch.nextgc, mainthread(g)->nextgc);
  setgcref(mainthread(g)->nextgc, o);
  makewhite(g, o);
  /* Resolve the __gc metamethod. */
  mo = lj_meta_fastg(g, tabref(gco2ud(o)->metatable), MM_gc);
  if (mo)
    gc_call_finalizer(g, L, mo, o);
}

/* Finalize all userdata objects from mmudata list. */
void lj_gc_finalize_udata(lua_State *L)
{
  while (gcref(G(L)->gc.mmudata) != NULL)
    gc_finalize(L);
}

#if LJ_HASFFI
/* Finalize all cdata objects from finalizer table. */
void lj_gc_finalize_cdata(lua_State *L)
{
  global_State *g = G(L);
  GCtab *t = tabref(g->gcroot[GCROOT_FFI_FIN]);
  Node *node = noderef(t->node);
  ptrdiff_t i;
  setgcrefnull(t->metatable);  /* Mark finalizer table as disabled. */
  for (i = (ptrdiff_t)t->hmask; i >= 0; i--)
    if (!tvisnil(&node[i].val) && tviscdata(&node[i].key)) {
      GCobj *o = gcV(&node[i].key);
      TValue tmp;
      makewhite(g, o);
      o->gch.marked &= (uint8_t)~LJ_GC_CDATA_FIN;
      copyTV(L, &tmp, &node[i].val);
      setnilV(&node[i].val);
      gc_call_finalizer(g, L, &tmp, o);
    }
}
#endif

/* Free all remaining GC objects. */
void lj_gc_freeall(global_State *g)
{
  MSize i;
  /* Free everything, except super-fixed objects (the main thread). */
  g->gc.currentwhite = LJ_GC_WHITES | LJ_GC_SFIXED;
  gc_fullsweep(g, &g->gc.root);
  for (i = g->str.mask; i != ~(MSize)0; i--)  /* Free all string hash chains. */
    gc_sweepstr(g, &g->str.tab[i]);
}

/* -- Collector ----------------------------------------------------------- */

/* Atomic part of the GC cycle, transitioning from mark to sweep phase. */
static void atomic(global_State *g, lua_State *L)
{
  size_t udsize;
#if LJ_54
  size_t work = 0;
#endif

  gc_mark_uv(g);  /* Need to remark open upvalues (the thread may be dead). */
#if LJ_54
  work = gc_work_add54(work, gc_propagate_gray(g));
#else
  gc_propagate_gray(g);
#endif

#if LJ_54
  gc_link_weak_lists_gray54(g);  /* Empty the weak-table lists. */
#else
  setgcrefr(g->gc.gray, g->gc.weak);  /* Empty the list of weak tables. */
  setgcrefnull(g->gc.weak);
#endif
  lj_assertG(!iswhite(obj2gco(mainthread(g))), "main thread turned white");
  gc_markobj(g, L);  /* Mark running thread. */
  gc_traverse_curtrace(g);  /* Traverse current trace. */
  gc_mark_gcroot(g);  /* Mark GC roots (again). */
#if LJ_54
  work = gc_work_add54(work, gc_propagate_gray(g));
  while (gc_mark_ephemeron(g, gcref(g->gc.ephemeron)))
    work = gc_work_add54(work, gc_propagate_gray(g));
#else
  gc_propagate_gray(g);  /* Propagate all of the above. */
#endif

  setgcrefr(g->gc.gray, g->gc.grayagain);  /* Empty the 2nd chance list. */
  setgcrefnull(g->gc.grayagain);
#if LJ_54
  work = gc_work_add54(work, gc_propagate_gray(g));
  while (gc_mark_ephemeron(g, gcref(g->gc.ephemeron)))
    work = gc_work_add54(work, gc_propagate_gray(g));

  /* Lua 5.4 clears weak values before separating objects for finalization. */
  gc_clearweakvalues(g, gcref(g->gc.weak));
  gc_clearweakvalues(g, gcref(g->gc.allweak));
#else
  gc_propagate_gray(g);  /* Propagate it. */
#endif

  udsize = lj_gc_separateudata(g, 0);  /* Separate userdata to be finalized. */
  gc_mark_mmudata(g);  /* Mark them. */
#if LJ_54
  {
    size_t finalwork = gc_work_add54(udsize, gc_propagate_gray(g));
    udsize = finalwork;
    work = gc_work_add54(work, finalwork);
  }
  while (gc_mark_ephemeron(g, gcref(g->gc.ephemeron))) {
    size_t ephemwork = gc_propagate_gray(g);
    udsize = gc_work_add54(udsize, ephemwork);
    work = gc_work_add54(work, ephemwork);
  }
#else
  udsize += gc_propagate_gray(g);  /* And propagate the marks. */
#endif

  /* All marking done, clear weak tables. */
#if LJ_54
  gc_clearweakkeys(g, gcref(g->gc.ephemeron));
  gc_clearweakkeys(g, gcref(g->gc.allweak));
  gc_clearweakvalues(g, gcref(g->gc.weak));
  gc_clearweakvalues(g, gcref(g->gc.allweak));
  g->gc_genatomicwork54 = work > LJ_MAX_MEM ? LJ_MAX_MEM : (GCSize)work;
#else
  gc_clearweakvalues(g, gcref(g->gc.weak));
  gc_clearweakkeys(g, gcref(g->gc.weak));
#endif

  lj_buf_shrink(L, &g->tmpbuf);  /* Shrink temp buffer. */

  /* Prepare for sweep phase. */
  g->gc.currentwhite = (uint8_t)otherwhite(g);  /* Flip current white. */
  g->strempty.marked = g->gc.currentwhite;
  setmref(g->gc.sweep, &g->gc.root);
  g->gc.estimate = g->gc.total - (GCSize)udsize;  /* Initial estimate. */
}

/* GC state machine. Returns a cost estimate for each step performed. */
static size_t gc_onestep(lua_State *L)
{
  global_State *g = G(L);
  switch (g->gc.state) {
  case GCSpause:
    gc_mark_start(g);  /* Start a new GC cycle by marking all GC roots. */
    return 0;
  case GCSpropagate:
    if (gcref(g->gc.gray) != NULL)
      return propagatemark(g);  /* Propagate one gray object. */
    g->gc.state = GCSatomic;  /* End of mark phase. */
    return 0;
  case GCSatomic:
    if (tvref(g->jit_base))  /* Don't run atomic phase on trace. */
      return LJ_MAX_MEM;
    atomic(g, L);
    g->gc.state = GCSsweepstring;  /* Start of sweep phase. */
    g->gc.sweepstr = 0;
#if LJ_54
    return g->gc_genatomicwork54 > (GCSize)~(size_t)0 ?
	   ~(size_t)0 : (size_t)g->gc_genatomicwork54;
#else
    return 0;
#endif
  case GCSsweepstring: {
    GCSize old = g->gc.total;
    gc_sweepstr(g, &g->str.tab[g->gc.sweepstr++]);  /* Sweep one chain. */
    if (g->gc.sweepstr > g->str.mask)
      g->gc.state = GCSsweep;  /* All string hash chains sweeped. */
    lj_assertG(old >= g->gc.total, "sweep increased memory");
    g->gc.estimate -= old - g->gc.total;
    return GCSWEEPCOST;
    }
  case GCSsweep: {
    GCSize old = g->gc.total;
    setmref(g->gc.sweep, gc_sweep(g, mref(g->gc.sweep, GCRef), GCSWEEPMAX));
    lj_assertG(old >= g->gc.total, "sweep increased memory");
    g->gc.estimate -= old - g->gc.total;
    if (gcref(*mref(g->gc.sweep, GCRef)) == NULL) {
      if (g->str.num <= (g->str.mask >> 2) && g->str.mask > LJ_MIN_STRTAB*2-1)
	lj_str_resize(L, g->str.mask >> 1);  /* Shrink string table. */
      if (gcref(g->gc.mmudata)) {  /* Need any finalizations? */
	g->gc.state = GCSfinalize;
      } else {  /* Otherwise skip this phase to help the JIT. */
	g->gc.state = GCSpause;  /* End of GC cycle. */
	g->gc.debt = 0;
      }
    }
    return GCSWEEPMAX*GCSWEEPCOST;
    }
  case GCSfinalize:
    if (gcref(g->gc.mmudata) != NULL) {
      GCSize old = g->gc.total;
      if (tvref(g->jit_base))  /* Don't call finalizers on trace. */
	return LJ_MAX_MEM;
      gc_finalize(L);  /* Finalize one userdata object. */
      if (old >= g->gc.total && g->gc.estimate > old - g->gc.total)
	g->gc.estimate -= old - g->gc.total;
      if (g->gc.estimate > GCFINALIZECOST)
	g->gc.estimate -= GCFINALIZECOST;
      return GCFINALIZECOST;
    }
    g->gc.state = GCSpause;  /* End of GC cycle. */
    g->gc.debt = 0;
    return 0;
  default:
    lj_assertG(0, "bad GC state");
    return 0;
  }
}

#if LJ_54
static void gc_gen_minor54(lua_State *L)
{
  global_State *g = G(L);
  MSize i;
  lj_assertG(g->gc_mode54 && g->gc_genactive54,
	     "bad state for generational minor collection");
  gc_gen_revisit_old54(g);
  g->gc.state = GCSatomic;
  atomic(g, L);
  for (i = g->str.mask; i != ~(MSize)0; i--)
    gc_sweepstrgen54(g, &g->str.tab[i]);
  gc_sweepgen54(L, g, &g->gc.root);
  gc_correctgraygen54(g);
  gc_clear_weak_lists54(g);
  while (gcref(g->gc.mmudata) != NULL)
    gc_finalize(L);
  g->gc.state = GCSpropagate;
  g->gc.threshold = gc_gen_threshold54(g);
}

static void gc_gen_major54(lua_State *L)
{
  global_State *g = G(L);
  GCSize majorbase = g->gc.estimate;
  GCSize majorinc = gc_gen_majorinc54(g, majorbase);
  GCSize lastatomic = g->gc_genlastatomic54;
  lj_gc_gen_whitelist54(g);
  lj_gc_fullgc(L);
  if (lastatomic != 0) {
    GCSize newatomic = gc_gen_atomicwork54(g);
    if (gc_gen_atomicwork_good54(newatomic, lastatomic)) {
      g->gc_genlastatomic54 = 0;
      g->gc.threshold = gc_gen_threshold54(g);
    } else {
      g->gc_genlastatomic54 = newatomic;
      g->gc.estimate = g->gc.total;
      gc_gen_setpause54(g);
    }
  } else if (majorinc != LJ_MAX_MEM &&
	     g->gc.total >= majorbase + (majorinc >> 1)) {
    g->gc_genlastatomic54 = gc_gen_atomicwork54(g);
    g->gc.estimate = g->gc.total;
    gc_gen_setpause54(g);
  } else {
    g->gc_genlastatomic54 = 0;
    g->gc.threshold = gc_gen_threshold54(g);
  }
}

static void gc_gen_fincheck_step54(global_State *g)
{
  if (g->gc.fin_check != 0) {
    g->gc.fin_check--;
    g->gc.threshold = g->gc.total;
  }
}
#endif

/* Perform a limited amount of incremental GC steps. */
int LJ_FASTCALL lj_gc_step(lua_State *L)
{
  global_State *g = G(L);
#if LJ_54
  GCSize stepsize = gc_stepsize54(g);
#else
  GCSize stepsize = GCSTEPSIZE;
#endif
  GCSize lim;
  int32_t ostate = g->vmstate;
  setvmstate(g, GC);
#if LJ_54
  if (g->gc_mode54 && g->gc_genactive54) {
    if (gc_gen_canminor54(g)) {
      if (g->gc_genlastatomic54 != 0 || gc_gen_needmajor54(g)) {
	gc_gen_major54(L);
	gc_gen_fincheck_step54(g);
	g->vmstate = ostate;
	return 1;
      } else {
	if (tvref(g->jit_base)) {
	  g->gc.threshold = g->gc.total + stepsize;
	  g->vmstate = ostate;
	  return -1;
	}
	gc_gen_minor54(L);
	gc_gen_fincheck_step54(g);
	g->vmstate = ostate;
	return 1;
      }
    }
    lj_gc_gen_whitelist54(g);
  }
#endif
  if (g->gc.stepmul == 0) {
    lim = LJ_MAX_MEM;
  } else {
    GCSize stepunit = stepsize/100;
    if (stepunit == 0)
      lim = 1;
    else if (stepunit > LJ_MAX_MEM / g->gc.stepmul)
      lim = LJ_MAX_MEM;
    else
      lim = stepunit * g->gc.stepmul;
  }
  if (g->gc.total > g->gc.threshold)
    g->gc.debt += g->gc.total - g->gc.threshold;
  do {
    lim -= (GCSize)gc_onestep(L);
    if (g->gc.state == GCSpause) {
#if LJ_54
      if (g->gc_mode54) {
	gc_gen_enter54(g);
	if (g->gc.fin_check != 0) {
	  g->gc.fin_check--;
	  g->gc.threshold = g->gc.total;
	} else {
	  g->gc.threshold = gc_gen_threshold54(g);
	}
      } else
#endif
	g->gc.threshold = (g->gc.estimate/100) * g->gc.pause;
      g->vmstate = ostate;
      return 1;  /* Finished a GC cycle. */
    }
  } while (sizeof(lim) == 8 ? ((int64_t)lim > 0) : ((int32_t)lim > 0));
  if (g->gc.debt < stepsize) {
#if LJ_54
    if (g->gc.fin_check != 0)
      g->gc.threshold = g->gc.total;
    else
#endif
    g->gc.threshold = g->gc.total + GCSTEPSIZE;
    g->vmstate = ostate;
    return -1;
  } else {
    g->gc.debt -= stepsize;
    g->gc.threshold = g->gc.total;
    g->vmstate = ostate;
    return 0;
  }
}

/* Ditto, but fix the stack top first. */
void LJ_FASTCALL lj_gc_step_fixtop(lua_State *L)
{
  if (curr_funcisL(L)) L->top = curr_topL(L);
  lj_gc_step(L);
}

#if LJ_HASJIT
/* Perform multiple GC steps. Called from JIT-compiled code. */
int LJ_FASTCALL lj_gc_step_jit(global_State *g, MSize steps)
{
  lua_State *L = gco2th(gcref(g->cur_L));
  L->base = tvref(G(L)->jit_base);
  L->top = curr_topL(L);
  while (steps-- > 0 && lj_gc_step(L) == 0)
    ;
  /* Return 1 to force a trace exit. */
  return (G(L)->gc.state == GCSatomic || G(L)->gc.state == GCSfinalize);
}
#endif

/* Perform a full GC cycle. */
void lj_gc_fullgc(lua_State *L)
{
  global_State *g = G(L);
  int32_t ostate = g->vmstate;
  setvmstate(g, GC);
#if LJ_54
  if (g->gc_mode54 && g->gc_genactive54)
    lj_gc_gen_whitelist54(g);
#endif
  if (g->gc.state <= GCSatomic) {  /* Caught somewhere in the middle. */
    setmref(g->gc.sweep, &g->gc.root);  /* Sweep everything (preserving it). */
    setgcrefnull(g->gc.gray);  /* Reset lists from partial propagation. */
    setgcrefnull(g->gc.grayagain);
#if LJ_54
    gc_clear_weak_lists54(g);
#else
    setgcrefnull(g->gc.weak);
#endif
    g->gc.state = GCSsweepstring;  /* Fast forward to the sweep phase. */
    g->gc.sweepstr = 0;
  }
  while (g->gc.state == GCSsweepstring || g->gc.state == GCSsweep)
    gc_onestep(L);  /* Finish sweep. */
  lj_assertG(g->gc.state == GCSfinalize || g->gc.state == GCSpause,
	     "bad GC state");
  /* Now perform a full GC. */
  g->gc.state = GCSpause;
  do { gc_onestep(L); } while (g->gc.state != GCSpause);
#if LJ_54
  if (g->gc_mode54) {
    gc_gen_enter54(g);
    g->gc.threshold = gc_gen_threshold54(g);
  } else
#endif
    g->gc.threshold = (g->gc.estimate/100) * g->gc.pause;
  g->vmstate = ostate;
}

/* -- Write barriers ------------------------------------------------------ */

/* Move the GC propagation frontier forward. */
void lj_gc_barrierf(global_State *g, GCobj *o, GCobj *v)
{
  lj_assertG(isblack(o) && iswhite(v) && !isdead(g, v) && !isdead(g, o),
	     "bad object states for forward barrier");
  lj_assertG(g->gc.state != GCSfinalize && g->gc.state != GCSpause,
	     "bad GC state");
  lj_assertG(o->gch.gct != ~LJ_TTAB, "barrier object is not a table");
  /* Preserve invariant during propagation. Otherwise it doesn't matter. */
  if (g->gc.state == GCSpropagate || g->gc.state == GCSatomic) {
    gc_mark(g, v);  /* Move frontier forward. */
#if LJ_54
    if (g->gc_mode54 && isoldgc(o) && !isoldgc(v))
      setgcage(v, LJ_GC_AGE_OLD0);
#endif
  } else {
    makewhite(g, o);  /* Make it white to avoid the following barrier. */
  }
}

/* Specialized barrier for closed upvalue. Pass &uv->tv. */
void LJ_FASTCALL lj_gc_barrieruv(global_State *g, TValue *tv)
{
#define TV2MARKED(x) \
  (*((uint8_t *)(x) - offsetof(GCupval, tv) + offsetof(GCupval, marked)))
#define TV2UPVAL(x) \
  ((GCupval *)((uint8_t *)(x) - offsetof(GCupval, tv)))
  if (g->gc.state == GCSpropagate || g->gc.state == GCSatomic) {
    GCobj *v = gcV(tv);
    gc_mark(g, v);
#if LJ_54
    if (g->gc_mode54 && isoldgc(obj2gco(TV2UPVAL(tv))) && !isoldgc(v))
      setgcage(v, LJ_GC_AGE_OLD0);
#endif
  }
  else
    TV2MARKED(tv) = (TV2MARKED(tv) & (uint8_t)~LJ_GC_COLORS) | curwhite(g);
#undef TV2UPVAL
#undef TV2MARKED
}

/* Close upvalue. Also needs a write barrier. */
void lj_gc_closeuv(global_State *g, GCupval *uv)
{
  GCobj *o = obj2gco(uv);
  /* Copy stack slot to upvalue itself and point to the copy. */
  copyTV(mainthread(g), &uv->tv, uvval(uv));
  setmref(uv->v, &uv->tv);
  uv->closed = 1;
  setgcrefr(o->gch.nextgc, g->gc.root);
  setgcref(g->gc.root, o);
  if (isgray(o)) {  /* A closed upvalue is never gray, so fix this. */
    if (g->gc.state == GCSpropagate || g->gc.state == GCSatomic) {
      gray2black(o);  /* Make it black and preserve invariant. */
      if (tviswhite(&uv->tv))
	lj_gc_barrierf(g, o, gcV(&uv->tv));
    } else {
      makewhite(g, o);  /* Make it white, i.e. sweep the upvalue. */
      lj_assertG(g->gc.state != GCSfinalize && g->gc.state != GCSpause,
		 "bad GC state");
    }
  }
}

#if LJ_HASJIT
/* Mark a trace if it's saved during the propagation phase. */
void lj_gc_barriertrace(global_State *g, uint32_t traceno)
{
  if (g->gc.state == GCSpropagate || g->gc.state == GCSatomic)
    gc_marktrace(g, traceno);
}
#endif

/* -- Allocator ----------------------------------------------------------- */

/* Call pluggable memory allocator to allocate or resize a fragment. */
void *lj_mem_realloc(lua_State *L, void *p, GCSize osz, GCSize nsz)
{
  global_State *g = G(L);
  lj_assertG((osz == 0) == (p == NULL), "realloc API violation");
  p = g->allocf(g->allocd, p, osz, nsz);
  if (p == NULL && nsz > 0)
    lj_err_mem(L);
  lj_assertG((nsz == 0) == (p == NULL), "allocf API violation");
  lj_assertG(checkptrGC(p),
	     "allocated memory address %p outside required range", p);
  g->gc.total = (g->gc.total - osz) + nsz;
  return p;
}

/* Non-throwing realloc for opportunistic memory trims.
** Lua 5.4 lets these shrinks fail; callers must leave their logical state
** unchanged when NULL is returned.
*/
void *lj_mem_realloc_noerr(lua_State *L, void *p, GCSize osz, GCSize nsz)
{
  global_State *g = G(L);
  void *np;
  lj_assertG((osz == 0) == (p == NULL), "realloc API violation");
  np = g->allocf(g->allocd, p, osz, nsz);
  if (np == NULL && nsz > 0)
    return NULL;
  lj_assertG((nsz == 0) == (np == NULL), "allocf API violation");
  lj_assertG(checkptrGC(np),
	     "allocated memory address %p outside required range", np);
  g->gc.total = (g->gc.total - osz) + nsz;
  return np;
}

/* Allocate new GC object and link it to the root set. */
void * LJ_FASTCALL lj_mem_newgco(lua_State *L, GCSize size)
{
  global_State *g = G(L);
  GCobj *o = (GCobj *)g->allocf(g->allocd, NULL, 0, size);
  if (o == NULL)
    lj_err_mem(L);
  lj_assertG(checkptrGC(o),
	     "allocated memory address %p outside required range", o);
  g->gc.total += size;
  setgcrefr(o->gch.nextgc, g->gc.root);
  setgcref(g->gc.root, o);
  newwhite(g, o);
  return o;
}

/* Resize growable vector. */
void *lj_mem_grow(lua_State *L, void *p, MSize *szp, MSize lim, MSize esz)
{
  MSize sz = (*szp) << 1;
  if (sz < LJ_MIN_VECSZ)
    sz = LJ_MIN_VECSZ;
  if (sz > lim)
    sz = lim;
  p = lj_mem_realloc(L, p, (*szp)*esz, sz*esz);
  *szp = sz;
  return p;
}
