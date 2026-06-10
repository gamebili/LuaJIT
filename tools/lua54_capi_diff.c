/*
** Differential C API probe: compile this same source twice, once against
** the official Lua 5.4.8 sources and once against the LuaJIT Lua 5.4
** compatibility build (-DLUAJIT_ENABLE_LUA54COMPAT, link src/lua51.dll),
** run both executables and diff the normalized output line-by-line.
**
** Output normalization rules:
**  - runs of >=8 hex digits (pointer text) are replaced by "ADDR"
**  - bytes outside 32..126 are escaped as \xNN
**  - GC byte counts and other legitimately divergent values are not printed
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static int g_probe_no = 0;

static void norm_write(const char *s, size_t len)
{
  size_t i = 0;
  while (i < len) {
    unsigned char c = (unsigned char)s[i];
    int ishex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F');
    if (ishex) {
      size_t j = i;
      while (j < len) {
        unsigned char d = (unsigned char)s[j];
        if (!((d >= '0' && d <= '9') || (d >= 'a' && d <= 'f') ||
              (d >= 'A' && d <= 'F')))
          break;
        j++;
      }
      if (j - i >= 8) {
        fputs("ADDR", stdout);
        i = j;
        continue;
      }
      while (i < j) { fputc(s[i], stdout); i++; }
      continue;
    }
    if (c < 32 || c > 126) {
      printf("\\x%02X", c);
    } else {
      fputc(c, stdout);
    }
    i++;
  }
}

static void emitf(const char *fmt, ...)
{
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  norm_write(buf, strlen(buf));
}

static void probe_head(const char *battery, const char *what)
{
  fflush(stdout);
  printf("\n[%s#%03d] %s | ", battery, ++g_probe_no, what);
}

static void dump_value(lua_State *L, int idx)
{
  int t = lua_type(L, idx);
  switch (t) {
  case LUA_TNONE: fputs("none", stdout); break;
  case LUA_TNIL: fputs("nil", stdout); break;
  case LUA_TBOOLEAN:
    fputs(lua_toboolean(L, idx) ? "true" : "false", stdout); break;
  case LUA_TNUMBER:
    /* numbers bypass norm_write: digit runs must not be pointer-mangled */
    if (lua_isinteger(L, idx)) {
      printf("int:%lld", (long long)lua_tointeger(L, idx));
    } else {
      lua_Number n = lua_tonumber(L, idx);
      if (isnan(n)) fputs("flt:nan", stdout);
      else if (isinf(n)) fputs(n > 0 ? "flt:inf" : "flt:-inf", stdout);
      else printf("flt:%.14g", n);
    }
    break;
  case LUA_TSTRING: {
    size_t len;
    const char *s = lua_tolstring(L, idx, &len);
    emitf("str(%u):\"", (unsigned)len);
    norm_write(s, len);
    fputs("\"", stdout);
    break;
  }
  case LUA_TTABLE: fputs("table", stdout); break;
  case LUA_TFUNCTION:
    fputs(lua_iscfunction(L, idx) ? "cfunction" : "function", stdout); break;
  case LUA_TUSERDATA: fputs("userdata", stdout); break;
  case LUA_TLIGHTUSERDATA: fputs("lightuserdata", stdout); break;
  case LUA_TTHREAD: fputs("thread", stdout); break;
  default: emitf("type%d", t); break;
  }
}

static void dump_stack(lua_State *L, int from)
{
  int top = lua_gettop(L);
  int i;
  emitf("stack[%d..%d]={", from, top);
  for (i = from; i <= top; i++) {
    if (i > from) fputs(",", stdout);
    dump_value(L, i);
  }
  fputs("}", stdout);
}

/* ---- value kinds ------------------------------------------------------- */

static volatile double v_zero = 0.0;

static int cf_dummy(lua_State *L) { (void)L; return 0; }

#define NKINDS 28

static const char *kind_name(int k)
{
  static const char *names[NKINDS] = {
    "nil", "false", "true", "int0", "int1", "intm1", "int2p31", "int2p40",
    "intmax", "intmin", "flt0", "flt1", "flt1_5", "fltm0", "fltinf",
    "fltminf", "fltnan", "flt2p40", "str_empty", "str_1", "str_1_5",
    "str_hex10", "str_sp2", "str_abc", "str_huge", "str_nul", "table",
    "cfunction"
  };
  return names[k];
}

static void push_kind(lua_State *L, int k)
{
  switch (k) {
  case 0: lua_pushnil(L); break;
  case 1: lua_pushboolean(L, 0); break;
  case 2: lua_pushboolean(L, 1); break;
  case 3: lua_pushinteger(L, 0); break;
  case 4: lua_pushinteger(L, 1); break;
  case 5: lua_pushinteger(L, -1); break;
  case 6: lua_pushinteger(L, (lua_Integer)2147483648LL); break;
  case 7: lua_pushinteger(L, (lua_Integer)1099511627776LL); break;
  case 8: lua_pushinteger(L, LUA_MAXINTEGER); break;
  case 9: lua_pushinteger(L, LUA_MININTEGER); break;
  case 10: lua_pushnumber(L, 0.0); break;
  case 11: lua_pushnumber(L, 1.0); break;
  case 12: lua_pushnumber(L, 1.5); break;
  case 13: lua_pushnumber(L, -v_zero); break;
  case 14: lua_pushnumber(L, HUGE_VAL); break;
  case 15: lua_pushnumber(L, -HUGE_VAL); break;
  case 16: lua_pushnumber(L, v_zero / v_zero); break;
  case 17: lua_pushnumber(L, 1099511627776.0); break;
  case 18: lua_pushliteral(L, ""); break;
  case 19: lua_pushliteral(L, "1"); break;
  case 20: lua_pushliteral(L, "1.5"); break;
  case 21: lua_pushliteral(L, "0x10"); break;
  case 22: lua_pushliteral(L, " 2 "); break;
  case 23: lua_pushliteral(L, "abc"); break;
  case 24: lua_pushliteral(L, "1e999"); break;
  case 25: lua_pushlstring(L, "a\0b", 3); break;
  case 26: lua_newtable(L); break;
  case 27: lua_pushcfunction(L, cf_dummy); break;
  default: lua_pushnil(L); break;
  }
}

