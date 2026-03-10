#include "cfg_builder.hpp"

#include <unordered_map>
#include <unordered_set>

Module CFGBuilder::build(IR::Code code) {
  Module mod;
  IR::Code current_func;
  
  // 先收集全局变量
  for (auto it = code.begin(); it != code.end();) {
    if (auto global = std::dynamic_pointer_cast<IR::Global>(*it)) {
      // 找到全局变量定义，添加到module中
      mod.global_vars.push_back(*it);
      it = code.erase(it);  
    } else {
      ++it;
    }
  }

  // 按函数分割IR代码
  for (const auto &inst : code) {
    if (auto func = std::dynamic_pointer_cast<IR::Function>(inst)) {
      if (!current_func.empty()) {
        mod.functions.push_back(build_single_func(current_func));
        current_func.clear();
      }
    }
    current_func.push_back(inst);
  }

  if (!current_func.empty()) {
    mod.functions.push_back(build_single_func(current_func));
  }

  return mod;
}

FunctionPtr CFGBuilder::build_single_func(IR::Code code) {
  std::vector<BasicBlockPtr> blocks;
  std::unordered_map<std::string, BasicBlockPtr> label_to_block;
  std::string func_name;
  
  bool ret_void = true;
  
  // 首先检查是否有给a0赋值的指令，如果有，说明是有返回值的函数
  for (const auto &inst : code) {
    if (auto ret = std::dynamic_pointer_cast<IR::Return>(inst)) {
      if (!ret->x.empty()) {
        ret_void = false;  
        break;
      }
    }
    else if (auto assign = std::dynamic_pointer_cast<IR::Assign>(inst)) {
      if (assign->x == "a0") {
        ret_void = false; 
        break;
      }
    }
  }

  BasicBlockPtr current_block = BasicBlock::create("entry");
  for (const auto &inst : code) {
    if (auto func = std::dynamic_pointer_cast<IR::Function>(inst)) {
      func_name = func->name;
      current_block->label = func_name + ".entry";
      current_block->ir_code.push_back(inst);
    } else if (auto ret = std::dynamic_pointer_cast<IR::Return>(inst)) {
      if (ret->x.empty()) {
        current_block->ir_code.push_back(IR::Goto::create(func_name + ".ret"));
        ret_void = true;
      } else {
        current_block->ir_code.push_back(IR::Assign::create("a0", ret->x));
        current_block->ir_code.push_back(IR::Goto::create(func_name + ".ret"));
      }
    } else {
      current_block->ir_code.push_back(inst);
    }
  }
  blocks.push_back(current_block);
  label_to_block[current_block->label] = current_block;

  auto exit_block = BasicBlock::create(func_name + ".ret");
  exit_block->ir_code.push_back(IR::Label::create(func_name + ".ret"));
  if (!ret_void) {
    // 有返回值的函数，返回a0
    exit_block->ir_code.push_back(IR::Return::create("a0"));
  } else {
    // 无返回值的函数，不返回任何值
    exit_block->ir_code.push_back(IR::Return::create());
  }
  blocks.push_back(exit_block);
  label_to_block[func_name + ".ret"] = exit_block;

  return Function::create(func_name, blocks);
}

std::string CFGBuilder::new_label() {
  static int label_count = 0;
  return "LN" + std::to_string(label_count++);
}
