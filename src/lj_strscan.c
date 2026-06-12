/*
** String scanning.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#include <math.h>
#include <locale.h>

#define lj_strscan_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_char.h"
#include "lj_strscan.h"

/* -- Scanning numbers ---------------------------------------------------- */

/*
** Rationale for the builtin string to number conversion library:
**
** It removes a dependency on libc's strtod(), which is a true portability
** nightmare. Mainly due to the plethora of supported OS and toolchain
** combinations. Sadly, the various implementations
** a) are often buggy, incomplete (no hex floats) and/or imprecise,
** b) sometimes crash or hang on certain inputs,
** c) return non-standard NaNs that need to be filtered out, and
** d) fail if the locale-specific decimal separator is not a dot,
**    which can only be fixed with atrocious workarounds.
**
** Also, most of the strtod() implementations are hopelessly bloated,
** which is not just an I-cache hog, but a problem for static linkage
** on embedded systems, too.
**
** OTOH the builtin conversion function is very compact. Even though it
** does a lot more, like parsing long longs, octal or imaginary numbers
** and returning the result in different formats:
** a) It needs less than 3 KB (!) of machine code (on x64 with -Os),
** b) it doesn't perform any dynamic allocation and,
** c) it needs only around 600 bytes of stack space.
**
** The builtin function is faster than strtod() for typical inputs, e.g.
** "123", "1.5" or "1e6". Arguably, it's slower for very large exponents,
** which are not very common (this could be fixed, if needed).
**
** And most importantly, the builtin function is equally precise on all
** platforms. It correctly converts and rounds any input to a double.
** If this is not the case, please send a bug report -- but PLEASE verify
** that the implementation you're comparing to is not the culprit!
**
** The implementation quickly pre-scans the entire string first and
** handles simple integers on-the-fly. Otherwise, it dispatches to the
** base-specific parser. Hex and octal is straightforward.
**
** Decimal to binary conversion uses a fixed-length circular buffer in
** base 100. Some simple cases are handled directly. For other cases, the
** number in the buffer is up-scaled or down-scaled until the integer part
** is in the proper range. Then the integer part is rounded and converted
** to a double which is finally rescaled to the result. Denormals need
** special treatment to prevent incorrect 'double rounding'.
*/

/* Definitions for circular decimal digit buffer (base 100 = 2 digits/byte). */
#define STRSCAN_DIG	1024
#define STRSCAN_MAXDIG	800		/* 772 + extra are sufficient. */
#define STRSCAN_DDIG	(STRSCAN_DIG/2)
#define STRSCAN_DMASK	(STRSCAN_DDIG-1)
#define STRSCAN_MAXEXP	(1 << 20)

/* Helpers for circular buffer. */
#define DNEXT(a)	(((a)+1) & STRSCAN_DMASK)
#define DPREV(a)	(((a)-1) & STRSCAN_DMASK)
#define DLEN(lo, hi)	((int32_t)(((lo)-(hi)) & STRSCAN_DMASK))

#define casecmp(c, k)	(((c) | 0x20) == k)

static int strscan_isdp(uint32_t c)
{
  if (c == '.') return 1;
#if LJ_54
  if (c == ',') {
    const char *dp = localeconv()->decimal_point;
    return dp && dp[0] == ',' && dp[1] == '\0';
  }
#endif
  return 0;
}

#if LJ_54 && LJ_TARGET_ARM64
static int strscan_fast_decimal54(GCstr *str, TValue *o)
{
  static const lua_Number pow10[] = {
    1.0, 10.0, 100.0, 1000.0, 10000.0,
    100000.0, 1000000.0, 10000000.0, 100000000.0, 1000000000.0
  };
  const uint8_t *p = (const uint8_t *)strdata(str);
  const uint8_t *e = p + str->len;
  uint64_t acc = 0;
  int neg = 0, seen_dp = 0, intdig = 0, fracdig = 0;
  while (p < e && lj_char_isspace(*p)) p++;
  if (p < e && (*p == '+' || *p == '-')) {
    neg = (*p == '-');
    p++;
  }
  while (p < e) {
    uint32_t c = *p;
    if (lj_char_isdigit(c)) {
      if (intdig + fracdig >= 15)
	return 0;
      acc = acc * 10u + (uint32_t)(c - '0');
      if (seen_dp) {
	if (++fracdig > 9)
	  return 0;
      } else {
	intdig++;
      }
      p++;
    } else if (c == '.' && !seen_dp) {
      seen_dp = 1;
      p++;
    } else {
      break;
    }
  }
  if (!seen_dp || intdig + fracdig == 0)
    return 0;
  while (p < e && lj_char_isspace(*p)) p++;
  if (p != e)
    return 0;
  o->n = (lua_Number)acc / pow10[fracdig];
  if (neg)
    o->n = -o->n;
  return 1;
}