/* run a C function under pcall and report status + result/error */
static void run_protected(lua_State *L, lua_CFunction f, int nargs)
{
  int status;
  lua_pushcfunction(L, f);
  if (nargs > 0) lua_insert(L, lua_gettop(L) - nargs);
  status = lua_pcall(L, nargs, LUA_MULTRET, 0);
  emitf("status=%d ", status);
  dump_stack(L, 1);
  lua_settop(L, 0);
}

/* ---- battery: type & conversion matrix --------------------------------- */

static void battery_typeconv(lua_State *L)
{
  int k;
  for (k = 0; k < NKINDS; k++) {
    int isnum, isint;
    lua_Number n;
    lua_Integer i;
    probe_head("typeconv", kind_name(k));
    push_kind(L, k);
    emitf("type=%s ", lua_typename(L, lua_type(L, -1)));
    emitf("isnumber=%d isinteger=%d isstring=%d iscfunction=%d "
          "isuserdata=%d toboolean=%d ",
          lua_isnumber(L, -1), lua_isinteger(L, -1), lua_isstring(L, -1),
          lua_iscfunction(L, -1), lua_isuserdata(L, -1),
          lua_toboolean(L, -1));
    n = lua_tonumberx(L, -1, &isnum);
    if (isnan(n)) emitf("tonumber=(nan,%d) ", isnum);
    else if (isinf(n)) emitf("tonumber=(%sinf,%d) ", n > 0 ? "" : "-", isnum);
    else emitf("tonumber=(%.14g,%d) ", n, isnum);
    i = lua_tointegerx(L, -1, &isint);
    emitf("tointeger=(%lld,%d) ", (long long)i, isint);
    emitf("rawlen=%llu", (unsigned long long)lua_rawlen(L, -1));
    lua_settop(L, 0);
  }
}

/* ---- battery: lua_tolstring in-place conversion ------------------------ */

static void battery_tolstring(lua_State *L)
{
  int k;
  for (k = 0; k < NKINDS; k++) {
    size_t len = 0;
    const char *s;
    probe_head("tolstring", kind_name(k));
    push_kind(L, k);
    s = lua_tolstring(L, -1, &len);
    if (s == NULL) {
      emitf("NULL posttype=%s", lua_typename(L, lua_type(L, -1)));
    } else {
      emitf("str(%u):\"", (unsigned)len);
      norm_write(s, len);
      emitf("\" posttype=%s", lua_typename(L, lua_type(L, -1)));
    }
    lua_settop(L, 0);
  }
}

/* ---- battery: lua_stringtonumber ---------------------------------------- */

static void battery_stringtonumber(lua_State *L)
{
  static const char *inputs[] = {
    "", "0", "1", "-1", " 42 ", "\t7\n", "0x10", "0X1P4", "1e2", "1E+2",
    "1.5", ".5", "5.", "-.0", "0x.8", "0xA.8p1", "1e999", "-1e999",
    "9223372036854775807", "9223372036854775808", "-9223372036854775808",
    "18446744073709551615", "0xffffffffffffffff", "0x10000000000000000",
    "inf", "nan", "INF", "0b101", "1,5", "1.5e", "0x", "  ", "1 2",
    "++1", "--1", "1.0.0", "0xg", "1e", "1e+", "00100", NULL
  };
  int i;
  for (i = 0; inputs[i]; i++) {
    size_t r;
    probe_head("str2num", "input");
    fputs("\"", stdout);
    norm_write(inputs[i], strlen(inputs[i]));
    fputs("\" -> ", stdout);
    r = lua_stringtonumber(L, inputs[i]);
    emitf("len=%u ", (unsigned)r);
    if (r != 0) { dump_value(L, -1); }
    lua_settop(L, 0);
  }
}

/* ---- battery: lua_arith -------------------------------------------------- */

static int g_arith_op = 0;
static int g_arith_ka = 0, g_arith_kb = 0;

static int cf_arith(lua_State *L)
{
  int unary = (g_arith_op == LUA_OPUNM || g_arith_op == LUA_OPBNOT);
  push_kind(L, g_arith_ka);
  if (!unary) push_kind(L, g_arith_kb);
  lua_arith(L, g_arith_op);
  return 1;
}

static void battery_arith(lua_State *L)
{
  static const int ops[] = {
    LUA_OPADD, LUA_OPSUB, LUA_OPMUL, LUA_OPMOD, LUA_OPPOW, LUA_OPDIV,
    LUA_OPIDIV, LUA_OPBAND, LUA_OPBOR, LUA_OPBXOR, LUA_OPSHL, LUA_OPSHR,
    LUA_OPUNM, LUA_OPBNOT
  };
  static const char *opn[] = {
    "add", "sub", "mul", "mod", "pow", "div", "idiv", "band", "bor",
    "bxor", "shl", "shr", "unm", "bnot"
  };
  /* operand kind subset that exercises int/float/string/coercion edges */
  static const int ka[] = {3, 4, 5, 8, 9, 11, 12, 16, 19, 20, 23, 0, 26};
  int oi, ai, bi;
  for (oi = 0; oi < (int)(sizeof(ops) / sizeof(ops[0])); oi++) {
    int unary = (ops[oi] == LUA_OPUNM || ops[oi] == LUA_OPBNOT);
    for (ai = 0; ai < (int)(sizeof(ka) / sizeof(ka[0])); ai++) {
      for (bi = 0; bi < (unary ? 1 : (int)(sizeof(ka) / sizeof(ka[0]))); bi++) {
        char what[96];
        g_arith_op = ops[oi];
        g_arith_ka = ka[ai];
        g_arith_kb = ka[bi];
        if (unary)
          snprintf(what, sizeof(what), "%s(%s)", opn[oi], kind_name(ka[ai]));
        else
          snprintf(what, sizeof(what), "%s(%s,%s)", opn[oi],
                   kind_name(ka[ai]), kind_name(ka[bi]));
        probe_head("arith", what);
        run_protected(L, cf_arith, 0);
      }
    }
  }
}

