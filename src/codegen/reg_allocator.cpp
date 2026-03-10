#include "reg_allocator.hpp"
#include <set>
#include <cassert>
#include <iostream>

// 寄存器分配整体控制
void RegAllocator::allocate(Module &mod) {
  // 处理每个函数的寄存器分配
  for (auto &func : mod.functions) {
    allocate(func);
  }
}

// 主函数：寄存器分配
void RegAllocator::allocate(FunctionPtr &func) {
  // 1. 收集所有变量并分配栈空间
  auto var_offsets = collectVariablesAndAllocateStack(func);
  
  // 2. 优化所有基本块的指令
  optimizeInstructions(func, var_offsets);
  
  // 3. 保存寄存器映射（虽然朴素策略不需要，但为了保持与其他接口一致）
  func->reg_map.clear();
}

std::map<std::string, int> RegAllocator::collectVariablesAndAllocateStack(FunctionPtr &func) {
  std::map<std::string, int> var_offsets;
  
  // 首先处理所有IR指令，识别DEC指令分配的变量
  for (auto &block : func->blocks) {
    for (auto &ir_inst : block->ir_code) {
      if (auto dec = std::dynamic_pointer_cast<IR::DEC>(ir_inst)) {
        // 为数组分配栈空间并记录偏移量
        int offset = func->alloc_temp(dec->size);
        var_offsets[dec->x] = offset;
      }
    }
  }
  
  // 收集其他虚拟寄存器并分配栈空间
  std::set<std::string> vars;
  for (auto &block : func->blocks) {
    for (auto &inst : block->asm_code) {
      for (auto &reg : inst->get_uses()) {
        if (!reg.is_phys() && var_offsets.find(reg.name) == var_offsets.end()) {
          vars.insert(reg.name);
        }
      }
      for (auto &reg : inst->get_defs()) {
        if (!reg.is_phys() && var_offsets.find(reg.name) == var_offsets.end()) {
          vars.insert(reg.name);
        }
      }
    }
  }
  
  // 为其他变量分配栈空间
  for (const auto &var : vars) {
    int offset = func->alloc_temp(4);
    var_offsets[var] = offset;
  }
  
  return var_offsets;
}

// 优化所有基本块的指令
void RegAllocator::optimizeInstructions(FunctionPtr &func, 
                                      const std::map<std::string, int> &var_offsets) {
  var_aliases.clear(); // 清空别名映射
  
  // 处理每个基本块的指令
  for (auto &block : func->blocks) {
    ASM::Code new_code;
    
    // 处理每条指令
    for (auto it = block->asm_code.begin(); it != block->asm_code.end(); ++it) {
      auto &inst = *it;
      
      // 特殊指令处理（标签，跳转，函数调用等）
      if (handleSpecialInstruction(inst, it, block->asm_code, new_code, var_offsets)) {
        continue;
      }
      
      // 所有其他指令用通用处理方法
      handleGenericInstruction(inst, new_code, var_offsets);
    }
    
    // 替换原始指令序列
    block->asm_code = std::move(new_code);
  }
}

// 处理特殊指令（标签、跳转、函数调用等）
bool RegAllocator::handleSpecialInstruction(ASM::InstPtr &inst,
                                          ASM::Code::iterator it,
                                          const ASM::Code &original_code,
                                          ASM::Code &new_code,
                                          const std::map<std::string, int> &var_offsets) {
  // 处理标签
  if (auto label = std::dynamic_pointer_cast<ASM::Label>(inst)) {
    new_code.push_back(inst);
    return true;
  }
  
  // 处理无条件跳转
  if (auto jump = std::dynamic_pointer_cast<ASM::Jump>(inst)) {
    new_code.push_back(inst);
    return true;
  }
  
  // 处理函数调用
  if (auto call = std::dynamic_pointer_cast<ASM::Call>(inst)) {
    new_code.push_back(call);
    
    // 处理函数返回值
    auto next_it = std::next(it);
    if (next_it != original_code.end()) {
      auto next_inst = *next_it;
      if (auto mv = std::dynamic_pointer_cast<ASM::Mv>(next_inst)) {
        if (mv->rs == ASM::Reg::a0 && !mv->rd.is_phys()) {
          // 函数返回值直接存入栈
          int offset = var_offsets.at(mv->rd.name);
          new_code.push_back(ASM::Store::create(ASM::Reg::sp, ASM::Reg::a0, offset));
          ++it; // 跳过下一条指令
        }
      }
    }
    return true;
  }
  
  // 处理返回指令
  if (auto ret = std::dynamic_pointer_cast<ASM::Ret>(inst)) {
    new_code.push_back(ret);
    return true;
  }
  
  // 不是特殊指令
  return false;
}