static int strscan_fast_int64_decimal54(GCstr *str, int64_t *ip)
{
  const uint8_t *p = (const uint8_t *)strdata(str);
  const uint8_t *e = p + str->len;
  uint64_t acc = 0;
  int neg = 0, dig = 0;
  while (p < e && lj_char_isspace(*p)) p++;
  if (p < e && (*p == '+' || *p == '-')) {
    neg = (*p == '-');
    p++;
  }
  while (p < e && lj_char_isdigit(*p)) {
    if (++dig > 18)
      return 0;
    acc = acc * 10u + (uint32_t)(*p - '0');
    p++;
  }
  if (dig == 0)
    return 0;
  while (p < e && lj_char_isspace(*p)) p++;
  if (p != e)
    return 0;
  *ip = neg ? -(int64_t)acc : (int64_t)acc;
  return 1;
}
#endif

#if LJ_54
int lj_strscan_rejectnum54(const char *sp, MSize len)
{
  const uint8_t *p = (const uint8_t *)sp;
  const uint8_t *e = p + len;
  while (p < e && lj_char_isspace(*p)) p++;
  if (p < e && (*p == '+' || *p == '-')) p++;
  if (e - p >= 3 &&
      casecmp(p[0], 'i') && casecmp(p[1], 'n') && casecmp(p[2], 'f')) {
    p += 3;
    if (e - p >= 5 &&
	casecmp(p[0], 'i') && casecmp(p[1], 'n') && casecmp(p[2], 'i') &&
	casecmp(p[3], 't') && casecmp(p[4], 'y'))
      p += 5;
    while (p < e && lj_char_isspace(*p)) p++;
    return p == e;
  } else if (e - p >= 3 &&
	     casecmp(p[0], 'n') && casecmp(p[1], 'a') &&
	     casecmp(p[2], 'n')) {
    p += 3;
    while (p < e && lj_char_isspace(*p)) p++;
    return p == e;
  }
  /* LuaJIT accepts binary integer strings as an extension; Lua 5.4 does not. */
  return e - p >= 2 && p[0] == '0' && casecmp(p[1], 'b');
}

int lj_strscan_tobaseint54(GCstr *str, int32_t base, int64_t *ip)
{
  const char *p = strdata(str);
  const char *pe = p + str->len;
  lua_Unsigned u = 0;
  int neg = 0;
  while (p < pe && lj_char_isspace((unsigned char)(*p))) p++;
  if (p < pe && *p == '-') { p++; neg = 1; }
  else if (p < pe && *p == '+') { p++; }
  if (p < pe && lj_char_isalnum((unsigned char)(*p))) {
    do {
      uint32_t digit = lj_char_isdigit((unsigned char)*p) ?
		       (uint32_t)(*p - '0') :
		       (uint32_t)((*p | 0x20) - 'a' + 10);
      if (digit >= (uint32_t)base)
	return 0;
      u = u * (lua_Unsigned)base + (lua_Unsigned)digit;
      p++;
    } while (p < pe && lj_char_isalnum((unsigned char)(*p)));
    while (p < pe && lj_char_isspace((unsigned char)(*p))) p++;
    if (p == pe) {
      *ip = (int64_t)(lua_Integer)(neg ? ~u + 1u : u);
      return 1;
    }
  }
  return 0;
}

#endif

/* Final conversion to double. */
static void strscan_double(uint64_t x, TValue *o, int32_t ex2, int32_t neg)
{
  double n;

  /* Avoid double rounding for denormals. */
  if (LJ_UNLIKELY(ex2 <= -1075 && x != 0)) {
    /* NYI: all of this generates way too much code on 32 bit CPUs. */
#if (defined(__GNUC__) || defined(__clang__)) && LJ_64
    int32_t b = (int32_t)(__builtin_clzll(x)^63);
#else
    int32_t b = (x>>32) ? 32+(int32_t)lj_fls((uint32_t)(x>>32)) :
			  (int32_t)lj_fls((uint32_t)x);
#endif
    if ((int32_t)b + ex2 <= -1023 && (int32_t)b + ex2 >= -1075) {
      uint64_t rb = (uint64_t)1 << (-1075-ex2);
      if ((x & rb) && ((x & (rb+rb+rb-1)))) x += rb+rb;
      x = (x & ~(rb+rb-1));
    }
  }

  /* Convert to double using a signed int64_t conversion, then rescale. */
  lj_assertX((int64_t)x >= 0, "bad double conversion");
  n = (double)(int64_t)x;
  if (neg) n = -n;
  if (ex2) n = ldexp(n, ex2);
  o->n = n;
}

#if LJ_54 && LJ_DUALNUM
static StrScanFmt strscan_luaint64(TValue *o, uint64_t x, int32_t neg)
{
  uint64_t u = neg ? ~x+1u : x;
  int64_t i = (int64_t)u;
  if (i == (int64_t)(int32_t)i) {
    o->i = (int32_t)i;
    return STRSCAN_INT;
  }
  o->u64 = u;
  return STRSCAN_I64;
}