/* ---- battery: lua_compare ------------------------------------------------ */

static int g_cmp_op = 0, g_cmp_ka = 0, g_cmp_kb = 0;

static int cf_compare(lua_State *L)
{
  int r;
  push_kind(L, g_cmp_ka);
  push_kind(L, g_cmp_kb);
  r = lua_compare(L, -2, -1, g_cmp_op);
  lua_pushboolean(L, r);
  return 1;
}

static void battery_compare(lua_State *L)
{
  static const int ops[] = {LUA_OPEQ, LUA_OPLT, LUA_OPLE};
  static const char *opn[] = {"eq", "lt", "le"};
  static const int kk[] = {3, 4, 8, 9, 11, 12, 16, 17, 7, 19, 23, 0, 1, 26};
  int oi, ai, bi;
  for (oi = 0; oi < 3; oi++) {
    for (ai = 0; ai < (int)(sizeof(kk) / sizeof(kk[0])); ai++) {
      for (bi = 0; bi < (int)(sizeof(kk) / sizeof(kk[0])); bi++) {
        char what[96];
        g_cmp_op = ops[oi];
        g_cmp_ka = kk[ai];
        g_cmp_kb = kk[bi];
        snprintf(what, sizeof(what), "%s(%s,%s)", opn[oi],
                 kind_name(kk[ai]), kind_name(kk[bi]));
        probe_head("compare", what);
        run_protected(L, cf_compare, 0);
      }
    }
  }
}

/* ---- battery: lauxlib argument checks ----------------------------------- */

static int g_check_fn = 0, g_check_kind = 0;

static int cf_check(lua_State *L)
{
  push_kind(L, g_check_kind);
  switch (g_check_fn) {
  case 0: lua_pushinteger(L, luaL_checkinteger(L, 1)); break;
  case 1: lua_pushnumber(L, luaL_checknumber(L, 1)); break;
  case 2: lua_pushstring(L, luaL_checkstring(L, 1)); break;
  case 3: luaL_checktype(L, 1, LUA_TTABLE); lua_pushboolean(L, 1); break;
  case 4: luaL_checkany(L, 1); lua_pushboolean(L, 1); break;
  case 5: lua_pushinteger(L, luaL_optinteger(L, 1, 77)); break;
  case 6: lua_pushstring(L, luaL_optstring(L, 1, "def")); break;
  case 7: luaL_checkudata(L, 1, "DiffProbeMeta"); lua_pushboolean(L, 1); break;
  case 8: {
    static const char *const opts[] = {"alpha", "beta", NULL};
    lua_pushinteger(L, luaL_checkoption(L, 1, "alpha", opts));
    break;
  }
  default: lua_pushnil(L); break;
  }
  return 1;
}

static void battery_lauxchecks(lua_State *L)
{
  static const char *fname[] = {
    "checkinteger", "checknumber", "checkstring", "checktype_table",
    "checkany", "optinteger", "optstring", "checkudata", "checkoption"
  };
  int f, k;
  luaL_newmetatable(L, "DiffProbeMeta");
  lua_pop(L, 1);
  for (f = 0; f < 9; f++) {
    for (k = 0; k < NKINDS; k++) {
      char what[80];
      g_check_fn = f;
      g_check_kind = k;
      snprintf(what, sizeof(what), "%s(%s)", fname[f], kind_name(k));
      probe_head("lauxcheck", what);
      run_protected(L, cf_check, 0);
    }
  }
}

/* ---- battery: lua_pushfstring -------------------------------------------- */

static void battery_pushfstring(lua_State *L)
{
  probe_head("pushf", "pct_d");
  lua_pushfstring(L, "<%d|%d|%d>", 0, -2147483647 - 1, 2147483647);
  dump_value(L, -1); lua_settop(L, 0);

  probe_head("pushf", "pct_I");
  lua_pushfstring(L, "<%I|%I|%I>", (LUAI_UACINT)LUA_MAXINTEGER,
                  (LUAI_UACINT)LUA_MININTEGER, (LUAI_UACINT)0);
  dump_value(L, -1); lua_settop(L, 0);

  probe_head("pushf", "pct_f");
  lua_pushfstring(L, "<%f|%f|%f>", (LUAI_UACNUMBER)1.0,
                  (LUAI_UACNUMBER)-0.5, (LUAI_UACNUMBER)1099511627776.0);
  dump_value(L, -1); lua_settop(L, 0);

  probe_head("pushf", "pct_s_null");
  lua_pushfstring(L, "<%s>", (const char *)NULL);
  dump_value(L, -1); lua_settop(L, 0);

  probe_head("pushf", "pct_c");
  lua_pushfstring(L, "<%c|%c>", 'A', 'z');
  dump_value(L, -1); lua_settop(L, 0);

  probe_head("pushf", "pct_U");
  lua_pushfstring(L, "<%U|%U|%U>", (long)0x41, (long)0x4E2D, (long)0x1F600);
  dump_value(L, -1); lua_settop(L, 0);

  probe_head("pushf", "pct_pct");
  lua_pushfstring(L, "<%%|%%d>");
  dump_value(L, -1); lua_settop(L, 0);

  probe_head("pushf", "pct_p_normalized");
  lua_pushfstring(L, "<%p>", (void *)L);
  /* pointer text normalizes to ADDR; only shape compared */
  dump_value(L, -1); lua_settop(L, 0);
}

