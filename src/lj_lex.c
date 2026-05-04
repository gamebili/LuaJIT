/*
** Lexical analyzer.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_lex_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"
#if LJ_HASFFI
#include "lj_tab.h"
#include "lj_ctype.h"
#include "lj_cdata.h"
#include "lualib.h"
#endif
#include "lj_state.h"
#include "lj_lex.h"
#include "lj_parse.h"
#include "lj_char.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"

/* Lua lexer token names. */
static const char *const tokennames[] = {
#define TKSTR1(name)		#name,
#define TKSTR2(name, sym)	#sym,
TKDEF(TKSTR1, TKSTR2)
#undef TKSTR1
#undef TKSTR2
  NULL
};

/* -- Buffer handling ----------------------------------------------------- */

#define LEX_EOF			(-1)
#define lex_iseol(ls)		(ls->c == '\n' || ls->c == '\r')

/* Get more input from reader. */
static LJ_NOINLINE LexChar lex_more(LexState *ls)
{
  size_t sz;
  const char *p = ls->rfunc(ls->L, ls->rdata, &sz);
  if (p == NULL || sz == 0) return LEX_EOF;
  if (sz >= LJ_MAX_BUF) {
    if (sz != ~(size_t)0) lj_err_mem(ls->L);
    sz = ~(uintptr_t)0 - (uintptr_t)p;
    if (sz >= LJ_MAX_BUF) sz = LJ_MAX_BUF-1;
    ls->endmark = 1;
  }
  ls->pe = p + sz;
  ls->p = p + 1;
  return (LexChar)(uint8_t)p[0];
}

/* Get next character. */
static LJ_AINLINE LexChar lex_next(LexState *ls)
{
  return (ls->c = ls->p < ls->pe ? (LexChar)(uint8_t)*ls->p++ : lex_more(ls));
}

/* Save character. */
static LJ_AINLINE void lex_save(LexState *ls, LexChar c)
{
  lj_buf_putb(&ls->sb, c);
}

/* Save previous character and get next character. */
static LJ_AINLINE LexChar lex_savenext(LexState *ls)
{
  lex_save(ls, ls->c);
  return lex_next(ls);
}

/* Skip line break. Handles "\n", "\r", "\r\n" or "\n\r". */
static void lex_newline(LexState *ls)
{
  LexChar old = ls->c;
  lj_assertLS(lex_iseol(ls), "bad usage");
  lex_next(ls);  /* Skip "\n" or "\r". */
  if (lex_iseol(ls) && ls->c != old) lex_next(ls);  /* Skip "\n\r" or "\r\n". */
  if (++ls->linenumber >= LJ_MAX_LINE)
    lj_lex_error(ls, ls->tok, LJ_ERR_XLINES);
}

/* -- Scanner for terminals ----------------------------------------------- */