static int strscan_luaint64_decok(uint64_t x, int32_t neg)
{
  return neg ? x <= U64x(80000000,00000000) :
	       x < U64x(80000000,00000000);
}
#endif

/* Parse hexadecimal number. */
static StrScanFmt strscan_hex(const uint8_t *p, TValue *o,
			      StrScanFmt fmt, uint32_t opt,
			      int32_t ex2, int32_t neg, uint32_t dig)
{
  uint64_t x = 0;
#if LJ_54 && LJ_DUALNUM
  const uint8_t *ps = p;
  int32_t ex2orig = ex2;
#endif
  uint32_t i;

  /* Scan hex digits. */
  for (i = dig > 16 ? 16 : dig ; i; i--, p++) {
    uint32_t d = (!strscan_isdp(*p) ? *p : *++p); if (d > '9') d += 9;
    x = (x << 4) + (d & 15);
  }

  /* Summarize rounding-effect of excess digits. */
  for (i = 16; i < dig; i++, p++)
    x |= ((!strscan_isdp(*p) ? *p : *++p) != '0'), ex2 += 4;

  /* Format-specific handling. */
#if LJ_54 && LJ_DUALNUM
  if (fmt == STRSCAN_INT && (opt & STRSCAN_OPT_TOINT) &&
      !(opt & (STRSCAN_OPT_TONUM|STRSCAN_OPT_C)) && ex2orig == 0) {
    uint64_t w = 0;
    const uint8_t *q = ps;
    /* Lua integer hex numerals wrap on overflow. */
    for (i = dig; i; i--, q++) {
      uint32_t d = *q; if (d > '9') d += 9;
      w = (w << 4) + (d & 15);
    }
    return strscan_luaint64(o, w, neg);
  }
#endif
  switch (fmt) {
  case STRSCAN_INT:
    if (!(opt & STRSCAN_OPT_TONUM) && x < 0x80000000u+neg &&
	!(x == 0 && neg)) {
      o->i = neg ? (int32_t)(~x+1u) : (int32_t)x;
      return STRSCAN_INT;  /* Fast path for 32 bit integers. */
    }
    if (!(opt & STRSCAN_OPT_C)) { fmt = STRSCAN_NUM; break; }
    /* fallthrough */
  case STRSCAN_U32:
    if (dig > 8) return STRSCAN_ERROR;
    o->i = neg ? (int32_t)(~x+1u) : (int32_t)x;
    return STRSCAN_U32;
  case STRSCAN_I64:
  case STRSCAN_U64:
    if (dig > 16) return STRSCAN_ERROR;
    o->u64 = neg ? ~x+1u : x;
    return fmt;
  default:
    break;
  }

  /* Reduce range, then convert to double. */
  if ((x & U64x(c0000000,0000000))) { x = (x >> 2) | (x & 3); ex2 += 2; }
  strscan_double(x, o, ex2, neg);
  return fmt;
}

/* Parse octal number. */
static StrScanFmt strscan_oct(const uint8_t *p, TValue *o,
			      StrScanFmt fmt, int32_t neg, uint32_t dig)
{
  uint64_t x = 0;

  /* Scan octal digits. */
  if (dig > 22 || (dig == 22 && *p > '1')) return STRSCAN_ERROR;
  while (dig-- > 0) {
    if (!(*p >= '0' && *p <= '7')) return STRSCAN_ERROR;
    x = (x << 3) + (*p++ & 7);
  }

  /* Format-specific handling. */
  switch (fmt) {
  case STRSCAN_INT:
    if (x >= 0x80000000u+neg) fmt = STRSCAN_U32;
    /* fallthrough */
  case STRSCAN_U32:
    if ((x >> 32)) return STRSCAN_ERROR;
    o->i = neg ? (int32_t)(~(uint32_t)x+1u) : (int32_t)x;
    break;
  default:
  case STRSCAN_I64:
  case STRSCAN_U64:
    o->u64 = neg ? ~x+1u : x;
    break;
  }
  return fmt;
}

