# Lua 5.4 Compatibility TODO

本清单基于当前 `LUAJIT_ENABLE_LUA54COMPAT` 实现、`test/smoke.lua`
覆盖范围，以及和本机 `lua5.4.8` 的行为探针对比。后续继续实现时仍按
“先写用例，再补实现，再跑完整测试”的顺序推进。

## 实施规划：按底层依赖成批推进

后续不再按 TODO 条目逐个零散修补，而按下面的底层依赖分批推进。每一批
先补稳定接口和回归用例，再把依赖该接口的多个 TODO 一起收口。

1. **VM unwind / to-be-closed 批次**
   - 目标：为 `<close>` 建立 VM 级关闭接口，而不是继续堆 parser helper。
   - 覆盖：`return f()` 动态多返回、error unwind、`pcall`/`xpcall`、coroutine close、generic for closing value 错误展开、`lua_toclose` 自动随 C frame 退出关闭。
   - 接口要求：需要能保存动态返回值数量、传递错误对象给 `__close(value, err)`，并在 `__close` 自身抛错时按 Lua 5.4 规则替换/传播错误。
   - 验证面：Lua smoke + C API smoke + PC/Android ARM64；iOS/Emscripten 在平台批次补工具链后纳入。
   - 接口草案：新增统一 runtime close 层，至少提供“声明时校验 closable”“按栈层级 LIFO close”“带错误对象 close”“return 动态结果保护后 close”四类入口；当前 `_lua54_checkclose` / `_lua54_closevalue` parser helper 后续只能作为临时桥接，不能继续扩成最终方案。
   - 当前进展：已新增 `lj_close` runtime helper，先统一声明校验、`__close` 查找和 `__close(value, err)` 调用入口；close-active `return f()` / `return fixed, ...` 已用私有 pack/close/unpack 桥接保存动态返回值和 nil 洞；error unwind 已先用 close-list 桥接覆盖 `pcall`/`xpcall`、`__close` 错误替换和 generic for 错误展开；`coroutine.close` / `lua_closethread` / `lua_resetthread` 已能关闭 suspended coroutine 的 active close locals；C API `lua_toclose` marked slot 已接入 x64/arm64 VM C return 路径，正常 C 返回会在 return hook 前关闭并保留返回值槽位；后续 VM unwind 接管时应替换这些桥接而不是继续扩大 parser helper。
   - 批量完成标准：同一批提交应同时覆盖普通 Lua return/error、generic-for closing value、coroutine reset/close 和 C API `lua_toclose` 自动关闭路径，避免每条控制流各写一套 close 调度。

2. **真实 `_ENV` upvalue 批次**
   - 目标：减少伪 `_ENV` 对 LuaJIT 函数环境的依赖，为隐式全局访问提供可被 debug API 操作的真实 upvalue 语义。
   - 覆盖：`debug.setupvalue(load("return x"), 1, non_table)`、`debug.upvaluejoin`、dump/load 后 `_ENV` identity、stripped dump upvalue 名称差异。
   - 接口要求：明确真实 `_ENV` upvalue 与旧 `fn->c.env` 的同步边界，保留默认构建 LuaJIT 5.1 ABI 不受影响。

3. **64 位整数 / 数值表示批次**
   - 目标：统一 `lua_Integer`、TValue 数字子类型、字符串扫描、格式化、算术、bitwise、numeric for 和 JIT recorder 的整数路径。
   - 覆盖：`1 << 40`、`math.mininteger/maxinteger`、`math.type(1.0)`、`"1.0"+2`、`math.tointeger`、`math.ult`、`string.pack("j/i8/I8")`、C API integer 边界。
   - 接口要求：先决定 dual-number / 64-bit integer 表示与 JIT IR 扩展策略，再批量改库函数；不要在单个库函数里继续做 32 位补丁。

4. **debug frame metadata 批次**
   - 目标：在 VM frame 层保留 Lua 5.4 hook/tailcall 所需元信息。
  - 覆盖：真实 `istailcall`、C function call/return hook transfer、tail call transfer、Lua/C hook 的边界一致性。
   - 接口要求：call/return hook 的 transfer 信息应来自统一 frame/dispatch 元数据，而不是每个 hook 特判。

5. **平台和 JIT 批次**
   - 目标：把 PC/Android/iOS/Emscripten 64 位构建矩阵变成常规验证门，并明确各平台 JIT/解释器策略。
   - 覆盖：Windows PC x64、Android ARM64、iOS ARM64、Emscripten wasm/wasm64；JIT on/off 和 Lua 5.4 compat smoke。
   - 接口要求：Emscripten 不能假设传统本机 JIT；需要单独 wasm/interpreter 后端或明确禁用 JIT 的构建路径。

6. **C API / lauxlib / 标准库收尾批次**
   - 目标：在上述底层接口稳定后，统一核对 ABI、头文件宏、错误文本和冷门边界。
   - 覆盖：continuation API、allocator 缩小失败语义、旧 LuaJIT API 默认构建兼容、lauxlib 冷门宏组合、标准库逐字错误文本。
   - 接口要求：新增外部 Lua 5.4 ABI wrapper 时必须保留内部旧 ABI，避免破坏 LuaJIT 自身和默认构建。

## P0：核心语义缺口

- [ ] `<close>` 的运行期 `__close` 调度。
  - 当前状态：已接受 `local x <close>` 语法并记录属性，声明点会按 Lua 5.4 校验非 `nil`/`false` 值必须带 `__close`；普通块自然执行到 `end`、固定返回值 `return`、`break` 退出循环、已知向后 `goto`、前向 `goto` 跳出 close local 作用域以及 generic for 第 4 个 closing value 的普通退出路径时会按 LIFO 调用 `__close(value, nil)`。
  - 当前进展：`io.lines(filename)` 已按 Lua 5.4 返回第 4 个 closing value，迭代器仍会在 EOF 时主动关闭文件；`local x <close> = 1` 已按 Lua 5.4 在运行期报 `variable 'x' got a non-closable value`，`nil` / `false` 声明会跳过校验，带 `__close` 的值可声明；C API 已补 `lua_toclose()` 的 closable 校验、只能高于当前 active close slot 的标记顺序检查，以及 `lua_closeslot()` 只能关闭最后一个 active marked slot 的检查；普通 fall-through block exit、固定返回值 `return`、`break`、已知向后 `goto`、前向 `goto` 离开作用域和 generic for 自然结束/`break` 退出已通过 parser helper 调度 close locals；动态多返回 `return f()` / `return fixed, ...` 已用临时 pack/close/unpack 桥接保持返回值数量和 nil 洞；error 展开已通过 close-list 桥接关闭 active close locals，并覆盖 `pcall`/`xpcall`、`__close(value, err)`、`__close` 自身抛错替换错误对象和 generic for 错误展开；`coroutine.close` / `lua_closethread` / `lua_resetthread` 已能关闭 suspended coroutine 中的 active close locals，yield 状态传 `nil` 错误对象，close 抛错时返回失败和替换后的错误；C API `lua_settop` / `lua_pop` 弹出 `lua_toclose` 标记槽位时会自动关闭，C 函数抛错展开时会把 body error 传给 `__close`；C 函数正常返回时会在 return hook 和返回值搬移前关闭 `lua_toclose` 标记槽位，`__close` 抛错会替换为返回错误，且被关闭槽位作为返回值时不会被清成 `nil`。
  - 当前进展：普通块退出和 close-active `return` 路径中的 `__close` 现在由 parser 生成普通 Lua 调用，已支持 `__close` 内 `coroutine.yield()` 后恢复继续执行，并保留 `return` 的多返回值和 `nil` 洞。
  - 需要补测试/实现：error unwind、`pcall`/`xpcall` 保护展开、C API `lua_toclose` 正常 C 返回以及 coroutine reset/close 路径中的 `__close` yield/continuation 边界；这些路径仍需要 VM 级 unwind continuation，不能继续用不可 yield 的 C `lua_pcall` 桥接。
  - 实现重点：普通 fall-through、固定返回值 return、动态多返回 return、break、goto、generic for 普通控制流退出、error unwind、coroutine reset/close 和 C API 弹栈关闭已先走 parser/helper/close-list 桥接；完整实现仍需要 VM/字节码/栈帧层提供统一 close 调度，最终替换动态 return 的 pack/unpack 临时桥和 error unwind close-list 桥接。

