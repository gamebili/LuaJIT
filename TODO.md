# Lua 5.4 Compatibility TODO

本清单基于当前 `LUAJIT_ENABLE_LUA54COMPAT` 实现、`test/smoke.lua`
覆盖范围，以及和本机 `lua5.4.8` 的行为探针对比。后续继续实现时仍按
“先写用例，再补实现，再跑完整测试”的顺序推进。

## P0：核心语义缺口

- [ ] `<close>` 的运行期 `__close` 调度。
  - 当前状态：只接受 `local x <close>` 语法并记录属性，不会在离开作用域时调用 `__close`。
  - 需要补测试：正常块退出、`return`、`break`、`goto`、错误展开、`pcall`/`xpcall`、协程关闭、多个待关闭变量的 LIFO 顺序、`__close` 接收错误对象、`__close` 自身抛错、`nil`/`false` 跳过关闭、非 closable 值在声明点报错、generic for 的 closing value、`io.lines(filename)` 返回 4 个值并在迭代结束自动关闭文件。
  - 实现重点：需要 VM/字节码/栈帧层支持作用域退出和错误展开时的关闭流程，不能只在解析器层处理。

- [ ] `_ENV` 的完整 upvalue 语义。
  - 当前状态：显式 `local _ENV = ...` 和 `load(..., env)` 的基础访问已可用，但隐式全局访问没有暴露为名为 `_ENV` 的第一个 upvalue。
  - 已知差异：`debug.getupvalue(assert(load("return x")), 1)` 当前返回 `nil`；Lua 5.4 返回 `_ENV, _G`。当前还把 `_ENV` 写进了 `_G` 表，`rawget(_G, "_ENV") == _G`，而 Lua 5.4 官方环境里 `rawget(_G, "_ENV") == nil`。
  - 需要补测试：chunk 的 `_ENV` upvalue 名称/位置、`debug.setupvalue` 替换环境、闭包继承局部 `_ENV`、`_G` 表中不应额外暴露 `_ENV` 键。

- [ ] `<const>` 的 debug API 不可变语义。
  - 当前状态：编译期赋值检查已覆盖，但 `debug.setlocal` / `debug.setupvalue` 仍能改写 const local/upvalue。
  - 已知差异：当前 `debug.setupvalue` 可以把捕获的 `<const>` 上值改成新值；Lua 5.4 会拒绝并保持原值。
  - 需要补测试：`debug.setlocal` 修改 const local、`debug.setupvalue` 修改 const upvalue、`debug.upvaluejoin` 对 const upvalue 的影响。

- [ ] 完整 Lua 5.4 64 位整数语义。
  - 当前状态：兼容层仍沿用 LuaJIT 当前 32 位内部整数策略，`math.mininteger`/`math.maxinteger` 是 `-2147483648..2147483647`。
  - 当前进展：`math.type()` 在非 dual-number 构建下会把当前 32 位范围内可精确表示为整数的 number 报告为 `integer`；`math.floor`、`math.ceil`、`math.modf` 的整数部分会尽量返回当前兼容整数表面。
  - 已知差异：`1 << 40` 当前得到 `0`，Lua 5.4 应保留 64 位位移结果；整数/浮点子类型仍不能像官方 Lua 5.4 一样完整区分所有字面量和运行期结果。
  - 需要补测试：64 位整数字面量边界、`math.tointeger`、`math.ult`、`math.random(0)` 全范围、位运算、整除、比较、`string.pack` 的 `j`/`I8`/`i8`、C API `lua_Integer` 边界。
  - 实现重点：需要统一 TValue 表示、数值转换、字符串扫描、格式化、运算符和库函数的整数路径。

- [ ] 数值 `for` 的 Lua 5.4 整数循环语义。
  - 当前状态：仍主要沿用 LuaJIT 旧数值 for 行为。
  - 已知差异：`for i=1,3,0 do end` 当前可能进入不退出的循环，Lua 5.4 会报 `'for' step is zero`；跨 32 位边界和接近 `math.maxinteger` 的整数循环也没有 Lua 5.4 的“不回绕”语义。
  - 需要补测试：零步长、正/负步长边界、`math.maxinteger` / `math.mininteger` 附近、整数和浮点控制变量的类型、循环变量不回绕、循环变量在 debug API 下的行为。

