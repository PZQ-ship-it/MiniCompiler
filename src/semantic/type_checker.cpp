#include "type_checker.hpp"
#include <iostream>
#include "common.hpp"

TypeChecker::TypeChecker() : current_function_return_type(nullptr) {
  // 你需要在这里对 symbol_table 进行初始化
  // 插入一些内置函数，如 read 和 write
  // 初始化符号表 - 创建全局作用域
  symbol_table.enter_scope();
  
 
  auto read_func_type = FuncType::create(
    PrimitiveType::Int,  
    {} 
  );
  symbol_table.add_symbol("read", read_func_type);
  
  // 添加内置函数 write
  auto write_func_type = FuncType::create(
    PrimitiveType::Void,  
    {PrimitiveType::Int}  
  );
  symbol_table.add_symbol("write", write_func_type);
}

TypePtr TypeChecker::check(AST::NodePtr node) {
#define CHECK_NODE(type)                                     \
  if (auto n = std::dynamic_pointer_cast<AST::type>(node)) { \
    return check##type(n);                                   \
  }

  // 递归检查 AST 的每个节点
  // 如果你添加了新的 AST 节点类型，记得在这里添加对应的检查函数
  CHECK_NODE(CompUnit)
  CHECK_NODE(FuncDef)
  CHECK_NODE(VarDecl)
  CHECK_NODE(Block)
  CHECK_NODE(AssignStmt)
  CHECK_NODE(ReturnStmt)
  CHECK_NODE(LVal)
  CHECK_NODE(IntConst)
  CHECK_NODE(FuncCall)
  CHECK_NODE(UnaryExp)
  CHECK_NODE(BinaryExp)
  CHECK_NODE(IfStmt)
  CHECK_NODE(WhileStmt)

  // 对于其他未实现检查的节点类型，返回null
  return nullptr;

#undef CHECK_NODE

  // ASSERT(false, "Unknown AST node type " + node->to_string() +
  //                   " in type checking at line " +
  //                   std::to_string(node->lineno));
}

TypePtr TypeChecker::checkCompUnit(AST::CompUnitPtr node) {
  for (auto &unit : node->units) {
    check(unit);
  }
  // 打印整个符号表内容
  print_symbol_table();
  return nullptr;
}

TypePtr TypeChecker::checkFuncDef(AST::FuncDefPtr node) {
  // 在这个函数中，你需要判断函数是否已经被定义过
  // 如果函数已经被定义过，你需要报错
  // 否则，你需要将函数插入符号表，并在符号表中创建一个新的作用域
  // 再将函数参数也插入符号表，并将符号表中对应的 symbol 挂到 FuncDef 节点上
  // 最后检查函数体的语句块

  // 检查函数是否已经被定义过
  auto symbol = symbol_table.find_symbol(node->name);
  if (symbol) {
    throw std::runtime_error("Function '" + node->name + "' already defined at line " + 
                             std::to_string(node->lineno));
  }
  
  
  std::vector<TypePtr> param_types;

  for (auto& param : node->params) {
    if (param->is_array) {
      // 数组参数处理
      std::vector<int> dims;
      dims.push_back(0); // 第一维的大小为0，表示可变
      for (int dim : param->array_dims) {
        dims.push_back(dim);
      }
      param_types.push_back(ArrayType::create(PrimitiveType::Int, dims));
    } else {
      param_types.push_back(PrimitiveType::Int);
    }
  }
  
 // 创建函数类型并添加到符号表
 auto return_type = node->return_btype == BasicType::Int ? 
 PrimitiveType::Int : PrimitiveType::Void;
 auto func_type = FuncType::create(return_type, param_types);

 // 添加到符号表并设置 node->symbol
 node->symbol = symbol_table.add_symbol(node->name, func_type);
  
  auto prev_return_type = current_function_return_type;
  current_function_return_type = return_type;
  
  symbol_table.enter_scope();
    
  // 添加函数参数到符号表
  for (size_t i = 0; i < node->params.size(); i++) {
    auto& param = node->params[i];
    TypePtr param_type;
    if (param->is_array) {
      std::vector<int> dims;
      dims.push_back(0);
      for (int dim : param->array_dims) {
        dims.push_back(dim);
      }
      param_type = ArrayType::create(PrimitiveType::Int, dims);
    } else {
      param_type = PrimitiveType::Int;
    }
    
    // 检查是否已存在同名参数
    auto& current_scope = symbol_table.scopes.back();
    if (current_scope.find(param->ident) != current_scope.end()) {
      throw std::runtime_error("Duplicate parameter name '" + param->ident + 
                              "' at line " + std::to_string(param->lineno));
    }
    
    // 为参数创建符号并指定一致的唯一名称格式
    auto symbol = symbol_table.add_symbol(param->ident, param_type);
    
    // 关键修改：强制设置参数的唯一名称为 ident_0, ident_1 等
    symbol->unique_name = param->ident + "_" + std::to_string(i);
    param->symbol = symbol;
  }

  // 检查函数体内的语句
  if (node->block) {
    for (auto &stmt : node->block->stmts) {
      check(stmt);
    }
  }
  
  symbol_table.exit_scope();
  
  current_function_return_type = prev_return_type;
  
  return func_type;
}