/* Parse decimal number. */
static StrScanFmt strscan_dec(const uint8_t *p, TValue *o,
			      StrScanFmt fmt, uint32_t opt,
			      int32_t ex10, int32_t neg, uint32_t dig)
{
  uint8_t xi[STRSCAN_DDIG], *xip = xi;

  if (dig) {
    uint32_t i = dig;
    if (i > STRSCAN_MAXDIG) {
      ex10 += (int32_t)(i - STRSCAN_MAXDIG);
      i = STRSCAN_MAXDIG;
    }
    /* Scan unaligned leading digit. */
    if (((ex10^i) & 1))
      *xip++ = ((!strscan_isdp(*p) ? *p : *++p) & 15), i--, p++;
    /* Scan aligned double-digits. */
    for ( ; i > 1; i -= 2) {
      uint32_t d = 10 * ((!strscan_isdp(*p) ? *p : *++p) & 15); p++;
      *xip++ = d + ((!strscan_isdp(*p) ? *p : *++p) & 15); p++;
    }
    /* Scan and realign trailing digit. */
    if (i) *xip++ = 10 * ((!strscan_isdp(*p) ? *p : *++p) & 15), ex10--, dig++, p++;

    /* Summarize rounding-effect of excess digits. */
    if (dig > STRSCAN_MAXDIG) {
      do {
	if ((!strscan_isdp(*p) ? *p : *++p) != '0') { xip[-1] |= 1; break; }
	p++;
      } while (--dig > STRSCAN_MAXDIG);
      dig = STRSCAN_MAXDIG;
    } else {  /* Simplify exponent. */
      while (ex10 > 0 && dig <= 18) *xip++ = 0, ex10 -= 2, dig += 2;
    }
  } else {  /* Only got zeros. */
    ex10 = 0;
    xi[0] = 0;
  }

  /* Fast path for numbers in integer format (but handles e.g. 1e6, too). */
  if (dig <= 20 && ex10 == 0) {
    uint8_t *xis;
    uint64_t x = xi[0];
    double n;
    for (xis = xi+1; xis < xip; xis++) x = x * 100 + *xis;
    if (!(dig == 20 && (xi[0] > 18 || (int64_t)x >= 0))) {  /* No overflow? */
      /* Format-specific handling. */
      switch (fmt) {
      case STRSCAN_INT:
	if (!(opt & STRSCAN_OPT_TONUM) && x < 0x80000000u+neg) {
	  o->i = neg ? (int32_t)(~x+1u) : (int32_t)x;
	  return STRSCAN_INT;  /* Fast path for 32 bit integers. */
	}
#if LJ_54 && LJ_DUALNUM
	if ((opt & STRSCAN_OPT_TOINT) &&
	    !(opt & (STRSCAN_OPT_TONUM|STRSCAN_OPT_C)) &&
	    strscan_luaint64_decok(x, neg))
	  return strscan_luaint64(o, x, neg);
#endif
	if (!(opt & STRSCAN_OPT_C)) { fmt = STRSCAN_NUM; goto plainnumber; }
	/* fallthrough */
      case STRSCAN_U32:
	if ((x >> 32) != 0) return STRSCAN_ERROR;
	o->i = neg ? (int32_t)(~x+1u) : (int32_t)x;
	return STRSCAN_U32;
      case STRSCAN_I64:
      case STRSCAN_U64:
	o->u64 = neg ? ~x+1u : x;
	return fmt;
      default:
      plainnumber:  /* Fast path for plain numbers < 2^63. */
	if ((int64_t)x < 0) break;
	n = (double)(int64_t)x;
	if (neg) n = -n;
	o->n = n;
	return fmt;
      }
    }
  }

  /* Slow non-integer path. */
  if (fmt == STRSCAN_INT) {
    if ((opt & STRSCAN_OPT_C)) return STRSCAN_ERROR;
    fmt = STRSCAN_NUM;
  } else if (fmt > STRSCAN_INT) {
    return STRSCAN_ERROR;
  }
  {
    uint32_t hi = 0, lo = (uint32_t)(xip-xi);
    int32_t ex2 = 0, idig = (int32_t)lo + (ex10 >> 1);

    lj_assertX(lo > 0 && (ex10 & 1) == 0, "bad lo %d ex10 %d", lo, ex10);

    /* Handle simple overflow/underflow. */
    if (idig > 310/2) { if (neg) setminfV(o); else setpinfV(o); return fmt; }
    else if (idig < -326/2) { o->n = neg ? -0.0 : 0.0; return fmt; }

    /* Scale up until we have at least 17 or 18 integer part digits. */
    while (idig < 9 && idig < DLEN(lo, hi)) {
      uint32_t i, cy = 0;
      ex2 -= 6;
      for (i = DPREV(lo); ; i = DPREV(i)) {
	uint32_t d = (xi[i] << 6) + cy;
	cy = (((d >> 2) * 5243) >> 17); d = d - cy * 100;  /* Div/mod 100. */
	xi[i] = (uint8_t)d;
	if (i == hi) break;
	if (d == 0 && i == DPREV(lo)) lo = i;
      }
      if (cy) {
	hi = DPREV(hi);
	if (xi[DPREV(lo)] == 0) lo = DPREV(lo);
	else if (hi == lo) { lo = DPREV(lo); xi[DPREV(lo)] |= xi[lo]; }
	xi[hi] = (uint8_t)cy; idig++;
      }
    }

    /* Scale down until no more than 17 or 18 integer part digits remain. */
    while (idig > 9) {
      uint32_t i = hi, cy = 0;
      ex2 += 6;
      do {
	cy += xi[i];
	xi[i] = (cy >> 6);
	cy = 100 * (cy & 0x3f);
	if (xi[i] == 0 && i == hi) hi = DNEXT(hi), idig--;
	i = DNEXT(i);
      } while (i != lo);
      while (cy) {
	if (hi == lo) { xi[DPREV(lo)] |= 1; break; }
	xi[lo] = (cy >> 6); lo = DNEXT(lo);
	cy = 100 * (cy & 0x3f);
      }
    }

    /* Collect integer part digits and convert to rescaled double. */
    {
      uint64_t x = xi[hi];
      uint32_t i;
      for (i = DNEXT(hi); --idig > 0 && i != lo; i = DNEXT(i))
	x = x * 100 + xi[i];
      if (i == lo) {
	while (--idig >= 0) x = x * 100;
      } else {  /* Gather round bit from remaining digits. */
	x <<= 1; ex2--;
	do {
	  if (xi[i]) { x |= 1; break; }
	  i = DNEXT(i);
	} while (i != lo);
      }
      strscan_double(x, o, ex2, neg);
    }
  }
  return fmt;
}