- [x] `_ENV` 的完整 upvalue 语义。
  - 当前状态：源码 main chunk 已在 Lua 5.4 兼容模式下创建隐藏但真实的首个 `_ENV` upvalue，普通隐式全局访问会降级为 `_ENV.name`，不再依赖只能保存 table 的 LuaJIT 函数环境伪 upvalue。
  - 当前进展：`debug.getupvalue` / `debug.getinfo(..., "u")` / `debug.setupvalue` / `debug.upvalueid` / `debug.upvaluejoin` 已能操作真实 `_ENV` upvalue；`debug.setupvalue(load("return x"), 1, 5)` 和把隐式 `_ENV` join 到非 table lexical `_ENV` 后，运行期会按 Lua 5.4 报 `_ENV` 非 table 索引错误。
  - 当前进展：Lua 5.4 兼容模式下 `rawget(_G, "_ENV")` 已对齐官方返回 `nil`，`next(_G)` / `pairs(_G)` 也会跳过内部兼容 `_ENV` 键，同时保留裸 `_ENV == _G` 的当前兼容表面。
  - 已覆盖：chunk 的 `_ENV` upvalue 名称/位置、`debug.setupvalue` 用 table 和非 table 替换环境、带真实上值且访问全局的闭包会把 `_ENV` 排在第一个 debug upvalue、闭包继承局部 `_ENV`、真实 lexical `_ENV` 的 `debug.upvalueid` / `debug.upvaluejoin`、非 table lexical `_ENV` 的 debug 枚举和运行期索引错误、隐式 `_ENV` join 到非 table lexical `_ENV`、dumped chunk 的真实首个 upvalue可由 `load(..., env)` 初始化为 table / number / false / nil、`rawget(_G, "_ENV") == nil`、`next(_G)` / `pairs(_G)` 不枚举 `_ENV`。

- [x] `<const>` 的 debug API 行为核对。
  - 当前状态：编译期赋值检查已覆盖；经本机 Lua 5.4.8 对照，`debug.setlocal`、`debug.setupvalue`、`debug.upvaluejoin` 可以绕过 `<const>` 的源码级赋值限制，当前实现保留该行为。
  - 已覆盖：`debug.setlocal` 修改 const local、`debug.setupvalue` 修改 const upvalue、`debug.upvaluejoin` 替换 const upvalue。
  - 说明：数值字面量 const 是否被官方实现优化到无 upvalue 属于优化/debug 形态差异，不再作为“debug API 必须拒绝修改”的缺口处理。

- [ ] 完整 Lua 5.4 64 位整数语义。
  - 当前状态：兼容层仍沿用 LuaJIT 当前 32 位内部整数策略，`math.mininteger`/`math.maxinteger` 是 `-2147483648..2147483647`。
  - 当前进展：`math.type()` 在非 dual-number 构建下会把当前 32 位范围内可精确表示为整数的 number 报告为 `integer`；`math.floor`、`math.ceil`、`math.modf` 的整数部分会尽量返回当前兼容整数表面。
  - 已知差异：`1 << 40` 当前得到 `0`，Lua 5.4 应保留 64 位位移结果；整数/浮点子类型仍不能像官方 Lua 5.4 一样完整区分所有字面量和运行期结果。
  - 需要补测试：64 位整数字面量边界、`math.tointeger`、`math.ult`、`math.random(0)` 全范围、位运算、整除、比较、`string.pack` 的 `j`/`I8`/`i8`、C API `lua_Integer` 边界。
  - 实现重点：需要统一 TValue 表示、数值转换、字符串扫描、格式化、运算符和库函数的整数路径。

- [ ] 数值 `for` 的 Lua 5.4 整数循环语义。
  - 当前状态：仍主要沿用 LuaJIT 旧数值 for 行为。
  - 当前进展：Lua 5.4 兼容模式会拒绝常量 `0` / `0.0` step，并在运行期通过跨平台 helper 拒绝动态变量 `0` 和字符串 `"0"` step，报 `'for' step is zero`。
  - 已知差异：跨 32 位边界和接近 `math.maxinteger` 的整数循环仍没有 Lua 5.4 的“不回绕”语义；完整整数/浮点控制变量类型仍受当前 32 位兼容层限制。
  - 需要补测试：正/负步长边界、`math.maxinteger` / `math.mininteger` 附近、整数和浮点控制变量的类型、循环变量不回绕、循环变量在 debug API 下的行为。

- [x] Lua 5.4 运算符元方法。
  - 当前状态：`//`、`&`、`|`、`~`、`<<`、`>>` 仍通过内部 helper 降级实现；helper 已在原始数值路径失败时查找并调用 `__idiv`、`__band`、`__bor`、`__bxor`、`__bnot`、`__shl`、`__shr`。
  - 当前进展：`//` helper 的无元方法失败路径已按 Lua 5.4 报 `attempt to idiv a '<lhs>' with a '<rhs>'`，不再泄露私有 helper 名，并保留 `__name` 类型名。
  - 当前进展：bitwise helper 的失败路径已按 Lua 5.4 报运算符错误，不再泄露私有 helper 名；无整数表示的 number 报 `number has no integer representation`，string/boolean/带 `__name` 的 table 报 `attempt to perform bitwise operation on ... value`。
  - 当前进展：官方 `testes/bwcoercion.lua` 通过 string metatable 覆盖 bitwise/idiv 字符串边界；当前 lowered helper 已规整 metamethod 返回栈，只返回 metamethod 第一个结果，不再把原操作数漏成额外返回值。
  - 当前进展：lowered helper 的原始数值结果路径也已规整返回槽；`local a, b = "1.0" // "2"` 不再把左操作数字符串漏到 `a`，只返回单个 Lua 5.4 结果。
  - 已覆盖：左右操作数元方法、反向查找、无元方法时报错、元方法返回值透传；一元 `~` 按 Lua 5.4 传入两份同一操作数。
  - 剩余边界：整数范围仍受当前 32 位兼容层限制，完整 64 位位运算归入“完整 Lua 5.4 64 位整数语义”继续处理。

- [x] `__le` 元方法不应再由 `__lt` 模拟。
  - 当前状态：只定义 `__lt` 的 table 做 `a <= b` 时不再走旧 Lua 5.1 风格的 `not (b < a)` 模拟路径，缺少 `__le` 会直接报错。
  - 已覆盖：只定义 `__lt`、只定义 `__le`、二者都定义、左右元表差异和基础错误路径。

- [x] `string.pack` / `string.unpack` / `string.packsize` 的完整格式布局。
  - 当前状态：已覆盖基础整数、浮点、字符串、`j`、`T`、`l` / `L`、`X` 和 `!n` 最大对齐，`packsize`、`pack`、`unpack` 三者共用当前位置对齐规则。
  - 已覆盖：`X`、`!n` 最大对齐、padding 字节、`unpack` 位置推进、混合 endian 与 alignment、非法格式基础错误。
  - 剩余边界：完整 64 位整数格式仍归入“完整 Lua 5.4 64 位整数语义”继续处理。

## P1：标准库和元语义缺口

- [x] table 的 `__gc` 元方法。
  - 当前状态：Lua 5.4 兼容构建会在 metatable 赋值时记录 table 是否已经具备 `__gc`，GC 回收时按 finalizer 队列调度。
  - 已覆盖：带 `__gc` 的 table 被回收、多个 table finalizer 的 LIFO 顺序、晚加 `__gc` 不触发、替换 `__gc` 后调用新函数、删除 `__gc` 后不调用，以及 finalizer 抛错进入 Lua 5.4 warning 通道。

- [x] 弱键表的 ephemeron 语义。
  - 当前状态：`__mode = "k"` 下 value 反向引用 key 时，value 不再反向保活 key；GC atomic 阶段会对弱键强值表做 ephemeron 固定点标记。
  - 已覆盖：只有 value 反向引用 key 的弱键表会在多次 GC 后清空，外部仍强引用 key 时对应 value 会保留。
  - 后续扩展：链式 ephemeron、弱键弱值组合和 finalizer 交互仍可继续补更细的压力用例。