- [x] Lua 5.4 运算符元方法。
  - 当前状态：`//`、`&`、`|`、`~`、`<<`、`>>` 仍通过内部 helper 降级实现；helper 已在原始数值路径失败时查找并调用 `__idiv`、`__band`、`__bor`、`__bxor`、`__bnot`、`__shl`、`__shr`。
  - 已覆盖：左右操作数元方法、反向查找、无元方法时报错、元方法返回值透传；一元 `~` 按 Lua 5.4 传入两份同一操作数。
  - 剩余边界：整数范围仍受当前 32 位兼容层限制，完整 64 位位运算归入“完整 Lua 5.4 64 位整数语义”继续处理。

- [x] `__le` 元方法不应再由 `__lt` 模拟。
  - 当前状态：只定义 `__lt` 的 table 做 `a <= b` 时，当前仍走旧 Lua 5.1 风格的 `not (b < a)` 模拟路径。
  - 已知差异：Lua 5.4 已移除用 `__lt` 模拟 `__le` 的行为；缺少 `__le` 时 `<=` 应直接报错。
  - 需要补测试：只定义 `__lt`、只定义 `__le`、二者都定义、左右 metatable 不同、错误消息。

- [x] `string.pack` / `string.unpack` / `string.packsize` 的完整格式布局。
  - 当前状态：已覆盖基础整数、浮点、字符串、`j`、`T`，但布局语义仍不完整。
  - 已知差异：格式 `X` 尚未支持；`!n` 只解析但没有按 Lua 5.4 自动填充对齐，例如 `string.packsize("!8bi8")` 当前为 `9`，Lua 5.4 为 `16`。
  - 需要补测试：`X`、`!n` 最大对齐、默认 native alignment、padding 字节、`unpack` 位置推进、混合 endian 与 alignment、非法格式错误文本。
  - 实现重点：格式解析需要维护当前位置和最大对齐，`packsize`、`pack`、`unpack` 三者必须共用同一套布局规则。

## P1：标准库和元语义缺口

- [ ] table 的 `__gc` 元方法。
  - 当前状态：table 设置 `__gc` 后不会在 GC 时调用；Lua 5.4 会支持 table finalizer。
  - 需要补测试：带 `__gc` 的 table 被回收、设置 metatable 时机、finalizer 顺序、finalizer 抛错行为。

- [ ] 弱键表的 ephemeron 语义。
  - 当前状态：普通 weak table 可用，但 `__mode = "k"` 下 value 反向引用 key 时，key 当前不会按 Lua 5.4 ephemeron 规则被回收。
  - 已知差异：`local t=setmetatable({}, {__mode="k"}); t[k]={k=k}` 这种只有 value 反向引用 key 的结构，Lua 5.4 多次 GC 后会清空，当前仍保留。
  - 需要补测试：弱键表、弱键弱值表、value 到 key 的反向引用、链式 ephemeron、finalizer 与 ephemeron 的交互。

- [ ] `__name` 元字段。
  - 当前状态：`tostring(setmetatable({}, {__name="Foo"}))` 已显示 `Foo: ...`；参数类型错误也会使用 `__name` 字符串。
  - 当前进展：`luaL_newmetatable()` 在 Lua 5.4 兼容模式下会把注册类型名写入 `__name`。
  - 当前进展：`luaL_tolstring()` 已使用 `__name` 作为默认对象前缀，C API smoke 已覆盖 lauxlib 路径。
  - 已覆盖：`tostring`、`math.abs` 参数类型错误、`luaL_tolstring`。
  - 剩余：其他边缘错误文本仍可继续和官方逐字收紧。

- [ ] `tonumber` 和 `string.format` 的 Lua 5.4 数值格式规则。
  - 当前状态：已补 `tonumber("0x10", 16) == nil`；整数格式 `%d`/`%i`/`%u`/`%x`/`%o` 已拒绝无整数表示的 number/string number；`string.format("%q", number)` 已输出可读回的十六进制浮点/整数文本；`string.format("%p", nil)` 已输出 `(null)`，非 nil 指针使用平台 C `%p` 文本。
  - 已覆盖：base16 的 `0x` 前缀、base34 下 `x` 仍作为有效数字、`%d` 严格整数检查、`%q` number/string 基础输出、`%p` nil。
  - 剩余：完整 64 位整数格式归入整数语义继续处理，错误文本仍可继续和官方逐字收紧。