/* ---- battery: table get/set with wide keys and metamethods --------------- */

static int cf_meta_index(lua_State *L)
{
  lua_pushliteral(L, "from-index");
  return 1;
}

static void battery_tableops(lua_State *L)
{
  static const lua_Integer keys[] = {
    0, 1, -1, 2147483647LL, 2147483648LL, -2147483648LL, -2147483649LL,
    1099511627776LL, LUA_MAXINTEGER, LUA_MININTEGER
  };
  int i;
  for (i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++) {
    char what[64];
    int t;
    snprintf(what, sizeof(what), "seti_geti_%lld", (long long)keys[i]);
    probe_head("tableops", what);
    lua_newtable(L);
    lua_pushinteger(L, 1000 + i);
    lua_seti(L, -2, keys[i]);
    t = lua_geti(L, -1, keys[i]);
    emitf("rettype=%s val=", lua_typename(L, t));
    dump_value(L, -1);
    lua_pop(L, 1);
    t = lua_rawgeti(L, -1, keys[i]);
    emitf(" rawtype=%s rawval=", lua_typename(L, t));
    dump_value(L, -1);
    lua_settop(L, 0);
  }

  probe_head("tableops", "geti_via_meta_index");
  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, cf_meta_index);
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);
  {
    int t = lua_geti(L, -1, 5);
    emitf("rettype=%s val=", lua_typename(L, t));
    dump_value(L, -1);
  }
  lua_settop(L, 0);

  probe_head("tableops", "getfield_missing");
  lua_newtable(L);
  {
    int t = lua_getfield(L, -1, "nope");
    emitf("rettype=%s ", lua_typename(L, t));
    dump_value(L, -1);
  }
  lua_settop(L, 0);
}

/* ---- battery: rawset edge keys ------------------------------------------- */

static int g_rawkey_kind = 0;

static int cf_rawset_edge(lua_State *L)
{
  lua_newtable(L);
  push_kind(L, g_rawkey_kind);
  lua_pushboolean(L, 1);
  lua_rawset(L, -3);
  lua_pushliteral(L, "ok");
  return 1;
}

static void battery_rawset_edge(lua_State *L)
{
  static const int kk[] = {0, 16, 11, 12, 13, 17};
  int i;
  for (i = 0; i < (int)(sizeof(kk) / sizeof(kk[0])); i++) {
    char what[64];
    g_rawkey_kind = kk[i];
    snprintf(what, sizeof(what), "rawset_key_%s", kind_name(kk[i]));
    probe_head("rawset", what);
    run_protected(L, cf_rawset_edge, 0);
  }
}

/* ---- battery: luaL_tolstring with metamethods ---------------------------- */

static int cf_tostring_str(lua_State *L)
{
  lua_pushliteral(L, "custom-tostring");
  return 1;
}

static int cf_tostring_num(lua_State *L)
{
  lua_pushinteger(L, 42);
  return 1;
}

static void battery_lualtolstring(lua_State *L)
{
  int k;
  size_t len;
  const char *s;
  for (k = 0; k < NKINDS; k++) {
    probe_head("lualtolstr", kind_name(k));
    push_kind(L, k);
    s = luaL_tolstring(L, -1, &len);
    emitf("str(%u):\"", (unsigned)len);
    norm_write(s, len);
    fputs("\"", stdout);
    lua_settop(L, 0);
  }

  probe_head("lualtolstr", "table_with_name");
  lua_newtable(L);
  lua_newtable(L);
  lua_pushliteral(L, "MyThing");
  lua_setfield(L, -2, "__name");
  lua_setmetatable(L, -2);
  s = luaL_tolstring(L, -1, &len);
  emitf("str(%u):\"", (unsigned)len);
  norm_write(s, len);
  fputs("\"", stdout);
  lua_settop(L, 0);

  probe_head("lualtolstr", "table_with_tostring_str");
  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, cf_tostring_str);
  lua_setfield(L, -2, "__tostring");
  lua_setmetatable(L, -2);
  s = luaL_tolstring(L, -1, &len);
  emitf("str(%u):\"", (unsigned)len);
  norm_write(s, len);
  fputs("\"", stdout);
  lua_settop(L, 0);

  probe_head("lualtolstr", "table_with_tostring_num");
  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, cf_tostring_num);
  lua_setfield(L, -2, "__tostring");
  lua_setmetatable(L, -2);
  s = luaL_tolstring(L, -1, &len);
  emitf("str(%u):\"", (unsigned)len);
  norm_write(s, len);
  fputs("\"", stdout);
  lua_settop(L, 0);
}

/* ---- battery: lua_concat -------------------------------------------------- */

static int g_concat_n = 0;
static int g_concat_kinds[4];

static int cf_concat(lua_State *L)
{
  int i;
  for (i = 0; i < g_concat_n; i++) push_kind(L, g_concat_kinds[i]);
  lua_concat(L, g_concat_n);
  return 1;
}

