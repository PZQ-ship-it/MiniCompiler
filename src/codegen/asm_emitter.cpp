#include "asm_emitter.hpp"

void ASMEmitter::emit(const Module &mod) {
  // 首先添加全局数据段
  if (!mod.global_vars.empty()) {
    output << "    .section .data" << std::endl;  // 修改为.section .data
    // 依次处理每个全局变量
    for (const auto &var : mod.global_vars) {
      if (auto global_var = std::dynamic_pointer_cast<IR::Global>(var)) {
        // 使用静态工厂方法创建GlobalVar指令
        if (!global_var->init_values.empty()) {
          // 如果有初始值，使用带初始值的create方法
          auto global = ASM::GlobalVar::create(global_var->name, global_var->size, global_var->init_values);
          output << global->to_string() << std::endl;
        } else {
          // 没有初始值，使用基本create方法
          auto global = ASM::GlobalVar::create(global_var->name, global_var->size);
          output << global->to_string() << std::endl;
        }
      }
    }
    output << std::endl;
  }

  // 添加 Venus 的 read 和 write 系统调用
  if (use_venus) {
    output << R"(
    .text
    .globl read
read:
    li a0, 6
    ecall
    ret

    .globl write
write:
    mv a1, a0
    li a0, 1
    ecall
    ret

)";
  }

   // 添加代码段标记
   output << "    .section .text" << std::endl;  // 修改为.section .text

  for (const auto &func : mod.functions) {
    emit(func);
  }
}

void ASMEmitter::emit(const FunctionPtr &func) {
  int stack_size = func->temp_stack_size + func->reg_stack_size;
  
  // 确保16字节对齐
  stack_size = (stack_size + 15) & ~15;
  
  // 输出函数标签
  auto label_inst = func->blocks.front()->asm_code.front();
  output << label_inst->to_string() << std::endl;

  // 添加函数全局声明（针对main函数）
  if (func->blocks.front()->ir_code.front()->to_string().find("FUNCTION main:") != std::string::npos) {
    output << "    .globl main" << std::endl;
  }
  
  // 设置栈帧
  if (stack_size > 0) {
    output << "    addi sp, sp, -" << stack_size << std::endl;
    output << "    sw ra, " << (stack_size - 4) << "(sp)" << std::endl; 
    output << "    sw fp, " << (stack_size - 8) << "(sp)" << std::endl;
    output << "    addi fp, sp, " << stack_size << std::endl; // fp指向栈帧顶部
  }

  // 查找函数的所有退出点（包含ret指令的位置）
  std::vector<ASM::InstPtr*> ret_insts;
  
  // 扫描所有块中的ret指令
  for (auto &block : func->blocks) {
    for (auto it = block->asm_code.begin(); it != block->asm_code.end(); ++it) {
      if (std::dynamic_pointer_cast<ASM::Ret>(*it)) {
        ret_insts.push_back(&(*it));
      }
    }
  }
  
  // 输出函数体，但在每个ret指令之前插入栈帧恢复代码
  for (size_t i = 0; i < func->blocks.size(); ++i) {
    auto &block = func->blocks[i];
    
    // 跳过第一个块的第一条指令（已经输出过的函数标签）
    auto start_it = (i == 0) ? ++block->asm_code.begin() : block->asm_code.begin();
    
    for (auto it = start_it; it != block->asm_code.end(); ++it) {
      // 如果是ret指令，先输出栈帧恢复代码
      if (std::dynamic_pointer_cast<ASM::Ret>(*it) && stack_size > 0) {
        output << "    lw ra, " << (stack_size - 4) << "(sp)" << std::endl;
        output << "    lw fp, " << (stack_size - 8) << "(sp)" << std::endl;
        output << "    addi sp, sp, " << stack_size << std::endl;
      }
      
      // 输出当前指令
      emit(*it);
    }
  }
  
  // 不再需要在函数最后添加栈帧恢复代码，因为已经在每个ret指令前添加了
}
void ASMEmitter::emit(const ASM::InstPtr &inst) {
  // 不再需要在这里替换寄存器引用，因为已经在RegAllocator中完成
  if (auto label = std::dynamic_pointer_cast<ASM::Label>(inst)) {
    output << label->to_string() << std::endl;
  } else {
    output << "    " << inst->to_string() << std::endl;
  }
}