// 在loadValueToReg函数中添加简单的优化逻辑

ASM::Reg RegAllocator::loadValueToReg(const std::string &varName,
                                    const ASM::Reg &targetReg,
                                    ASM::Code &new_code,
                                    const std::map<std::string, int> &var_offsets) {
  // 检查是否是物理寄存器
  ASM::Reg varReg(varName);
  if (varReg.is_phys()) {
    if (varName != targetReg.name) {
      new_code.push_back(ASM::Mv::create(targetReg, varReg));
    }
    return targetReg;
  }
  
  // 查找实际的变量名，处理别名
  std::string actual_var = resolveAlias(varName);
  
  // 确保变量有栈空间分配
  if (var_offsets.find(actual_var) == var_offsets.end()) {
    std::cerr << "错误: 变量 " << varName << " 未分配栈空间" << std::endl;
    assert(false && "变量未在栈上分配空间");
  }
  
  // 从栈上加载
  int offset = var_offsets.at(actual_var);
  new_code.push_back(ASM::Load::create(targetReg, ASM::Reg::sp, offset));
  return targetReg;
}

void RegAllocator::storeRegToStack(const std::string &varName,
                                 const ASM::Reg &sourceReg,
                                 ASM::Code &new_code,
                                 const std::map<std::string, int> &var_offsets) {
  // 检查是否是物理寄存器
  ASM::Reg varReg(varName);
  if (varReg.is_phys()) {
    if (varName != sourceReg.name) {
      new_code.push_back(ASM::Mv::create(varReg, sourceReg));
    }
    return;
  }
  
  // 处理虚拟寄存器
  std::string actual_var = resolveAlias(varName);
  
  // 检查变量在栈上的位置
  if (var_offsets.find(actual_var) == var_offsets.end()) {
    std::cerr << "错误: 变量 " << varName << " (实际: " << actual_var 
              << ") 未在栈上分配空间" << std::endl;
    assert(false && "变量未在栈上分配空间");
  }
  
  // 存储到栈上
  int offset = var_offsets.at(actual_var);
  new_code.push_back(ASM::Store::create(ASM::Reg::sp, sourceReg, offset));
}

// 递归解析变量别名
std::string RegAllocator::resolveAlias(const std::string &varName) {
  std::string result = varName;
  std::set<std::string> visited;
  
  while (var_aliases.find(result) != var_aliases.end() && 
         visited.find(result) == visited.end()) {
    visited.insert(result);
    result = var_aliases[result];
  }
  
  return result;
}