TypePtr TypeChecker::checkVarDecl(AST::VarDeclPtr node) {
  for (auto var_def : node->defs) {
    checkVarDef(var_def, node->btype);
  }
  return nullptr;
}


bool TypeChecker::checkArrayInitList(const std::vector<int>& dims, const AST::ArrayInitValPtr& init_val, 
                          int& used_elems, int line_no, TypePtr expected_element_type) {
  // 计算当前维度的数组总元素个数
  int total_elements = 1;
  for (size_t i = 0; i < dims.size(); i++) {
    total_elements *= dims[i];
  }
  
  // 计算数组各维度的步长
  std::vector<int> strides(dims.size());
  strides[dims.size() - 1] = 1;
  for (int i = dims.size() - 2; i >= 0; i--) {
    strides[i] = strides[i + 1] * dims[i + 1];
  }
  
  // 遍历初始化列表中的每个元素
  for (size_t i = 0; i < init_val->values.size(); i++) {
    // 检查是否超出数组容量
    if (used_elems >= total_elements) {
      throw std::runtime_error("Excess elements in array initializer at line " + 
                             std::to_string(line_no));
    }
    
    auto nested_init = std::dynamic_pointer_cast<AST::ArrayInitVal>(init_val->values[i]);
    if (nested_init) {
      // 处理嵌套的初始化列表
      bool aligned = false;
      for (size_t dim = 0; dim < dims.size() - 1; dim++) {
        if (used_elems % strides[dim] == 0) {
          std::vector<int> sub_dims(dims.begin() + dim + 1, dims.end());
          int sub_elements = 0;
          
          if (!checkArrayInitList(sub_dims, nested_init, sub_elements, line_no)) {
            return false;
          }
          
          used_elems += sub_elements;
          
          // 对齐到下一个边界
          int alignment = strides[dim];
          used_elems = ((used_elems + alignment - 1) / alignment) * alignment;
          if (used_elems > total_elements) {
            throw std::runtime_error("Array initializer exceeds array bounds at line " + 
                                   std::to_string(line_no));
          }
          
          aligned = true;
          break;
        }
      }
      
      if (!aligned) {
        throw std::runtime_error("Invalid array initializer alignment at line " + 
                               std::to_string(line_no));
      }
    } else {
      TypePtr expr_type = check(init_val->values[i]);
      
      // 检查是否为数组类型
      if (std::dynamic_pointer_cast<ArrayType>(expr_type)) {
        throw std::runtime_error("Invalid conversion from array type to scalar type at line " + 
                               std::to_string(line_no));
      }
      
      used_elems++;
    }
  }
  
  return true;
}

