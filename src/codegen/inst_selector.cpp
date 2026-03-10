#include "inst_selector.hpp"

#include <cassert>
#include <memory>

#include "codegen/asm_emitter.hpp"

void InstSelector::select(Module &mod) {
  for (auto &func : mod.functions) {
    select(func);
  }
}

void InstSelector::select(FunctionPtr &func) {
  // 设置当前函数
  // 如果是 DEC，需要调用当前函数中的 alloc_temp，在栈上分配空间
  current_func = func;
  for (auto &block : func->blocks) {
    block->asm_code = select(block->ir_code);
  }
}

ASM::Code InstSelector::select(const IR::Code &ir_code) {
  ASM::Code asm_code;
  for (const auto &node : ir_code) {
    auto code = select(node);
    asm_code.insert(asm_code.end(), code.begin(), code.end());
  }
  return asm_code;
}

ASM::Code InstSelector::select(const IR::NodePtr &node) {
#define SELECT_NODE(type)                                   \
  if (auto p = std::dynamic_pointer_cast<IR::type>(node)) { \
    return select##type(p);                                 \
  }

  // 对于每种不同类型的 IR 节点，调用相应的 select 函数
  // 如果你添加了新的 IR 节点类型，记得在这里添加对应的 select 函数
  SELECT_NODE(LoadImm)
  SELECT_NODE(Assign)
  SELECT_NODE(Binary)
  SELECT_NODE(Unary)
  SELECT_NODE(Label)
  SELECT_NODE(Goto)
  SELECT_NODE(Function)
  SELECT_NODE(Call)
  SELECT_NODE(Arg)
  SELECT_NODE(Return)
  SELECT_NODE(If)
  SELECT_NODE(Load)
  SELECT_NODE(Store)
  SELECT_NODE(GetAddr)
  SELECT_NODE(DEC)
  SELECT_NODE(Param)
  SELECT_NODE(Global)

#warning Add more IR node types if needed

  assert(false && "Unknown IR node type");
}

ASM::Code InstSelector::selectLoadImm(const IR::LoadImmPtr &node) {
  ASM::Code code;
  // x = #k -> li reg(x), k
  code.push_back(ASM::Li::create(ASM::Reg(node->x), node->k));
  return code;
}

ASM::Code InstSelector::selectAssign(const IR::AssignPtr &node) {
  ASM::Code code;
  // a = b	-> mv reg(a), reg(b)
  code.push_back(ASM::Mv::create(ASM::Reg(node->x), ASM::Reg(node->y)));
  return code;
}

ASM::Code InstSelector::selectBinary(const IR::BinaryPtr &node) {
  ASM::Code code;
  
  // 如果是立即数操作 (x = y op #k)
  if (node->z[0] == '#') {
    int imm = std::stoi(node->z.substr(1));
    ASM::ArithImm::Op op;
    switch (node->op) {
      case BinaryOp::Add: op = ASM::ArithImm::Op::Addi; break;
      case BinaryOp::Sub: op = ASM::ArithImm::Op::Addi; imm = -imm; break;
      // 其他立即数操作...
      default: assert(false && "Unsupported binary operation with immediate");
    }
    
    code.push_back(ASM::ArithImm::create(
        ASM::Reg(node->x), ASM::Reg(node->y), imm, op));
    
    return code;
  }

  // 非立即数操作 (x = y op z)
  ASM::Arith::Op op;
  switch (node->op) {
    case BinaryOp::Add: op = ASM::Arith::Op::Add; break;
    case BinaryOp::Sub: op = ASM::Arith::Op::Sub; break;
    case BinaryOp::Mul: op = ASM::Arith::Op::Mul; break;
    case BinaryOp::Div: op = ASM::Arith::Op::Div; break;
    case BinaryOp::Mod: op = ASM::Arith::Op::Rem; break;
    default: assert(false && "Unsupported binary operation");
  }
  
  code.push_back(ASM::Arith::create(
      ASM::Reg(node->x), ASM::Reg(node->y), ASM::Reg(node->z), op));
  
  return code;
}


ASM::Code InstSelector::selectUnary(const IR::UnaryPtr &node) {
  ASM::Code code;
  // x = unop y

  switch (node->op) {
    case BinaryOp::Add:  // 正号: x = +y -> mv reg(x), reg(y)
      code.push_back(ASM::Mv::create(ASM::Reg(node->x), ASM::Reg(node->y)));
      break;
    case BinaryOp::Sub:  // 负号: x = -y -> sub reg(x), zero, reg(y)
      code.push_back(ASM::Arith::create(
          ASM::Reg(node->x), ASM::Reg::zero, ASM::Reg(node->y), ASM::Arith::Op::Sub));
      break;
    default:
      assert(false && "Unsupported unary operation");
  }

  return code;
}

ASM::Code InstSelector::selectLabel(const IR::LabelPtr &node) {
  ASM::Code code;
  // LABEL label:	-> label:
  code.push_back(ASM::Label::create(node->label));
  return code;
}

ASM::Code InstSelector::selectGoto(const IR::GotoPtr &node) {
  ASM::Code code;
  // GOTO label	-> j label
  code.push_back(ASM::Jump::create(node->label));
  return code;
}

ASM::Code InstSelector::selectFunction(const IR::FunctionPtr &node) {
  ASM::Code code;
  // FUNCTION func: -> func:
  
  // 保存当前函数名称以供其他指令使用
  func_name = node->name;
  
  // 重置参数计数器
  param_index = 0;
  
  // 创建函数标签
  code.push_back(ASM::Label::create(node->name));
  
  return code;
}

