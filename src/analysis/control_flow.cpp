#include "control_flow.hpp"

#include "common.hpp"

int Function::alloc_temp(int size, bool is_array) {
  int offset = temp_stack_size;
  temp_stack_size += size;
  
  // 对于数组，返回正偏移量；对于普通变量，返回负偏移量
  return is_array ? offset : -offset - size;
}

int Function::alloc_reg(int size) {
  // 为保存寄存器分配空间，保存寄存器通常分配在靠近 fp 的位置
  // 返回值是相对于 fp 的偏移
  int offset = reg_stack_size;
  reg_stack_size += size;
  // 返回相对于fp的偏移，由于保存的寄存器位于fp下方，所以使用负偏移
  return -offset - size;
}

IR::Code Module::get_ir() const {
  IR::Code code;
  
  // 首先添加全局变量定义
  std::copy(global_vars.begin(), global_vars.end(), std::back_inserter(code));
  
  // 然后添加函数定义
  for (const auto &func : functions) {
    for (const auto &block : func->blocks) {
      std::copy(block->ir_code.begin(), block->ir_code.end(),
                std::back_inserter(code));
    }
  }
  return code;
}