/* Parse binary number. */
static StrScanFmt strscan_bin(const uint8_t *p, TValue *o,
			      StrScanFmt fmt, uint32_t opt,
			      int32_t ex2, int32_t neg, uint32_t dig)
{
  uint64_t x = 0;
  uint32_t i;

  if (ex2 || dig > 64) return STRSCAN_ERROR;

  /* Scan binary digits. */
  for (i = dig; i; i--, p++) {
    if ((*p & ~1) != '0') return STRSCAN_ERROR;
    x = (x << 1) | (*p & 1);
  }

  /* Format-specific handling. */
  switch (fmt) {
  case STRSCAN_INT:
    if (!(opt & STRSCAN_OPT_TONUM) && x < 0x80000000u+neg) {
      o->i = neg ? (int32_t)(~x+1u) : (int32_t)x;
      return STRSCAN_INT;  /* Fast path for 32 bit integers. */
    }
    if (!(opt & STRSCAN_OPT_C)) { fmt = STRSCAN_NUM; break; }
    /* fallthrough */
  case STRSCAN_U32:
    if (dig > 32) return STRSCAN_ERROR;
    o->i = neg ? (int32_t)(~x+1u) : (int32_t)x;
    return STRSCAN_U32;
  case STRSCAN_I64:
  case STRSCAN_U64:
    o->u64 = neg ? ~x+1u : x;
    return fmt;
  default:
    break;
  }

  /* Reduce range, then convert to double. */
  if ((x & U64x(c0000000,0000000))) { x = (x >> 2) | (x & 3); ex2 += 2; }
  strscan_double(x, o, ex2, neg);
  return fmt;
}