- [x] `__name` 元字段。
  - 当前状态：`tostring(setmetatable({}, {__name="Foo"}))` 已显示 `Foo: ...`；参数类型错误也会使用 `__name` 字符串。
  - 当前进展：`luaL_newmetatable()` 在 Lua 5.4 兼容模式下会把注册类型名写入 `__name`。
  - 当前进展：`luaL_tolstring()` 已使用 `__name` 作为默认对象前缀，C API smoke 已覆盖 lauxlib 路径。
  - 当前进展：`coroutine.resume()` / `coroutine.close()` 的 Lua 5.4 兼容路径已改用通用 `thread expected, got <__name>` 类型错误；默认 LuaJIT 构建仍保留旧 `coroutine expected` 文本。
  - 当前进展：`coroutine.isyieldable([co])` 已支持 Lua 5.4 可选 thread 参数；挂起或死亡的非主 coroutine 返回 true，非 thread 参数使用 Lua 5.4 风格函数名和 `__name` 类型文本。
  - 当前进展：`coroutine.create()` / `resume()` / `status()` / `wrap()` / `close()` 的基础参数错误会带 `coroutine.xxx` 函数名，并使用 Lua 5.4 的 `function/thread expected` 文本。
  - 当前进展：主线程直接 `coroutine.yield()` 会按 Lua 5.4 报 `attempt to yield from outside a coroutine`；其他不可 yield 的 C 边界仍保留 LuaJIT 现有错误路径。
  - 已覆盖：`tostring`、`type` 不受 `__name` 影响、非字符串 `__name` 被忽略、`math.abs` 参数类型错误、`coroutine.resume` / `coroutine.close` / `coroutine.isyieldable` 线程类型错误、`coroutine.isyieldable([co])` 可选 thread 参数、`coroutine` 基础参数错误函数名、`luaL_tolstring`。
  - 说明：其他函数名/逐字错误文本继续归入“标准库错误消息与边界参数完全对齐”。

- [ ] `tonumber` 和 `string.format` 的 Lua 5.4 数值格式规则。
  - 当前状态：已补 `tonumber("0x10", 16) == nil`；`tonumber` 的显式 base 参数会拒绝无整数表示的 number，仍接受字符串数字；`tonumber()` 在 Lua 5.4 兼容模式下已拒绝 C 风格 `inf` / `infinity` / `nan` 字符串和 LuaJIT 扩展 `0b` / `0B` 二进制前缀字符串；整数格式 `%d`/`%i`/`%u`/`%x`/`%o` 和字符格式 `%c` 已拒绝无整数表示的 number/string number；`string.format("%q", number)` 已输出可读回文本，覆盖整数、十六进制浮点、负零、NaN 和正负无穷；`%q` 对 table 等没有 Lua 字面量形式的值会报错，不再走 `__tostring`；`string.format("%p", nil/boolean/number)` 已输出 `(null)`，GC 对象继续使用平台 C `%p` 文本。
  - 已覆盖：base16 的 `0x` 前缀、base34 下 `x` 仍作为有效数字、base 参数 fraction number 报错、base 字符串数字转换、`tonumber` 拒绝 `inf` / `infinity` / `nan` 及大小写/符号/空白变体、`0b` / `0B` 及符号/空白变体、`%d` 严格整数检查、`%c` fraction number 报错和字符串数字转换、`%q` nil/boolean/integer/float/negative-zero/NaN/Inf/string 基础输出、`%q` table/`__tostring` table 报错、`%p` nil/boolean/number/string。
  - 剩余：完整 64 位整数格式归入整数语义继续处理，错误文本仍可继续和官方逐字收紧。

- [ ] 字符串到数字的运算转换细节。
  - 当前状态：普通算术路径仍依赖 LuaJIT 旧转换；`//` helper 已支持字符串数字；bitwise helper 继续拒绝 string；Lua 5.4 兼容模式下通用字符串数字转换已拒绝 `inf` / `nan` / `0b` 等 LuaJIT 扩展数字文本。
  - 当前进展：普通算术中字符串无法转换且无元方法时，错误文本已按 Lua 5.4 报具体操作名和左右操作数类型，例如 `attempt to add a 'string' with a 'number'`。
  - 当前进展：字符串 metatable 显式提供 `__add` / `__mul` / `__unm` 时，会优先于字符串数字转换执行，覆盖 `"1" + 2`、`2 + "1"`、`"1" * 2` 和 `-"1"`。
  - 已知差异：Lua 5.4 把字符串到数字的算术转换放到 string 库元方法层，算术可转换但位运算不可转换，并且会保留字符串数字的整数/浮点隐式类型；当前非 dual-number 兼容层仍不能完整保留 `"1.0" + 2` 这类结果的 `float` 子类型。
  - 已覆盖：`"1" + "2"` 基础算术转换、字符串算术元方法优先级、普通算术字符串失败路径、`//` 字符串失败路径、`math.abs()` / `math.tointeger()` / `math.type()` 以及 C API 数字转换拒绝扩展数字文本，同时保留 `"1e9999"` 溢出为无穷大的 Lua 5.4 行为。
  - 需要补测试/实现：`"1.0" + 2` 的结果 `math.type`；本机 Lua 5.4.8 对照为 `float`，当前非 dual-number 兼容层仍会把精确整数值 `3.0` 报为 `integer`。

- [x] `string.gmatch` 的 `init` 参数。
  - 当前状态：第三个 `init` 参数已按 Lua 5.4 规则处理正数、负数和越界起点。
  - 已覆盖：正数 init、负数 init、越界 init、带捕获和无捕获模式；本机 Lua 5.4.8 对照的负数起点探针已确认一致。

- [x] `warn()` 参数转换规则。
  - 当前状态：`warn()` 已按 Lua 5.4 兼容转换 number / nil / 带 `__tostring` 的值，boolean 仍按官方行为报错。
  - 当前进展：默认 warning 输出已补 `Lua warning: ` 前缀，`warn("@on")` 和 standalone `-W` 路径共用 `lua_warning()` 输出逻辑。
  - 已覆盖：number、boolean、nil、带 `__tostring` 的值、`@on`/`@off` 控制消息、未知 `@xxx` 控制消息、默认 warning 前缀。

- [x] `math.randomseed()` 无参和 PRNG 细节。
  - 当前状态：`math.randomseed(x, y)` 已接受两个种子并返回种子对；无参数调用会返回两个可转整数的非零占位种子，不再固定返回 `0, 0`。
  - 已覆盖：无参返回两个可用整数种子且不同时为零、带两个参数返回实际种子、`math.random(0)` 和整数区间随机的基础 Lua 5.4 表面。
  - 剩余边界：完整随机算法、64 位全范围和统计均匀性仍受当前 32 位整数兼容层限制。

- [x] `utf8` lax 模式。
  - 当前状态：基础 `utf8` 函数可用，`utf8.char()` 支持扩展码点范围，`utf8.codes(s, true)` / `utf8.len(s, i, j, true)` 的宽松解码也已接受扩展码点和 surrogate 字节序列。
  - 已覆盖：超过 `0x10ffff` 的扩展码点、surrogate 字节序列、严格模式报错、lax 模式成功返回码点；本机 Lua 5.4.8 对照探针已确认一致。

- [x] `debug.getuservalue` / `debug.setuservalue` 的 indexed uservalue 语义。
  - 当前状态：Lua 5.4 兼容模式不再暴露 LuaJIT userdata 内部环境表；`debug.getuservalue(io.stdout, 1)` 返回 `nil`，`debug.setuservalue(io.stdout, {}, 1)` 返回 `nil`。
  - 当前进展：C API 已补 `lua_newuserdatauv` / `lua_getiuservalue` / `lua_setiuservalue`，用 LuaJIT userdata 环境表保存声明数量和值；超出声明范围返回 `LUA_TNONE` / `0`。
  - 当前进展：Lua debug 库已改为通过 `lua_getiuservalue` / `lua_setiuservalue` 访问 declared uservalue；对未声明槽位返回 `nil`，`setuservalue` 返回 `nil`。
  - 已覆盖：无 declared uservalue 的内置 userdata、indexed 参数表面、`setuservalue` 返回值、C API 声明两个 user values 后的 get/set/out-of-range 行为，以及 C 创建 userdata 后通过 Lua `debug.getuservalue` / `debug.setuservalue` 读写 declared slot。
  - 剩余：仍未支持 `<close>` 自动作用域退出相关的 to-be-closed userdata 语义；`lua_closeslot()` 的显式关闭路径已覆盖。

- [ ] 真实 Lua 5.4 GC 模式。
  - 当前状态：`collectgarbage("generational")` / `"incremental"` 只是兼容返回值和模式记录，底层仍是 LuaJIT 自身 GC。
  - 当前进展：Lua 5.4 兼容构建的公开初始 `stepmul` 已对齐 Lua 5.4，`collectgarbage("setstepmul", n)` 首次返回 `100`；默认 LuaJIT 构建仍保留原 `LUAI_GCMUL`。
  - 当前进展：经本机 Lua 5.4.8 对照，`collectgarbage("minor")` / `"major"` 不是官方有效选项，当前 invalid option 行为已进入 smoke。
  - 当前进展：`setpause` / `setstepmul` 参数会按 Lua 5.4 公开表面压到 `0..1000`，并按 4 的粒度向下取整。
  - 当前进展：`step` / `setpause` / `setstepmul` 的第二参数已拒绝无整数表示的 number，仍接受字符串数字。
  - 已覆盖：`generational`/`incremental` 参数和旧模式返回、`minor`/`major` invalid option、`setpause` 初始返回 `200`、`setstepmul` 初始返回 `100`，以及负数、非 4 对齐值、超过 1000、fraction number、string number 的参数边界。
  - 实现重点：如果不重做 GC，至少要明确哪些行为是 shim，哪些行为可以做到语义兼容。