TypePtr TypeChecker::checkVarDef(AST::VarDefPtr node, BasicType var_type) {
  TypePtr type;
  
  if (node->is_array) {
    type = ArrayType::create(PrimitiveType::create(var_type), node->array_dims);
  } else {
    type = PrimitiveType::create(var_type);
  }

  auto existing_symbol = symbol_table.find_symbol(node->ident);
  if (existing_symbol) {
    // 检查是否在当前作用域(最近的作用域)定义
    auto& current_scope = symbol_table.scopes.back();
    if (current_scope.find(node->ident) != current_scope.end()) {
      throw std::runtime_error("Variable '" + node->ident + 
                              "' already defined in current scope at line " + 
                              std::to_string(node->lineno));
    }
  }
  
  if (auto var_init = std::dynamic_pointer_cast<AST::VarDefInit>(node)) {
    if (var_init->init_val) {
      check(var_init->init_val);
      
      if (node->is_array) {
        // 数组初始化
        auto array_init = std::dynamic_pointer_cast<AST::ArrayInitVal>(var_init->init_val);
        if (!array_init) {
          throw std::runtime_error("Array initializer must be an initializer list at line " + 
                                 std::to_string(node->lineno));
        }
        
        int used_elements = 0;
        checkArrayInitList(node->array_dims, array_init, used_elements, node->lineno, PrimitiveType::Int);
      } else {
        // 标量初始化
        auto array_init = std::dynamic_pointer_cast<AST::ArrayInitVal>(var_init->init_val);
        if (array_init) {
          // 检查花括号初始化的标量
          if (array_init->values.size() != 1) {
            throw std::runtime_error("Scalar initializer with braces must contain exactly one expression at line " + 
                                  std::to_string(node->lineno));
          }
          
          TypePtr expr_type = check(array_init->values[0]);
          
          // 检查不是嵌套的花括号也不是数组类型
          if (std::dynamic_pointer_cast<AST::ArrayInitVal>(array_init->values[0]) || 
              std::dynamic_pointer_cast<ArrayType>(expr_type)) {
            throw std::runtime_error("Invalid type for scalar initialization at line " + 
                                  std::to_string(node->lineno));
          }
        }
      }
    }
  }

  // 将变量插入符号表，并将符号表中的 symbol 挂到 VarDef 节点上
  node->symbol = symbol_table.add_symbol(node->ident, type);
  return type;
}

TypePtr TypeChecker::checkBlock(AST::BlockPtr node, bool new_scope) {
  // 检查块内的每个语句
  // 如果 new_scope 为 true
  // 你需要在进入和退出块时更新符号表，创建、销毁新的作用域

  symbol_table.enter_scope();
  
  // 检查块内的每个语句
  for (auto &stmt : node->stmts) {
    check(stmt);
  }
  symbol_table.exit_scope();
  return nullptr;

}

TypePtr TypeChecker::checkAssignStmt(AST::AssignStmtPtr node) {
  TypePtr lval_type = check(node->lval);
  TypePtr expr_type = check(node->exp);
  // 判断赋值号两边的类型是否相同
  // 我们实验中只支持 int 类型
  // 因此你需要判断 lval_type 和 expr_type 是否都为 int 类型

  // 检查左值类型是否为数组类型，如果是则报错
  auto array_type = std::dynamic_pointer_cast<ArrayType>(lval_type);
  if (array_type) {
    throw std::runtime_error("Cannot assign to an array at line " + 
                           std::to_string(node->lineno));
  }
  
  // 检查左值和右值类型是否匹配
  if (!lval_type->equals(expr_type)) {
    throw std::runtime_error("Type mismatch in assignment at line " + 
                           std::to_string(node->lineno) + 
                           ": left is " + lval_type->to_string() + 
                           ", right is " + expr_type->to_string());
  }

  // 赋值语句没有类型，但可以返回左值类型用于后续检查
  return lval_type;
}

TypePtr TypeChecker::checkIfStmt(AST::IfStmtPtr node) {
  TypePtr cond_type = check(node->cond);
  
  // 条件表达式必须是整型
  auto int_type = std::dynamic_pointer_cast<PrimitiveType>(cond_type);
  if (!int_type || int_type->basic_type != BasicType::Int) {
    throw std::runtime_error("Condition expression must be int at line " + 
                           std::to_string(node->lineno));
  }
  
  // 检查 then 分支
  check(node->then_stmt);
  
  // 检查 else 分支（如果存在）
  if (node->else_stmt) {
    check(node->else_stmt);
  }
  
  return nullptr;
}