- [ ] 字符串到数字的运算转换细节。
  - 当前状态：普通算术路径仍依赖 LuaJIT 旧转换；`//` helper 已支持字符串数字；bitwise helper 继续拒绝 string。
  - 已知差异：Lua 5.4 把字符串到数字的算术转换放到 string 库元方法层，算术可转换但位运算不可转换，并且会保留字符串数字的整数/浮点隐式类型；`"1" + "2"` 在非 dual-number 构建下仍不能表现为整数结果。
  - 需要补测试：`"1" + "2"`、`"1.0" + 2`、不可转换字符串、元方法优先级、结果 `math.type`。

- [x] `string.gmatch` 的 `init` 参数。
  - 当前状态：函数接受第三个参数但没有按 Lua 5.4 规则处理负数起点。
  - 已知差异：`string.gmatch("abcabc", "a", -2)()` 当前仍从开头返回 `"a"`，Lua 5.4 会从倒数位置开始，结果为 `nil`。
  - 需要补测试：正数 init、负数 init、越界 init、带捕获和无捕获模式、`^` 在 gmatch 中不作为锚点的既有规则。

- [x] `warn()` 参数转换规则。
  - 当前状态：当前实现要求每个 warning 参数都是 string。
  - 已知差异：Lua 5.4 官方 `warn("a", 1)` 不报错；当前报 `bad argument #2 ... string expected`。
  - 需要补测试：number、boolean、nil、带 `__tostring` 的值、`@on`/`@off` 控制消息、未知 `@xxx` 控制消息。

- [x] `math.randomseed()` 无参和 PRNG 细节。
  - 当前状态：`math.randomseed(x, y)` 已接受两个种子并返回种子对，但无参数调用当前返回 `0, 0`。
  - 已知差异：Lua 5.4 无参 `math.randomseed()` 会用系统熵/时间生成两个实际种子并返回它们；当前兼容实现只是固定返回零种子对。
  - 需要补测试：无参返回两个可用整数种子、连续无参调用的种子变化、带一个/两个参数的返回类型、区间随机的均匀性边界。

- [x] `utf8` lax 模式。
  - 当前状态：基础 `utf8` 函数可用，`utf8.char()` 也支持扩展码点范围，但 `utf8.codes(s, true)` / `utf8.len(s, i, j, true)` 的宽松解码仍不完整。
  - 已知差异：对 `utf8.char(0x200000)` 或代理区间字节序列，Lua 5.4 在 lax 模式下可以迭代/计数；当前 `utf8.codes(..., true)` 仍按严格模式报 `invalid UTF-8 code`。
  - 需要补测试：超过 `0x10ffff` 的扩展码点、surrogate 字节序列、严格模式报错、lax 模式成功返回码点。

- [ ] `debug.getuservalue` / `debug.setuservalue` 的 indexed uservalue 语义。
  - 当前状态：Lua 5.4 兼容模式不再暴露 LuaJIT userdata 内部环境表；`debug.getuservalue(io.stdout, 1)` 返回 `nil`，`debug.setuservalue(io.stdout, {}, 1)` 返回 `nil`。
  - 当前进展：C API 已补 `lua_newuserdatauv` / `lua_getiuservalue` / `lua_setiuservalue`，用 LuaJIT userdata 环境表保存声明数量和值；超出声明范围返回 `LUA_TNONE` / `0`。
  - 已覆盖：无 declared uservalue 的内置 userdata、indexed 参数表面、`setuservalue` 返回值、C API 声明两个 user values 后的 get/set/out-of-range 行为。
  - 剩余：Lua debug 库如果要暴露自定义 C userdata 的 declared uservalue，还需把当前隐藏策略和 C API 存储策略打通。

- [ ] 真实 Lua 5.4 GC 模式。
  - 当前状态：`collectgarbage("generational")` / `"incremental"` 只是兼容返回值和模式记录，底层仍是 LuaJIT 自身 GC。
  - 需要补测试：`generational`/`incremental` 参数、返回旧模式、`minor`/`major` 行为、`setpause`/`setstepmul` 返回值与 Lua 5.4 对齐。
  - 实现重点：如果不重做 GC，至少要明确哪些行为是 shim，哪些行为可以做到语义兼容。

- [ ] `debug.getinfo` 的 Lua 5.4 选项和 hook 字段。
  - 当前状态：`debug.getinfo(f, "u")` 已有 `nparams`/`isvararg`；`"t"` 选项已接受并返回保守的 `istailcall=false`。
  - 当前进展：`lua_Debug` 已补 Lua 5.4 的 `nparams`、`isvararg`、`istailcall`、`ftransfer`、`ntransfer` 字段；`lua_getinfo(..., "ut")` 会填入 `nparams/isvararg`，并对尚未精确支持的 tail/transfer 字段返回保守 `0/false`。
  - 已覆盖：C API smoke 读取新增 `lua_Debug` 字段。
  - 需要补测试：真实 tail call 场景的 `istailcall`、hook 里的 call/return transfer 字段。