- [ ] `debug.getinfo` 的 Lua 5.4 选项和 hook 字段。
  - 当前状态：`debug.getinfo(f, "u")` 已有 `nparams`/`isvararg`；`"t"` 选项已接受，普通 Lua tail call frame 会报告 `istailcall=true`。
  - 当前进展：`lua_Debug` 已补 Lua 5.4 的 `nparams`、`isvararg`、`istailcall`、`ftransfer`、`ntransfer` 字段；`lua_getinfo(..., "ut")` 会填入 `nparams/isvararg`，普通 Lua tail call hook/return hook 会填入 `istailcall=true`，非 hook transfer 场景仍返回保守 `0/0`。
  - 当前进展：`debug.getinfo(..., "r")` 和 C API `lua_getinfo(..., "r")` 已接受 Lua 5.4 transfer-info 选项；非 hook 场景返回保守的 `ftransfer=0` / `ntransfer=0`，普通 Lua 函数（含 vararg）call/return hook 已能报告参数和返回值 transfer 范围，普通 C 函数和 fast C 函数的 call/return hook 已能按实际参数和返回值报告 transfer 范围。
  - 当前进展：C return hook 已新增统一 dispatch 入口并在 x64 / ARM64 VM 返回路径接入；fast C 函数入口会保留原始参数 transfer 起点，避免返回路径覆盖结果数量后丢失 Lua 5.4 debug 元数据。
  - 当前进展：普通 Lua `BC_CALLT` 路径已在 x64 / ARM64 VM 中保存 side marker，`debug.getinfo(..., "t")`、Lua hook 和 C API hook 都能识别 `tail call` / `LUA_HOOKTAILCALL`，并在该 frame 返回时清理 marker，避免后续普通调用误判；vararg pseudo-frame 已会回溯到真实 Lua frame 判断 `istailcall`，因此 vararg tailcall return hook 也能报告 `istailcall=true`。
  - 当前进展：Lua 5.4 兼容模式下，debug 库的 level/index/count 参数已改用严格整数检查；`debug.getinfo`、`getlocal`、`setlocal`、`getupvalue`、`setupvalue`、`upvalueid`、`upvaluejoin`、`sethook`、`traceback`、`getuservalue`、`setuservalue` 和 `setcstacklimit` 都会拒绝无整数表示的 number，同时保留字符串数字转换。
  - 已覆盖：C API smoke 读取新增 `lua_Debug` 字段，并通过 `lua_sethook` / `lua_getinfo(..., "r")` 覆盖普通 Lua 函数固定参数和 vararg 的 hook transfer、Lua tail call hook transfer / `istailcall`、vararg tailcall return hook 的 `istailcall`、tail-position C return 不误报 Lua tail hook，以及普通 C 函数 call/return hook transfer；Lua smoke 覆盖非 hook 场景和 `debug.getinfo(2, "r")` 的固定参数 / vararg call-return hook transfer、C 函数 call hook transfer、fast C 函数 return hook transfer、Lua tail call hook/return transfer、vararg tailcall return transfer、tail-position C return transfer、`pcall` / `xpcall` protected C return hook transfer、tailcall error unwind 后普通调用不误报 `istailcall`，以及直接调用清 marker。
  - 需要补测试：更多 hook transfer 字段边界。