/* Parse a number literal. */
static void lex_number(LexState *ls, TValue *tv)
{
  StrScanFmt fmt;
  uint32_t opt;
  LexChar c, xp = 'e';
  lj_assertLS(lj_char_isdigit(ls->c), "bad usage");
  if ((c = ls->c) == '0' && (lex_savenext(ls) | 0x20) == 'x')
    xp = 'p';
  while (lj_char_isident(ls->c) || ls->c == '.' ||
	 ((ls->c == '-' || ls->c == '+') && (c | 0x20) == xp)) {
    c = ls->c;
    lex_savenext(ls);
  }
  lex_save(ls, '\0');
  if (LJ_54 && sbuflen(&ls->sb) > 2 && ls->sb.b[0] == '0' &&
      ((ls->sb.b[1] | 0x20) == 'b')) {
    /* 0b... is a LuaJIT extension, not a Lua 5.4 numeric literal. */
    lj_lex_error(ls, TK_number, LJ_ERR_XNUMBER);
  }
  opt = (LJ_DUALNUM ? STRSCAN_OPT_TOINT : STRSCAN_OPT_TONUM);
  /* Keep LuaJIT numeric literal suffixes out of the Lua 5.4 syntax surface. */
  if (LJ_HASFFI && !LJ_54)
    opt |= (STRSCAN_OPT_LL|STRSCAN_OPT_IMAG);
  fmt = lj_strscan_scan((const uint8_t *)ls->sb.b, sbuflen(&ls->sb)-1, tv, opt);
  if (LJ_DUALNUM && fmt == STRSCAN_INT) {
    setitype(tv, LJ_TISNUM);
  } else if (fmt == STRSCAN_NUM) {
    /* Already in correct format. */
#if LJ_HASFFI
  } else if (fmt != STRSCAN_ERROR) {
    lua_State *L = ls->L;
    GCcdata *cd;
    lj_assertLS(fmt == STRSCAN_I64 || fmt == STRSCAN_U64 || fmt == STRSCAN_IMAG,
		"unexpected number format %d", fmt);
    ctype_loadffi(L);
    if (fmt == STRSCAN_IMAG) {
      cd = lj_cdata_new_(L, CTID_COMPLEX_DOUBLE, 2*sizeof(double));
      ((double *)cdataptr(cd))[0] = 0;
      ((double *)cdataptr(cd))[1] = numV(tv);
    } else {
      cd = lj_cdata_new_(L, fmt==STRSCAN_I64 ? CTID_INT64 : CTID_UINT64, 8);
      *(uint64_t *)cdataptr(cd) = tv->u64;
    }
    lj_parse_keepcdata(ls, tv, cd);
#endif
  } else {
    lj_assertLS(fmt == STRSCAN_ERROR,
		"unexpected number format %d", fmt);
    lj_lex_error(ls, TK_number, LJ_ERR_XNUMBER);
  }
}

/* Skip equal signs for "[=...=[" and "]=...=]" and return their count. */
static int lex_skipeq(LexState *ls)
{
  int count = 0;
  LexChar s = ls->c;
  lj_assertLS(s == '[' || s == ']', "bad usage");
  while (lex_savenext(ls) == '=' && count < 0x20000000)
    count++;
  return (ls->c == s) ? count : (-count) - 1;
}

/* Parse a long string or long comment (tv set to NULL). */
static void lex_longstring(LexState *ls, TValue *tv, int sep)
{
  lex_savenext(ls);  /* Skip second '['. */
  if (lex_iseol(ls))  /* Skip initial newline. */
    lex_newline(ls);
  for (;;) {
    switch (ls->c) {
    case LEX_EOF:
      lj_lex_error(ls, TK_eof, tv ? LJ_ERR_XLSTR : LJ_ERR_XLCOM);
      break;
    case ']':
      if (lex_skipeq(ls) == sep) {
	lex_savenext(ls);  /* Skip second ']'. */
	goto endloop;
      }
      break;
    case '\n':
    case '\r':
      lex_save(ls, '\n');
      lex_newline(ls);
      if (!tv) lj_buf_reset(&ls->sb);  /* Don't waste space for comments. */
      break;
    default:
      lex_savenext(ls);
      break;
    }
  } endloop:
  if (tv) {
    GCstr *str = lj_parse_keepstr(ls, ls->sb.b + (2 + (MSize)sep),
				      sbuflen(&ls->sb) - 2*(2 + (MSize)sep));
    setstrV(ls->L, tv, str);
  }
}