TypePtr TypeChecker::checkWhileStmt(AST::WhileStmtPtr node) {
  TypePtr cond_type = check(node->cond);
  
  // 条件表达式必须是整型
  auto int_type = std::dynamic_pointer_cast<PrimitiveType>(cond_type);
  if (!int_type || int_type->basic_type != BasicType::Int) {
    throw std::runtime_error("While condition expression must be int at line " + 
                           std::to_string(node->lineno));
  }
  
  // 检查循环体
  check(node->body);
  
  return nullptr;
}

TypePtr TypeChecker::checkReturnStmt(AST::ReturnStmtPtr node) {
  // 检查返回值表达式的类型（如果存在）
  TypePtr expr_type = node->exp ? check(node->exp) : PrimitiveType::Void;
  
  // 使用成员变量获取当前函数的返回类型
  if (!current_function_return_type) {
    throw std::runtime_error("Return statement outside of function at line " + 
                           std::to_string(node->lineno));
  }
  
  // 检查返回类型是否匹配
  if (!expr_type->equals(current_function_return_type)) {
    // 特殊情况处理：void函数中的返回语句
    auto void_type = std::dynamic_pointer_cast<PrimitiveType>(current_function_return_type);
    if (void_type && void_type->basic_type == BasicType::Void) {
      if (node->exp) {
        throw std::runtime_error("Return with a value in void function at line " + 
                               std::to_string(node->lineno));
      }
    } else {
      // 非void函数返回类型不匹配
      throw std::runtime_error("Return type mismatch at line " + 
                             std::to_string(node->lineno) + 
                             ": function expects " + current_function_return_type->to_string() + 
                             ", but got " + expr_type->to_string());
    }
  }
  
  return expr_type;
}

TypePtr TypeChecker::checkLVal(AST::LValPtr node) {
  // 在符号表中查找变量
  // 根据符号表中的信息设置 LVal 的类型
  // 若变量未定义，你需要报错
  // 否则，将符号表中的 symbol 挂到 LVal 节点上
  // 如果 LVal 是数组，你还需要根据下标索引来设置 LVal 的类型

  auto symbol = symbol_table.find_symbol(node->ident);
  if (!symbol) {
    throw std::runtime_error("Variable '" + node->ident + "' used before definition at line " + 
                          std::to_string(node->lineno));
  }

  // 设置节点的symbol成员
  node->symbol = symbol;
  node->ident = symbol->unique_name;
  TypePtr base_type = symbol->type;

  // 如果不是数组访问，直接返回基本类型
  if (!node->is_array_element) {
    return base_type;
  }
  
  // 数组元素访问
  auto array_type = std::dynamic_pointer_cast<ArrayType>(base_type);
  if (!array_type) {
    throw std::runtime_error("Variable '" + symbol->name + "' is not an array at line " + 
                          std::to_string(node->lineno));
  }
  
  // 检查索引表达式
  for (auto &index : node->indices) {
    auto index_type = check(index);
    if (!std::dynamic_pointer_cast<PrimitiveType>(index_type) || 
        std::dynamic_pointer_cast<PrimitiveType>(index_type)->basic_type != BasicType::Int) {
      throw std::runtime_error("Array index must be integer at line " + 
                            std::to_string(index->lineno));
    }
  }
  
  // 检查索引数量
  if (node->indices.size() > array_type->dims.size()) {
    throw std::runtime_error("Too many indices for array at line " + 
                          std::to_string(node->lineno));
  }
  
  // 根据索引数量确定返回类型
  if (node->indices.size() == array_type->dims.size()) {
    return PrimitiveType::Int;
  } else {
    std::vector<int> new_dims(array_type->dims.begin() + node->indices.size(), 
                             array_type->dims.end());
    return ArrayType::create(PrimitiveType::Int, new_dims);
  }
}

TypePtr TypeChecker::checkIntConst(AST::IntConstPtr node) {
  // 整数常量的类型是 int
  return PrimitiveType::Int;
}

