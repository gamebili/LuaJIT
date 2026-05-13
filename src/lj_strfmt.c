/*
** String formatting.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#include <stdio.h>

#define lj_strfmt_c
#define LUA_CORE

#include "lauxlib.h"

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_meta.h"
#include "lj_debug.h"
#include "lj_state.h"
#include "lj_char.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#include "lj_lib.h"

/* -- Format parser ------------------------------------------------------- */

static const uint8_t strfmt_map[('x'-'A')+1] = {
  STRFMT_A,0,0,0,STRFMT_E,STRFMT_F,STRFMT_G,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,STRFMT_X,0,0,
  0,0,0,0,0,0,
  STRFMT_A,0,STRFMT_C,STRFMT_D,STRFMT_E,STRFMT_F,STRFMT_G,0,STRFMT_I,0,0,0,0,
  0,STRFMT_O,STRFMT_P,STRFMT_Q,0,STRFMT_S,0,STRFMT_U,0,0,STRFMT_X
};

SFormat LJ_FASTCALL lj_strfmt_parse(FormatState *fs)
{
  const uint8_t *p = fs->p, *e = fs->e;
  fs->str = (const char *)p;
  for (; p < e; p++) {
    if (*p == '%') {  /* Escape char? */
      if (p[1] == '%') {  /* '%%'? */
	fs->p = ++p+1;
	goto retlit;
      } else {
	SFormat sf = 0;
	uint32_t c;
	if (p != (const uint8_t *)fs->str)
	  break;
	for (p++; (uint32_t)*p - ' ' <= (uint32_t)('0' - ' '); p++) {
	  /* Parse flags. */
	  if (*p == '-') sf |= STRFMT_F_LEFT;
	  else if (*p == '+') sf |= STRFMT_F_PLUS;
	  else if (*p == '0') sf |= STRFMT_F_ZERO;
	  else if (*p == ' ') sf |= STRFMT_F_SPACE;
	  else if (*p == '#') sf |= STRFMT_F_ALT;
	  else break;
	}
	if ((uint32_t)*p - '0' < 10) {  /* Parse width. */
	  uint32_t width = (uint32_t)*p++ - '0';
	  if ((uint32_t)*p - '0' < 10)
	    width = (uint32_t)*p++ - '0' + width*10;
	  sf |= (width << STRFMT_SH_WIDTH);
	}
	if (*p == '.') {  /* Parse precision. */
	  uint32_t prec = 0;
	  p++;
	  if ((uint32_t)*p - '0' < 10) {
	    prec = (uint32_t)*p++ - '0';
	    if ((uint32_t)*p - '0' < 10)
	      prec = (uint32_t)*p++ - '0' + prec*10;
	  }
	  sf |= ((prec+1) << STRFMT_SH_PREC);
	}
	/* Parse conversion. */
	c = (uint32_t)*p - 'A';
	if (LJ_LIKELY(c <= (uint32_t)('x' - 'A'))) {
	  uint32_t sx = strfmt_map[c];
	  if (sx) {
	    fs->p = p+1;
	    return (sf | sx | ((c & 0x20) ? 0 : STRFMT_F_UPPER));
	  }
	}
	/* Return error location. */
	if (*p >= 32) p++;
	fs->len = (MSize)(p - (const uint8_t *)fs->str);
	fs->p = fs->e;
	return STRFMT_ERR;
      }
    }
  }
  fs->p = p;
retlit:
  fs->len = (MSize)(p - (const uint8_t *)fs->str);
  return fs->len ? STRFMT_LIT : STRFMT_EOF;
}

#if LJ_54
static MSize strfmt_speclen_lua54(const FormatState *fs)
{
  const uint8_t *p = (const uint8_t *)fs->str, *q = p + 1, *e = fs->e;
  while (q < e && (*q == '-' || *q == '+' || *q == '0' || *q == ' ' ||
		  *q == '#'))
    q++;
  while (q < e && lj_char_isdigit(*q))
    q++;
  if (q < e && *q == '.') {
    q++;
    while (q < e && lj_char_isdigit(*q))
      q++;
  }
  if (q < e)
    q++;
  return (MSize)(q - p);
}

static int strfmt_validconv_lua54(char c)
{
  switch (c) {
  case 'a': case 'A': case 'c': case 'd': case 'e': case 'E':
  case 'f': case 'g': case 'G': case 'i': case 'o': case 'p':
  case 'q': case 's': case 'u': case 'x': case 'X':
    return 1;
  default:
    return 0;
  }
}

static void strfmt_convmsg_lua54(lua_State *L, const char *fmt, MSize len,
				 const char *msgfmt)
{
  GCstr *s = lj_str_new(L, fmt, len);
  /* Lua 5.4 reports invalid format specifications through luaL_error(), so
  ** direct pcall(string.format, ...) has no location prefix, while a Lua
  ** wrapper frame still contributes its source location.
  */
  luaL_error(L, msgfmt, strdata(s));
}

static void strfmt_badconv_lua54(lua_State *L, const char *fmt, MSize len)
{
  strfmt_convmsg_lua54(L, fmt, len, "invalid conversion '%s' to 'format'");
}

static void strfmt_badspec_lua54(lua_State *L, const char *fmt, MSize len)
{
  strfmt_convmsg_lua54(L, fmt, len,
		       "invalid conversion specification: '%s'");
}