/* Scan string containing a number. Returns format. Returns value in o. */
StrScanFmt lj_strscan_scan(const uint8_t *p, MSize len, TValue *o,
			   uint32_t opt)
{
  int32_t neg = 0;
  const uint8_t *pe = p + len;

  /* Remove leading space, parse sign and non-numbers. */
  if (LJ_UNLIKELY(!lj_char_isdigit(*p))) {
    while (lj_char_isspace(*p)) p++;
    if (*p == '+' || *p == '-') neg = (*p++ == '-');
    if (LJ_UNLIKELY(*p >= 'A')) {  /* Parse "inf", "infinity" or "nan". */
      TValue tmp;
      setnanV(&tmp);
      if (casecmp(p[0],'i') && casecmp(p[1],'n') && casecmp(p[2],'f')) {
	if (neg) setminfV(&tmp); else setpinfV(&tmp);
	p += 3;
	if (casecmp(p[0],'i') && casecmp(p[1],'n') && casecmp(p[2],'i') &&
	    casecmp(p[3],'t') && casecmp(p[4],'y')) p += 5;
      } else if (casecmp(p[0],'n') && casecmp(p[1],'a') && casecmp(p[2],'n')) {
	p += 3;
      }
      while (lj_char_isspace(*p)) p++;
      if (*p || p < pe) return STRSCAN_ERROR;
      o->u64 = tmp.u64;
      return STRSCAN_NUM;
    }
  }

  /* Parse regular number. */
  {
    StrScanFmt fmt = STRSCAN_INT;
    int cmask = LJ_CHAR_DIGIT;
    int base = (opt & STRSCAN_OPT_C) && *p == '0' ? 0 : 10;
    const uint8_t *sp, *dp = NULL;
    uint32_t dig = 0, hasdig = 0, x = 0;
    int32_t ex = 0;

    /* Determine base and skip leading zeros. */
    if (LJ_UNLIKELY(*p <= '0')) {
      if (*p == '0') {
	if (casecmp(p[1], 'x'))
	  base = 16, cmask = LJ_CHAR_XDIGIT, p += 2;
	else if (casecmp(p[1], 'b'))
	  base = 2, cmask = LJ_CHAR_DIGIT, p += 2;
      }
      for ( ; ; p++) {
	if (*p == '0') {
	  hasdig = 1;
	} else if (strscan_isdp(*p)) {
	  if (dp) return STRSCAN_ERROR;
	  dp = p;
	} else {
	  break;
	}
      }
    }

    /* Preliminary digit and decimal point scan. */
    for (sp = p; ; p++) {
      if (LJ_LIKELY(lj_char_isa(*p, cmask))) {
	x = x * 10 + (*p & 15);  /* For fast path below. */
	dig++;
      } else if (strscan_isdp(*p)) {
	if (dp) return STRSCAN_ERROR;
	dp = p;
      } else {
	break;
      }
    }
    if (!(hasdig | dig)) return STRSCAN_ERROR;

    /* Handle decimal point. */
    if (dp) {
      if (base == 2) return STRSCAN_ERROR;
      fmt = STRSCAN_NUM;
      if (dig) {
	ex = (int32_t)(dp-(p-1)); dp = p-1;
	while (ex < 0 && *dp-- == '0') ex++, dig--;  /* Skip trailing zeros. */
	if (ex <= -STRSCAN_MAXEXP) return STRSCAN_ERROR;
	if (base == 16) ex *= 4;
      }
    }

    /* Parse exponent. */
    if (base >= 10 && casecmp(*p, (uint32_t)(base == 16 ? 'p' : 'e'))) {
      uint32_t xx;
      int negx = 0;
      fmt = STRSCAN_NUM; p++;
      if (*p == '+' || *p == '-') negx = (*p++ == '-');
      if (!lj_char_isdigit(*p)) return STRSCAN_ERROR;
      xx = (*p++ & 15);
      while (lj_char_isdigit(*p)) {
	xx = xx * 10 + (*p & 15);
	if (xx >= STRSCAN_MAXEXP) return STRSCAN_ERROR;
	p++;
      }
      ex += negx ? (int32_t)(~xx+1u) : (int32_t)xx;
    }

    /* Parse suffix. */
    if (*p) {
      /* I (IMAG), U (U32), LL (I64), ULL/LLU (U64), L (long), UL/LU (ulong). */
      /* NYI: f (float). Not needed until cp_number() handles non-integers. */
      if (casecmp(*p, 'i')) {
	if (!(opt & STRSCAN_OPT_IMAG)) return STRSCAN_ERROR;
	p++; fmt = STRSCAN_IMAG;
      } else if (fmt == STRSCAN_INT) {
	if (casecmp(*p, 'u')) p++, fmt = STRSCAN_U32;
	if (casecmp(*p, 'l')) {
	  p++;
	  if (casecmp(*p, 'l')) p++, fmt += STRSCAN_I64 - STRSCAN_INT;
	  else if (!(opt & STRSCAN_OPT_C)) return STRSCAN_ERROR;
	  else if (sizeof(long) == 8) fmt += STRSCAN_I64 - STRSCAN_INT;
	}
	if (casecmp(*p, 'u') && (fmt == STRSCAN_INT || fmt == STRSCAN_I64))
	  p++, fmt += STRSCAN_U32 - STRSCAN_INT;
	if ((fmt == STRSCAN_U32 && !(opt & STRSCAN_OPT_C)) ||
	    (fmt >= STRSCAN_I64 && !(opt & STRSCAN_OPT_LL)))
	  return STRSCAN_ERROR;
      }
      while (lj_char_isspace(*p)) p++;
      if (*p) return STRSCAN_ERROR;
    }
    if (p < pe) return STRSCAN_ERROR;

    /* Fast path for decimal 32 bit integers. */
    if (fmt == STRSCAN_INT && base == 10 &&
	(dig < 10 || (dig == 10 && *sp <= '2' && x < 0x80000000u+neg))) {
      if ((opt & STRSCAN_OPT_TONUM)) {
	o->n = neg ? -(double)x : (double)x;
	return STRSCAN_NUM;
      } else if (x == 0 && neg) {
	o->n = -0.0;
	return STRSCAN_NUM;
      } else {
	o->i = neg ? (int32_t)(~x+1u) : (int32_t)x;
	return STRSCAN_INT;
      }
    }

    /* Dispatch to base-specific parser. */
    if (base == 0 && !(fmt == STRSCAN_NUM || fmt == STRSCAN_IMAG))
      return strscan_oct(sp, o, fmt, neg, dig);
    if (base == 16)
      fmt = strscan_hex(sp, o, fmt, opt, ex, neg, dig);
    else if (base == 2)
      fmt = strscan_bin(sp, o, fmt, opt, ex, neg, dig);
    else
      fmt = strscan_dec(sp, o, fmt, opt, ex, neg, dig);

    /* Try to convert number to integer, if requested. */
    if (fmt == STRSCAN_NUM && (opt & STRSCAN_OPT_TOINT)) {
#if LJ_54
      /* Lua 5.4 keeps decimal/exponent/hex-float forms as floats even when
      ** their value has an integer representation, e.g. 1.0 or 0x1p0.
      */
      return fmt;
#else
      int64_t tmp;
      if (lj_num2int_check(o->n, tmp, o->i) && !tvismzero(o))
	return STRSCAN_INT;
#endif
    }
    return fmt;
  }
}