ASM::Code InstSelector::selectCall(const IR::CallPtr &node) {
  ASM::Code code;
  // CALL f -> call f
  // 如果有返回值则将其保存到指定寄存器
  code.push_back(ASM::Call::create(node->func));
  
  // 修复：使用 node->x 替代 node->ret
  if (!node->x.empty()) {
    code.push_back(ASM::Mv::create(ASM::Reg(node->x), ASM::Reg::a0));
  }
  
  return code;
}

ASM::Code InstSelector::selectArg(const IR::ArgPtr &node) {
  ASM::Code code;
  // ARG x -> mv ak, reg(x)
  // k是参数的索引
  
  // 根据RISC-V调用约定:
  // 前8个参数通过a0-a7寄存器传递
  // 超过8个的参数通过栈传递
  
  if (node->k < 8) {
    // 前8个参数使用寄存器
    ASM::Reg arg_reg("a" + std::to_string(node->k));
    code.push_back(ASM::Mv::create(arg_reg, ASM::Reg(node->x)));
  } else {
    // 超过8个参数通过栈传递
    // 参数在调用者栈帧中的位置：sp + (k-8)*4
    int offset = (node->k - 8) * 4;
    code.push_back(ASM::Store::create(ASM::Reg::sp, ASM::Reg(node->x), offset));
  }
  
  return code;
}

ASM::Code InstSelector::selectReturn(const IR::ReturnPtr &node) {
  ASM::Code code;
  
  // 如果有返回值，则将其移动到a0寄存器
  if (!node->x.empty()) {
    code.push_back(ASM::Mv::create(ASM::Reg::a0, ASM::Reg(node->x)));
  }
  
  // 添加ret指令
  code.push_back(ASM::Ret::create());
  
  return code;
}
ASM::Code InstSelector::selectIf(const IR::IfPtr &node) {
  ASM::Code code;
  // IF x relop y GOTO label
  
  ASM::Branch::Op op;
  switch (node->op) {
    case BinaryOp::Eq:  // ==
      op = ASM::Branch::Op::Eq;
      break;
    case BinaryOp::Ne:  // !=
      op = ASM::Branch::Op::Ne;
      break;
    case BinaryOp::Lt:  // <
      op = ASM::Branch::Op::Lt;
      break;
    case BinaryOp::Le:  // <=
      op = ASM::Branch::Op::Le;
      break;
    case BinaryOp::Gt:  // >
      op = ASM::Branch::Op::Gt;
      break;
    case BinaryOp::Ge:  // >=
      op = ASM::Branch::Op::Ge;
      break;
    default:
      assert(false && "Unsupported relational operation");
  }
  
  code.push_back(ASM::Branch::create(
      ASM::Reg(node->x), ASM::Reg(node->y), op, node->label));
  
  return code;
}

ASM::Code InstSelector::selectLoad(const IR::LoadPtr &node) {
  ASM::Code code;
  // x = *y -> lw reg(x), 0(reg(y))
  // x = *(y + #k) -> lw reg(x), k(reg(y))
  code.push_back(ASM::Load::create(ASM::Reg(node->x), ASM::Reg(node->y), node->offset));
  return code;
}

ASM::Code InstSelector::selectStore(const IR::StorePtr &node) {
  ASM::Code code;
  // *(x + #k) = y -> sw reg(y), k(reg(x))
  // 使用虚拟寄存器，让寄存器分配器负责后续替换
  code.push_back(ASM::Store::create(ASM::Reg(node->x), ASM::Reg(node->y), node->offset));
  return code;
}

ASM::Code InstSelector::selectGetAddr(const IR::GetAddrPtr &node) {
  ASM::Code code;
  // x = &label -> la reg(x), label
  code.push_back(ASM::La::create(ASM::Reg(node->x), node->label));
  return code;
}

ASM::Code InstSelector::selectDEC(const IR::DECPtr &node) {
  ASM::Code code;
  
  // 为数组分配栈空间，使用is_array=true标记
  int offset = current_func->alloc_temp(node->size, true);
  
  // 直接使用计算出的偏移量
  code.push_back(ASM::ArithImm::create(
      ASM::Reg(node->x), ASM::Reg::sp, offset, ASM::ArithImm::Op::Addi));
  
  return code;
}

ASM::Code InstSelector::selectParam(const IR::ParamPtr &node) {
  ASM::Code code;
  
  // 每当遇到函数开始时重置参数索引
  if (node == current_func->blocks.front()->ir_code.front()) {
    param_index = 0;
  }
  
  // 根据RISC-V调用约定:
  // 前8个参数通过a0-a7寄存器传递，需要移动到本地变量
  // 超过8个的参数通过栈传递，需要从栈上加载
  if (param_index < 8) {
    // 从对应的参数寄存器加载参数值
    ASM::Reg arg_reg("a" + std::to_string(param_index));
    code.push_back(ASM::Mv::create(ASM::Reg(node->x), arg_reg));
  } else {
    // 超过8个参数从帧指针上方获取，即调用者的栈上
    // 假设fp已经正确设置，参数位置为：fp + (param_index-8)*4 + 8
    int offset = (param_index - 8) * 4 + 8; // +8是因为fp上方还有返回地址和原fp
    code.push_back(ASM::Load::create(ASM::Reg(node->x), ASM::Reg::fp, offset));
  }
  
  // 递增参数索引，为下一个参数做准备
  param_index++;
  
  return code;
}

// 添加selectGlobal函数的实现
ASM::Code InstSelector::selectGlobal(const IR::GlobalPtr &node) {
  ASM::Code code;
  
  if (node->init_values.empty()) {
    // GLOBAL x: #k -> x: .zero k
    code.push_back(ASM::GlobalVar::create(node->name, node->size));
  } else {
    // GLOBAL x: #k = #v1, #v2, ... -> x: .word v1, v2, ...
    code.push_back(ASM::GlobalVar::create(node->name, node->size, node->init_values));
  }
  
  return code;
}