- [ ] Lua 5.4 C API / 头文件兼容。
  - 当前状态：`lua.h` 已开始按官方 Lua 5.4.8 源码收紧外部表面，但仍有部分 C API 语义缺失或保持 LuaJIT 内部 ABI wrapper，例如真实 continuation、allocator 缩小失败、完整 64 位 integer ABI 和更多冷门宏组合。
  - 当前进展：已补 `lua_Unsigned`、`LUA_MAXINTEGER`、`LUA_MININTEGER`、`LUA_NUMTAGS`、`LUA_RIDX_LAST`、`LUA_EXTRASPACE`、`LUA_GNAME`、`LUA_FILEHANDLE`、`LUA_VERSUFFIX`、`LUAMOD_API`、`luaopen_coroutine`、Lua 5.4 外部 `luaopen_base()` 单返回值包装、`lua_absindex`、`lua_isinteger`、`lua_isyieldable`、`lua_rawlen`、`lua_geti`、`lua_seti`、`lua_rawgetp`、`lua_rawsetp`、`lua_pushglobaltable`、`lua_arith`、`lua_compare`、`lua_len`、`lua_numbertointeger`、`lua_rotate`、`lua_stringtonumber`、`lua_getextraspace`、`lua_getallocf`、`lua_setallocf`、`lua_newuserdatauv`、`lua_newuserdata`、`lua_getiuservalue`、`lua_setiuservalue`、`lua_getuservalue`、`lua_setuservalue`、`lua_setwarnf`、`lua_warning`、`lua_version` Lua 5.4 value-returning wrapper、`lua_toclose`、`lua_closeslot`、`lua_resetthread`、`lua_closethread`。
  - 当前进展：`lua_stringtonumber()`、`lua_isnumber()`、`lua_tonumberx()`、`lua_tointegerx()` 和 `luaL_checknumber()` 已和 Lua 5.4 `tonumber()` 对齐，拒绝 `inf` / `nan` / `0b` 等 LuaJIT 扩展数字字符串。
  - 当前进展：Lua 5.4 兼容模式下 `lua_tointegerx()` 已对无整数表示的 number 返回失败状态，和 `lua_numbertointeger()` 的精确整数语义保持一致。
  - 当前进展：已补 `LUA_VERSION_MAJOR`、`LUA_VERSION_MINOR`、`LUA_VERSION_RELEASE`、`LUA_VERSION_RELEASE_NUM`、`LUA_NUMTYPES`、`LUA_NUMTAGS` 头文件宏；兼容构建已按官方 Lua 5.4.8 暴露 `LUA_VERSION_RELEASE "8"`、`LUA_RELEASE "Lua 5.4.8"`、`LUA_VERSION_RELEASE_NUM 50408`、`LUA_COPYRIGHT`、`LUA_AUTHORS` 和外部 `lua_ident` 符号。
  - 当前进展：已补 `lua_KContext`、`lua_KFunction`、`lua_WarnFunction`、`LUA_RIDX_MAINTHREAD`、`LUA_RIDX_GLOBALS`、`LUA_LOADED_TABLE`、`LUA_PRELOAD_TABLE`、`LUA_HOOKTAILCALL`、`LUA_GCGEN`、`LUA_GCINC`；Lua 5.4 外部兼容头已隐藏旧 `LUA_HOOKTAILRET`；registry 中会写入主线程和全局表；`lua_gc()` 在兼容构建下已改为 Lua 5.4 变参签名；`lua_sethook()` 已按官方 Lua 5.4 外部头暴露为 `void` 返回表面；`lua_callk` / `lua_pcallk` / `lua_yieldk` 已按官方 Lua 5.4 头文件暴露为可取函数指针的真实导出函数，`lua_call` / `lua_pcall` / `lua_yield` 继续作为官方宏映射到 `*k(..., 0, NULL)`；`*k` 当前仍只覆盖 NULL-continuation 路径。
  - 当前进展：已按官方 Lua 5.4.8 `luaconf.h` / `lua.h` 对齐外部 `LUAI_IS32INT`、`LUAI_MAXSTACK`、`LUA_REGISTRYINDEX (-LUAI_MAXSTACK - 1000)` 和 `lua_upvalueindex(i) (LUA_REGISTRYINDEX - (i))` 公式；运行时 `index2adr` 会识别官方 registry/upvalue 伪索引并映射回 LuaJIT 内部旧 ABI，默认构建不变。
  - 当前进展：已补 `LUA_COMPAT_APIINTCASTS` 下的 `lua_pushunsigned` / `lua_tounsignedx` / `lua_tounsigned` / `luaL_checkunsigned` / `luaL_optunsigned` 兼容宏，并用单独 C smoke 覆盖编译和基础运行。
  - 当前进展：Lua 5.4 外部兼容头已把 `lua_newuserdata()`、`lua_getuservalue()`、`lua_setuservalue()` 暴露为官方 slot 1 alias 宏，默认构建仍保留 LuaJIT 旧函数 ABI。
  - 当前进展：Lua 5.4 兼容头已把外部 `lua_resume(L, from, nargs, nresults)` 映射到 `lua_resume54()` 包装入口，并支持按官方 `lua_resume` 名取函数指针；内部仍保留 LuaJIT 旧 2 参数 ABI，包装入口会填写 yield/return 的结果数量。
  - 当前进展：Lua 5.4 外部兼容头已隐藏 `LUA_ENVIRONINDEX`、`LUA_GLOBALSINDEX`、`lua_strlen`、`lua_open`、`lua_getregistry`、`lua_getgccount`、`lua_Chunkreader`、`lua_Chunkwriter`、`lua_setlevel` 以及旧 `lua_equal`、`lua_lessthan`、`lua_objlen`、`lua_cpcall`、`lua_getfenv`、`lua_setfenv` 声明；LuaJIT 内部和默认构建通过内部标记继续使用旧 ABI。
  - 当前进展：Lua 5.4 外部兼容头已隐藏旧 lauxlib 注册入口 `luaL_openlib` / `luaL_register` / `luaL_pushmodule`、旧 `luaL_typerror` 以及 LuaJIT helper `luaL_findtable`；默认构建仍通过 C API smoke 覆盖这些 LuaJIT/Lua 5.1 API。
  - 当前进展：外部兼容头下的 `lua_pushglobaltable()` / `lua_getglobal()` / `lua_setglobal()` 已改走 registry globals 表，不再依赖 `LUA_GLOBALSINDEX`；`lua_pushglobaltable()` 宏已按官方 Lua 5.4 暴露为 `void` 返回表面。
  - 当前进展：外部兼容头下的 `lua_gettable()`、`lua_getfield()`、`lua_geti()`、`lua_rawget()`、`lua_rawgeti()`、`lua_rawgetp()` 已通过 `*54` 包装入口返回取到值的 Lua 类型，并用官方函数名的源级别别名支持外部代码按 Lua 5.4 签名取函数指针；内部仍保留 LuaJIT 旧 `void` ABI。
  - 当前进展：外部兼容头下的 `lua_rawgeti()` / `lua_rawseti()` 索引参数已通过 `*54` 包装入口暴露为 `lua_Integer`，并支持按官方函数名取函数指针；内部仍转发到 LuaJIT 当前 32 位整数表槽路径。
  - 当前进展：Lua 5.4 外部兼容头已把 `lua_load(L, reader, data, chunkname, mode)` 映射到现有 `lua_loadx()`，并支持按官方 `lua_load` 名取函数指针；C API smoke 覆盖 text 模式加载和 binary-only 模式拒绝 text chunk。
  - 当前进展：Lua 5.4 外部兼容头已把 `lua_dump(L, writer, data, strip)` 映射到 `lua_dump54()`，并支持按官方 `lua_dump` 名取函数指针；内部旧 3 参数 `lua_dump()` ABI 保持不变，`strip` 会转发到 LuaJIT bytecode writer。
  - 当前进展：Lua 5.4 外部兼容头已把 `lua_insert()` / `lua_remove()` / `lua_replace()` 暴露为官方宏，映射到 `lua_rotate()` / `lua_copy()`，默认构建仍保留 LuaJIT 旧函数 ABI。
  - 当前进展：Lua 5.4 外部兼容头已通过 `lua_pushlstring54()` / `lua_pushstring54()` wrapper 暴露 `lua_pushlstring()` / `lua_pushstring()` 的官方返回值表面，并支持按官方函数名取函数指针；默认构建仍保留 LuaJIT 5.1 的 `void` 头文件 ABI。
  - 当前进展：Lua 5.4 外部兼容头已把 `lua_tonumber()` / `lua_tointeger()` 暴露为官方宏，映射到 `lua_tonumberx(..., NULL)` / `lua_tointegerx(..., NULL)`；默认构建仍保留 LuaJIT 旧函数 ABI。
  - 当前进展：Lua 5.4 外部兼容头已把 `lua_getextraspace()` 暴露为宏，并映射到当前 pointer-sized `L->exdata` 兼容存储；`lua_newstate()` / `lua_getallocf()` / `lua_setallocf()` 自定义 allocator、`lua_atpanic()`、`lua_pushthread()`、`lua_checkstack()`、`lua_settop()`、`lua_pushvalue()`、`lua_concat()`、`lua_next()`、`lua_error()`、`lua_type()`、`lua_typename()`、`lua_rawequal()`、`lua_iscfunction()`、`lua_tocfunction()`、`lua_touserdata()`、`lua_topointer()`、`lua_toboolean()` 和 light/full userdata 判定表面已进入 C API smoke；默认构建仍保留 LuaJIT 旧函数 ABI。
  - 当前进展：Lua 5.4 外部兼容头已把 `lua_rawlen()` 暴露为 `lua_Unsigned` 返回值表面，内部和默认构建继续保留 LuaJIT 旧 `size_t` ABI；当前仍受兼容层 32 位 `lua_Unsigned` 限制，完整 64 位整数 ABI 归入整数语义大项。
  - 当前进展：Lua 5.4 外部兼容头已把 `lua_version` 暴露为官方 value-returning 函数 wrapper，支持按 `lua_Number (*)(lua_State *)` 取函数指针；内部和默认构建仍保留 LuaJIT 旧指针 ABI。
  - 当前进展：Lua 5.4 兼容头已声明现有 `lua_copy()`，C API smoke 覆盖把一个栈槽复制到另一个栈槽。
  - 当前进展：已补 `lua_setcstacklimit()` C API shim，和 Lua 层 `debug.setcstacklimit()` 一样返回稳定兼容上限。
  - 当前进展：`LUA_GCCOUNTB` 已进入 C API smoke，覆盖返回 0..1023 byte remainder 的基础契约；兼容构建下 `lua_gc(L, what, ...)` 可用官方变参调用形态。
  - 当前进展：已补 lauxlib 常用 Lua 5.4 表面：`luaL_pushfail`、`luaL_len`、`luaL_getsubtable`、`luaL_requiref`、`luaL_tolstring`、`luaL_typeerror`、`luaL_argexpected`、`luaL_checkversion` 以及 buffer API；`luaL_checkversion_()` 现在会实际校验版本号和 numeric ABI 尺寸。
  - 已覆盖：新增 `test/lua54_capi_smoke.c` 和 `make smoketest-capi-lua54compat`，包含 `lua_isyieldable()` 主 C frame / resumed coroutine C frame 表面。
  - 当前进展：已补 `lua_closethread()` / `lua_resetthread()` 的 `<close>` 基础表面，覆盖 yielded/fresh coroutine 返回 `LUA_OK`、清空栈并恢复 OK 状态，且会关闭 suspended coroutine 中的 active close locals；已补 `lua_toclose()` / `lua_closeslot()` C API 表面，覆盖 closable 校验、false/nil `lua_toclose` 跳过、`lua_closeslot` 只接受最后一个 active marked slot、显式 `__close(value, nil)` 后置空槽位、`lua_settop` / `lua_pop` 弹栈关闭、C 函数错误展开关闭，以及 C 函数正常返回自动关闭。
  - 需要补语义：真实 continuation 版 `lua_yieldk` / `lua_callk` / `lua_pcallk`；当前已完成这些调用入口的函数 ABI 和 NULL-continuation 路径，但 VM 还不会保存并恢复 Lua 5.4 C continuation。
  - 需要补常量/类型/宏：继续核对完整 ABI 细节。
  - 需要清理/兼容旧 API：默认构建保留 LuaJIT/Lua 5.1 API；Lua 5.4 外部兼容头已隐藏一批旧 5.1/LuaJIT 表面、旧 lauxlib 注册入口、`luaL_typerror` 和 `luaL_findtable`，并补了常见 getter/number 转换/字符串 push 返回值签名和栈操作宏表面，但仍需继续核对更多旧兼容宏和完整 ABI 细节。
  - 需要补内存分配语义：Lua 5.4 允许 allocator 在缩小内存块时失败；当前仍需核对 LuaJIT 分配器契约和错误处理。
  - 当前进展：已补默认构建 C API smoke，覆盖旧 LuaJIT 5.1 头文件宏和 ABI 入口仍可编译、链接、运行。
  - 需要补测试：更完整 ABI 兼容测试，以及更多旧 LuaJIT API 在默认构建下不受影响的覆盖。