/* Parse a string. */
static void lex_string(LexState *ls, TValue *tv)
{
  LexChar delim = ls->c;  /* Delimiter is '\'' or '"'. */
  lex_savenext(ls);
  while (ls->c != delim) {
    switch (ls->c) {
    case LEX_EOF:
      lj_lex_error(ls, TK_eof, LJ_ERR_XSTR);
      continue;
    case '\n':
    case '\r':
      lj_lex_error(ls, TK_string, LJ_ERR_XSTR);
      continue;
    case '\\': {
      LexChar c = lex_next(ls);  /* Skip the '\\'. */
      switch (c) {
      case 'a': c = '\a'; break;
      case 'b': c = '\b'; break;
      case 'f': c = '\f'; break;
      case 'n': c = '\n'; break;
      case 'r': c = '\r'; break;
      case 't': c = '\t'; break;
      case 'v': c = '\v'; break;
      case 'x':  /* Hexadecimal escape '\xXX'. */
	c = (lex_next(ls) & 15u) << 4;
	if (!lj_char_isdigit(ls->c)) {
	  if (!lj_char_isxdigit(ls->c)) goto err_xesc;
	  c += 9 << 4;
	}
	c += (lex_next(ls) & 15u);
	if (!lj_char_isdigit(ls->c)) {
	  if (!lj_char_isxdigit(ls->c)) goto err_xesc;
	  c += 9;
	}
	break;
      case 'u': {  /* Unicode escape '\u{XX...}'. */
	uint32_t cp = 0;
	if (lex_next(ls) != '{') goto err_xesc;
	lex_next(ls);
	do {
	  uint32_t digit;
	  if (!lj_char_isdigit(ls->c)) {
	    if (!lj_char_isxdigit(ls->c)) goto err_xesc;
	    digit = (ls->c & 15u) + 9u;
	  } else {
	    digit = ls->c & 15u;
	  }
	  if (LJ_54) {
	    if (cp > (0x7fffffffu >> 4)) goto err_xesc;
	  } else if (cp > (0x10ffffu >> 4)) {
	    goto err_xesc;
	  }
	  cp = (cp << 4) | digit;
	  if (LJ_54) {
	    if (cp > 0x7fffffffu) goto err_xesc;  /* Lua 5.4 max. */
	  } else if (cp >= 0x110000) {
	    goto err_xesc;  /* Out of Unicode range. */
	  }
	} while (lex_next(ls) != '}');
	if (LJ_54 && cp >= 0x200000) {
	  /* Lua 5.4 accepts extended UTF-8 escapes up to 0x7fffffff; mirror
	  ** utf8.char() so source literals and runtime construction agree.
	  */
	  if (cp < 0x4000000) {
	    lex_save(ls, 0xf8 | (cp >> 24));
	  } else {
	    lex_save(ls, 0xfc | (cp >> 30));
	    lex_save(ls, 0x80 | ((cp >> 24) & 0x3f));
	  }
	  lex_save(ls, 0x80 | ((cp >> 18) & 0x3f));
	  lex_save(ls, 0x80 | ((cp >> 12) & 0x3f));
	  lex_save(ls, 0x80 | ((cp >> 6) & 0x3f));
	} else if (cp < 0x800) {
	  if (cp < 0x80) { c = (LexChar)cp; break; }
	  lex_save(ls, 0xc0 | (cp >> 6));
	} else {
	  if (cp >= 0x10000) {
	    lex_save(ls, 0xf0 | (cp >> 18));
	    lex_save(ls, 0x80 | ((cp >> 12) & 0x3f));
	  } else {
	    if (!LJ_54 && cp >= 0xd800 && cp < 0xe000)
	      goto err_xesc;  /* No surrogates outside 5.4 compat. */
	    lex_save(ls, 0xe0 | (cp >> 12));
	  }
	  lex_save(ls, 0x80 | ((cp >> 6) & 0x3f));
	}
	c = 0x80 | (cp & 0x3f);
	break;
	}
      case 'z':  /* Skip whitespace. */
	lex_next(ls);
	while (lj_char_isspace(ls->c))
	  if (lex_iseol(ls)) lex_newline(ls); else lex_next(ls);
	continue;
      case '\n': case '\r': lex_save(ls, '\n'); lex_newline(ls); continue;
      case '\\': case '\"': case '\'': break;
      case LEX_EOF: continue;
      default:
	if (!lj_char_isdigit(c))
	  goto err_xesc;
	c -= '0';  /* Decimal escape '\ddd'. */
	if (lj_char_isdigit(lex_next(ls))) {
	  c = c*10 + (ls->c - '0');
	  if (lj_char_isdigit(lex_next(ls))) {
	    c = c*10 + (ls->c - '0');
	    if (c > 255) {
	    err_xesc:
	      lj_lex_error(ls, TK_string, LJ_ERR_XESC);
	    }
	    lex_next(ls);
	  }
	}
	lex_save(ls, c);
	continue;
      }
      lex_save(ls, c);
      lex_next(ls);
      continue;
      }
    default:
      lex_savenext(ls);
      break;
    }
  }
  lex_savenext(ls);  /* Skip trailing delimiter. */
  setstrV(ls->L, tv,
	  lj_parse_keepstr(ls, ls->sb.b+1, sbuflen(&ls->sb)-2));
}