int LJ_FASTCALL lj_strscan_num(GCstr *str, TValue *o)
{
#if LJ_54
  if (lj_strscan_rejectnum54(strdata(str), str->len))
    return 0;  /* Lua 5.4 string-to-number conversion excludes extensions. */
#endif
  StrScanFmt fmt = lj_strscan_scan((const uint8_t *)strdata(str), str->len, o,
				   STRSCAN_OPT_TONUM);
  lj_assertX(fmt == STRSCAN_ERROR || fmt == STRSCAN_NUM, "bad scan format");
  return (fmt != STRSCAN_ERROR);
}

int LJ_FASTCALL lj_strscan_numtype54(GCstr *str)
{
#if LJ_54
  TValue o;
  StrScanFmt fmt;
#if LJ_TARGET_ARM64
  int64_t i;
  if (strscan_fast_int64_decimal54(str, &i))
    return 1;
  if (strscan_fast_decimal54(str, &o))
    return 2;
#endif
  if (lj_strscan_rejectnum54(strdata(str), str->len))
    return 3;  /* LuaJIT-only numeric extension rejected by Lua 5.4. */
  fmt = lj_strscan_scan((const uint8_t *)strdata(str), str->len, &o,
			STRSCAN_OPT_TOINT);
  return (fmt == STRSCAN_INT || fmt == STRSCAN_I64) ? 1 :
	 fmt == STRSCAN_NUM ? 2 : 0;
#else
  UNUSED(str);
  return 0;
#endif
}

int LJ_FASTCALL lj_strscan_numtype54s(GCstr *str)
{
  return lj_strscan_numtype54(str);
}

int32_t LJ_FASTCALL lj_strscan_toint54(GCstr *str)
{
#if LJ_54
  TValue o;
  int ok = lj_strscan_number(str, &o);
  lj_assertX(ok && tvisint(&o), "bad integer string guard");
  if (!ok || !tvisint(&o))
    return 0;
  return intV(&o);
#else
  UNUSED(str);
  return 0;
#endif
}

lua_Number LJ_FASTCALL lj_strscan_tonum54s(GCstr *str)
{
#if LJ_54
  TValue o;
  int ok;
#if LJ_TARGET_ARM64
  if (strscan_fast_decimal54(str, &o))
    return numV(&o);
#endif
  ok = lj_strscan_number(str, &o);
  lj_assertX(ok && tvisnumber(&o), "bad numeric string guard");
  if (!ok)
    return 0;
  return tvisint(&o) ? (lua_Number)intV(&o) : numV(&o);
#else
  UNUSED(str);
  return 0;
#endif
}

static int strscan_tocheckint54(GCstr *str, int32_t *ip)
{
#if LJ_54
  TValue o;
  lua_Number n;
  int64_t k;
  if (!lj_strscan_number(str, &o))
    return 0;
  if (tvisint(&o)) {
    *ip = intV(&o);
    return 1;
  }
  if (!tvisnum(&o))
    return 0;
  n = numV(&o);
  if (!(n >= (double)LUA_MININTEGER && n <= (double)LUA_MAXINTEGER))
    return 0;
  k = lj_num2i64(n);
  if ((lua_Number)k != n)
    return 0;
  *ip = (int32_t)k;
  return 1;
#else
  UNUSED(str); UNUSED(ip);
  return 0;
#endif
}

static int strscan_toi6454(GCstr *str, int64_t *ip)
{
#if LJ_54
  TValue o;
  StrScanFmt fmt;
#if LJ_TARGET_ARM64
  if (strscan_fast_int64_decimal54(str, ip))
    return 1;
#endif
  if (lj_strscan_rejectnum54(strdata(str), str->len))
    return 0;
  fmt = lj_strscan_scan((const uint8_t *)strdata(str), str->len, &o,
			STRSCAN_OPT_TOINT);
  if (fmt == STRSCAN_INT) {
    *ip = (int64_t)o.i;
    return 1;
  } else if (fmt == STRSCAN_I64) {
    *ip = (int64_t)o.u64;
    return 1;
  } else if (fmt == STRSCAN_NUM) {
    lua_Number n = numV(&o);
    int64_t k;
    if (!(n >= (-9223372036854775807.0 - 1.0) &&
	  n < 9223372036854775808.0))
      return 0;
    k = lj_num2i64(n);
    if ((lua_Number)k != n)
      return 0;
    *ip = k;
    return 1;
  }
#else
  UNUSED(str);
#endif
  UNUSED(ip);
  return 0;
}