- [ ] Lua 5.4 auxiliary library / lauxlib 兼容。
  - 当前状态：`lauxlib.h` 仍以 Lua 5.1/LuaJIT 接口为主，只补了部分 5.2+ 辅助函数。
  - 当前进展：`luaL_addgsub`、`luaL_argexpected`、`luaL_buffaddr`、`luaL_bufflen`、`luaL_buffsub`、`luaL_checkversion`、`luaL_getsubtable`、`luaL_intop`、`luaL_len`、`luaL_newmetatable` 设置 `__name`、`luaL_pushfail`、`luaL_pushresultsize`、`luaL_requiref`、`luaL_tolstring`、`luaL_typeerror` 已补；外部兼容头中的 `luaL_argexpected` / `luaL_pushfail` 已按官方 Lua 5.4 暴露为宏。
  - 当前进展：Lua 5.4 兼容模式下 `luaL_prepbuffer()` / `luaL_prepbuffsize()` / `luaL_buffinitsize()` 会按请求尺寸增长 buffer；外部兼容头中的 `luaL_prepbuffer` 已按官方 Lua 5.4 暴露为宏，默认构建仍保留旧 LuaJIT buffer 结构。
  - 当前进展：`lauxlib.h` 已补 Lua 5.4 的 `luaL_Stream` 类型定义，以及官方放在 lauxlib 的 `LUA_GNAME` / `LUA_FILEHANDLE` / `LUA_LOADED_TABLE` / `LUA_PRELOAD_TABLE` / `lua_writestring` / `lua_writeline` / `lua_writestringerror` 宏，并进入 C API smoke 编译覆盖；Lua 5.4 兼容构建的 `LUAL_BUFFERSIZE` 已按官方 `16 * sizeof(void*) * sizeof(lua_Number)` 公式暴露，默认构建继续保留 LuaJIT/BUFSIZ 旧 ABI；`lualib.h` 已补 `LUA_VERSUFFIX`；外部 Lua 5.4 `lua.h` 单独包含时不会暴露 lauxlib-only 的 loaded/preload 宏。
  - 当前进展：`luaL_loadfilex` / `luaL_loadbufferx` 的 `mode` 参数已进入 C API smoke，覆盖 text 模式加载以及 binary-only 模式拒绝 text chunk。
  - 当前进展：`luaL_loadfilex` / `luaL_loadbufferx` 的 mode 不匹配错误文本已按 Lua 5.4 收紧，C API smoke 覆盖 `attempt to load a text chunk (mode is 'b')`。
  - 当前进展：外部兼容头中的 `luaL_loadfile()` / `luaL_loadbuffer()` 已按官方 Lua 5.4 暴露为映射到 `luaL_loadfilex(..., NULL)` / `luaL_loadbufferx(..., NULL)` 的宏。
  - 当前进展：外部兼容头中的 `luaL_newlib()` 已按 Lua 5.4 宏形态先执行 `luaL_checkversion()`，默认构建仍保留 LuaJIT 旧宏。
  - 当前进展：外部兼容头已隐藏旧 `luaL_typerror`，改用 Lua 5.4 的 `luaL_typeerror`；同时隐藏 LuaJIT helper `luaL_findtable`；默认构建继续保留并覆盖这些旧表面。
  - 当前进展：Lua 5.4 兼容模式下 `luaL_checkinteger()` / `luaL_optinteger()` 已拒绝无整数表示的 number，并进入 C API smoke。
  - 当前进展：`luaL_pushresultsize()` / `luaL_buffinitsize()` 已按官方 Lua 5.4 暴露为可取函数指针的真实 lauxlib 函数；默认构建仍保留 LuaJIT 旧宏表面。
  - 当前进展：C API smoke 已补充 `luaL_fileresult()`、`luaL_execresult()`、`luaL_newlib()`、`luaL_setfuncs()`、`luaL_newmetatable()` / `luaL_getmetatable()`、`luaL_setmetatable()`、`luaL_testudata()`、`luaL_checkudata()`、`luaL_traceback()` 和 `luaL_dostring()` 覆盖。
  - 已覆盖：最小 C 程序覆盖 buffer API、`LUAL_BUFFERSIZE` 官方公式、`luaL_prepbuffer` / `luaL_argexpected` / `luaL_pushfail` / `luaL_loadfile` / `luaL_loadbuffer` 宏可见性、`luaL_newlib` 版本检查宏形态、`luaL_prepbuffsize()` / `luaL_buffinitsize()` 大于 `LUAL_BUFFERSIZE` 的写入、`luaL_addgsub`、`luaL_tolstring`、`luaL_pushfail`、`luaL_getsubtable`、`luaL_requiref`、`luaL_loadbufferx`、`luaL_loadfilex`。
  - 已覆盖：`luaL_checkinteger()` / `luaL_optinteger()` 的 fraction number 错误，`lua_tointegerx()` 的 fraction status，`luaL_intop()` 的 32 位公开整数范围 wraparound，`luaL_checkoption()`、`luaL_gsub()`、`luaL_ref()` / `luaL_unref()`、`luaL_getmetafield()`、`luaL_callmeta()`、`luaL_where()`、`luaL_error()`、`luaL_typename()`，`lauxlib.h` 单独暴露 `LUA_GNAME` / `LUA_FILEHANDLE` / `LUA_LOADED_TABLE` / `LUA_PRELOAD_TABLE` 以及输出宏，`lua.h` 单独包含时隐藏 loaded/preload 宏，外部 5.4 头隐藏旧 `luaL_typerror` / `luaL_findtable`，以及上述常用 lauxlib 5.4 辅助入口的编译/链接/运行表面。
  - 剩余：更冷门的 lauxlib 宏组合和错误文本仍可继续核对。

- [ ] Lua 5.4 binary chunk / `string.dump` 兼容性。
  - 当前状态：仍使用 LuaJIT 自身 bytecode 格式，不兼容官方 Lua 5.4 binary chunk。
  - 当前进展：C API `lua_dump(..., strip)` 已支持 Lua 5.4 外部调用表面，并可写出带 strip 标志的 LuaJIT bytecode；这不是官方 Lua 5.4 binary chunk 格式兼容。
  - 当前进展：Lua 层 `string.dump(f, strip)` 已进入 smoke，覆盖 full/stripped LuaJIT bytecode 写出、`mode="b"` 回读执行，以及 binary chunk 被 `mode="t"` 拒绝。
  - 当前进展：已将官方 Lua 5.4.8 dump 的加载失败固化为 smoke；兼容构建会明确拒绝官方 Lua 5.4 binary chunk，并保留 `mode="t"` 的 binary chunk 拒绝错误。
  - 当前进展：LuaJIT dump 回读时，带真实 upvalue 的函数会按 Lua 5.4 `load` 规则把第一个 upvalue 初始化为当前全局环境；带第 4 个 env 参数时会初始化为指定 env，且真实 upvalue 槽支持 table / number / false / nil 这类非 table 值。
  - 当前进展：LuaJIT stripped dump 回读后的 debug upvalue 枚举已避免把无真实 upvalue 的 plain dump 误暴露为 `_ENV`，带真实 upvalue 的 stripped dump 也不再额外插入伪 `_ENV`；使用全局名的 stripped dump 会保留真实首个 env upvalue，名称按 LuaJIT stripped 规则为空字符串。
  - 已知差异：官方 Lua 5.4 stripped dump 的 upvalue 名称为 `(no name)`；LuaJIT stripped bytecode 继续用空字符串表示 stripped upvalue name。
  - 需要补测试：mode=`"b"`/`"t"` 的更多错误消息差异，以及更复杂嵌套 dump 的 `_ENV`/upvalue 表现。
  - 实现重点：如不支持官方 bytecode，应在文档中明确边界；如支持，需要单独的 reader/writer。

## P2：继续做一致性回归的边缘面

