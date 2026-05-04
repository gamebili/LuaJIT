/*
** String library.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lib_string_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_state.h"
#include "lj_ff.h"
#include "lj_bcdump.h"
#include "lj_char.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "lj_lib.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_string

#if LJ_54
static void string_argerror_named54(lua_State *L, int narg, const char *fname,
				    const char *msg)
{
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      narg, fname, msg));
}

static const char *string_argtypename54(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (o < L->top) {
    MSize tlen;
    const char *tname = lj_meta_objtypename(L, o, &tlen);
    UNUSED(tlen);
    return tname;
  }
  return lj_obj_typename[0];
}

static void string_argtype_named54(lua_State *L, int narg, const char *fname,
				   const char *xname)
{
  string_argerror_named54(L, narg, fname,
    lj_strfmt_pushf(L, "%s expected, got %s", xname,
		    string_argtypename54(L, narg)));
}

static GCstr *string_checkstr_named54(lua_State *L, int narg,
				      const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o < L->top) {
    if (tvisstr(o)) {
      return strV(o);
    } else if (tvisnumber(o)) {
      GCstr *s = lj_strfmt_number(L, o);
      setstrV(L, o, s);
      return s;
    }
  }
  string_argtype_named54(L, narg, fname, "string");
  return NULL;  /* unreachable */
}

static const char *string_checklstring_named54(lua_State *L, int narg,
					       size_t *len,
					       const char *fname)
{
  GCstr *s = string_checkstr_named54(L, narg, fname);
  if (len)
    *len = s->len;
  return strdata(s);
}

static GCstr *string_optstr_named54(lua_State *L, int narg,
				    const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o >= L->top || tvisnil(o))
    return NULL;
  return string_checkstr_named54(L, narg, fname);
}

static lua_Number string_checknum_named54(lua_State *L, int narg,
					  const char *fname)
{
  TValue tmp;
  cTValue *o = L->base + narg-1;
  if (o >= L->top)
    string_argtype_named54(L, narg, fname, "number");
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      string_argtype_named54(L, narg, fname, "number");
    o = &tmp;
  }
  if (tvisint(o))
    return (lua_Number)intV(o);
  if (!tvisnum(o))
    string_argtype_named54(L, narg, fname, "number");
  return numV(o);
}

static int32_t string_checkint_named54(lua_State *L, int narg,
				       const char *fname)
{
  lua_Number n = string_checknum_named54(L, narg, fname);
  int64_t k;
  /* Lua 5.4 string-library positions and counts use exact integer
  ** conversion. Keep string numerals, reject fractions instead of truncating.
  */
  if (!(n >= (lua_Number)LUA_MININTEGER && n <= (lua_Number)LUA_MAXINTEGER))
    string_argerror_named54(L, narg, fname,
			    "number has no integer representation");
  k = lj_num2i64(n);
  if ((lua_Number)k != n)
    string_argerror_named54(L, narg, fname,
			    "number has no integer representation");
  return (int32_t)k;
}

static int32_t string_optint_named54(lua_State *L, int narg, int32_t def,
				     const char *fname)
{
  TValue *o = L->base + narg-1;
  return (o < L->top && !tvisnil(o)) ?
	 string_checkint_named54(L, narg, fname) : def;
}

static GCproto *string_checkLproto_named54(lua_State *L, int narg,
					   const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o < L->top) {
    if (tvisproto(o))
      return protoV(o);
    if (tvisfunc(o)) {
      GCfunc *fn = funcV(o);
      if (isluafunc(fn))
	return funcproto(fn);
      return NULL;
    }
  }
  string_argtype_named54(L, narg, fname, "function");
  return NULL;  /* unreachable */
}
#endif

LJLIB_LUA(string_len) /*
  function(s)
    CHECK_str(s)
    return #s
  end
*/

LJLIB_ASM(string_byte)		LJLIB_REC(string_range 0)
{
#if LJ_54
  GCstr *s = string_checkstr_named54(L, 1, "string.byte");
#else
  GCstr *s = lj_lib_checkstr(L, 1);
#endif
  int32_t len = (int32_t)s->len;
#if LJ_54
  int32_t start = string_optint_named54(L, 2, 1, "string.byte");
  int32_t stop = string_optint_named54(L, 3, start, "string.byte");
#else
  int32_t start = lj_lib_optint(L, 2, 1);
  int32_t stop = lj_lib_optint(L, 3, start);
#endif
  int32_t n, i;
  const unsigned char *p;
  if (stop < 0) stop += len+1;
  if (start < 0) start += len+1;
  if (start <= 0) start = 1;
  if (stop > len) stop = len;
  if (start > stop) return FFH_RES(0);  /* Empty interval: return no results. */
  start--;
  n = stop - start;
  if ((uint32_t)n > LUAI_MAXCSTACK)
    lj_err_caller(L, LJ_ERR_STRSLC);
  lj_state_checkstack(L, (MSize)n);
  p = (const unsigned char *)strdata(s) + start;
  for (i = 0; i < n; i++)
    setintV(L->base + i-1-LJ_FR2, p[i]);
  return FFH_RES(n);
}

LJLIB_ASM(string_char)		LJLIB_REC(.)
{
  int i, nargs = (int)(L->top - L->base);
  char *buf = lj_buf_tmp(L, (MSize)nargs);
  for (i = 1; i <= nargs; i++) {
#if LJ_54
    int32_t k = string_checkint_named54(L, i, "string.char");
#else
    int32_t k = lj_lib_checkint(L, i);
#endif
    if (!checku8(k))
#if LJ_54
      string_argerror_named54(L, i, "string.char", "value out of range");
#else
      lj_err_arg(L, i, LJ_ERR_BADVAL);
#endif
    buf[i-1] = (char)k;
  }
  setstrV(L, L->base-1-LJ_FR2, lj_str_new(L, buf, (size_t)nargs));
  return FFH_RES(1);
}

LJLIB_ASM(string_sub)		LJLIB_REC(string_range 1)
{
#if LJ_54
  string_checkstr_named54(L, 1, "string.sub");
#else
  lj_lib_checkstr(L, 1);
#endif
#if LJ_54
  string_checkint_named54(L, 2, "string.sub");
  setintV(L->base+2, string_optint_named54(L, 3, -1, "string.sub"));
#else
  lj_lib_checkint(L, 2);
  setintV(L->base+2, lj_lib_optint(L, 3, -1));
#endif
  return FFH_RETRY;
}