static void strfmt_parseerr_lua54(lua_State *L, const FormatState *fs)
{
  MSize len = strfmt_speclen_lua54(fs);
  /* Lua 5.4's getformat() rejects len >= MAX_FORMAT - 10, where len excludes
  ** the leading '%'.  strfmt_speclen_lua54() includes it, so 23 is the first
  ** total byte length that must report "invalid format (too long)".
  */
  if (len >= 23)
    luaL_error(L, "invalid format (too long)");
  if (len > 1 && strfmt_validconv_lua54(fs->str[len-1]))
    strfmt_badspec_lua54(L, fs->str, len);
  else
    strfmt_badconv_lua54(L, fs->str, len);
}

static void strfmt_checkconv_lua54(lua_State *L, SFormat sf,
				   const char *fmt, MSize len)
{
  uint32_t flags = sf & (STRFMT_F_LEFT|STRFMT_F_PLUS|STRFMT_F_ZERO|
			 STRFMT_F_SPACE|STRFMT_F_ALT);
  int hasprec = ((sf >> STRFMT_SH_PREC) & 255u) != 0;
  /* Successful parses can still be too long, e.g. many '0' flag bytes. */
  if (len >= 23)
    luaL_error(L, "invalid format (too long)");
  switch (STRFMT_TYPE(sf)) {
  case STRFMT_INT:
    if ((flags & STRFMT_F_ALT))
      strfmt_badspec_lua54(L, fmt, len);
    break;
  case STRFMT_UINT:
    if ((sf & (STRFMT_T_HEX|STRFMT_T_OCT))) {
      if ((flags & ~(STRFMT_F_LEFT|STRFMT_F_ZERO|STRFMT_F_ALT)))
	strfmt_badspec_lua54(L, fmt, len);
    } else if ((flags & ~(STRFMT_F_LEFT|STRFMT_F_ZERO))) {
      strfmt_badspec_lua54(L, fmt, len);
    }
    break;
  case STRFMT_NUM:
    if ((sf & STRFMT_F_UPPER) && (sf & STRFMT_T_FP_G) == STRFMT_T_FP_F)
      strfmt_badconv_lua54(L, fmt, len);  /* Lua 5.4 does not accept %F. */
    /* PUC Lua intentionally leaves modified %a/%A unimplemented; only the
    ** bare hexadecimal-float conversion is accepted by string.format().
    */
    if (STRFMT_FP(sf) == STRFMT_FP(STRFMT_T_FP_A) &&
	(flags || STRFMT_WIDTH(sf) != 0 || hasprec))
      luaL_error(L, "modifiers for format '%%a'/'%%A' not implemented");
    break;
  case STRFMT_STR:
    if ((sf & STRFMT_T_QUOTED)) {
      if (sf != STRFMT_Q)
	luaL_error(L, "specifier '%%q' cannot have modifiers");
    } else if ((flags & ~STRFMT_F_LEFT)) {
      strfmt_badspec_lua54(L, fmt, len);
    }
    break;
  case STRFMT_CHAR:
    if ((flags & ~STRFMT_F_LEFT) || hasprec)
      strfmt_badspec_lua54(L, fmt, len);
    break;
  case STRFMT_PTR:
    if ((flags & ~STRFMT_F_LEFT) || hasprec)
      strfmt_badspec_lua54(L, fmt, len);
    break;
  default:
    break;
  }
}
#endif

/* -- Raw conversions ----------------------------------------------------- */

#define WINT_R(x, sh, sc) \
  { uint32_t d = (x*(((1<<sh)+sc-1)/sc))>>sh; x -= d*sc; *p++ = (char)('0'+d); }

/* Write integer to buffer. */
char * LJ_FASTCALL lj_strfmt_wint(char *p, int32_t k)
{
  uint32_t u = (uint32_t)k;
  if (k < 0) { u = ~u+1u; *p++ = '-'; }
  if (u < 10000) {
    if (u < 10) goto dig1;
    if (u < 100) goto dig2;
    if (u < 1000) goto dig3;
  } else {
    uint32_t v = u / 10000; u -= v * 10000;
    if (v < 10000) {
      if (v < 10) goto dig5;
      if (v < 100) goto dig6;
      if (v < 1000) goto dig7;
    } else {
      uint32_t w = v / 10000; v -= w * 10000;
      if (w >= 10) WINT_R(w, 10, 10)
      *p++ = (char)('0'+w);
    }
    WINT_R(v, 23, 1000)
    dig7: WINT_R(v, 12, 100)
    dig6: WINT_R(v, 10, 10)
    dig5: *p++ = (char)('0'+v);
  }
  WINT_R(u, 23, 1000)
  dig3: WINT_R(u, 12, 100)
  dig2: WINT_R(u, 10, 10)
  dig1: *p++ = (char)('0'+u);
  return p;
}
#undef WINT_R

static char *strfmt_wi64(char *p, int64_t k)
{
  char buf[20], *q = buf + sizeof(buf);
  uint64_t u;
  if (k < 0) {
    *p++ = '-';
    u = ~(uint64_t)k + 1;
  } else {
    u = (uint64_t)k;
  }
  do {
    *--q = (char)('0' + (u % 10));
    u /= 10;
  } while (u);
  while (q < buf + sizeof(buf))
    *p++ = *q++;
  return p;
}