static void battery_concat(lua_State *L)
{
  static const int duos[][2] = {
    {19, 23}, {4, 4}, {11, 19}, {12, 12}, {7, 18}, {23, 26}, {0, 19},
    {16, 19}, {14, 18}
  };
  int i;
  probe_head("concat", "n0");
  g_concat_n = 0;
  run_protected(L, cf_concat, 0);

  probe_head("concat", "n1_table");
  g_concat_n = 1;
  g_concat_kinds[0] = 26;
  run_protected(L, cf_concat, 0);

  for (i = 0; i < (int)(sizeof(duos) / sizeof(duos[0])); i++) {
    char what[64];
    g_concat_n = 2;
    g_concat_kinds[0] = duos[i][0];
    g_concat_kinds[1] = duos[i][1];
    snprintf(what, sizeof(what), "n2(%s,%s)", kind_name(duos[i][0]),
             kind_name(duos[i][1]));
    probe_head("concat", what);
    run_protected(L, cf_concat, 0);
  }
}

/* ---- battery: lua_len / luaL_len ------------------------------------------ */

static int g_len_mode = 0;

static int cf_badlen(lua_State *L)
{
  lua_pushliteral(L, "not-a-number");
  return 1;
}

static int cf_fraclen(lua_State *L)
{
  lua_pushnumber(L, 2.5);
  return 1;
}

static int cf_len(lua_State *L)
{
  switch (g_len_mode) {
  case 0: lua_pushliteral(L, "hello"); break;
  case 1:
    lua_createtable(L, 3, 0);
    lua_pushinteger(L, 1); lua_rawseti(L, -2, 1);
    lua_pushinteger(L, 2); lua_rawseti(L, -2, 2);
    lua_pushinteger(L, 3); lua_rawseti(L, -2, 3);
    break;
  case 2: case 3: case 4:
    lua_newtable(L);
    lua_newtable(L);
    if (g_len_mode == 2) lua_pushcfunction(L, cf_tostring_num);
    else if (g_len_mode == 3) lua_pushcfunction(L, cf_badlen);
    else lua_pushcfunction(L, cf_fraclen);
    lua_setfield(L, -2, "__len");
    lua_setmetatable(L, -2);
    break;
  case 5: lua_pushinteger(L, 9); break;
  }
  if (g_len_mode >= 100) { /* luaL_len variants */
    g_len_mode -= 100;
    cf_len(L);
    lua_pushinteger(L, luaL_len(L, -1));
    return 1;
  }
  lua_len(L, -1);
  return 1;
}

static void battery_len(lua_State *L)
{
  static const char *what[] = {
    "lua_len_string", "lua_len_table", "lua_len_meta_int",
    "lua_len_meta_str", "lua_len_meta_frac", "lua_len_int"
  };
  int m;
  for (m = 0; m < 6; m++) {
    g_len_mode = m;
    probe_head("len", what[m]);
    run_protected(L, cf_len, 0);
  }
  for (m = 0; m < 6; m++) {
    char w[64];
    g_len_mode = 100 + m;
    snprintf(w, sizeof(w), "luaL_%s", what[m] + 4);
    probe_head("len", w);
    run_protected(L, cf_len, 0);
  }
}

/* ---- battery: luaL_ref / luaL_unref --------------------------------------- */

static void battery_ref(lua_State *L)
{
  int r[6], i;
  lua_newtable(L); /* private ref table at index 1 */
  probe_head("ref", "sequence");
  for (i = 0; i < 4; i++) {
    lua_pushinteger(L, 100 + i);
    r[i] = luaL_ref(L, 1);
    emitf("ref%d=%d ", i, r[i]);
  }
  luaL_unref(L, 1, r[1]);
  luaL_unref(L, 1, r[2]);
  lua_pushinteger(L, 200);
  r[4] = luaL_ref(L, 1);
  lua_pushinteger(L, 201);
  r[5] = luaL_ref(L, 1);
  emitf("reref1=%d reref2=%d ", r[4], r[5]);
  lua_pushnil(L);
  emitf("refnil=%d ", luaL_ref(L, 1));
  emitf("noref=%d refnilconst=%d", LUA_NOREF, LUA_REFNIL);
  lua_settop(L, 0);
}

/* ---- battery: userdata & uservalues --------------------------------------- */

static int cf_uservalue(lua_State *L)
{
  int t;
  lua_newuserdatauv(L, 16, 2);  /* userdata sits at absolute index 1 */
  emitf("rawlen=%llu ", (unsigned long long)lua_rawlen(L, 1));
  t = lua_getiuservalue(L, 1, 1);
  emitf("get1=%d/", t); dump_value(L, -1); lua_pop(L, 1);
  t = lua_getiuservalue(L, 1, 2);
  emitf(" get2=%d/", t); dump_value(L, -1); lua_pop(L, 1);
  t = lua_getiuservalue(L, 1, 3);
  emitf(" get3=%d/", t); dump_value(L, -1); lua_pop(L, 1);
  t = lua_getiuservalue(L, 1, 0);
  emitf(" get0=%d/", t); dump_value(L, -1); lua_pop(L, 1);
  lua_pushliteral(L, "uv1");
  emitf(" set1=%d", lua_setiuservalue(L, 1, 1));
  lua_pushliteral(L, "uv3");
  emitf(" set3=%d", lua_setiuservalue(L, 1, 3));
  t = lua_getiuservalue(L, 1, 1);
  emitf(" reget1=%d/", t); dump_value(L, -1); lua_pop(L, 1);
  lua_pushliteral(L, "done");
  return 1;
}

static void battery_uservalue(lua_State *L)
{
  probe_head("uservalue", "newuserdatauv_2");
  run_protected(L, cf_uservalue, 0);
}

/* ---- battery: luaL_Buffer --------------------------------------------------- */