LJLIB_CF(string_rep)		LJLIB_REC(.)
{
#if LJ_54
  GCstr *s = string_checkstr_named54(L, 1, "string.rep");
  int32_t rep = string_checkint_named54(L, 2, "string.rep");
  GCstr *sep = string_optstr_named54(L, 3, "string.rep");
#else
  GCstr *s = lj_lib_checkstr(L, 1);
  int32_t rep = lj_lib_checkint(L, 2);
  GCstr *sep = lj_lib_optstr(L, 3);
#endif
  SBuf *sb = lj_buf_tmp_(L);
  if (sep && rep > 1) {
    GCstr *s2 = lj_buf_cat2str(L, sep, s);
    lj_buf_reset(sb);
    lj_buf_putstr(sb, s);
    s = s2;
    rep--;
  }
  sb = lj_buf_putstr_rep(sb, s, rep);
  setstrV(L, L->top-1, lj_buf_str(L, sb));
  lj_gc_check(L);
  return 1;
}

LJLIB_ASM(string_reverse)  LJLIB_REC(string_op IRCALL_lj_buf_putstr_reverse)
{
#if LJ_54
  string_checkstr_named54(L, 1, "string.reverse");
#else
  lj_lib_checkstr(L, 1);
#endif
  return FFH_RETRY;
}
LJLIB_ASM_(string_lower)  LJLIB_REC(string_op IRCALL_lj_buf_putstr_lower)
LJLIB_ASM_(string_upper)  LJLIB_REC(string_op IRCALL_lj_buf_putstr_upper)

/* ------------------------------------------------------------------------ */

static int writer_buf(lua_State *L, const void *p, size_t size, void *sb)
{
  lj_buf_putmem((SBuf *)sb, p, (MSize)size);
  UNUSED(L);
  return 0;
}

LJLIB_CF(string_dump)
{
#if LJ_54
  GCproto *pt = string_checkLproto_named54(L, 1, "string.dump");
#else
  GCproto *pt = lj_lib_checkLproto(L, 1, 1);
#endif
  uint32_t flags = 0;
  SBuf *sb;
  TValue *o = L->base+1;
  if (o < L->top) {
    if (tvisstr(o)) {
      const char *mode = strVdata(o);
      char c;
      while ((c = *mode++)) {
	if (c == 's') flags |= BCDUMP_F_STRIP;
	if (c == 'd') flags |= BCDUMP_F_DETERMINISTIC;
      }
    } else if (tvistruecond(o)) {
      flags |= BCDUMP_F_STRIP;
    }
  }
  sb = lj_buf_tmp_(L);  /* Assumes lj_bcwrite() doesn't use tmpbuf. */
  L->top = L->base+1;
  if (!pt || lj_bcwrite(L, pt, writer_buf, sb, flags))
    lj_err_caller(L, LJ_ERR_STRDUMP);
  setstrV(L, L->top-1, lj_buf_str(L, sb));
  lj_gc_check(L);
  return 1;
}

/* ------------------------------------------------------------------------ */

/* macro to `unsign' a character */
#define uchar(c)	((unsigned char)(c))

#define CAP_UNFINISHED	(-1)
#define CAP_POSITION	(-2)

typedef struct MatchState {
  const char *src_init;  /* init of source string */
  const char *src_end;  /* end (`\0') of source string */
  const char *p_end;  /* Lua 5.4 patterns may contain embedded NUL bytes. */
  lua_State *L;
  int level;  /* total number of captures (finished or unfinished) */
  int depth;
  struct {
    const char *init;
    ptrdiff_t len;
  } capture[LUA_MAXCAPTURES];
} MatchState;

#define L_ESC		'%'

static int check_capture(MatchState *ms, int l)
{
  l -= '1';
  if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED)
    lj_err_caller(ms->L, LJ_ERR_STRCAPI);
  return l;
}

static int capture_to_close(MatchState *ms)
{
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  lj_err_caller(ms->L, LJ_ERR_STRPATC);
  return 0;  /* unreachable */
}

static const char *classend(MatchState *ms, const char *p)
{
  switch (*p++) {
  case L_ESC:
    if (p == ms->p_end)
      lj_err_caller(ms->L, LJ_ERR_STRPATE);
    return p+1;
  case '[':
    if (*p == '^') p++;
    do {  /* look for a `]' */
      if (p == ms->p_end)
	lj_err_caller(ms->L, LJ_ERR_STRPATM);
      if (*(p++) == L_ESC && p < ms->p_end)
	p++;  /* skip escapes (e.g. `%]') */
    } while (p == ms->p_end || *p != ']');
    return p+1;
  default:
    return p;
  }
}

static const unsigned char match_class_map[32] = {
  0,LJ_CHAR_ALPHA,0,LJ_CHAR_CNTRL,LJ_CHAR_DIGIT,0,0,LJ_CHAR_GRAPH,0,0,0,0,
  LJ_CHAR_LOWER,0,0,0,LJ_CHAR_PUNCT,0,0,LJ_CHAR_SPACE,0,
  LJ_CHAR_UPPER,0,LJ_CHAR_ALNUM,LJ_CHAR_XDIGIT,0,0,0,0,0,0,0
};

static int match_class(int c, int cl)
{
  if ((cl & 0xc0) == 0x40) {
    int t = match_class_map[(cl&0x1f)];
    if (t) {
      t = lj_char_isa(c, t);
      return (cl & 0x20) ? t : !t;
    }
    if (cl == 'z') return c == 0;
    if (cl == 'Z') return c != 0;
  }
  return (cl == c);
}