/* Write pointer to buffer. */
char * LJ_FASTCALL lj_strfmt_wptr(char *p, const void *v)
{
  ptrdiff_t x = (ptrdiff_t)v;
  MSize i, n = STRFMT_MAXBUF_PTR;
  if (x == 0) {
    *p++ = 'N'; *p++ = 'U'; *p++ = 'L'; *p++ = 'L';
    return p;
  }
#if LJ_64
  /* Shorten output for 64 bit pointers. */
  n = 2+2*4+((x >> 32) ? 2+2*(lj_fls((uint32_t)(x >> 32))>>3) : 0);
#endif
  p[0] = '0';
  p[1] = 'x';
  for (i = n-1; i >= 2; i--, x >>= 4)
    p[i] = "0123456789abcdef"[(x & 15)];
  return p+n;
}

/* Write ULEB128 to buffer. */
char * LJ_FASTCALL lj_strfmt_wuleb128(char *p, uint32_t v)
{
  for (; v >= 0x80; v >>= 7)
    *p++ = (char)((v & 0x7f) | 0x80);
  *p++ = (char)v;
  return p;
}

/* Return string or write number to tmp buffer and return pointer to start. */
const char *lj_strfmt_wstrnum(lua_State *L, cTValue *o, MSize *lenp)
{
  SBuf *sb;
  if (tvisstr(o)) {
    *lenp = strV(o)->len;
    return strVdata(o);
  } else if (tvisbuf(o)) {
    SBufExt *sbx = bufV(o);
    *lenp = sbufxlen(sbx);
    return sbx->r ? sbx->r : "";
#if LJ_54
  } else if (tvisnumber(o) || tvisi64(o)) {
    GCstr *str = lj_strfmt_number(L, o);
    *lenp = str->len;
    return strdata(str);
#endif
  } else if (tvisint(o)) {
    sb = lj_strfmt_putint(lj_buf_tmp_(L), intV(o));
  } else if (tvisnum(o)) {
    sb = lj_strfmt_putfnum(lj_buf_tmp_(L), STRFMT_G14, o->n);
  } else {
    return NULL;
  }
  *lenp = sbuflen(sb);
  return sb->b;
}

/* -- Unformatted conversions to buffer ----------------------------------- */

/* Add integer to buffer. */
SBuf * LJ_FASTCALL lj_strfmt_putint(SBuf *sb, int32_t k)
{
  sb->w = lj_strfmt_wint(lj_buf_more(sb, STRFMT_MAXBUF_INT), k);
  return sb;
}

SBuf * LJ_FASTCALL lj_strfmt_puti64(SBuf *sb, int64_t k)
{
  if (checki32(k))
    return lj_strfmt_putint(sb, (int32_t)k);
  return lj_strfmt_putfxint(sb, STRFMT_INT, (uint64_t)k);
}

#if LJ_HASJIT
/* Add number to buffer. */
SBuf * LJ_FASTCALL lj_strfmt_putnum(SBuf *sb, cTValue *o)
{
  return lj_strfmt_putfnum(sb, STRFMT_G14, o->n);
}
#endif

SBuf * LJ_FASTCALL lj_strfmt_putptr(SBuf *sb, const void *v)
{
  sb->w = lj_strfmt_wptr(lj_buf_more(sb, STRFMT_MAXBUF_PTR), v);
  return sb;
}

/* Add quoted string to buffer. */
static SBuf *strfmt_putquotedlen(SBuf *sb, const char *s, MSize len)
{
  lj_buf_putb(sb, '"');
  while (len--) {
    uint32_t c = (uint32_t)(uint8_t)*s++;
    char *w = lj_buf_more(sb, 4);
    if (c == '"' || c == '\\' || c == '\n') {
      *w++ = '\\';
    } else if (lj_char_iscntrl(c)) {  /* This can only be 0-31 or 127. */
      uint32_t d;
      *w++ = '\\';
      if (c >= 100 || lj_char_isdigit((uint8_t)*s)) {
	*w++ = (char)('0'+(c >= 100)); if (c >= 100) c -= 100;
	goto tens;
      } else if (c >= 10) {
      tens:
	d = (c * 205) >> 11; c -= d * 10; *w++ = (char)('0'+d);
      }
      c += '0';
    }
    *w++ = (char)c;
    sb->w = w;
  }
  lj_buf_putb(sb, '"');
  return sb;
}

#if LJ_HASJIT
SBuf * LJ_FASTCALL lj_strfmt_putquoted(SBuf *sb, GCstr *str)
{
  return strfmt_putquotedlen(sb, strdata(str), str->len);
}
#endif

/* -- Formatted conversions to buffer ------------------------------------- */

/* Add formatted char to buffer. */
SBuf *lj_strfmt_putfchar(SBuf *sb, SFormat sf, int32_t c)
{
  MSize width = STRFMT_WIDTH(sf);
  char *w = lj_buf_more(sb, width > 1 ? width : 1);
  if ((sf & STRFMT_F_LEFT)) *w++ = (char)c;
  while (width-- > 1) *w++ = ' ';
  if (!(sf & STRFMT_F_LEFT)) *w++ = (char)c;
  sb->w = w;
  return sb;
}

