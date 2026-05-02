# Change Log

## 当前进展

- 已按要求采用 `Change.md` 记录修改、新功能和进展；仓库中未创建 `Modify.md`。
- 已完成实验性 Lua 5.4 兼容模式的阶段性实现与测试。
- 已通过 `make test` 验证默认构建和 Lua 5.4 兼容构建的 smoke 测试。

## 修改内容

- 新增 `LUAJIT_ENABLE_LUA54COMPAT` 构建开关。
- 新增内部 `LJ_54` 兼容标识。
- Lua 5.4 兼容模式会自动启用现有 Lua 5.2 兼容层作为基础。
- 默认构建保持 LuaJIT 原有 Lua 5.1 行为不变。
- 更新文档说明实验性 Lua 5.4 兼容模式、当前范围和限制。
- 新增顶层测试目标：
  - `make smoketest`
  - `make smoketest-lua54compat`
  - `make test`

## 新功能

- Lua 5.4 兼容构建中：
  - `_VERSION` 报告为 `Lua 5.4`。
  - `jit.lua54compat` 报告为 `true`。
  - 新增全局 `warn()`。
  - `warn()` 支持 `@on` / `@off` 控制 warning 输出。
  - `warn()` 输出写入 stderr。
  - warning 开关状态保存在 `global_State` 中。
- 默认构建中：
  - `_VERSION` 仍为 `Lua 5.1`。
  - `jit.lua54compat` 为 `false`。
  - 不暴露 `warn()`。

## 测试进展

- 新增 `test/smoke.lua`，覆盖默认模式和 Lua 5.4 兼容模式。
- 覆盖 `jit` 基本元信息。
- 覆盖 Lua 5.4 兼容模式下的 `_VERSION` 和 `jit.lua54compat`。
- 覆盖 Lua 5.4 兼容模式下的 `coroutine.running()` 主线程返回值。
- 覆盖 Lua 5.4 兼容模式下 `__len` 元方法行为。
- 覆盖 `warn()` 是否按模式暴露。
- 覆盖 `warn()` 参数类型检查。
- 覆盖 `warn()` stderr 输出。

## 验证结果

- `make test` 已通过。
- CodeQL 检查未发现安全告警。
- 自动代码审查提出的问题已处理：
  - 避免 `warn()` 使用进程级静态开关状态。
  - 调整 C89 风格变量声明。
  - 增加 stderr 输出测试。
  - 确保 warning 输出测试会检查 `luajit` 命令退出状态。