static int matchbracketclass(int c, const char *p, const char *ec)
{
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the `^' */
  }
  while (++p < ec) {
    if (*p == L_ESC) {
      p++;
      if (match_class(c, uchar(*p)))
	return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < ec)) {
      p+=2;
      if (uchar(*(p-2)) <= c && c <= uchar(*p))
	return sig;
    }
    else if (uchar(*p) == c) return sig;
  }
  return !sig;
}

static int singlematch(int c, const char *p, const char *ep)
{
  switch (*p) {
  case '.': return 1;  /* matches any char */
  case L_ESC: return match_class(c, uchar(*(p+1)));
  case '[': return matchbracketclass(c, p, ep-1);
  default:  return (uchar(*p) == c);
  }
}

static const char *match(MatchState *ms, const char *s, const char *p);

static const char *matchbalance(MatchState *ms, const char *s, const char *p)
{
  if (p+1 >= ms->p_end)
    lj_err_caller(ms->L, LJ_ERR_STRPATU);
  if (*s != *p) {
    return NULL;
  } else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
	if (--cont == 0) return s+1;
      } else if (*s == b) {
	cont++;
      }
    }
  }
  return NULL;  /* string ends out of balance */
}

static const char *max_expand(MatchState *ms, const char *s,
			      const char *p, const char *ep)
{
  ptrdiff_t i = 0;  /* counts maximum expand for item */
  while ((s+i)<ms->src_end && singlematch(uchar(*(s+i)), p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char *res = match(ms, (s+i), ep+1);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}

static const char *min_expand(MatchState *ms, const char *s,
			      const char *p, const char *ep)
{
  for (;;) {
    const char *res = match(ms, s, ep+1);
    if (res != NULL)
      return res;
    else if (s<ms->src_end && singlematch(uchar(*s), p, ep))
      s++;  /* try with one more repetition */
    else
      return NULL;
  }
}

static const char *start_capture(MatchState *ms, const char *s,
				 const char *p, int what)
{
  const char *res;
  int level = ms->level;
  if (level >= LUA_MAXCAPTURES) lj_err_caller(ms->L, LJ_ERR_STRCAPN);
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level+1;
  if ((res=match(ms, s, p)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}

static const char *end_capture(MatchState *ms, const char *s,
			       const char *p)
{
  int l = capture_to_close(ms);
  const char *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}

static const char *match_capture(MatchState *ms, const char *s, int l)
{
  size_t len;
  l = check_capture(ms, l);
  len = (size_t)ms->capture[l].len;
  if ((size_t)(ms->src_end-s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
    return s+len;
  else
    return NULL;
}

static const char *match(MatchState *ms, const char *s, const char *p)
{
  if (++ms->depth > LJ_MAX_XLEVEL)
    lj_err_caller(ms->L, LJ_ERR_STRPATX);
  init: /* using goto's to optimize tail recursion */
  if (p == ms->p_end)
    goto done;  /* end of pattern; embedded '\0' is just another byte. */
  switch (*p) {
  case '(':  /* start capture */
    if (p+1 < ms->p_end && *(p+1) == ')')  /* position capture? */
      s = start_capture(ms, s, p+2, CAP_POSITION);
    else
      s = start_capture(ms, s, p+1, CAP_UNFINISHED);
    break;
  case ')':  /* end capture */
    s = end_capture(ms, s, p+1);
    break;
  case L_ESC:
    switch (*(p+1)) {
    case 'b':  /* balanced string? */
      s = matchbalance(ms, s, p+2);
      if (s == NULL) break;
      p+=4;
      goto init;  /* else s = match(ms, s, p+4); */
    case 'f': {  /* frontier? */
      const char *ep; char previous;
      p += 2;
      if (p == ms->p_end || *p != '[')
	lj_err_caller(ms->L, LJ_ERR_STRPATB);
      ep = classend(ms, p);  /* points to what is next */
      previous = (s == ms->src_init) ? '\0' : *(s-1);
      if (matchbracketclass(uchar(previous), p, ep-1) ||
	 !matchbracketclass(uchar(*s), p, ep-1)) { s = NULL; break; }
      p=ep;
      goto init;  /* else s = match(ms, s, ep); */
      }
    default:
      if (lj_char_isdigit(uchar(*(p+1)))) {  /* capture results (%0-%9)? */
	s = match_capture(ms, s, uchar(*(p+1)));
	if (s == NULL) break;
	p+=2;
	goto init;  /* else s = match(ms, s, p+2) */
      }
      goto dflt;  /* case default */
    }
    break;
  case '$':
    /* is the `$' the last char in pattern? */
    if (p+1 != ms->p_end) goto dflt;
    if (s != ms->src_end) s = NULL;  /* check end of string */
    break;
  default: dflt: {  /* it is a pattern item */
    const char *ep = classend(ms, p);  /* points to what is next */
    int m = s<ms->src_end && singlematch(uchar(*s), p, ep);
    switch (ep < ms->p_end ? *ep : '\0') {
    case '?': {  /* optional */
      const char *res;
      if (m && ((res=match(ms, s+1, ep+1)) != NULL)) {
	s = res;
	break;
      }
      p=ep+1;
      goto init;  /* else s = match(ms, s, ep+1); */
      }
    case '*':  /* 0 or more repetitions */
      s = max_expand(ms, s, p, ep);
      break;
    case '+':  /* 1 or more repetitions */
      s = (m ? max_expand(ms, s+1, p, ep) : NULL);
      break;
    case '-':  /* 0 or more repetitions (minimum) */
      s = min_expand(ms, s, p, ep);
      break;
    default:
      if (m) { s++; p=ep; goto init; }  /* else s = match(ms, s+1, ep); */
      s = NULL;
      break;
    }
    break;
    }
  }
done:
  ms->depth--;
  return s;
}

static void push_onecapture(MatchState *ms, int i, const char *s, const char *e)
{
  if (i >= ms->level) {
    if (i == 0)  /* ms->level == 0, too */
      lua_pushlstring(ms->L, s, (size_t)(e - s));  /* add whole match */
    else
      lj_err_caller(ms->L, LJ_ERR_STRCAPI);
  } else {
    ptrdiff_t l = ms->capture[i].len;
    if (l == CAP_UNFINISHED) lj_err_caller(ms->L, LJ_ERR_STRCAPU);
    if (l == CAP_POSITION)
      lua_pushinteger(ms->L, ms->capture[i].init - ms->src_init + 1);
    else
      lua_pushlstring(ms->L, ms->capture[i].init, (size_t)l);
  }
}

static int push_captures(MatchState *ms, const char *s, const char *e)
{
  int i;
  int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
  luaL_checkstack(ms->L, nlevels, "too many captures");
  for (i = 0; i < nlevels; i++)
    push_onecapture(ms, i, s, e);
  return nlevels;  /* number of strings pushed */
}