/* Add formatted string to buffer. */
static SBuf *strfmt_putfstrlen(SBuf *sb, SFormat sf, const char *s, MSize len)
{
  MSize width = STRFMT_WIDTH(sf);
  char *w;
  if (len > STRFMT_PREC(sf)) len = STRFMT_PREC(sf);
  w = lj_buf_more(sb, width > len ? width : len);
  if ((sf & STRFMT_F_LEFT)) w = lj_buf_wmem(w, s, len);
  while (width-- > len) *w++ = ' ';
  if (!(sf & STRFMT_F_LEFT)) w = lj_buf_wmem(w, s, len);
  sb->w = w;
  return sb;
}

#if LJ_HASJIT
SBuf *lj_strfmt_putfstr(SBuf *sb, SFormat sf, GCstr *str)
{
  return strfmt_putfstrlen(sb, sf, strdata(str), str->len);
}
#endif

/* Add formatted signed/unsigned integer to buffer. */
SBuf *lj_strfmt_putfxint(SBuf *sb, SFormat sf, uint64_t k)
{
  char buf[STRFMT_MAXBUF_XINT], *q = buf + sizeof(buf), *w;
#ifdef LUA_USE_ASSERT
  char *ws;
#endif
  MSize prefix = 0, len, prec, pprec, width, need;

  /* Figure out signed prefixes. */
  if (STRFMT_TYPE(sf) == STRFMT_INT) {
    if ((int64_t)k < 0) {
      k = ~k+1u;
      prefix = 256 + '-';
    } else if ((sf & STRFMT_F_PLUS)) {
      prefix = 256 + '+';
    } else if ((sf & STRFMT_F_SPACE)) {
      prefix = 256 + ' ';
    }
  }

  /* Convert number and store to fixed-size buffer in reverse order. */
  prec = STRFMT_PREC(sf);
  if ((int32_t)prec >= 0) sf &= ~STRFMT_F_ZERO;
  if (k == 0) {  /* Special-case zero argument. */
    if (prec != 0 ||
	(sf & (STRFMT_T_OCT|STRFMT_F_ALT)) == (STRFMT_T_OCT|STRFMT_F_ALT))
      *--q = '0';
  } else if (!(sf & (STRFMT_T_HEX|STRFMT_T_OCT))) {  /* Decimal. */
    uint32_t k2;
    while ((k >> 32)) { *--q = (char)('0' + k % 10); k /= 10; }
    k2 = (uint32_t)k;
    do { *--q = (char)('0' + k2 % 10); k2 /= 10; } while (k2);
  } else if ((sf & STRFMT_T_HEX)) {  /* Hex. */
    const char *hexdig = (sf & STRFMT_F_UPPER) ? "0123456789ABCDEF" :
						 "0123456789abcdef";
    do { *--q = hexdig[(k & 15)]; k >>= 4; } while (k);
    if ((sf & STRFMT_F_ALT)) prefix = 512 + ((sf & STRFMT_F_UPPER) ? 'X' : 'x');
  } else {  /* Octal. */
    do { *--q = (char)('0' + (uint32_t)(k & 7)); k >>= 3; } while (k);
    if ((sf & STRFMT_F_ALT)) *--q = '0';
  }

  /* Calculate sizes. */
  len = (MSize)(buf + sizeof(buf) - q);
  if ((int32_t)len >= (int32_t)prec) prec = len;
  width = STRFMT_WIDTH(sf);
  pprec = prec + (prefix >> 8);
  need = width > pprec ? width : pprec;
  w = lj_buf_more(sb, need);
#ifdef LUA_USE_ASSERT
  ws = w;
#endif

  /* Format number with leading/trailing whitespace and zeros. */
  if ((sf & (STRFMT_F_LEFT|STRFMT_F_ZERO)) == 0)
    while (width-- > pprec) *w++ = ' ';
  if (prefix) {
    if ((char)prefix >= 'X') *w++ = '0';
    *w++ = (char)prefix;
  }
  if ((sf & (STRFMT_F_LEFT|STRFMT_F_ZERO)) == STRFMT_F_ZERO)
    while (width-- > pprec) *w++ = '0';
  while (prec-- > len) *w++ = '0';
  while (q < buf + sizeof(buf)) *w++ = *q++;  /* Add number itself. */
  if ((sf & STRFMT_F_LEFT))
    while (width-- > pprec) *w++ = ' ';

  lj_assertX(need == (MSize)(w - ws), "miscalculated format size");
  sb->w = w;
  return sb;
}

/* Add number formatted as signed integer to buffer. */
SBuf *lj_strfmt_putfnum_int(SBuf *sb, SFormat sf, lua_Number n)
{
  int64_t k = lj_num2i64(n);
  if (checki32(k) && sf == STRFMT_INT)
    return lj_strfmt_putint(sb, (int32_t)k);  /* Shortcut for plain %d. */
  else
    return lj_strfmt_putfxint(sb, sf, (uint64_t)k);
}

/* Add number formatted as unsigned integer to buffer. */
SBuf *lj_strfmt_putfnum_uint(SBuf *sb, SFormat sf, lua_Number n)
{
  return lj_strfmt_putfxint(sb, sf, lj_num2u64(n));
}

#if LJ_54
static const char *strfmt_callname54(lua_State *L, const char **kindp)
{
  const char *name = NULL;
  const char *kind = lj_debug_funcname(L, L->base-1, &name);
  if (kindp) *kindp = kind;
  /* Direct pcall(string.format, ...) has no Lua call expression to name; keep
  ** the public fallback there, but use field/local/upvalue names when source
  ** code actually called the formatter.
  */
  if (kind && name && *kind)
    return name;
  return "string.format";
}