- [ ] Lua 5.4 C API / 头文件兼容。
  - 当前状态：`lua.h` 会在兼容模式报告 `LUA_VERSION_NUM 504`，但大量 Lua 5.4 C API 仍缺失、保持旧签名，或仍暴露 Lua 5.1 宏/索引，例如 `LUA_GLOBALSINDEX`、`lua_objlen`、`lua_getfenv`、`lua_setfenv`。
  - 当前进展：已补 `lua_Unsigned`、`LUA_MAXINTEGER`、`LUA_MININTEGER`、`lua_absindex`、`lua_isinteger`、`lua_rawlen`、`lua_geti`、`lua_seti`、`lua_rawgetp`、`lua_rawsetp`、`lua_pushglobaltable`、`lua_arith`、`lua_compare`、`lua_len`、`lua_numbertointeger`、`lua_rotate`、`lua_stringtonumber`、`lua_getextraspace`、`lua_newuserdatauv`、`lua_getiuservalue`、`lua_setiuservalue`、`lua_setwarnf`、`lua_warning`。
  - 当前进展：已补 `lua_KContext`、`lua_KFunction`、`lua_WarnFunction`、`LUA_RIDX_MAINTHREAD`、`LUA_RIDX_GLOBALS`、`LUA_LOADED_TABLE`、`LUA_PRELOAD_TABLE`、`LUA_HOOKTAILCALL`、`LUA_GCGEN`、`LUA_GCINC`；registry 中会写入主线程和全局表；`lua_callk` / `lua_pcallk` / `lua_yieldk` 以不支持 continuation 的宏兼容旧调用形态。
  - 当前进展：已补 lauxlib 常用 Lua 5.4 表面：`luaL_pushfail`、`luaL_len`、`luaL_getsubtable`、`luaL_requiref`、`luaL_tolstring`、`luaL_typeerror`、`luaL_argexpected`、`luaL_checkversion` 以及基础 buffer 宏。
  - 已覆盖：新增 `test/lua54_capi_smoke.c` 和 `make smoketest-capi-lua54compat`。
  - 需要补 API：`lua_resetthread`、`lua_toclose`、真实 continuation 版 `lua_yieldk` / `lua_callk` / `lua_pcallk`、Lua 5.4 版 `lua_resume(lua_State *L, lua_State *from, int nargs, int *nresults)`。
  - 需要补常量/类型/宏：继续核对 `LUA_GCCOUNTB`、旧 API 隐藏策略和完整 ABI 细节。
  - 需要清理/兼容旧 API：默认构建保留 LuaJIT/Lua 5.1 API；Lua 5.4 兼容头应避免把旧 `lua_equal`、`lua_lessthan`、`lua_objlen`、`lua_cpcall`、`lua_getfenv`、`lua_setfenv`、`LUA_ENVIRONINDEX`、`LUA_GLOBALSINDEX` 当成官方 5.4 表面暴露。
  - 需要补内存分配语义：Lua 5.4 允许 allocator 在缩小内存块时失败；当前仍需核对 LuaJIT 分配器契约和错误处理。
  - 需要补测试：最小 C 程序编译测试、链接测试、ABI 兼容测试、旧 LuaJIT API 在默认构建下不受影响。

- [ ] Lua 5.4 auxiliary library / lauxlib 兼容。
  - 当前状态：`lauxlib.h` 仍以 Lua 5.1/LuaJIT 接口为主，只补了部分 5.2+ 辅助函数。
  - 当前进展：`luaL_argexpected`、`luaL_buffaddr`、`luaL_bufflen`、`luaL_buffsub`、`luaL_checkversion`、`luaL_getsubtable`、`luaL_len`、`luaL_newmetatable` 设置 `__name`、`luaL_pushfail`、`luaL_pushresultsize`、`luaL_requiref`、`luaL_tolstring`、`luaL_typeerror` 已补。
  - 已覆盖：最小 C 程序覆盖 buffer API、`luaL_tolstring`、`luaL_pushfail`、`luaL_getsubtable`、`luaL_requiref`。
  - 剩余：`luaL_loadfilex` / `luaL_loadbufferx` 的 Lua 5.4 细节和错误文本仍需继续核对。