static int str_find_aux(lua_State *L, int find, const char *fname)
{
#if LJ_54
  GCstr *s = string_checkstr_named54(L, 1, fname);
  GCstr *p = string_checkstr_named54(L, 2, fname);
#else
  GCstr *s = lj_lib_checkstr(L, 1);
  GCstr *p = lj_lib_checkstr(L, 2);
#endif
#if LJ_54
  int32_t start = string_optint_named54(L, 3, 1, fname);
#else
  int32_t start = lj_lib_optint(L, 3, 1);
#endif
  MSize st;
  if (start < 0) start += (int32_t)s->len; else start--;
  if (start < 0) start = 0;
  st = (MSize)start;
  if (st > s->len) {
#if LJ_52
    setnilV(L->top-1);
    return 1;
#else
    st = s->len;
#endif
  }
  if (find && ((L->base+3 < L->top && tvistruecond(L->base+3)) ||
	       !lj_str_haspattern(p))) {  /* Search for fixed string. */
    const char *q = lj_str_find(strdata(s)+st, strdata(p), s->len-st, p->len);
    if (q) {
      setintV(L->top-2, (int32_t)(q-strdata(s)) + 1);
      setintV(L->top-1, (int32_t)(q-strdata(s)) + (int32_t)p->len);
      return 2;
    }
  } else {  /* Search for pattern. */
    MatchState ms;
    const char *pstr = strdata(p);
    const char *pend = pstr + p->len;
    const char *sstr = strdata(s) + st;
    int anchor = 0;
    if (pstr < pend && *pstr == '^') { pstr++; anchor = 1; }
    ms.L = L;
    ms.src_init = strdata(s);
    ms.src_end = strdata(s) + s->len;
    ms.p_end = pend;
    do {  /* Loop through string and try to match the pattern. */
      const char *q;
      ms.level = ms.depth = 0;
      q = match(&ms, sstr, pstr);
      if (q) {
	if (find) {
	  setintV(L->top++, (int32_t)(sstr-(strdata(s)-1)));
	  setintV(L->top++, (int32_t)(q-strdata(s)));
	  return push_captures(&ms, NULL, NULL) + 2;
	} else {
	  return push_captures(&ms, sstr, q);
	}
      }
    } while (sstr++ < ms.src_end && !anchor);
  }
  setnilV(L->top-1);  /* Not found. */
  return 1;
}

LJLIB_CF(string_find)		LJLIB_REC(.)
{
  return str_find_aux(L, 1, "string.find");
}

LJLIB_CF(string_match)
{
  return str_find_aux(L, 0, "string.match");
}

LJLIB_NOREG LJLIB_CF(string_gmatch_aux)
{
  GCstr *pat = strV(lj_lib_upvalue(L, 2));
  const char *p = strdata(pat);
  GCstr *str = strV(lj_lib_upvalue(L, 1));
  const char *s = strdata(str);
  TValue *tvpos = lj_lib_upvalue(L, 3);
  const char *src = s + tvpos->u32.lo;
  MatchState ms;
  ms.L = L;
  ms.src_init = s;
  ms.src_end = s + str->len;
  ms.p_end = p + pat->len;
  for (; src <= ms.src_end; src++) {
    const char *e;
    ms.level = ms.depth = 0;
    if ((e = match(&ms, src, p)) != NULL) {
      int32_t pos = (int32_t)(e - s);
      if (e == src) pos++;  /* Ensure progress for empty match. */
      tvpos->u32.lo = (uint32_t)pos;
      return push_captures(&ms, src, e);
    }
  }
  return 0;  /* not found */
}

LJLIB_CF(string_gmatch)
{
#if LJ_54
  GCstr *s = string_checkstr_named54(L, 1, "string.gmatch");
  int32_t start = string_optint_named54(L, 3, 1, "string.gmatch");
#else
  GCstr *s = lj_lib_checkstr(L, 1);
  int32_t start = lj_lib_optint(L, 3, 1);
#endif
  MSize st;
#if LJ_54
  string_checkstr_named54(L, 2, "string.gmatch");
#else
  lj_lib_checkstr(L, 2);
#endif
  if (start < 0) start += (int32_t)s->len; else start--;
  if (start < 0) start = 0;
  st = (MSize)start;
  if (st > s->len)
    st = s->len + 1;
  L->top = L->base+3;
  (L->top-1)->u64 = (uint64_t)st;
  lj_lib_pushcc(L, lj_cf_string_gmatch_aux, FF_string_gmatch_aux, 3);
  return 1;
}

static void add_s(MatchState *ms, luaL_Buffer *b, const char *s, const char *e)
{
  size_t l, i;
  const char *news = lua_tolstring(ms->L, 3, &l);
  for (i = 0; i < l; i++) {
    if (news[i] != L_ESC) {
      luaL_addchar(b, news[i]);
    } else {
      i++;  /* skip ESC */
      if (!lj_char_isdigit(uchar(news[i]))) {
	luaL_addchar(b, news[i]);
      } else if (news[i] == '0') {
	luaL_addlstring(b, s, (size_t)(e - s));
      } else {
	push_onecapture(ms, news[i] - '1', s, e);
	luaL_addvalue(b);  /* add capture to accumulated result */
      }
    }
  }
}

static void add_value(MatchState *ms, luaL_Buffer *b,
		      const char *s, const char *e)
{
  lua_State *L = ms->L;
  switch (lua_type(L, 3)) {
    case LUA_TNUMBER:
    case LUA_TSTRING: {
      add_s(ms, b, s, e);
      return;
    }
    case LUA_TFUNCTION: {
      int n;
      lua_pushvalue(L, 3);
      n = push_captures(ms, s, e);
      lua_call(L, n, 1);
      break;
    }
    case LUA_TTABLE: {
      push_onecapture(ms, 0, s, e);
      lua_gettable(L, 3);
      break;
    }
  }
  if (!lua_toboolean(L, -1)) {  /* nil or false? */
    lua_pop(L, 1);
    lua_pushlstring(L, s, (size_t)(e - s));  /* keep original text */
  } else if (!lua_isstring(L, -1)) {
    lj_err_callerv(L, LJ_ERR_STRGSRV, luaL_typename(L, -1));
  }
  luaL_addvalue(b);  /* add result to accumulator */
}