static void strfmt_argerror_named54(lua_State *L, int arg, const char *msg)
{
  const char *kind = NULL;
  const char *name = strfmt_callname54(L, &kind);
  int showarg = kind && kind[3] == 'h' ? arg - 1 : arg;
  lj_err_callermsg(L, lua_pushfstring(L,
    "bad argument #%d to '%s' (%s)", showarg, name, msg));
}

static void strfmt_argtype_named54(lua_State *L, int arg, const char *xname)
{
  cTValue *o = L->base + arg-1;
  MSize len;
  const char *tname = o < L->top ? lj_meta_objtypename(L, o, &len) : "no value";
  UNUSED(len);
  strfmt_argerror_named54(L, arg,
    lua_pushfstring(L, "%s expected, got %s", xname, tname));
}

static lua_Number strfmt_checknum_named54(lua_State *L, int arg)
{
  TValue tmp;
  cTValue *o = L->base + arg-1;
  if (o >= L->top)
    strfmt_argtype_named54(L, arg, "number");
  if (tvisstr(o)) {
    if (!lj_strscan_number(strV(o), &tmp))
      strfmt_argtype_named54(L, arg, "number");
    o = &tmp;
  }
  if (tvisint(o))
    return (lua_Number)intV(o);
  if (tvisi64(o))
    return (lua_Number)i64V(o);
  if (tvisnum(o))
    return numV(o);
  strfmt_argtype_named54(L, arg, "number");
  return 0;  /* unreachable */
}

static int strfmt_numisinf(lua_Number n)
{
  return n != 0 && n == n * 0.5;
}

static int strfmt_putqnum_lua54(SBuf *sb, lua_Number n)
{
  int64_t k;
  if (n != n) {
    lj_buf_putmem(sb, "(0/0)", 5);
    return 1;
  }
  if (strfmt_numisinf(n)) {
    if (n < 0)
      lj_buf_putmem(sb, "-1e9999", 7);
    else
      lj_buf_putmem(sb, "1e9999", 6);
    return 1;
  }
  if (n == 0 && 1.0 / n < 0) {
    lj_buf_putmem(sb, "-0x0p+0", 7);
    return 1;
  }
  k = lj_num2i64(n);
  if (checki32(k) && (lua_Number)k == n) {
    lj_strfmt_putint(sb, (int32_t)k);
    return 1;
  }
  lj_strfmt_putfnum(sb, STRFMT_A, n);
  return 1;
}

static lua_Number strfmt_checkintegernum(lua_State *L, int arg)
{
  lua_Number n = strfmt_checknum_named54(L, arg);
  int64_t k = lj_num2i64(n);
  /* Lua 5.4 refuses integer formats for numbers without an integer value. */
  if ((lua_Number)k != n)
    strfmt_argerror_named54(L, arg, "number has no integer representation");
  return n;
}
#endif