static int strscan_toint6454(GCstr *str, int64_t *ip)
{
#if LJ_54
  TValue o;
  StrScanFmt fmt;
#if LJ_TARGET_ARM64
  if (strscan_fast_int64_decimal54(str, ip))
    return 1;
#endif
  if (lj_strscan_rejectnum54(strdata(str), str->len))
    return 0;
  fmt = lj_strscan_scan((const uint8_t *)strdata(str), str->len, &o,
			STRSCAN_OPT_TOINT);
  if (fmt == STRSCAN_INT) {
    *ip = (int64_t)o.i;
    return 1;
  } else if (fmt == STRSCAN_I64) {
    *ip = (int64_t)o.u64;
    return 1;
  }
#else
  UNUSED(str);
#endif
  UNUSED(ip);
  return 0;
}

int lj_strscan_toint64try54(GCstr *str, int64_t *ip)
{
  return strscan_toint6454(str, ip);
}

int LJ_FASTCALL lj_strscan_toi64ok54(GCstr *str)
{
  int64_t i;
  return strscan_toi6454(str, &i);
}

int64_t LJ_FASTCALL lj_strscan_toi6454(GCstr *str)
{
  int64_t i = 0;
  int ok = strscan_toi6454(str, &i);
  lj_assertX(ok, "bad string-to-int64 guard");
  if (!ok)
    return 0;
  return i;
}

int LJ_FASTCALL lj_strscan_toint64ok54(GCstr *str)
{
  int64_t i;
  return strscan_toint6454(str, &i);
}

int64_t LJ_FASTCALL lj_strscan_toint6454(GCstr *str)
{
  int64_t i = 0;
  int ok = strscan_toint6454(str, &i);
  lj_assertX(ok, "bad integer-format string guard");
  if (!ok)
    return 0;
  return i;
}

int LJ_FASTCALL lj_strscan_tocheckintok54(GCstr *str)
{
  int32_t i;
  return strscan_tocheckint54(str, &i);
}

int32_t LJ_FASTCALL lj_strscan_tocheckint54(GCstr *str)
{
  int32_t i = 0;
  int ok = strscan_tocheckint54(str, &i);
  lj_assertX(ok, "bad string-to-integer guard");
  if (!ok)
    return 0;
  return i;
}

int lj_strscan_tobaseintok54(GCstr *str, int32_t base)
{
#if LJ_54
  int64_t i;
  return lj_strscan_tobaseint54(str, base, &i);
#else
  UNUSED(str); UNUSED(base);
  return 0;
#endif
}

int64_t lj_strscan_tobaseintvalue54(GCstr *str, int32_t base)
{
#if LJ_54
  int64_t i = 0;
  int ok = lj_strscan_tobaseint54(str, base, &i);
  lj_assertX(ok, "bad explicit-base integer string guard");
  if (!ok)
    return 0;
  return i;
#else
  UNUSED(str); UNUSED(base);
  return 0;
#endif
}

#if LJ_DUALNUM
int LJ_FASTCALL lj_strscan_number(GCstr *str, TValue *o)
{
#if LJ_54
  if (lj_strscan_rejectnum54(strdata(str), str->len))
    return 0;  /* Keep integer-preserving conversion aligned with Lua 5.4. */
#endif
  StrScanFmt fmt = lj_strscan_scan((const uint8_t *)strdata(str), str->len, o,
				   STRSCAN_OPT_TOINT);
  lj_assertX(fmt == STRSCAN_ERROR || fmt == STRSCAN_NUM ||
	     fmt == STRSCAN_INT || fmt == STRSCAN_I64,
	     "bad scan format");
  if (fmt == STRSCAN_INT) setitype(o, LJ_TISNUM);
  else if (fmt == STRSCAN_I64) setnumV(o, (lua_Number)(int64_t)o->u64);
  return (fmt != STRSCAN_ERROR);
}

#if LJ_54
int LJ_FASTCALL lj_strscan_number54(lua_State *L, GCstr *str, TValue *o)
{
  StrScanFmt fmt;
#if LJ_TARGET_ARM64
  int64_t i;
  if (strscan_fast_int64_decimal54(str, &i)) {
    if (i == (int64_t)(int32_t)i) {
      setintV(o, (int32_t)i);
    } else {
      lj_obj_setint64(L, o, i);
    }
    return 1;
  }
  if (strscan_fast_decimal54(str, o)) {
    setnumV(o, numV(o));
    return 1;
  }
#endif
  if (lj_strscan_rejectnum54(strdata(str), str->len))
    return 0;
  fmt = lj_strscan_scan((const uint8_t *)strdata(str), str->len, o,
			STRSCAN_OPT_TOINT);
  lj_assertX(fmt == STRSCAN_ERROR || fmt == STRSCAN_NUM ||
	     fmt == STRSCAN_INT || fmt == STRSCAN_I64,
	     "bad scan format");
  if (fmt == STRSCAN_INT) {
    setitype(o, LJ_TISNUM);
  } else if (fmt == STRSCAN_I64) {
    lj_obj_setint64(L, o, (int64_t)o->u64);
  }
  return (fmt != STRSCAN_ERROR);
}
#endif
#endif

#undef DNEXT
#undef DPREV
#undef DLEN