static int cf_buffer(lua_State *L)
{
  luaL_Buffer b;
  char *p;
  luaL_buffinit(L, &b);
  luaL_addchar(&b, 'x');
  luaL_addlstring(&b, "a\0b", 3);
  luaL_addstring(&b, "tail");
  lua_pushinteger(L, 42);
  luaL_addvalue(&b);
  lua_pushnumber(L, 1.0);
  luaL_addvalue(&b);
  p = luaL_prepbuffsize(&b, 3000);
  memset(p, 'z', 3000);
  luaL_addsize(&b, 3000);
  luaL_pushresult(&b);
  lua_pushinteger(L, (lua_Integer)lua_rawlen(L, -1));
  return 2;
}

static void battery_buffer(lua_State *L)
{
  size_t len;
  const char *s;
  probe_head("buffer", "mixed_adds_len_only");
  lua_pushcfunction(L, cf_buffer);
  if (lua_pcall(L, 0, 2, 0) != LUA_OK) {
    emitf("ERR "); dump_value(L, -1);
  } else {
    s = lua_tolstring(L, -2, &len);
    emitf("len=%u head=\"", (unsigned)len);
    norm_write(s, len > 12 ? 12 : len);
    emitf("\" total=");
    dump_value(L, -1);
  }
  lua_settop(L, 0);
}

/* ---- battery: luaL_gsub ----------------------------------------------------- */

static void battery_gsub(lua_State *L)
{
  static const char *cases[][3] = {
    {"hello world", "o", "0"},
    {"aaa", "aa", "b"},
    {"abc", "x", "y"},
    /* empty pattern omitted: official luaL_gsub loops forever on it */
    {"a.b.c", ".", ".."},
  };
  int i;
  for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
    probe_head("gsub", cases[i][0]);
    luaL_gsub(L, cases[i][0], cases[i][1], cases[i][2]);
    dump_value(L, -1);
    lua_settop(L, 0);
  }
}

/* ---- battery: load / syntax errors ------------------------------------------ */

static void battery_load(lua_State *L)
{
  static const char *chunks[] = {
    "return 1 + 2",
    "return",
    "x =",
    "return 1 +",
    "local 1bad = 3",
    "if then end",
    "return 'unfinished",
    "goto nowhere",
    "::a:: ::a::",
    "local x <close> = nil return 1",
    "local x <wat> = 1",
    "break",
    "return ...",
    NULL
  };
  int i;
  for (i = 0; chunks[i]; i++) {
    int st;
    probe_head("load", "chunk");
    fputs("\"", stdout);
    norm_write(chunks[i], strlen(chunks[i]));
    fputs("\" -> ", stdout);
    st = luaL_loadbufferx(L, chunks[i], strlen(chunks[i]), "=probe", "t");
    emitf("status=%d ", st);
    if (st != LUA_OK) dump_value(L, -1);
    else fputs("ok", stdout);
    lua_settop(L, 0);
  }

  probe_head("load", "mode_b_rejects_text");
  {
    const char *c = "return 1";
    int st = luaL_loadbufferx(L, c, strlen(c), "=probe", "b");
    emitf("status=%d ", st);
    dump_value(L, -1);
    lua_settop(L, 0);
  }
}

/* ---- battery: runtime error texts via dostring -------------------------------- */

static void battery_runerr(lua_State *L)
{
  static const char *chunks[] = {
    "local t = nil; return t.x",
    "local t = nil; t.x = 1",
    "return 1 + {}",
    "return #nil",
    "return -'abc'",
    "local t = setmetatable({}, {__index = function() error('boom') end}); return t.k",
    "error()",
    "error(nil)",
    "error({code = 1})",
    "error('msg', 0)",
    "local a, b = 1; return a < 'x'",
    "return ('x') .. nil",
    "local t = {} t[nil] = 1",
    "local t = {} t[0/0] = 1",
    "return math.floor('abc')",
    "return string.rep('x', -1)",
    "return ('abc'):bad()",
    NULL
  };
  int i;
  for (i = 0; chunks[i]; i++) {
    int st;
    probe_head("runerr", "chunk");
    fputs("\"", stdout);
    norm_write(chunks[i], strlen(chunks[i]));
    fputs("\" -> ", stdout);
    st = luaL_loadbufferx(L, chunks[i], strlen(chunks[i]), "=probe", "t");
    if (st == LUA_OK) st = lua_pcall(L, 0, LUA_MULTRET, 0);
    emitf("status=%d ", st);
    dump_stack(L, 1);
    lua_settop(L, 0);
  }
}

/* ---- battery: coroutine C API -------------------------------------------------- */

static int cf_co_body_yield(lua_State *L)
{
  emitf("[inbody_yieldable=%d]", lua_isyieldable(L));
  lua_pushliteral(L, "y1");
  lua_pushliteral(L, "y2");
  return lua_yield(L, 2);
}

static int cf_co_body_plain(lua_State *L)
{
  lua_pushliteral(L, "ret");
  return 1;
}