/* -- Main lexical scanner ------------------------------------------------ */

/* Get next lexical token. */
static LexToken lex_scan(LexState *ls, TValue *tv)
{
  lj_buf_reset(&ls->sb);
  for (;;) {
    if (lj_char_isident(ls->c)) {
      GCstr *s;
      if (lj_char_isdigit(ls->c)) {  /* Numeric literal. */
	lex_number(ls, tv);
	return TK_number;
      }
      /* Identifier or reserved word. */
      do {
	lex_savenext(ls);
      } while (lj_char_isident(ls->c));
      s = lj_parse_keepstr(ls, ls->sb.b, sbuflen(&ls->sb));
      setstrV(ls->L, tv, s);
      if (s->reserved > 0)  /* Reserved word? */
	return TK_OFS + s->reserved;
      return TK_name;
    }
    switch (ls->c) {
    case '\n':
    case '\r':
      lex_newline(ls);
      continue;
    case ' ':
    case '\t':
    case '\v':
    case '\f':
      lex_next(ls);
      continue;
    case '-':
      lex_next(ls);
      if (ls->c != '-') return '-';
      lex_next(ls);
      if (ls->c == '[') {  /* Long comment "--[=*[...]=*]". */
	int sep = lex_skipeq(ls);
	lj_buf_reset(&ls->sb);  /* `lex_skipeq' may dirty the buffer */
	if (sep >= 0) {
	  lex_longstring(ls, NULL, sep);
	  lj_buf_reset(&ls->sb);
	  continue;
	}
      }
      /* Short comment "--.*\n". */
      while (!lex_iseol(ls) && ls->c != LEX_EOF)
	lex_next(ls);
      continue;
    case '[': {
      int sep = lex_skipeq(ls);
      if (sep >= 0) {
	lex_longstring(ls, tv, sep);
	return TK_string;
      } else if (sep == -1) {
	return '[';
      } else {
	lj_lex_error(ls, TK_string, LJ_ERR_XLDELIM);
	continue;
      }
      }
    case '=':
      lex_next(ls);
      if (ls->c != '=') return '='; else { lex_next(ls); return TK_eq; }
    case '<':
      lex_next(ls);
      if (LJ_54 && ls->c == '<') { lex_next(ls); return TK_shl; }
      if (ls->c != '=') return '<'; else { lex_next(ls); return TK_le; }
    case '>':
      lex_next(ls);
      if (LJ_54 && ls->c == '>') { lex_next(ls); return TK_shr; }
      if (ls->c != '=') return '>'; else { lex_next(ls); return TK_ge; }
    case '/':
      lex_next(ls);
      if (LJ_54 && ls->c == '/') { lex_next(ls); return TK_idiv; }
      return '/';
    case '~':
      lex_next(ls);
      if (ls->c != '=') return '~'; else { lex_next(ls); return TK_ne; }
    case ':':
      lex_next(ls);
      if (ls->c != ':') return ':'; else { lex_next(ls); return TK_label; }
    case '"':
    case '\'':
      lex_string(ls, tv);
      return TK_string;
    case '.':
      if (lex_savenext(ls) == '.') {
	lex_next(ls);
	if (ls->c == '.') {
	  lex_next(ls);
	  return TK_dots;   /* ... */
	}
	return TK_concat;   /* .. */
      } else if (!lj_char_isdigit(ls->c)) {
	return '.';
      } else {
	lex_number(ls, tv);
	return TK_number;
      }
    case LEX_EOF:
      return TK_eof;
    default: {
      LexChar c = ls->c;
      lex_next(ls);
      return c;  /* Single-char tokens (+ - / ...). */
    }
    }
  }
}

/* -- Lexer API ----------------------------------------------------------- */