- [ ] Android / iOS / PC / Emscripten 64 位构建矩阵。
  - 当前状态：当前环境已反复覆盖 Windows PC 64 位默认构建和 `LUAJIT_ENABLE_LUA54COMPAT` 构建，并通过 `make test`。
  - 当前进展：已新增 `tools/lua54_platform_matrix.ps1`，可一键在当前 Windows/MSYS2 环境构建并运行 PC x64 default 与 Lua 5.4 compat smoke；检测到 Android NDK 时会实际构建 Android ARM64 Lua 5.4 compat 静态 artifact，并用 `llvm-readelf` 确认 AArch64；检测到在线 Android 设备时会自动 push `luajit`、`test/smoke.lua` 和 `src/jit` LuaJIT 工具模块到 `/data/local/tmp` 并运行 Lua 5.4 smoke；当前已在 Android 设备 `R5CN30J05BT` 取得 `test/smoke.lua lua54compat` PASS；iOS/Emscripten 目标先做工具链探测并输出明确 `SKIP` 原因。
  - 当前进展：Android ARM64 交叉编译暴露的 `floor` 隐式声明警告已通过补 `<math.h>` 清理。
  - 已知缺口：Android ARM64 artifact 构建和在线设备 smoke 已取得 PASS；iOS ARM64、Emscripten wasm/wasm64 仍未完成实际跨平台编译命令、artifact 检查和目标运行 smoke；当前用 `emcc 5.0.6` 直接走 Makefile 会在 `lj_arch.h` 报 wasm 架构不受支持，Emscripten 还缺 interpreter/wasm VM 后端可行路径。
  - 需要补测试/脚本：继续按实际工具链补 iOS SDK ARM64、Emscripten 的构建入口；Android 继续补设备/模拟器 smoke；每个目标至少验证编译完成、`LUAJIT_ENABLE_LUA54COMPAT` 可打开、目标可运行时执行 smoke，不可直接运行时产出可检查 artifact。
  - 实现重点：Emscripten 通常不能使用传统本机 JIT，需要明确解释器/wasm 可行路径；Android/iOS 需要分别确认 JIT 权限、mcode 分配和平台 ABI。

- [ ] 标准库错误消息与边界参数完全对齐。
  - 范围：`math`、`utf8`、`string.pack`、`table.move`、`require`、`load`/`loadfile`。
  - 当前进展：`math.type()` 已按 Lua 5.4 对齐“无参数报错，非 number 参数返回 nil”的边界。
  - 当前进展：Lua 5.4 兼容模式下的 `math.min` / `math.max` 已改用普通 `<` 比较，支持 string 和带 `__lt` 的对象，且无参数时报 value error。
  - 当前进展：`math.type()` / `math.tointeger()` / `math.ult()` / `math.min()` / `math.max()` 的缺参参数错误会带 `math.xxx` 函数名；`math.ult()` 的整数表示错误也会带实际函数名。
  - 当前进展：`dofile()` 和 `collectgarbage()` 的 Lua 5.4 参数错误已带实际函数名，覆盖 option、字符串参数和整数参数边界。
  - 当前进展：`table.concat()` / `insert()` / `remove()` / `sort()` / `move()` 的 Lua 5.4 入口参数错误已带 `table.xxx` 函数名，覆盖表、分隔符、整数位置和 comparator 边界。
  - 当前进展：`os.date()` / `difftime()` / `execute()` / `getenv()` / `remove()` / `rename()` / `setlocale()` / `time()` 的 Lua 5.4 基础参数错误已带 `os.xxx` 函数名。
  - 当前进展：`debug.getinfo()` / `getlocal()` / `setlocal()` / `getupvalue()` / `setupvalue()` / `upvalueid()` / `upvaluejoin()` / `sethook()` / `getuservalue()` / `setuservalue()` / `setcstacklimit()` 的 Lua 5.4 基础参数错误已带 `debug.xxx` 函数名。
  - 当前进展：`utf8.char()` / `codepoint()` / `codes()` / `len()` / `offset()` 的 Lua 5.4 基础参数错误已带 `utf8.xxx` 函数名。
  - 当前进展：`string.byte()` / `char()` / `dump()` / `find()` / `format()` / `gmatch()` / `gsub()` / `len()` / `lower()` / `match()` / `rep()` / `reverse()` / `sub()` / `upper()` / `pack()` / `unpack()` / `packsize()` 的 Lua 5.4 基础参数错误已带 `string.xxx` 函数名。
  - 当前进展：`error(message, level)` 的 level 参数已拒绝无整数表示的 number；Lua 5.4 兼容模式下非 string 错误对象会保留原值，不再被当成 5.1 风格 string 加位置信息。
  - 当前进展：`select(index, ...)` 的 index 参数已拒绝无整数表示的 number，仍接受字符串数字。
  - 当前进展：Lua 5.4 兼容模式下 `getmetatable()` 无参数已报 value error，并保留 `__metatable` 保护返回值。
  - 当前进展：debug 库整数边界已按 Lua 5.4 收紧，覆盖 stack level、local/upvalue index、hook count、traceback level、uservalue slot 和 `setcstacklimit`。
  - 当前进展：string / utf8 库的常见整数参数已按 Lua 5.4 收紧，覆盖 `string.byte`、`char`、`sub`、`rep`、`find`、`match`、`gmatch`、`gsub`、`pack`、`unpack` 以及 `utf8.char`、`codepoint`、`len`、`offset`，都会拒绝无整数表示的 number。
  - 当前进展：`string.char()` 的越界错误文本已收紧为 Lua 5.4 风格的 `value out of range`。
  - 当前进展：`utf8.char()` 对整数可表示但超出 0..0x7fffffff 扩展码点范围的输入，也已按 Lua 5.4 报 `value out of range`。
  - 当前进展：`string.format()` 的整数格式转换错误、整数格式的 number 类型错误以及 `%q` 无 Lua 字面量形式的错误，均已带 `string.format` 函数名，不再在内部格式化 helper 中显示为 `?`。
  - 当前进展：`load()` / `loadfile()` 底层 mode 不匹配错误文本已按 Lua 5.4 收紧，会说明被拒绝的是 text 还是 binary chunk，并回显传入 mode。
  - 当前进展：`package.searchpath()` / `package.searchers` 返回的错误片段已按 Lua 5.4 去掉前导换行缩进，`require()` 组装最终 module-not-found 错误时再补 `\n\t`。
  - 当前进展：`os.rename()` 失败时已按 Lua 5.4 返回原始系统错误文本，不再把源文件名拼进错误字符串；`os.remove()` 失败返回保留文件名前缀并已进入 smoke 覆盖。
  - 当前进展：`pairs()` 已按 Lua 5.4 延迟 table 检查；没有 `__pairs` 时会返回原始 `next, value, nil`，由后续 `next()` 调用决定是否报错。
  - 当前进展：`assert()`、`type()`、`tostring()`、`pcall()`、`xpcall()`、`select()`、`error()`、`tonumber()`、`load()`、`loadfile()`、`next()`、`pairs()`、`ipairs()`、`getmetatable()`、`setmetatable()`、`rawget()` / `rawset()` / `rawequal()` / `rawlen()` 的基础参数错误会带实际函数名；`rawlen()` 非 table/string 的期望类型文本已收紧为 `table or string`；`assert(false, value)` 在 Lua 5.4 兼容模式下会保留 number/table 等非 string 错误对象。
  - 当前进展：`math.deg()` / `math.rad()` 在 Lua 5.4 兼容模式下已从 LuaJIT 内置 Lua 片段改为带参数检查的 C helper，缺参和错误类型会报标准参数错误并保留数值字符串转换。
  - 当前进展：`//` helper 的无元方法失败路径已从私有 C helper 参数错误收紧为 Lua 5.4 `idiv` 运算符错误，覆盖左右操作数类型和 `__name`。
  - 当前进展：位运算 helper 的错误文本已从私有 C helper 参数错误收紧为 Lua 5.4 运算符错误，覆盖 fraction number、string、boolean 和带 `__name` 的 table。
  - 做法：将本机 `lua5.4.8` 的边界行为固化为对照测试，先覆盖返回值和是否报错，再逐步收紧错误文本。

