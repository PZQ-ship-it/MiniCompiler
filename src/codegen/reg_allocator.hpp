#ifndef CODEGEN_REG_ALLOCATOR_HPP
#define CODEGEN_REG_ALLOCATOR_HPP

#include "analysis/control_flow.hpp"
#include "asm.hpp"
#include <map>
#include <string>

class RegAllocator {
public:
  // 构造函数
  RegAllocator() = default;
  
  // 主函数
  void allocate(Module &mod);
  void allocate(FunctionPtr &func);

private:
  // 变量别名映射 (如果T1是a的副本，则var_aliases[T1] = a)
  std::map<std::string, std::string> var_aliases;
  
  // 收集变量并分配栈空间
  std::map<std::string, int> collectVariablesAndAllocateStack(FunctionPtr &func);
  
  // 优化指令
  void optimizeInstructions(FunctionPtr &func, const std::map<std::string, int> &var_offsets);
  void optimizeBlockInstructions(BasicBlockPtr &block, const std::map<std::string, int> &var_offsets);
  
  // 处理特殊指令
  bool handleSpecialInstruction(ASM::InstPtr &inst, 
                              ASM::Code::iterator it,
                              const ASM::Code &original_code,
                              ASM::Code &new_code,
                              const std::map<std::string, int> &var_offsets);
  
  // 从栈中加载变量到寄存器
  ASM::Reg loadValueToReg(const std::string &varName, 
                         const ASM::Reg &targetReg,
                         ASM::Code &new_code,
                         const std::map<std::string, int> &var_offsets);
  
  // 将寄存器值存回栈
  void storeRegToStack(const std::string &varName,
                      const ASM::Reg &sourceReg,
                      ASM::Code &new_code,
                      const std::map<std::string, int> &var_offsets);
  
    // 添加resolveAlias函数声明
  std::string resolveAlias(const std::string &varName);
  
  // 修改handleGenericInstruction函数声明
  void handleGenericInstruction(ASM::InstPtr &inst,
                              ASM::Code &new_code,
                              const std::map<std::string, int> &var_offsets);
};

#endif // CODEGEN_REG_ALLOCATOR_HPP