/* Setup lexer state. */
int lj_lex_setup(lua_State *L, LexState *ls)
{
  int header = 0;
  ls->L = L;
  ls->fs = NULL;
  ls->pe = ls->p = NULL;
  ls->vstack = NULL;
  ls->sizevstack = 0;
  ls->vtop = 0;
  ls->bcstack = NULL;
  ls->sizebcstack = 0;
  ls->tok = 0;
  ls->lookahead = TK_eof;  /* No look-ahead token. */
  ls->linenumber = 1;
  ls->lastline = 1;
  ls->endmark = 0;
  ls->fr2 = LJ_FR2;  /* Generate native bytecode by default. */
  lex_next(ls);  /* Read-ahead first char. */
  if (ls->c == 0xef && ls->p + 2 <= ls->pe && (uint8_t)ls->p[0] == 0xbb &&
      (uint8_t)ls->p[1] == 0xbf) {  /* Skip UTF-8 BOM (if buffered). */
    ls->p += 2;
    lex_next(ls);
    header = 1;
  }
  if (ls->c == '#') {  /* Skip POSIX #! header line. */
    do {
      lex_next(ls);
      if (ls->c == LEX_EOF) return 0;
    } while (!lex_iseol(ls));
    lex_newline(ls);
    header = 1;
  }
  if (ls->c == LUA_SIGNATURE[0]) {  /* Bytecode dump. */
    if (header) {
      /*
      ** Loading bytecode with an extra header is disabled for security
      ** reasons. This may circumvent the usual check for bytecode vs.
      ** Lua code by looking at the first char. Since this is a potential
      ** security violation no attempt is made to echo the chunkname either.
      */
      setstrV(L, L->top++, lj_err_str(L, LJ_ERR_BCBAD));
      lj_err_throw(L, LUA_ERRSYNTAX);
    }
    return 1;
  }
  return 0;
}

/* Cleanup lexer state. */
void lj_lex_cleanup(lua_State *L, LexState *ls)
{
  global_State *g = G(L);
  lj_mem_freevec(g, ls->bcstack, ls->sizebcstack, BCInsLine);
  lj_mem_freevec(g, ls->vstack, ls->sizevstack, VarInfo);
  lj_buf_free(g, &ls->sb);
}

/* Return next lexical token. */
void lj_lex_next(LexState *ls)
{
  ls->lastline = ls->linenumber;
  if (LJ_LIKELY(ls->lookahead == TK_eof)) {  /* No lookahead token? */
    ls->tok = lex_scan(ls, &ls->tokval);  /* Get next token. */
  } else {  /* Otherwise return lookahead token. */
    ls->tok = ls->lookahead;
    ls->lookahead = TK_eof;
    ls->tokval = ls->lookaheadval;
  }
}

/* Look ahead for the next token. */
LexToken lj_lex_lookahead(LexState *ls)
{
  lj_assertLS(ls->lookahead == TK_eof, "double lookahead");
  ls->lookahead = lex_scan(ls, &ls->lookaheadval);
  return ls->lookahead;
}

/* Convert token to string. */
const char *lj_lex_token2str(LexState *ls, LexToken tok)
{
  if (tok > TK_OFS)
    return tokennames[tok-TK_OFS-1];
  else if (!lj_char_iscntrl(tok))
    return lj_strfmt_pushf(ls->L, "%c", tok);
  else
    return lj_strfmt_pushf(ls->L, "char(%d)", tok);
}

/* Lexer error. */
void lj_lex_error(LexState *ls, LexToken tok, ErrMsg em, ...)
{
  const char *tokstr;
  va_list argp;
  if (tok == 0) {
    tokstr = NULL;
  } else if (tok == TK_name || tok == TK_string || tok == TK_number) {
    lex_save(ls, '\0');
    tokstr = ls->sb.b;
  } else {
    tokstr = lj_lex_token2str(ls, tok);
  }
  va_start(argp, em);
  lj_err_lex(ls->L, ls->chunkname, tokstr, ls->linenumber, em, argp);
  va_end(argp);
}

/* Initialize strings for reserved words. */
void lj_lex_init(lua_State *L)
{
  uint32_t i;
  for (i = 0; i < TK_RESERVED; i++) {
    GCstr *s = lj_str_newz(L, tokennames[i]);
    fixstring(s);  /* Reserved words are never collected. */
    s->reserved = (uint8_t)(i+1);
  }
}