/* Format stack arguments to buffer. */
int lj_strfmt_putarg(lua_State *L, SBuf *sb, int arg, int retry)
{
  int narg = (int)(L->top - L->base);
  GCstr *fmt = lj_lib_checkstr(L, arg);
  FormatState fs;
  SFormat sf;
  lj_strfmt_init(&fs, strdata(fmt), fmt->len);
  while ((sf = lj_strfmt_parse(&fs)) != STRFMT_EOF) {
    if (sf == STRFMT_LIT) {
      lj_buf_putmem(sb, fs.str, fs.len);
    } else if (sf == STRFMT_ERR) {
#if LJ_54
      /* Lua 5.4 checks that a format item has a matching argument before it
      ** reports malformed conversion text.  Thus string.format("%") without
      ** a second argument is a no-value argument error, but "%", 1 still
      ** reports the invalid conversion itself.
      */
      if (arg >= narg)
	strfmt_argerror_named54(L, arg+1, "no value");
      strfmt_parseerr_lua54(L, &fs);
#else
      lj_err_callerv(L, LJ_ERR_STRFMT,
		     strdata(lj_str_new(L, fs.str, fs.len)));
#endif
    } else {
      TValue *o;
      o = &L->base[arg++];
#if LJ_54
      if (arg > narg)
	strfmt_argerror_named54(L, arg, "no value");
      /* Lua 5.4 reports missing arguments before rejecting an otherwise parsed
      ** but invalid conversion specification such as "%#i" or "%F".
      */
      strfmt_checkconv_lua54(L, sf, fs.str,
			     (MSize)((const uint8_t *)fs.p -
				     (const uint8_t *)fs.str));
#else
      if (arg > narg)
	lj_err_arg(L, arg, LJ_ERR_NOVAL);
#endif
      switch (STRFMT_TYPE(sf)) {
      case STRFMT_INT:
	if (tvisint(o)) {
	  int32_t k = intV(o);
	  if (sf == STRFMT_INT)
	    lj_strfmt_putint(sb, k);  /* Shortcut for plain %d. */
	  else
	    lj_strfmt_putfxint(sb, sf, k);
	  break;
	} else if (tvisi64(o)) {
	  lj_strfmt_putfxint(sb, sf, (uint64_t)i64V(o));
	  break;
	}
#if LJ_HASFFI
	if (tviscdata(o)) {
	  GCcdata *cd = cdataV(o);
	  if (cd->ctypeid == CTID_INT64 || cd->ctypeid == CTID_UINT64) {
	    lj_strfmt_putfxint(sb, sf, *(uint64_t *)cdataptr(cd));
	    break;
	  }
	}
#endif
#if LJ_54
	lj_strfmt_putfnum_int(sb, sf, strfmt_checkintegernum(L, arg));
#else
	lj_strfmt_putfnum_int(sb, sf, lj_lib_checknum(L, arg));
#endif
	break;
      case STRFMT_UINT:
	if (tvisint(o)) {
#if LJ_54
	  lj_strfmt_putfxint(sb, sf, (uint64_t)(int64_t)intV(o));
#else
	  lj_strfmt_putfxint(sb, sf, (uint32_t)intV(o));
#endif
	  break;
	} else if (tvisi64(o)) {
	  lj_strfmt_putfxint(sb, sf, (uint64_t)i64V(o));
	  break;
	}
#if LJ_HASFFI
	if (tviscdata(o)) {
	  GCcdata *cd = cdataV(o);
	  if (cd->ctypeid == CTID_INT64 || cd->ctypeid == CTID_UINT64) {
	    lj_strfmt_putfxint(sb, sf, *(uint64_t *)cdataptr(cd));
	    break;
	  }
	}
#endif
#if LJ_54
	lj_strfmt_putfnum_uint(sb, sf, strfmt_checkintegernum(L, arg));
#else
	lj_strfmt_putfnum_uint(sb, sf, lj_lib_checknum(L, arg));
#endif
	break;
      case STRFMT_NUM:
#if LJ_54
	lj_strfmt_putfnum(sb, sf, strfmt_checknum_named54(L, arg));
#else
	lj_strfmt_putfnum(sb, sf, lj_lib_checknum(L, arg));
#endif
	break;
      case STRFMT_STR: {
	MSize len;
	const char *s;
	cTValue *mo;
#if LJ_54
	if ((sf & STRFMT_T_QUOTED)) {
	  /* Lua 5.4 quotes strings, but prints primitive literals directly. */
	  if (tvisint(o)) {
	    if (intV(o) == (int32_t)0x80000000u) {
	      /* Avoid reparsing the most-negative 32 bit integer as a float:
	      ** the source spelling is unary minus applied to 2147483648.
	      */
	      lj_buf_putmem(sb, "(-2147483647 - 1)", 17);
	    } else {
	      lj_strfmt_putint(sb, intV(o));
	    }
	    break;
	  }
	  if (tvisi64(o)) {
	    int64_t k = i64V(o);
	    if (k == (int64_t)U64x(80000000,00000000))
	      lj_strfmt_putfxint(sb, STRFMT_X|STRFMT_F_ALT, (uint64_t)k);
	    else
	      lj_strfmt_puti64(sb, k);
	    break;
	  }
	  if (tvisnum(o)) { strfmt_putqnum_lua54(sb, numV(o)); break; }
	  if (tvisnil(o)) { lj_buf_putmem(sb, "nil", 3); break; }
	  if (tvisfalse(o)) { lj_buf_putmem(sb, "false", 5); break; }
	  if (tvistrue(o)) { lj_buf_putmem(sb, "true", 4); break; }
#if LJ_HASBUFFER
	  if (!tvisstr(o) && !tvisbuf(o))
#else
	  if (!tvisstr(o))
#endif
	    strfmt_argerror_named54(L, arg, "value has no literal form");
	}
#endif
	if (LJ_UNLIKELY(!tvisstr(o) && !tvisbuf(o)) && retry >= 0 &&
	    !tvisnil(mo = lj_meta_lookup(L, o, MM_tostring))) {
	  /* Call __tostring metamethod once. */
	  copyTV(L, L->top++, mo);
	  copyTV(L, L->top++, o);
	  lua_call(L, 1, 1);
#if LJ_54
	  if (!tvisstr(L->top-1) &&
	      !(tvisnumber(L->top-1) || tvisi64(L->top-1))) {
	    /* string.format("%s") follows luaL_tolstring(): __tostring may
	    ** return a string or number, but other values are a hard error.
	    */
	    lj_err_callermsg(L, "'__tostring' must return a string");
	  }
#endif
	  o = &L->base[arg-1];  /* Stack may have been reallocated. */
	  copyTV(L, o, --L->top);  /* Replace inline for retry. */
	  if (retry < 2) {  /* Global buffer may have been overwritten. */
	    retry = 1;
	    break;
	  }
	}
	if (LJ_LIKELY(tvisstr(o))) {
	  len = strV(o)->len;
	  s = strVdata(o);
#if LJ_HASBUFFER
	} else if (tvisbuf(o)) {
	  SBufExt *sbx = bufV(o);
	  if (sbx == (SBufExt *)sb) lj_err_arg(L, arg+1, LJ_ERR_BUFFER_SELF);
	  len = sbufxlen(sbx);
	  s = sbx->r;
#endif
	} else {
	  GCstr *str = lj_strfmt_obj(L, o);
	  len = str->len;
	  s = strdata(str);
	}
#if LJ_54
	if (!(sf & STRFMT_T_QUOTED) && sf != STRFMT_STR) {
	  MSize i;
	  /* Lua 5.4 routes modified %s through C printf, where embedded NULs
	  ** would truncate the value. Reject them instead of formatting partial
	  ** data; plain "%s" still preserves Lua strings byte-for-byte.
	  */
	  for (i = 0; i < len; i++)
	    if (s[i] == '\0')
	      strfmt_argerror_named54(L, arg, "string contains zeros");
	}
#endif
	if ((sf & STRFMT_T_QUOTED))
	  strfmt_putquotedlen(sb, s, len);  /* No formatting. */
	else
	  strfmt_putfstrlen(sb, sf, s, len);
	break;
	}
      case STRFMT_CHAR:
#if LJ_54
	/* %c is an integer conversion in Lua 5.4. Keep string numerals, but
	** reject fractions instead of truncating them to a byte.
	*/
	lj_strfmt_putfchar(sb, sf, lj_num2int(strfmt_checkintegernum(L, arg)));
#else
	lj_strfmt_putfchar(sb, sf, lj_lib_checkint(L, arg));
#endif
	break;
      case STRFMT_PTR: {  /* No formatting. */
#if LJ_54
	const char *pstr;
	MSize plen;
	char pbuf[64];
	if (!tvisgcv(o)) {
	  /* Lua 5.4 delegates %p to lua_topointer(), so primitive values
	  ** without an addressable GC object format as a null pointer. */
	  pstr = "(null)";
	  plen = 6;
	} else {
	  int len = snprintf(pbuf, sizeof(pbuf), "%p", lj_obj_ptr(G(L), o));
	  if (len < 0 || len >= (int)sizeof(pbuf)) {
	    /* Keep a deterministic internal fallback, but still run it through
	    ** the formatted string path so width and left alignment are honored.
	    */
	    plen = (MSize)(lj_strfmt_wptr(pbuf, lj_obj_ptr(G(L), o)) - pbuf);
	  } else {
	    plen = (MSize)len;
	  }
	  pstr = pbuf;
	}
	/* Official Lua formats %p with the parsed C width/alignment. Precision
	** was previously ignored for %p here, so clear it before reusing %s logic.
	*/
	strfmt_putfstrlen(sb, sf & ~((SFormat)255u << STRFMT_SH_PREC), pstr, plen);
#else
	lj_strfmt_putptr(sb, lj_obj_ptr(G(L), o));
#endif
	break;
	}
      default:
	lj_assertL(0, "bad string format type");
	break;
      }
    }
  }
  return retry;
}