static void battery_coroutine(lua_State *L)
{
  lua_State *co;
  int st, nres = -1;

  probe_head("coro", "yield_then_dead");
  co = lua_newthread(L);
  lua_pushcfunction(co, cf_co_body_yield);
  st = lua_resume(co, L, 0, &nres);
  emitf("resume1=(%d,%d) ", st, nres);
  emitf("status=%d isyieldable=%d ", lua_status(co), lua_isyieldable(co));
  dump_stack(co, 1);
  lua_settop(co, 0);
  st = lua_resume(co, L, 0, &nres);
  emitf(" resume2=(%d,%d) ", st, nres);
  dump_stack(co, 1);
  st = lua_resume(co, L, 0, &nres);
  emitf(" resume3_dead=(%d,%d) ", st, nres);
  if (lua_gettop(co) > 0) dump_value(co, -1);
  lua_settop(L, 0);

  probe_head("coro", "lua_body_error");
  co = lua_newthread(L);
  luaL_loadstring(co, "error('inner')");
  st = lua_resume(co, L, 0, &nres);
  /* On error the defined surface is the status and the error object on the
  ** stack top; official leaves additional unspecified unwind residue below
  ** it (and counts it in nresults), so neither is compared here. */
  emitf("resume=%d top_err=", st);
  dump_value(co, -1);
  emitf(" status_after=%d", lua_status(co));
  st = lua_resetthread(co);
  emitf(" reset=%d", st);
  lua_settop(L, 0);

  probe_head("coro", "closethread_fresh");
  co = lua_newthread(L);
  st = lua_closethread(co, L);
  emitf("close=%d status=%d", st, lua_status(co));
  lua_settop(L, 0);

  probe_head("coro", "main_isyieldable");
  emitf("main=%d pushthread_ismain=%d", lua_isyieldable(L),
        lua_pushthread(L));
  lua_settop(L, 0);

  probe_head("coro", "isyieldable_states");
  co = lua_newthread(L);
  emitf("fresh=%d ", lua_isyieldable(co));
  lua_pushcfunction(co, cf_co_body_plain);
  st = lua_resume(co, L, 0, &nres);
  emitf("after_plain=(%d) dead_ok=%d ", st, lua_isyieldable(co));
  co = lua_newthread(L);
  luaL_loadstring(co, "error('x')");
  st = lua_resume(co, L, 0, &nres);
  emitf("after_err=(%d) dead_err=%d", st, lua_isyieldable(co));
  lua_settop(L, 0);
}

/* ---- battery: stack shape ops ---------------------------------------------------- */

static void battery_stackops(lua_State *L)
{
  int i;
  probe_head("stackops", "rotate_pos2");
  for (i = 1; i <= 5; i++) lua_pushinteger(L, i);
  lua_rotate(L, 1, 2);
  dump_stack(L, 1);
  lua_settop(L, 0);

  probe_head("stackops", "rotate_neg2");
  for (i = 1; i <= 5; i++) lua_pushinteger(L, i);
  lua_rotate(L, 1, -2);
  dump_stack(L, 1);
  lua_settop(L, 0);

  probe_head("stackops", "rotate_sub_window");
  for (i = 1; i <= 6; i++) lua_pushinteger(L, i);
  lua_rotate(L, 3, 1);
  dump_stack(L, 1);
  lua_settop(L, 0);

  probe_head("stackops", "insert_remove_replace_copy");
  for (i = 1; i <= 4; i++) lua_pushinteger(L, i);
  lua_pushinteger(L, 99);
  lua_insert(L, 2);
  dump_stack(L, 1);
  lua_remove(L, 3);
  fputs(" after_remove:", stdout);
  dump_stack(L, 1);
  lua_pushinteger(L, 777);
  lua_replace(L, 1);
  fputs(" after_replace:", stdout);
  dump_stack(L, 1);
  lua_copy(L, 1, 4);
  fputs(" after_copy:", stdout);
  dump_stack(L, 1);
  lua_settop(L, 0);

  probe_head("stackops", "absindex");
  lua_pushinteger(L, 1);
  lua_pushinteger(L, 2);
  /* abs(0) omitted: official computes a garbage index, compat raises a
  ** deliberate release "invalid value" guard (documented divergence). */
  emitf("abs(-1)=%d abs(-2)=%d abs(1)=%d abs(REGISTRY)=%d",
        lua_absindex(L, -1), lua_absindex(L, -2), lua_absindex(L, 1),
        lua_absindex(L, LUA_REGISTRYINDEX));
  lua_settop(L, 0);

  probe_head("stackops", "settop_extend_fills_nil");
  lua_pushinteger(L, 1);
  lua_settop(L, 4);
  dump_stack(L, 1);
  lua_settop(L, 0);
}

/* ---- battery: globals via registry ----------------------------------------------- */

