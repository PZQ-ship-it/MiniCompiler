# MiniCompiler

一个基于 C++17 的编译器项目，用于将类 C/SysY 风格的源语言编译为 RISC‑V 汇编（支持生成兼容 Venus 的汇编），涵盖从词法/语法分析到中间表示、控制流图、寄存器分配和最终代码生成的完整编译流程。

## 功能概览

- **词法分析**：使用 Flex (`lexer/lexer.l`)
- **语法分析**：使用 Bison (`parser/parser.y`)
- **抽象语法树 (AST)**：`ast/` 下的结点定义与树构建
- **语义分析**：符号表与类型检查（`semantic/`）
- **中间表示 (IR)**：IR 翻译与构建（`ir/`）
- **控制流图 (CFG)**：在 IR 上构建控制流图（`analysis/`）
- **指令选择与寄存器分配**：`codegen/inst_selector.*`、`codegen/reg_allocator.*`
- **汇编生成**：输出 RISC‑V 汇编，支持 Venus 模式（`codegen/asm_emitter.*`）
- **可选输出**：
  - 输出 IR 文本
  - 生成 AST 的 Graphviz DOT 文件用于可视化

## 目录结构

- `src/`
  - `lexer/`：词法分析器（Flex 描述文件）
  - `parser/`：语法分析器（Bison 描述文件）
  - `ast/`：抽象语法树结点与相关操作
  - `semantic/`：符号表、类型系统与语义检查
  - `ir/`：中间表示及其翻译
  - `analysis/`：控制流分析与 CFG 构建
  - `codegen/`：指令选择、寄存器分配与汇编输出
  - `common.hpp`：公共类型与工具
  - `main.cpp`：编译器入口
- `lab0/`：示例/课程实验用测试程序（`.sy` 源文件）
- `reports/`：报告或文档占位目录
- `test.c` / `test.s`：简单的 C 示例及对应汇编
- `Makefile`：构建脚本
- `build.sh`：一键构建脚本（内部直接调用 `make`）
- `config.toml`：测试脚本配置（如是否使用 QEMU、是否开启并行测试等）

## 构建说明

### 环境依赖

建议在类 Unix 环境（Linux / macOS / WSL 等）下构建，需安装：

- `g++`（支持 C++17）
- `make`
- `flex`
- `bison`
- （可选）`clang-format`：用于代码格式化

### 编译

在项目根目录执行：

```bash
make
```

成功后会在根目录生成可执行文件：

- `compiler`：编译器主程序

也可以使用简单的构建脚本：

```bash
./build.sh
```

### 清理

```bash
make clean
```

会删除中间文件、生成的词法/语法分析器代码以及编译出的 `compiler` 可执行文件。

## 使用方法

编译器入口在 `src/main.cpp`，命令行参数格式为：

```bash
./compiler <input file> [output file] [--ir] [--venus] [--draw-ast [dot file]]
```

含义说明：

- `<input file>`：输入源文件（类 C/SysY 语言）
- `[output file]`：可选，输出文件路径；省略时输出到标准输出
- `--ir`：输出中间表示 IR 文本，而非汇编
- `--venus`：生成兼容 Venus 的 RISC‑V 汇编
- `--draw-ast [dot file]`：生成 AST 的 Graphviz DOT 文件；
  - 若省略 `dot file`，默认文件名为 `ast.dot`

> 注意：`--ir` 与 `--venus` 不能同时使用。

### 示例

1. 将 SysY 源文件编译为汇编：

```bash
./compiler lab0/test.sy output.S
```

2. 仅输出 IR：

```bash
./compiler lab0/test.sy --ir > ir.txt
```

3. 生成 AST 的 DOT 文件并可视化：

```bash
./compiler lab0/test.sy --draw-ast ast.dot
# 之后可用 Graphviz 生成图片
# dot -Tpng ast.dot -o ast.png
```

4. 生成兼容 Venus 的汇编：

```bash
./compiler lab0/test.sy --venus output_venus.S
```

## 测试与配置

项目根目录下的 `config.toml` 用于控制测试脚本行为（如课程提供的测试仓库 `sp25-tests` 等）：

- `use_qemu`：是否使用 rv32 工具链与 QEMU 运行部分实验的测试
- `use_accipit`：是否使用 accipit IR 进行测试
- `parallel`：是否开启并行测试
- `check_ssa`：是否在 IR 解释执行时检查 SSA 形式
- `precise_timing`：是否在使用 ELF 运行程序时启用精确计时
- `timeout`：单个测试的编译与运行超时时间（秒）

如果课程环境中提供了 `sp25-tests` 等测试脚本，可在项目根目录执行：

```bash
make test
```

具体测试仓库的放置位置及使用方式以课程说明为准。

## 开发与格式化

- 代码风格由根目录下的 `.clang-format` 提供统一配置
- 可以通过下面命令对 `src/` 下的 C++ 源码自动格式化：

```bash
make format
```