LJLIB_CF(string_gsub)
{
  size_t srcl;
#if LJ_54
  const char *src = string_checklstring_named54(L, 1, &srcl, "string.gsub");
  GCstr *pat = string_checkstr_named54(L, 2, "string.gsub");
  const char *p = strdata(pat);
  const char *pend = p + pat->len;
#else
  size_t plen;
  const char *src = luaL_checklstring(L, 1, &srcl);
  const char *p = luaL_checklstring(L, 2, &plen);
  const char *pend = p + plen;
#endif
  int  tr = lua_type(L, 3);
#if LJ_54
  int max_s = string_optint_named54(L, 4, (int)(srcl+1), "string.gsub");
#else
  int max_s = luaL_optint(L, 4, (int)(srcl+1));
#endif
  int anchor = (p < pend && *p == '^') ? (p++, 1) : 0;
  int n = 0;
  MatchState ms;
  luaL_Buffer b;
  if (!(tr == LUA_TNUMBER || tr == LUA_TSTRING ||
	tr == LUA_TFUNCTION || tr == LUA_TTABLE))
#if LJ_54
    string_argerror_named54(L, 3, "string.gsub",
			    "string/function/table expected");
#else
    lj_err_arg(L, 3, LJ_ERR_NOSFT);
#endif
  luaL_buffinit(L, &b);
  ms.L = L;
  ms.src_init = src;
  ms.src_end = src+srcl;
  ms.p_end = pend;
  while (n < max_s) {
    const char *e;
    ms.level = ms.depth = 0;
    e = match(&ms, src, p);
    if (e) {
      n++;
      add_value(&ms, &b, src, e);
    }
    if (e && e>src) /* non empty match? */
      src = e;  /* skip it */
    else if (src < ms.src_end)
      luaL_addchar(&b, *src++);
    else
      break;
    if (anchor)
      break;
  }
  luaL_addlstring(&b, src, (size_t)(ms.src_end-src));
  luaL_pushresult(&b);
  lua_pushinteger(L, n);  /* number of substitutions */
  return 2;
}

/* ------------------------------------------------------------------------ */

LJLIB_CF(string_format)		LJLIB_REC(.)
{
  int retry = 0;
  SBuf *sb;
#if LJ_54
  string_checkstr_named54(L, 1, "string.format");
#endif
  do {
    sb = lj_buf_tmp_(L);
    retry = lj_strfmt_putarg(L, sb, 1, -retry);
  } while (retry > 0);
  setstrV(L, L->top-1, lj_buf_str(L, sb));
  lj_gc_check(L);
  return 1;
}

/* ------------------------------------------------------------------------ */

#if LJ_54
static int string_pack_native_little(void)
{
  uint32_t x = 1;
  return *(const unsigned char *)&x == 1;
}

static const char *string_pack_readsize(const char *fmt, size_t *szp,
					size_t def)
{
  size_t sz = 0;
  int has = 0;
  while (*fmt >= '0' && *fmt <= '9') {
    has = 1;
    sz = sz * 10 + (size_t)(*fmt - '0');
    fmt++;
  }
  *szp = has ? sz : def;
  return fmt;
}

static uint64_t string_pack_umax(size_t sz)
{
  return sz >= 8 ? ~(uint64_t)0 : (((uint64_t)1 << (sz * 8)) - 1);
}

static void string_pack_checksize(lua_State *L, size_t sz)
{
  if (sz < 1 || sz > 8)
    luaL_error(L, "integral size (%d) out of limits [1,8]", (int)sz);
}

static void string_pack_checkmaxalign(lua_State *L, size_t align)
{
  if (align < 1 || align > 16)
    luaL_argerror(L, 1, "format asks for invalid alignment");
}

static size_t string_pack_align(lua_State *L, size_t sz, size_t maxalign)
{
  size_t align = sz < maxalign ? sz : maxalign;
  if (align <= 1)
    return 1;
  if ((align & (align - 1)) != 0)
    luaL_argerror(L, 1, "format asks for alignment not power of 2");
  return align;
}

static size_t string_pack_padding(lua_State *L, size_t pos, size_t sz,
				  size_t maxalign)
{
  size_t align = string_pack_align(L, sz, maxalign);
  return align <= 1 ? 0 : ((align - (pos & (align - 1))) & (align - 1));
}

static void string_pack_addpadding(luaL_Buffer *b, size_t pad)
{
  while (pad-- > 0)
    luaL_addchar(b, '\0');
}

static const char *string_pack_xsize(lua_State *L, const char *fmt,
				     size_t *szp)
{
  char opt;
  while (*fmt == ' ' || *fmt == '\f' || *fmt == '\n' ||
	 *fmt == '\r' || *fmt == '\t' || *fmt == '\v')
    fmt++;
  opt = *fmt++;
  switch (opt) {
  case 'b': case 'B': *szp = 1; return fmt;
  case 'h': case 'H': *szp = 2; return fmt;
  case 'l': case 'L': *szp = sizeof(long); return fmt;
  case 'j': *szp = sizeof(lua_Integer); return fmt;
  case 'T': *szp = sizeof(size_t); return fmt;
  case 'f': *szp = sizeof(float); return fmt;
  case 'd': case 'n': *szp = sizeof(double); return fmt;
  case 'x': *szp = 1; return fmt;
  case 'i': case 'I':
    fmt = string_pack_readsize(fmt, szp, 4);
    string_pack_checksize(L, *szp);
    return fmt;
  case 's':
    return string_pack_readsize(fmt, szp, 4);
  default:
    luaL_argerror(L, 1, "invalid next option for option 'X'");
    return fmt;
  }
}

static int string_pack_endian(int endian)
{
  return endian < 0 ? string_pack_native_little() : endian;
}

static void string_pack_writeint(luaL_Buffer *b, uint64_t u, size_t sz,
				 int endian)
{
  char buf[8];
  size_t i;
  int le = string_pack_endian(endian);
  for (i = 0; i < sz; i++) {
    size_t shift = le ? i : (sz - 1 - i);
    buf[i] = (char)((u >> (shift * 8)) & 0xff);
  }
  luaL_addlstring(b, buf, sz);
}

static uint64_t string_pack_readint(const unsigned char *s, size_t sz,
				    int endian)
{
  uint64_t u = 0;
  size_t i;
  int le = string_pack_endian(endian);
  for (i = 0; i < sz; i++) {
    size_t shift = le ? i : (sz - 1 - i);
    u |= (uint64_t)s[i] << (shift * 8);
  }
  return u;
}