/* -- Conversions to strings ---------------------------------------------- */

/* Convert integer to string. */
GCstr * LJ_FASTCALL lj_strfmt_int(lua_State *L, int32_t k)
{
  char buf[STRFMT_MAXBUF_INT];
  MSize len = (MSize)(lj_strfmt_wint(buf, k) - buf);
  return lj_str_new(L, buf, len);
}

GCstr * LJ_FASTCALL lj_strfmt_i64(lua_State *L, int64_t k)
{
  char buf[STRFMT_MAXBUF_I64];
  MSize len = (MSize)(strfmt_wi64(buf, k) - buf);
  return lj_str_new(L, buf, len);
}

/* Convert integer or number to string. */
GCstr * LJ_FASTCALL lj_strfmt_number(lua_State *L, cTValue *o)
{
  return tvisint(o) ? lj_strfmt_int(L, intV(o)) :
	 tvisi64(o) ? lj_strfmt_i64(L, i64V(o)) : lj_strfmt_num(L, o);
}

#if LJ_HASJIT
/* Convert char value to string. */
GCstr * LJ_FASTCALL lj_strfmt_char(lua_State *L, int c)
{
  char buf[1];
  buf[0] = c;
  return lj_str_new(L, buf, 1);
}
#endif

/* Raw conversion of object to string. */
GCstr * LJ_FASTCALL lj_strfmt_obj(lua_State *L, cTValue *o)
{
  if (tvisstr(o)) {
    return strV(o);
  } else if (tvisnumber(o) || tvisi64(o)) {
    return lj_strfmt_number(L, o);
  } else if (tvisnil(o)) {
    return lj_str_newlit(L, "nil");
  } else if (tvisfalse(o)) {
    return lj_str_newlit(L, "false");
  } else if (tvistrue(o)) {
    return lj_str_newlit(L, "true");
  } else {
    char buf[8+2+2+16], *p = buf;
    MSize len;
    const char *name = lj_meta_objtypename(L, o, &len);
    p = lj_buf_wmem(p, name, len);
    *p++ = ':'; *p++ = ' ';
    if (tvisfunc(o) && isffunc(funcV(o))) {
      p = lj_buf_wmem(p, "builtin#", 8);
      p = lj_strfmt_wint(p, funcV(o)->c.ffid);
    } else {
      p = lj_strfmt_wptr(p, lj_obj_ptr(G(L), o));
    }
    return lj_str_new(L, buf, (size_t)(p - buf));
  }
}

/* -- Internal string formatting ------------------------------------------ */

/*
** These functions are only used for lua_pushfstring(), lua_pushvfstring()
** and for internal string formatting (e.g. error messages). Caveat: unlike
** string.format(), only a limited subset of formats and flags are supported!
**
** LuaJIT has support for a couple more formats than Lua 5.1/5.2:
** - %d %u %o %x with full formatting, 32 bit integers only.
** - %f and other FP formats are really %.14g.
** - %s %c %p without formatting.
*/