- [x] table 库的 Lua 5.4 边界语义。
  - 当前状态：当前清单中的 table 库 Lua 5.4 边界已进入 smoke 覆盖。
  - 当前进展：`table.concat` 默认终点已改为尊重 Lua 5.4 的长度操作，因此带 `__len` 的表会按元方法返回的终点检查 nil 洞。
  - 当前进展：`table.concat({1,nil,3}, ",")` 已按 Lua 5.4 参考行为检查到 index 2 的 nil 并报错；实现只在 `table.concat` 默认终点补数组构造洞的兼容，不改变全局 `#table` 行为。
  - 当前进展：`table.concat` / `table.insert` / `table.remove` 的默认长度路径共用 Lua 5.4 表库长度兼容逻辑，`__len` 返回无整数表示的值时会报 `object length is not an integer`。
  - 当前进展：`table.insert` / `table.remove` / `table.move` 在 Lua 5.4 兼容模式下已改用严格整数参数检查；`1.2` 等无整数表示的位置参数会报错，数字字符串仍按官方 Lua 5.4 接受。
  - 当前进展：`table.insert` / `table.remove` 在 Lua 5.4 兼容模式下移动元素时会通过 `__index` 读取、通过 `__newindex` 写入代理表。
  - 当前进展：`table.move` 在 Lua 5.4 兼容模式下已改用 API get/set 路径移动元素，因此会通过 `__index` 读取源值、通过 `__newindex` 写入目标值。
  - 当前进展：`table.move` 的 Lua 5.4 参数检查顺序已对齐，先检查 `f/e/t` 整数参数，再检查源表和目标表；缺参时会优先报第 2 个参数。
  - 当前进展：`table.insert` / `table.remove` 的默认长度已对齐当前兼容层的 Lua 5.4 表库长度语义，支持带 `__len` 的表以及 `{1,nil,3}` 这类新建 list table 中间 nil 洞的默认尾部操作。
  - 当前进展：`table.concat` 的显式 `i` / `j` 已改用严格整数参数检查。
  - 当前进展：`table.unpack` 的默认终点已共用 Lua 5.4 表库长度兼容逻辑，并对显式 `i` / `j` 做严格整数检查；读取元素时会通过 `__index` 访问代理表。
  - 当前进展：`table.concat` 在 Lua 5.4 兼容模式下读取元素时会通过 `__index` 访问代理表，并保留 nil/非 string/number 元素的错误检查。
  - 当前进展：`table.sort` 的排序范围已共用 Lua 5.4 表库长度兼容逻辑，因此会尊重 `__len`，并在 `__len` 返回无整数表示的值时报 `object length is not an integer`。
  - 当前进展：`table.sort` 在 Lua 5.4 兼容模式下会在分区扫描到 pivot 哨兵时拒绝非严格 comparator，例如 `a <= b` / `a >= b`，并报 `invalid order function for sorting`。
  - 当前进展：`table.sort` 在 Lua 5.4 兼容模式下已改用 API get/set 路径读写元素，因此代理表排序会通过 `__index` 读取、通过 `__newindex` 写入。
  - 当前进展：`table.unpack` 不再入口强制 table；默认终点会先触发 length 语义，显式空范围可对 nil/number 直接返回空结果，实际读取时再由普通索引路径报错。
  - 已覆盖：显式 `i/j` 空范围、`table.sort` comparator 错误传播、非严格 comparator 报错，以及官方 5.4.8 对照中允许的 always-true / always-false comparator 结果。
  - 说明：逐字错误文本继续归入“标准库错误消息与边界参数完全对齐”。

- [x] 严格 Lua 5.4 语法表面。
  - 当前状态：Lua 5.4 兼容模式会拒绝 LuaJIT-only 数字字面量扩展；FFI 库本身仍作为 LuaJIT 扩展保留，但不再开放这些非官方 numeric literal 语法。
  - 当前进展：`load("return 0b1010")`、`load("return 1L")`、`load("return 1LL")`、`load("return 1UL")`、`load("return 1ULL")`、`load("return 1uLL")`、`load("return 1i")` 在 Lua 5.4 兼容模式下均报 malformed number。
  - 已覆盖：二进制数字字面量、FFI integer suffix 和 imaginary suffix。

- [x] standalone / 环境变量兼容边界。
  - 当前状态：已完成当前清单中列出的 Lua 5.4 standalone 行为核对和 smoke 覆盖。
  - 当前进展：Lua 5.4 兼容构建的 standalone 已优先读取 `LUA_INIT_5_4`，找不到时再回退 `LUA_INIT`；package 初始化会优先读取 `LUA_PATH_5_4` / `LUA_CPATH_5_4`，再回退旧 `LUA_PATH` / `LUA_CPATH`。
  - 当前进展：Lua 5.4 兼容构建无脚本、仅执行 `-e` 时，`arg[0]` 现在是程序名，`arg[1]` 是 `-e`，`arg[2]` 是命令字符串；有脚本时仍保持 `arg[0]` 为脚本名。
  - 当前进展：Lua 5.4 兼容构建的 `-l g=mod` 已按官方 standalone 行为把 `require(mod)` 的返回值写入 `_G[g]`。
  - 当前进展：Lua 5.4 兼容构建已支持 standalone `-W`，会在执行 `-e` chunk 或脚本前开启 warning 输出。
  - 当前进展：`-E` 已覆盖忽略 `LUA_INIT_5_4` 和 versioned package path/cpath 环境变量。
  - 当前进展：Lua 5.4 兼容构建的 `-i` 交互启动不再额外打印 LuaJIT 的 `JIT:` 状态行，保留默认 LuaJIT 构建的原有交互输出。
  - 当前进展：脚本文件、`-- script` 和 stdin `-` 的 `arg` 表组合已按官方 Lua 5.4.8 对照进入 smoke。
  - 当前进展：兼容构建默认 `package.path` / `package.cpath` 已改用 Lua 5.4 风格搜索顺序；Windows 覆盖 executable-local `lua` 目录、`..\\share\\lua\\5.4`、`..\\lib\\lua\\5.4` 和当前目录 fallback，非 Windows 覆盖 `/usr/local/share/lua/5.4`、`/usr/local/lib/lua/5.4` 和当前目录 fallback。
  - 已覆盖：`LUA_INIT_5_4` 覆盖旧 `LUA_INIT`，`LUA_PATH_5_4` / `LUA_CPATH_5_4` 覆盖旧 path/cpath 环境变量，`-E` 忽略环境变量，无脚本 `-e`、脚本文件、`-- script` 和 stdin `-` 的 `arg` 表形态，`-l g=mod`，`-W` 开启 warning，`-i` 不输出额外 `JIT:` 状态行，以及默认 `package.path` / `package.cpath` 的 Lua 5.4 搜索目录形态。

- [ ] JIT/trace 对 Lua 5.4 新语义的记录。
  - 当前状态：多个新语法通过 helper 调用实现，语义优先于 JIT 性能。
  - 当前进展：已补 JIT smoke，在当前 PC 兼容构建中开启 JIT、降低 hotloop 后运行包含 `//`、位运算和局部 `_ENV` 的热循环，并用 `jit.util.traceinfo()` 确认产生 trace。
  - 当前进展：已补 JIT smoke，覆盖开启 JIT 后 `math.random(1, 4)` 区间路径在热循环内返回整数区间值，并确认产生 trace。
  - 当前进展：已补 JIT smoke，覆盖开启 JIT 后 `tonumber()` 在热循环中仍拒绝 `inf` / `nan` / `0b` 等 LuaJIT 扫描器扩展数字字符串，并确认产生 trace。
  - 当前进展：已补 JIT smoke，覆盖字符串 metatable 的 `__add` 优先于字符串数字转换，并在热循环中产生 trace。
  - 当前进展：已补 JIT smoke，覆盖 Lua 5.4 generic for 第 4 个 closing value 采用 `false` 时，`next` 热循环仍可产生 trace。
  - 当前进展：已补 JIT smoke，覆盖真实 `_ENV` upvalue 经 `debug.setupvalue` 替换为自定义 table 后，隐式全局访问仍可在热循环中执行并产生 trace。
  - 当前进展：`jit._lua54_*` helper 字段访问已处理大 chunk 常量表超过 255 时的 `TGETS` 索引截断问题，超出 8 位范围时改用 `KSTR + TGETV`。
  - 当前进展：Lua 5.4 兼容模式仍隐藏启动全局 `bit`，但已把 `bit` 作为显式 preload 模块保留，避免 `jit.dump` / `-jdump` 因内部 `require("bit")` 失败。
  - 需要补测试：继续扩展到更多 Lua 5.4 helper 路径，并在 unsupported trace 路径上补退出或 recorder。

## 已确认不列入当前 TODO 的已实现项

- `_VERSION == "Lua 5.4"`、`jit.lua54compat == true`。
- Lua 5.4 模式隐藏旧 Lua 5.1/LuaJIT API：`getfenv`、`setfenv`、`module`、`newproxy`、`loadstring`、全局 `unpack`、`bit` 等。
- `rawlen`、`table.pack`、`table.unpack`、`table.move`、`coroutine.isyieldable([co])` 已可见并覆盖可选 thread 参数。
- `load(..., env)` 已能让 chunk 使用传入环境。
- `pairs` 已支持 `__pairs`；`ipairs` 已按 Lua 5.4 使用普通索引访问，不走旧 `__ipairs`。
- `package.searchers`、`require` loader data、`utf8` 基础库、`warn`、`math.randomseed(x, y)`、`math.random` 的基础 Lua 5.4 行为已进入 smoke 覆盖。