TypePtr TypeChecker::checkFuncCall(AST::FuncCallPtr node) {
  // 首先需要查找函数是否被定义过
  // 然后需要判断函数调用的参数个数和类型是否和声明一致
  // 最后设置函数调用表达式的类型为函数的返回值类型
  // 并将函数的 symbol 挂到 FuncCall 节点上

  // 查找函数是否被定义过
  auto symbol = symbol_table.find_symbol(node->name);
  if (!symbol) {
    throw std::runtime_error("Function '" + node->name + "' used before definition at line " + 
                             std::to_string(node->lineno));
  }
  
  node->symbol = symbol;
  node->name = symbol->unique_name;
  
  // 检查符号是否是函数类型
  auto func_type = std::dynamic_pointer_cast<FuncType>(symbol->type);
  if (!func_type) {
    throw std::runtime_error("'" + symbol->name + "' is not a function at line " + 
                             std::to_string(node->lineno));
  }
  
  // 检查参数个数是否匹配
  if (func_type->param_types.size() != node->args.size()) {
    throw std::runtime_error("Function '" + symbol->name + "' called with wrong number of arguments at line " + 
                             std::to_string(node->lineno) + 
                             ": expected " + std::to_string(func_type->param_types.size()) + 
                             ", got " + std::to_string(node->args.size()));
  }
  
  // 检查每个参数的类型是否匹配
  for (size_t i = 0; i < node->args.size(); i++) {
    TypePtr arg_type = check(node->args[i]);
    if (!arg_type->equals(func_type->param_types[i])) {
      throw std::runtime_error("Type mismatch for argument " + std::to_string(i + 1) + 
                               " of function '" + symbol->name + "' at line " + 
                               std::to_string(node->lineno));
    }
  }
  
  TypePtr return_type = func_type->return_type;
  return return_type;
}


TypePtr TypeChecker::checkUnaryExp(AST::UnaryExpPtr node) {
  // 一元表达式只支持 int 类型，因此你需要判断 type 是否为 int
  auto type = check(node->exp);
  
  // 检查操作数类型是否为int
  auto int_type = std::dynamic_pointer_cast<PrimitiveType>(type);
  if (!int_type || int_type->basic_type != BasicType::Int) {
    throw std::runtime_error("Operand of unary expression must be int at line " + 
                           std::to_string(node->lineno));
  }
  
  return PrimitiveType::Int;
}

TypePtr TypeChecker::checkBinaryExp(AST::BinaryExpPtr node) {
  // 检查左右操作数的类型
  TypePtr left_type = check(node->left);
  TypePtr right_type = check(node->right);
  
  // 检查左右操作数是否为基本类型而非数组
  auto left_array = std::dynamic_pointer_cast<ArrayType>(left_type);
  auto right_array = std::dynamic_pointer_cast<ArrayType>(right_type);
  
  if (left_array || right_array) {
    throw std::runtime_error("Cannot apply binary operator to array type at line " + 
                           std::to_string(node->lineno));
  }
  
  // 检查左右操作数类型是否为int
  auto left_int = std::dynamic_pointer_cast<PrimitiveType>(left_type);
  auto right_int = std::dynamic_pointer_cast<PrimitiveType>(right_type);
  
  if (!left_int || left_int->basic_type != BasicType::Int || 
      !right_int || right_int->basic_type != BasicType::Int) {
    throw std::runtime_error("Operands of binary expression must be int at line " + 
                           std::to_string(node->lineno));
  }
  
  // 根据操作符类型设置表达式结果类型
  if (node->op == BinaryOp::Lt || node->op == BinaryOp::Le ||
      node->op == BinaryOp::Gt || node->op == BinaryOp::Ge ||
      node->op == BinaryOp::Eq || node->op == BinaryOp::Ne ||
      node->op == BinaryOp::And || node->op == BinaryOp::Or) {
    // 关系和逻辑运算符产生布尔结果，但在SysY中用int表示
    return PrimitiveType::Int;
  }
  
  // 算术运算符(+,-,*,/,%)产生int结果
  return PrimitiveType::Int;
}
// 添加打印符号表的辅助方法
void TypeChecker::print_symbol_table() const {
  std::cout << "\n==== Symbol Table Contents ====\n";
  for (size_t i = 0; i < symbol_table.scopes.size(); i++) {
    std::cout << "Scope " << i << ":\n";
    for (const auto& entry : symbol_table.scopes[i]) {
      std::cout << "  Name: " << entry.first 
                << ", Unique Name: " << entry.second->unique_name
                << ", Type: " << entry.second->type->to_string() << "\n";
    }
  }
  std::cout << "==============================\n";
}