static uint64_t string_pack_checkint(lua_State *L, int arg, size_t sz,
				     int issigned)
{
#if LJ_54
  lua_Number n = string_checknum_named54(L, arg, "string.pack");
  int64_t v;
  if (!(n >= -9223372036854775808.0 && n <= 9223372036854775807.0))
    string_argerror_named54(L, arg, "string.pack",
			    "number has no integer representation");
  v = lj_num2i64(n);
  /* Pack formats define their own signed/unsigned range. Do the exact
  ** integer test here instead of using the current 32-bit lua_Integer shim,
  ** so existing Lua 5.4 pack cases such as I4/4000000000 keep working.
  */
  if ((lua_Number)v != n)
    string_argerror_named54(L, arg, "string.pack",
			    "number has no integer representation");
  if (issigned) {
    if (sz < 8) {
      int bits = (int)(sz * 8);
      int64_t minv = -(int64_t)((uint64_t)1 << (bits - 1));
      int64_t maxv = (int64_t)(((uint64_t)1 << (bits - 1)) - 1);
      if (v < minv || v > maxv)
	string_argerror_named54(L, arg, "string.pack", "integer overflow");
    }
    return (uint64_t)v;
  } else {
    uint64_t maxv = string_pack_umax(sz);
    if (v < 0 || (uint64_t)v > maxv)
      string_argerror_named54(L, arg, "string.pack", "unsigned overflow");
    return (uint64_t)v;
  }
#else
  lua_Integer v = luaL_checkinteger(L, arg);
  if (issigned) {
    if (sz < 8) {
      int bits = (int)(sz * 8);
      int64_t minv = -(int64_t)((uint64_t)1 << (bits - 1));
      int64_t maxv = (int64_t)(((uint64_t)1 << (bits - 1)) - 1);
      if ((int64_t)v < minv || (int64_t)v > maxv)
	luaL_argerror(L, arg, "integer overflow");
    }
    return (uint64_t)(int64_t)v;
  } else {
    uint64_t maxv = string_pack_umax(sz);
    if (v < 0 || (uint64_t)v > maxv)
      luaL_argerror(L, arg, "unsigned overflow");
    return (uint64_t)v;
  }
#endif
}

static void string_pack_writenum(luaL_Buffer *b, lua_State *L, int arg,
				 size_t sz, int endian)
{
  union { float f; double d; unsigned char b[8]; } u;
  char out[8];
  size_t i;
  int same = string_pack_endian(endian) == string_pack_native_little();
  if (sz == sizeof(float))
    u.f = (float)string_checknum_named54(L, arg, "string.pack");
  else
    u.d = (double)string_checknum_named54(L, arg, "string.pack");
  for (i = 0; i < sz; i++)
    out[i] = (char)(same ? u.b[i] : u.b[sz - 1 - i]);
  luaL_addlstring(b, out, sz);
}

static void string_pack_readnum(lua_State *L, const unsigned char *s,
				size_t sz, int endian)
{
  union { float f; double d; unsigned char b[8]; } u;
  size_t i;
  int same = string_pack_endian(endian) == string_pack_native_little();
  for (i = 0; i < sz; i++)
    u.b[i] = same ? s[i] : s[sz - 1 - i];
  if (sz == sizeof(float))
    lua_pushnumber(L, (lua_Number)u.f);
  else
    lua_pushnumber(L, (lua_Number)u.d);
}

static void string_pack_checkdata(lua_State *L, size_t pos, size_t need,
				  size_t len)
{
  if (pos > len || need > len - pos)
    luaL_argerror(L, 2, "data string too short");
}

static int lj_cf_string_pack(lua_State *L)
{
  const char *fmt = strdata(string_checkstr_named54(L, 1, "string.pack"));
  int endian = -1;  /* -1 means native; 1 means little; 0 means big. */
  int arg = 2;
  size_t pos = 0, maxalign = 1;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (*fmt) {
    char opt = *fmt++;
    size_t sz, pad;
    if (opt == ' ' || opt == '\f' || opt == '\n' ||
	opt == '\r' || opt == '\t' || opt == '\v')
      continue;
    if (opt == '<') { endian = 1; continue; }
    if (opt == '>') { endian = 0; continue; }
    if (opt == '=') { endian = -1; continue; }
    if (opt == '!') {
      fmt = string_pack_readsize(fmt, &maxalign, 8);
      string_pack_checkmaxalign(L, maxalign);
      continue;
    }
    switch (opt) {
    case 'b': case 'B': sz = 1; goto pack_int;
    case 'h': case 'H': sz = 2; goto pack_int;
    case 'l': case 'L': sz = sizeof(long); goto pack_int;
    case 'j': sz = sizeof(lua_Integer); goto pack_int;
    case 'T': sz = sizeof(size_t); goto pack_int;
    case 'i': case 'I':
      fmt = string_pack_readsize(fmt, &sz, 4);
      string_pack_checksize(L, sz);
    pack_int:
      pad = string_pack_padding(L, pos, sz, maxalign);
      string_pack_addpadding(&b, pad);
      pos += pad;
      string_pack_writeint(&b, string_pack_checkint(L, arg++, sz,
			      opt == 'b' || opt == 'h' || opt == 'i' ||
			      opt == 'l' || opt == 'j'),
			    sz, endian);
      pos += sz;
      break;
    case 'f':
      sz = sizeof(float);
      pad = string_pack_padding(L, pos, sz, maxalign);
      string_pack_addpadding(&b, pad);
      pos += pad;
      string_pack_writenum(&b, L, arg++, sizeof(float), endian);
      pos += sz;
      break;
    case 'd': case 'n':
      sz = sizeof(double);
      pad = string_pack_padding(L, pos, sz, maxalign);
      string_pack_addpadding(&b, pad);
      pos += pad;
      string_pack_writenum(&b, L, arg++, sizeof(double), endian);
      pos += sz;
      break;
    case 'c': {
      const char *s;
      size_t len, i;
      fmt = string_pack_readsize(fmt, &sz, 0);
      s = string_checklstring_named54(L, arg++, &len, "string.pack");
      if (len > sz)
	luaL_argerror(L, arg-1, "string longer than given size");
      luaL_addlstring(&b, s, len);
      for (i = len; i < sz; i++)
	luaL_addchar(&b, '\0');
      pos += sz;
      break;
    }
    case 'z': {
      const char *s;
      size_t len;
      s = string_checklstring_named54(L, arg++, &len, "string.pack");
      if (memchr(s, '\0', len) != NULL)
	luaL_argerror(L, arg-1, "string contains zeros");
      luaL_addlstring(&b, s, len);
      luaL_addchar(&b, '\0');
      pos += len + 1;
      break;
    }
    case 's': {
      const char *s;
      size_t len;
      fmt = string_pack_readsize(fmt, &sz, 4);
      string_pack_checksize(L, sz);
      s = string_checklstring_named54(L, arg++, &len, "string.pack");
      if (len > string_pack_umax(sz))
	luaL_argerror(L, arg-1, "string length does not fit in given size");
      pad = string_pack_padding(L, pos, sz, maxalign);
      string_pack_addpadding(&b, pad);
      pos += pad;
      string_pack_writeint(&b, (uint64_t)len, sz, endian);
      pos += sz;
      luaL_addlstring(&b, s, len);
      pos += len;
      break;
    }
    case 'x':
      luaL_addchar(&b, '\0');
      pos++;
      break;
    case 'X':
      fmt = string_pack_xsize(L, fmt, &sz);
      pad = string_pack_padding(L, pos, sz, maxalign);
      string_pack_addpadding(&b, pad);
      pos += pad;
      break;
    default:
      luaL_error(L, "invalid format option '%c'", opt);
      break;
    }
  }
  luaL_pushresult(&b);
  return 1;
}