- [ ] Lua 5.4 binary chunk / `string.dump` 兼容性。
  - 当前状态：仍使用 LuaJIT 自身 bytecode 格式，不兼容官方 Lua 5.4 binary chunk。
  - 已知差异：`string.dump` 后再 `load` 带 upvalue 的函数，Lua 5.4 会把第一个 upvalue 初始化为全局环境；当前 LuaJIT dump 重新加载后该 upvalue 是 `nil`。
  - 需要补测试：官方 Lua 5.4 dump 的加载失败说明、LuaJIT dump 在兼容模式下的 `_ENV`/upvalue 表现、strip 参数行为、mode=`"b"`/`"t"` 的错误消息差异。
  - 实现重点：如不支持官方 bytecode，应在文档中明确边界；如支持，需要单独的 reader/writer。

## P2：继续做一致性回归的边缘面

- [ ] 标准库错误消息与边界参数完全对齐。
  - 范围：`math`、`utf8`、`string.pack`、`table.move`、`require`、`load`/`loadfile`。
  - 做法：将本机 `lua5.4.8` 的边界行为固化为对照测试，先覆盖返回值和是否报错，再逐步收紧错误文本。

- [ ] table 库的 Lua 5.4 边界语义。
  - 当前状态：核心函数可见，但部分边界仍像 LuaJIT/Lua 5.1。
  - 当前进展：`table.concat` 默认终点已改为尊重 Lua 5.4 的长度操作，因此带 `__len` 的表会按元方法返回的终点检查 nil 洞。
  - 已知差异：`table.concat({1,nil,3}, ",")` 在 LuaJIT 当前表构造/长度边界下仍可能只拼到第一个边界并返回 `"1"`；Lua 5.4 对同样新建 list table 会检查到 nil 并报 `invalid value (nil) at index 2`。
  - 需要补测试：新建 list table 带洞数组的默认终点、显式 `i/j` 范围、`table.insert`/`remove`/`move` 的整数参数检查、`table.sort` comparator 错误传播。

- [ ] 严格 Lua 5.4 语法表面。
  - 当前状态：兼容模式还保留少量 LuaJIT 扩展语法。
  - 当前进展：`load("return 0b1010")`、`load("return 1LL")`、`load("return 1i")` 在 Lua 5.4 兼容模式下已报 malformed number。
  - 需要补测试：其他 LuaJIT-only numeric literal、FFI/cdata 相关扩展语法在 Lua 5.4 兼容模式下是否应该继续允许；如果决定保留扩展，需要在文档里明确这是 LuaJIT 扩展而非官方 Lua 5.4 行为。

- [ ] standalone / 环境变量兼容边界。
  - 当前状态：兼容工作主要集中在库和 VM，尚未系统核对 Lua 5.4 standalone 行为。
  - 需要补测试：`LUA_INIT_5_4`、`LUA_PATH_5_4`、`LUA_CPATH_5_4` 优先级，`lua`/`luajit` 命令行 `-e`/`-l`/`-i` 行为，`arg` 表形态，`package.path`/`package.cpath` 初始化差异。
  - 实现重点：如果保留 LuaJIT standalone 行为，应在兼容文档中说明 CLI 和官方 Lua 5.4 不完全等价。

- [ ] JIT/trace 对 Lua 5.4 新语义的记录。
  - 当前状态：多个新语法通过 helper 调用实现，语义优先于 JIT 性能。
  - 需要补测试：开启 JIT 后的 `//`、位运算、`_ENV` 访问、`math.random` 区间路径是否稳定；必要时给 unsupported trace 路径加退出或 recorder。

## 已确认不列入当前 TODO 的已实现项

- `_VERSION == "Lua 5.4"`、`jit.lua54compat == true`。
- Lua 5.4 模式隐藏旧 Lua 5.1/LuaJIT API：`getfenv`、`setfenv`、`module`、`newproxy`、`loadstring`、全局 `unpack`、`bit` 等。
- `rawlen`、`table.pack`、`table.unpack`、`table.move`、`coroutine.isyieldable` 已可见。
- `load(..., env)` 已能让 chunk 使用传入环境。
- `pairs` 已支持 `__pairs`；`ipairs` 已按 Lua 5.4 使用普通索引访问，不走旧 `__ipairs`。
- `package.searchers`、`require` loader data、`utf8` 基础库、`warn`、`math.randomseed(x, y)`、`math.random` 的基础 Lua 5.4 行为已进入 smoke 覆盖。