static void battery_globals(lua_State *L)
{
  int t;
  probe_head("globals", "setglobal_getglobal");
  lua_pushinteger(L, 31337);
  lua_setglobal(L, "diffprobe_g");
  t = lua_getglobal(L, "diffprobe_g");
  emitf("rettype=%s val=", lua_typename(L, t));
  dump_value(L, -1);
  lua_settop(L, 0);

  probe_head("globals", "pushglobaltable_is_registry_globals");
  lua_pushglobaltable(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  emitf("same=%d", lua_rawequal(L, -1, -2));
  lua_settop(L, 0);

  probe_head("globals", "registry_mainthread");
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  emitf("type=%s", lua_typename(L, lua_type(L, -1)));
  lua_settop(L, 0);
}

/* ---- battery: lua_next over array part -------------------------------------------- */

static void battery_next(lua_State *L)
{
  probe_head("next", "array_inorder");
  lua_createtable(L, 3, 0);
  lua_pushliteral(L, "a"); lua_rawseti(L, -2, 1);
  lua_pushliteral(L, "b"); lua_rawseti(L, -2, 2);
  lua_pushliteral(L, "c"); lua_rawseti(L, -2, 3);
  lua_pushnil(L);
  while (lua_next(L, 1) != 0) {
    fputs("(", stdout); dump_value(L, -2);
    fputs("=", stdout); dump_value(L, -1);
    fputs(")", stdout);
    lua_pop(L, 1);
  }
  lua_settop(L, 0);

  probe_head("next", "empty");
  lua_newtable(L);
  lua_pushnil(L);
  emitf("next=%d", lua_next(L, 1));
  lua_settop(L, 0);
}

/* ---- battery: warning capture ------------------------------------------------------ */

static char g_warnbuf[512];
static int g_warnlen = 0;

static void warn_cb(void *ud, const char *msg, int tocont)
{
  (void)ud;
  g_warnlen += snprintf(g_warnbuf + g_warnlen,
                        sizeof(g_warnbuf) - (size_t)g_warnlen,
                        "[%s|%d]", msg, tocont);
}

static void battery_warning(lua_State *L)
{
  probe_head("warn", "fragments_and_control");
  g_warnlen = 0;
  g_warnbuf[0] = '\0';
  lua_setwarnf(L, warn_cb, NULL);
  lua_warning(L, "part1 ", 1);
  lua_warning(L, "part2", 0);
  lua_warning(L, "@control", 0);
  lua_warning(L, "single", 0);
  norm_write(g_warnbuf, strlen(g_warnbuf));
  lua_setwarnf(L, NULL, NULL);
  lua_settop(L, 0);
}

/* ---- battery: misc state queries ----------------------------------------------------- */

static void battery_misc(lua_State *L)
{
  probe_head("misc", "version_status_gcrunning");
  emitf("version=%.0f status=%d ", lua_version(L), lua_status(L));
  emitf("gcrunning=%d ", lua_gc(L, LUA_GCISRUNNING));
  emitf("stop=%d running_after_stop=%d ", lua_gc(L, LUA_GCSTOP),
        lua_gc(L, LUA_GCISRUNNING));
  emitf("restart=%d running_after_restart=%d", lua_gc(L, LUA_GCRESTART),
        lua_gc(L, LUA_GCISRUNNING));
  lua_settop(L, 0);

  probe_head("misc", "checkstack");
  emitf("grow100=%d grow_huge=%d neg_handled_by_release_guard=skipped",
        lua_checkstack(L, 100), lua_checkstack(L, 1000000000));
  lua_settop(L, 0);

  probe_head("misc", "rawequal_nan_and_strings");
  lua_pushnumber(L, v_zero / v_zero);
  lua_pushnumber(L, v_zero / v_zero);
  lua_pushliteral(L, "xyz");
  lua_pushliteral(L, "xyz");
  lua_pushinteger(L, 3);
  lua_pushnumber(L, 3.0);
  emitf("nan_nan=%d str_str=%d int_flt=%d",
        lua_rawequal(L, 1, 2), lua_rawequal(L, 3, 4), lua_rawequal(L, 5, 6));
  lua_settop(L, 0);

  probe_head("misc", "getmetatable_default");
  lua_pushliteral(L, "s");
  emitf("string_has_mt=%d ", lua_getmetatable(L, -1));
  lua_settop(L, 0);
  lua_newtable(L);
  emitf("table_has_mt=%d", lua_getmetatable(L, -1));
  lua_settop(L, 0);

  probe_head("misc", "luaL_typename_each_kind");
  {
    int k;
    for (k = 0; k < NKINDS; k++) {
      push_kind(L, k);
      emitf("%s=%s ", kind_name(k), luaL_typename(L, -1));
      lua_settop(L, 0);
    }
  }
}

/* ---- battery: luaL_getmetafield / luaL_callmeta --------------------------------------- */

static void battery_metafield(lua_State *L)
{
  int t;
  probe_head("metafield", "present_absent");
  lua_newtable(L);
  lua_newtable(L);
  lua_pushinteger(L, 7);
  lua_setfield(L, -2, "__custom");
  lua_setmetatable(L, -2);
  t = luaL_getmetafield(L, 1, "__custom");
  emitf("custom=%s/", lua_typename(L, t));
  dump_value(L, -1); lua_pop(L, 1);
  t = luaL_getmetafield(L, 1, "__missing");
  emitf(" missing=%s top_unchanged=%d", lua_typename(L, t),
        lua_gettop(L) == 1);
  lua_settop(L, 0);

  probe_head("metafield", "callmeta_tostring");
  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, cf_tostring_str);
  lua_setfield(L, -2, "__tostring");
  lua_setmetatable(L, -2);
  emitf("ret=%d val=", luaL_callmeta(L, 1, "__tostring"));
  dump_value(L, -1);
  lua_settop(L, 0);
}

/* ---- battery: traceback shape ----------------------------------------------------------- */

static void battery_traceback(lua_State *L)
{
  probe_head("traceback", "lua_level1");
  if (luaL_dostring(L,
        "function diffprobe_tb()\n"
        "  local function inner() error('tberr') end\n"
        "  inner()\n"
        "end") != LUA_OK) {
    emitf("setup_err="); dump_value(L, -1); lua_settop(L, 0); return;
  }
  lua_getglobal(L, "diffprobe_tb");
  if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
    size_t len;
    const char *msg = lua_tolstring(L, -1, &len);
    luaL_traceback(L, L, msg, 0);
    dump_value(L, -1);
  }
  lua_settop(L, 0);
}

int main(void)
{
  lua_State *L;
  if (getenv("CAPI_DIFF_UNBUFFERED")) setvbuf(stdout, NULL, _IONBF, 0);
  L = luaL_newstate();
  if (L == NULL) { fputs("newstate failed\n", stderr); return 1; }
  luaL_openlibs(L);
  luaL_checkversion(L);

  battery_typeconv(L);
  battery_tolstring(L);
  battery_stringtonumber(L);
  battery_arith(L);
  battery_compare(L);
  battery_lauxchecks(L);
  battery_pushfstring(L);
  battery_tableops(L);
  battery_rawset_edge(L);
  battery_lualtolstring(L);
  battery_concat(L);
  battery_len(L);
  battery_ref(L);
  battery_uservalue(L);
  battery_buffer(L);
  battery_gsub(L);
  battery_load(L);
  battery_runerr(L);
  battery_coroutine(L);
  battery_stackops(L);
  battery_globals(L);
  battery_next(L);
  battery_warning(L);
  battery_misc(L);
  battery_metafield(L);
  battery_traceback(L);

  lua_close(L);
  printf("\nCAPI-DIFF-DONE probes=%d\n", g_probe_no);
  return 0;
}