// 通用指令处理函数
void RegAllocator::handleGenericInstruction(ASM::InstPtr &inst,
                                          ASM::Code &new_code,
                                          const std::map<std::string, int> &var_offsets) {
  // 固定使用t0, t1, t2临时寄存器
  ASM::Reg t0 = ASM::Reg::t0;
  ASM::Reg t1 = ASM::Reg::t1;
  ASM::Reg t2 = ASM::Reg::t2;
  
  // 处理移动指令（特殊处理，可以建立别名关系）
  if (auto mv = std::dynamic_pointer_cast<ASM::Mv>(inst)) {
    // 跳过自我移动
    if (mv->rd.name == mv->rs.name) return;
    
    // 建立变量别名关系
    if (!mv->rd.is_phys() && !mv->rs.is_phys()) {
      var_aliases[mv->rd.name] = mv->rs.name;
    }
    
    // 加载源值
    if (mv->rs.is_phys()) {
      // 源是物理寄存器
      if (mv->rd.is_phys()) {
        // 目标也是物理寄存器，直接移动
        new_code.push_back(ASM::Mv::create(mv->rd, mv->rs));
      } else {
        // 目标是虚拟寄存器，存入栈
        storeRegToStack(mv->rd.name, mv->rs, new_code, var_offsets);
      }
    } else {
      // 源是虚拟寄存器
      loadValueToReg(mv->rs.name, t0, new_code, var_offsets);
      if (mv->rd.is_phys()) {
        // 目标是物理寄存器，直接移动
        new_code.push_back(ASM::Mv::create(mv->rd, t0));
      } else {
        // 目标是虚拟寄存器，存入栈
        storeRegToStack(mv->rd.name, t0, new_code, var_offsets);
      }
    }
    return;
  }
  
  // 处理算术指令
  if (auto arith = std::dynamic_pointer_cast<ASM::Arith>(inst)) {
    // 加载操作数
    loadValueToReg(arith->rs1.name, t0, new_code, var_offsets);
    loadValueToReg(arith->rs2.name, t1, new_code, var_offsets);
    
    // 执行运算
    new_code.push_back(ASM::Arith::create(t2, t0, t1, arith->op));
    
    // 存储结果
    storeRegToStack(arith->rd.name, t2, new_code, var_offsets);
    return;
  }
  
  if (auto arithImm = std::dynamic_pointer_cast<ASM::ArithImm>(inst)) {
    // 检测是否是数组基址计算指令(DEC指令生成的)
    if (arithImm->rs == ASM::Reg::sp && 
        arithImm->rd.name.find("a_") == 0 && 
        arithImm->op == ASM::ArithImm::Op::Addi) {
      // 直接使用原始指令，无需修改偏移量
      // DEC指令生成的数组地址已经正确设置了偏移量(正值)
      new_code.push_back(ASM::ArithImm::create(
          t0, ASM::Reg::sp, arithImm->imm, arithImm->op));
      
      // 存储结果到变量
      storeRegToStack(arithImm->rd.name, t0, new_code, var_offsets);
      return;
    }
    
    // 正常处理其他立即数算术指令
    loadValueToReg(arithImm->rs.name, t0, new_code, var_offsets);
    new_code.push_back(ASM::ArithImm::create(t1, t0, arithImm->imm, arithImm->op));
    storeRegToStack(arithImm->rd.name, t1, new_code, var_offsets);
    return;
  }
  
  // 处理加载立即数指令
  if (auto li = std::dynamic_pointer_cast<ASM::Li>(inst)) {
    // 执行加载
    new_code.push_back(ASM::Li::create(t0, li->imm));
    
    // 存储结果
    storeRegToStack(li->rd.name, t0, new_code, var_offsets);
    return;
  }
  
  // 处理加载地址指令
  if (auto la = std::dynamic_pointer_cast<ASM::La>(inst)) {
    // 执行加载地址
    new_code.push_back(ASM::La::create(t0, la->symbol));
    
    // 存储结果
    storeRegToStack(la->rd.name, t0, new_code, var_offsets);
    return;
  }
  
  // 修改Store指令的处理部分
  if (auto store = std::dynamic_pointer_cast<ASM::Store>(inst)) {
    // 从栈上加载基址 - 确保加载的是正确的地址值
    if (store->rs1.name.find("a_") == 0) {
      // 对于数组基址变量，确保直接从栈上加载基址值
      if (var_offsets.find(store->rs1.name) != var_offsets.end()) {
        int base_offset = var_offsets.at(store->rs1.name);
        new_code.push_back(ASM::Load::create(t0, ASM::Reg::sp, base_offset));
      } else {
        loadValueToReg(store->rs1.name, t0, new_code, var_offsets);
      }
    } else {
      loadValueToReg(store->rs1.name, t0, new_code, var_offsets);
    }
    
    // 加载要存储的值
    loadValueToReg(store->rs2.name, t1, new_code, var_offsets);
    
    // 使用正确的基址和偏移执行存储
    new_code.push_back(ASM::Store::create(t0, t1, store->offset));
    return;
  }
  
  // 修改Load指令的处理部分
  if (auto load = std::dynamic_pointer_cast<ASM::Load>(inst)) {
    // 从栈上加载基址 - 确保加载的是正确的地址值
    if (load->rs1.name.find("a_") == 0) {
      // 对于数组基址变量，确保直接从栈上加载基址值
      if (var_offsets.find(load->rs1.name) != var_offsets.end()) {
        int base_offset = var_offsets.at(load->rs1.name);
        new_code.push_back(ASM::Load::create(t0, ASM::Reg::sp, base_offset));
      } else {
        loadValueToReg(load->rs1.name, t0, new_code, var_offsets);
      }
    } else {
      loadValueToReg(load->rs1.name, t0, new_code, var_offsets);
    }
    
    // 从加载的地址中获取值
    new_code.push_back(ASM::Load::create(t1, t0, load->offset));
    
    // 存储结果
    storeRegToStack(load->rd.name, t1, new_code, var_offsets);
    return;
  }
  
  // 处理条件分支指令
  if (auto branch = std::dynamic_pointer_cast<ASM::Branch>(inst)) {
    // 加载比较的值
    loadValueToReg(branch->rs1.name, t0, new_code, var_offsets);
    loadValueToReg(branch->rs2.name, t1, new_code, var_offsets);
    
    // 执行分支
    new_code.push_back(ASM::Branch::create(t0, t1, branch->op, branch->label));
    return;
  }
  
  // 其他类型的指令直接添加
  new_code.push_back(inst);
}