#if LJ_54
static void strfmt_pututf8_lua54(SBuf *sb, unsigned long x)
{
  char buf[8];
  int n = 1;
  if (x < 0x80) {
    buf[7] = (char)x;
  } else {
    unsigned int mfb = 0x3f;
    do {
      buf[8 - (n++)] = (char)(0x80 | (x & 0x3f));
      x >>= 6;
      mfb >>= 1;
    } while (x > mfb);
    buf[8 - n] = (char)((~mfb << 1) | x);
  }
  lj_buf_putmem(sb, buf + 8 - n, (MSize)n);
}

static void strfmt_pushf_bad_lua54(lua_State *L, const FormatState *fs)
{
  char c = fs->str[0] == '%' &&
	   (const uint8_t *)fs->str + 1 < fs->e ? fs->str[1] : '?';
  lj_err_callermsg(L, lj_strfmt_pushf(L,
    "invalid option '%%%c' to 'lua_pushfstring'", c));
}

static char strfmt_pushf_conv_lua54(lua_State *L, const FormatState *fs)
{
  if ((MSize)((const uint8_t *)fs->p - (const uint8_t *)fs->str) != 2)
    strfmt_pushf_bad_lua54(L, fs);
  return fs->p[-1];
}
#endif

/* Push formatted message as a string object to Lua stack. va_list variant. */
const char *lj_strfmt_pushvf(lua_State *L, const char *fmt, va_list argp)
{
  SBuf *sb = lj_buf_tmp_(L);
  FormatState fs;
  SFormat sf;
  GCstr *str;
  lj_strfmt_init(&fs, fmt, (MSize)strlen(fmt));
  while ((sf = lj_strfmt_parse(&fs)) != STRFMT_EOF) {
    switch (STRFMT_TYPE(sf)) {
    case STRFMT_LIT:
      lj_buf_putmem(sb, fs.str, fs.len);
      break;
    case STRFMT_INT:
#if LJ_54
      if (strfmt_pushf_conv_lua54(L, &fs) != 'd')
	strfmt_pushf_bad_lua54(L, &fs);
#endif
      lj_strfmt_putfxint(sb, sf, va_arg(argp, int32_t));
      break;
    case STRFMT_UINT:
#if LJ_54
      strfmt_pushf_bad_lua54(L, &fs);
#endif
      lj_strfmt_putfxint(sb, sf, va_arg(argp, uint32_t));
      break;
    case STRFMT_NUM:
#if LJ_54
      if (strfmt_pushf_conv_lua54(L, &fs) != 'f')
	strfmt_pushf_bad_lua54(L, &fs);
      if (fs.p > (const uint8_t *)fs.str && fs.p[-1] == 'f') {
	TValue tv;
	setnumV(&tv, va_arg(argp, lua_Number));
	/* Lua 5.4's lua_pushfstring("%f") uses the raw number-to-string
	** conversion, so integral floats must retain their ".0" subtype cue.
	*/
	lj_buf_putstr(sb, lj_strfmt_number(L, &tv));
	break;
      }
#endif
      lj_strfmt_putfnum(sb, STRFMT_G14, va_arg(argp, lua_Number));
      break;
    case STRFMT_STR: {
      const char *s = va_arg(argp, char *);
#if LJ_54
      if (strfmt_pushf_conv_lua54(L, &fs) != 's')
	strfmt_pushf_bad_lua54(L, &fs);
#endif
      if (s == NULL) s = "(null)";
      lj_buf_putmem(sb, s, (MSize)strlen(s));
      break;
      }
    case STRFMT_CHAR:
#if LJ_54
      if (strfmt_pushf_conv_lua54(L, &fs) != 'c')
	strfmt_pushf_bad_lua54(L, &fs);
#endif
      lj_buf_putb(sb, va_arg(argp, int));
      break;
    case STRFMT_PTR:
#if LJ_54
      if (strfmt_pushf_conv_lua54(L, &fs) != 'p')
	strfmt_pushf_bad_lua54(L, &fs);
#endif
      lj_strfmt_putptr(sb, va_arg(argp, void *));
      break;
    case STRFMT_ERR:
#if LJ_54
      if (fs.len == 2 && fs.str[0] == '%' && fs.str[1] == 'I') {
	char ibuf[64];
	lua_integer2str(ibuf, sizeof(ibuf), va_arg(argp, lua_Integer));
	/* %I is specific to lua_pushfstring() in Lua 5.4. Keep it out of
	** the shared string.format() parser so Lua-visible formatting stays
	** governed by string.format's own conversion set.
	*/
	lj_buf_putmem(sb, ibuf, (MSize)strlen(ibuf));
	fs.p = (const uint8_t *)fs.str + 2;
	break;
      } else if (fs.len == 2 && fs.str[0] == '%' && fs.str[1] == 'U') {
	strfmt_pututf8_lua54(sb, (unsigned long)va_arg(argp, long));
	fs.p = (const uint8_t *)fs.str + 2;
	break;
      }
      strfmt_pushf_bad_lua54(L, &fs);
#endif
      /* fallthrough */
    default:
      lj_buf_putb(sb, '?');
      lj_assertL(0, "bad string format near offset %d", fs.len);
      break;
    }
  }
  str = lj_buf_str(L, sb);
  setstrV(L, L->top, str);
  incr_top(L);
  return strdata(str);
}

/* Push formatted message as a string object to Lua stack. Vararg variant. */
const char *lj_strfmt_pushf(lua_State *L, const char *fmt, ...)
{
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = lj_strfmt_pushvf(L, fmt, argp);
  va_end(argp);
  return msg;
}