static int lj_cf_string_unpack(lua_State *L)
{
  const char *fmt = strdata(string_checkstr_named54(L, 1, "string.unpack"));
  size_t len;
  const unsigned char *data =
    (const unsigned char *)string_checklstring_named54(L, 2, &len,
						       "string.unpack");
  lua_Integer init = string_optint_named54(L, 3, 1, "string.unpack");
  size_t pos;
  size_t maxalign = 1;
  int endian = -1;
  int nres = 0;
  if (init < 1)
    string_argerror_named54(L, 3, "string.unpack",
			    "initial position out of string");
  pos = (size_t)init - 1;
  while (*fmt) {
    char opt = *fmt++;
    size_t sz;
    if (opt == ' ' || opt == '\f' || opt == '\n' ||
	opt == '\r' || opt == '\t' || opt == '\v')
      continue;
    if (opt == '<') { endian = 1; continue; }
    if (opt == '>') { endian = 0; continue; }
    if (opt == '=') { endian = -1; continue; }
    if (opt == '!') {
      fmt = string_pack_readsize(fmt, &maxalign, 8);
      string_pack_checkmaxalign(L, maxalign);
      continue;
    }
    switch (opt) {
    case 'b': case 'B': sz = 1; goto unpack_int;
    case 'h': case 'H': sz = 2; goto unpack_int;
    case 'l': case 'L': sz = sizeof(long); goto unpack_int;
    case 'j': sz = sizeof(lua_Integer); goto unpack_int;
    case 'T': sz = sizeof(size_t); goto unpack_int;
    case 'i': case 'I':
      fmt = string_pack_readsize(fmt, &sz, 4);
      string_pack_checksize(L, sz);
    unpack_int: {
      uint64_t u;
      pos += string_pack_padding(L, pos, sz, maxalign);
      string_pack_checkdata(L, pos, sz, len);
      u = string_pack_readint(data + pos, sz, endian);
      if (opt == 'b' || opt == 'h' || opt == 'i' ||
	  opt == 'l' || opt == 'j') {
	if (sz < 8) {
	  uint64_t sign = (uint64_t)1 << (sz * 8 - 1);
	  if (u & sign)
	    u |= ~string_pack_umax(sz);
	}
	lua_pushinteger(L, (lua_Integer)(int64_t)u);
      } else {
	lua_pushinteger(L, (lua_Integer)u);
      }
      pos += sz;
      nres++;
      break;
    }
    case 'f':
      sz = sizeof(float);
      pos += string_pack_padding(L, pos, sz, maxalign);
      string_pack_checkdata(L, pos, sz, len);
      string_pack_readnum(L, data + pos, sz, endian);
      pos += sz;
      nres++;
      break;
    case 'd': case 'n':
      sz = sizeof(double);
      pos += string_pack_padding(L, pos, sz, maxalign);
      string_pack_checkdata(L, pos, sz, len);
      string_pack_readnum(L, data + pos, sz, endian);
      pos += sz;
      nres++;
      break;
    case 'c':
      fmt = string_pack_readsize(fmt, &sz, 0);
      string_pack_checkdata(L, pos, sz, len);
      lua_pushlstring(L, (const char *)data + pos, sz);
      pos += sz;
      nres++;
      break;
    case 'z': {
      size_t start = pos;
      while (pos < len && data[pos] != 0)
	pos++;
      if (pos >= len)
	luaL_argerror(L, 2, "unfinished string for format 'z'");
      lua_pushlstring(L, (const char *)data + start, pos - start);
      pos++;
      nres++;
      break;
    }
    case 's': {
      uint64_t slen;
      fmt = string_pack_readsize(fmt, &sz, 4);
      string_pack_checksize(L, sz);
      pos += string_pack_padding(L, pos, sz, maxalign);
      string_pack_checkdata(L, pos, sz, len);
      slen = string_pack_readint(data + pos, sz, endian);
      pos += sz;
      if (slen > (uint64_t)(len - pos))
	luaL_argerror(L, 2, "data string too short");
      lua_pushlstring(L, (const char *)data + pos, (size_t)slen);
      pos += (size_t)slen;
      nres++;
      break;
    }
    case 'x':
      string_pack_checkdata(L, pos, 1, len);
      pos++;
      break;
    case 'X':
      fmt = string_pack_xsize(L, fmt, &sz);
      pos += string_pack_padding(L, pos, sz, maxalign);
      string_pack_checkdata(L, pos, 0, len);
      break;
    default:
      luaL_error(L, "invalid format option '%c'", opt);
      break;
    }
  }
  lua_pushinteger(L, (lua_Integer)pos + 1);
  return nres + 1;
}

static int lj_cf_string_packsize(lua_State *L)
{
  const char *fmt = strdata(string_checkstr_named54(L, 1, "string.packsize"));
  size_t total = 0;
  size_t maxalign = 1;
  while (*fmt) {
    char opt = *fmt++;
    size_t sz;
    if (opt == ' ' || opt == '\f' || opt == '\n' ||
	opt == '\r' || opt == '\t' || opt == '\v')
      continue;
    if (opt == '<' || opt == '>' || opt == '=') continue;
    if (opt == '!') {
      fmt = string_pack_readsize(fmt, &maxalign, 8);
      string_pack_checkmaxalign(L, maxalign);
      continue;
    }
    switch (opt) {
    case 'b': case 'B': sz = 1; goto size_fixed;
    case 'h': case 'H': sz = 2; goto size_fixed;
    case 'l': case 'L': sz = sizeof(long); goto size_fixed;
    case 'j': sz = sizeof(lua_Integer); goto size_fixed;
    case 'T': sz = sizeof(size_t); goto size_fixed;
    case 'i': case 'I':
      fmt = string_pack_readsize(fmt, &sz, 4);
      string_pack_checksize(L, sz);
    size_fixed:
      total += string_pack_padding(L, total, sz, maxalign);
      total += sz;
      break;
    case 'f':
      sz = sizeof(float);
      total += string_pack_padding(L, total, sz, maxalign);
      total += sz;
      break;
    case 'd': case 'n':
      sz = sizeof(double);
      total += string_pack_padding(L, total, sz, maxalign);
      total += sz;
      break;
    case 'c':
      fmt = string_pack_readsize(fmt, &sz, 0);
      total += sz;
      break;
    case 'x': total += 1; break;
    case 'X':
      fmt = string_pack_xsize(L, fmt, &sz);
      total += string_pack_padding(L, total, sz, maxalign);
      break;
    case 'z': case 's':
      luaL_argerror(L, 1, "variable-length format");
      break;
    default:
      luaL_error(L, "invalid format option '%c'", opt);
      break;
    }
  }
  lua_pushinteger(L, (lua_Integer)total);
  return 1;
}
#endif

/* ------------------------------------------------------------------------ */

#if LJ_54
static int lj_cf_string_char54(lua_State *L)
{
  int i, nargs = lua_gettop(L);
  char *buf = lj_buf_tmp(L, (MSize)nargs);
  for (i = 1; i <= nargs; i++) {
    int32_t k = string_checkint_named54(L, i, "string.char");
    if (!checku8(k))
      string_argerror_named54(L, i, "string.char", "value out of range");
    buf[i-1] = (char)k;
  }
  lua_pushlstring(L, buf, (size_t)nargs);
  return 1;
}

static int lj_cf_string_sub54(lua_State *L)
{
  size_t len;
  const char *s = string_checklstring_named54(L, 1, &len, "string.sub");
  int32_t start = string_checkint_named54(L, 2, "string.sub");
  int32_t stop = string_optint_named54(L, 3, -1, "string.sub");
  int32_t l = (int32_t)len;
  if (start < 0) start += l+1;
  if (stop < 0) stop += l+1;
  if (start < 1) start = 1;
  if (stop > l) stop = l;
  if (start <= stop)
    lua_pushlstring(L, s + start-1, (size_t)(stop - start + 1));
  else
    lua_pushliteral(L, "");
  return 1;
}

static int lj_cf_string_len54(lua_State *L)
{
  GCstr *s = string_checkstr_named54(L, 1, "string.len");
  lua_pushinteger(L, (lua_Integer)s->len);
  return 1;
}

static int lj_cf_string_reverse54(lua_State *L)
{
  size_t len, i;
  const char *s = string_checklstring_named54(L, 1, &len, "string.reverse");
  char *buf = lj_buf_tmp(L, (MSize)len);
  for (i = 0; i < len; i++)
    buf[i] = s[len - 1 - i];
  lua_pushlstring(L, buf, len);
  return 1;
}

static int lj_cf_string_lower54(lua_State *L)
{
  size_t len, i;
  const char *s = string_checklstring_named54(L, 1, &len, "string.lower");
  char *buf = lj_buf_tmp(L, (MSize)len);
  for (i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    buf[i] = (char)lj_char_tolower(c);
  }
  lua_pushlstring(L, buf, len);
  return 1;
}

static int lj_cf_string_upper54(lua_State *L)
{
  size_t len, i;
  const char *s = string_checklstring_named54(L, 1, &len, "string.upper");
  char *buf = lj_buf_tmp(L, (MSize)len);
  for (i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    buf[i] = (char)lj_char_toupper(c);
  }
  lua_pushlstring(L, buf, len);
  return 1;
}
#endif

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_string(lua_State *L)
{
  GCtab *mt;
  global_State *g;
  LJ_LIB_REG(L, LUA_STRLIBNAME, string);
  mt = lj_tab_new(L, 0, 1);
  /* NOBARRIER: basemt is a GC root. */
  g = G(L);
  setgcref(basemt_it(g, LJ_TSTR), obj2gco(mt));
  settabV(L, lj_tab_setstr(L, mt, mmname_str(g, MM_index)), tabV(L->top-1));
  mt->nomm = (uint8_t)(~(1u<<MM_index));
#if LJ_54
  lua_pushcfunction(L, lj_cf_string_len54);
  lua_setfield(L, -2, "len");
  lua_pushcfunction(L, lj_cf_string_char54);
  lua_setfield(L, -2, "char");
  lua_pushcfunction(L, lj_cf_string_sub54);
  lua_setfield(L, -2, "sub");
  lua_pushcfunction(L, lj_cf_string_reverse54);
  lua_setfield(L, -2, "reverse");
  lua_pushcfunction(L, lj_cf_string_lower54);
  lua_setfield(L, -2, "lower");
  lua_pushcfunction(L, lj_cf_string_upper54);
  lua_setfield(L, -2, "upper");
  lua_pushcfunction(L, lj_cf_string_pack);
  lua_setfield(L, -2, "pack");
  lua_pushcfunction(L, lj_cf_string_unpack);
  lua_setfield(L, -2, "unpack");
  lua_pushcfunction(L, lj_cf_string_packsize);
  lua_setfield(L, -2, "packsize");
#endif
#if LJ_HASBUFFER
  lj_lib_prereg(L, LUA_STRLIBNAME ".buffer", luaopen_string_buffer, tabV(L->top-1));
#endif
  return 1;
}

