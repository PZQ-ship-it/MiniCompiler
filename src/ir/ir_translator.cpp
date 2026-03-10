#include "ir_translator.hpp"

#include <cassert>

std::string IRTranslator::new_temp() {
  static int temp_count = 0;
  return "T" + std::to_string(temp_count++);
}

IR::Code IRTranslator::translate(AST::NodePtr node) {
#define TRANSLATE_NODE(type)                                 \
  if (auto n = std::dynamic_pointer_cast<AST::type>(node)) { \
    return translate##type(n);                               \
  }
  // 递归翻译 AST 的每个节点
  // 如果你添加了新的 AST 节点类型，记得在这里添加对应的翻译函数

  TRANSLATE_NODE(CompUnit)
  TRANSLATE_NODE(FuncDef)
  TRANSLATE_NODE(Block)
  TRANSLATE_NODE(VarDecl)
  TRANSLATE_NODE(VarDef)
  TRANSLATE_NODE(AssignStmt)
  TRANSLATE_NODE(ReturnStmt)
  TRANSLATE_NODE(LVal)
  TRANSLATE_NODE(BinaryExp)
  TRANSLATE_NODE(UnaryExp)
  TRANSLATE_NODE(FuncCall)
  TRANSLATE_NODE(IntConst)
  TRANSLATE_NODE(IfStmt)
  TRANSLATE_NODE(WhileStmt)
  TRANSLATE_NODE(EmptyStmt)

#undef TRANSLATE_NODE

  ASSERT(false,
         "Unknown AST node type " + node->to_string() + " in IR translation");
}

IR::Code IRTranslator::translateExp(AST::NodePtr node,
                                    const std::string &place) {
#define TRANSLATE_EXP_NODE(type)                             \
  if (auto n = std::dynamic_pointer_cast<AST::type>(node)) { \
    return translate##type(n, place);                        \
  }

  TRANSLATE_EXP_NODE(BinaryExp)
  TRANSLATE_EXP_NODE(UnaryExp)
  TRANSLATE_EXP_NODE(FuncCall)
  TRANSLATE_EXP_NODE(IntConst)
  TRANSLATE_EXP_NODE(LVal)
  
  // 根据不同类型的表达式调用对应的翻译函数
  if (auto n = std::dynamic_pointer_cast<AST::BinaryExp>(node)) {
    return translateBinaryExp(n, place);
  }

#warning Add more AST node types if needed

#undef TRANSLATE_EXP_NODE

  ASSERT(false, "No translateExp for node " + node->to_string());
}

IR::Code IRTranslator::translateCompUnit(AST::CompUnitPtr node) {
  IR::Code ir;
  for (auto &unit : node->units) {
    auto unit_ir = translate(unit);
    std::move(unit_ir.begin(), unit_ir.end(), std::back_inserter(ir));
  }
  return ir;
}

IR::Code IRTranslator::translateFuncDef(AST::FuncDefPtr node) {
  IR::Code ir;
  ir.push_back(IR::Function::create(node->name));
  
  // 添加参数声明 - 直接使用符号表中的唯一名称
  for (auto& param : node->params) {
    if (param->symbol) {
      ir.push_back(IR::Param::create(param->symbol->unique_name));
    } else {
      std::string param_name = param->ident + "_0";
      ir.push_back(IR::Param::create(param_name));
    }
  }
  
  // 翻译函数体
  auto block_ir = translate(node->block);
  std::move(block_ir.begin(), block_ir.end(), std::back_inserter(ir));
  return ir;
}

IR::Code IRTranslator::translateBlock(AST::BlockPtr node) {
  IR::Code ir;
  for (auto &stmt : node->stmts) {
    auto stmt_ir = translate(stmt);
    std::move(stmt_ir.begin(), stmt_ir.end(), std::back_inserter(ir));
  }
  return ir;
}

IR::Code IRTranslator::translateVarDecl(AST::VarDeclPtr node) {
  IR::Code ir;
  for (auto &def : node->defs) {
    auto def_ir = translate(def);
    std::move(def_ir.begin(), def_ir.end(), std::back_inserter(ir));
  }
  return ir;
}

IR::Code IRTranslator::translateVarDef(AST::VarDefPtr node) {
  IR::Code ir;
  bool is_global = node->symbol && node->symbol->scope_level == 1;
  
  if (node->is_array) {
    // 计算数组总大小(字节)
    int total_size = 4; 
    for (int dim : node->array_dims) {
      total_size *= dim;
    }

    if (is_global) {
      // 处理全局数组
      if (auto init_node = std::dynamic_pointer_cast<AST::VarDefInit>(node)) {
        if (auto array_init = std::dynamic_pointer_cast<AST::ArrayInitVal>(init_node->init_val)) {
          // 递归初始化全局数组
          std::vector<int> init_values(total_size / 4, 0); // 预先填充0
          int current_pos = 0;
          initGlobalArrayRecursive(array_init, init_values, node->array_dims, current_pos);
          ir.push_back(IR::Global::create(node->symbol->unique_name, total_size, init_values));
        } else {
          // 无初始值，全部初始化为0
          ir.push_back(IR::Global::create(node->symbol->unique_name, total_size));
        }
      } else {
        // 无初始值，全部初始化为0
        ir.push_back(IR::Global::create(node->symbol->unique_name, total_size));
      }
    } else {
      // 局部数组声明
      ir.push_back(IR::DEC::create(node->symbol->unique_name, total_size));
      
      // 处理数组初始化
      if (auto init_node = std::dynamic_pointer_cast<AST::VarDefInit>(node)) {
        if (init_node->init_val) {
          if (auto array_init = std::dynamic_pointer_cast<AST::ArrayInitVal>(init_node->init_val)) {
            // 关键修改1: 先将所有元素初始化为0
            std::string zero_temp = new_temp();
            ir.push_back(IR::LoadImm::create(zero_temp, 0));
            
            for (int i = 0; i < total_size / 4; i++) {
              ir.push_back(IR::Store::create(node->symbol->unique_name, i * 4, zero_temp));
            }
            
            // 然后再进行特定元素的初始化
            int current_pos = 0;
            auto init_ir = initLocalArrayRecursive(node->symbol->unique_name, array_init, 
                                                  node->array_dims, current_pos);
            std::move(init_ir.begin(), init_ir.end(), std::back_inserter(ir));
            
            // 移除此处的零填充代码，因为已经预先填充过了
          }
        }
      }
    }
  }

  // 处理普通变量
  else {
    if (is_global) {
      // 处理全局普通变量
      if (auto init_node = std::dynamic_pointer_cast<AST::VarDefInit>(node)) {
        if (auto const_val = std::dynamic_pointer_cast<AST::IntConst>(init_node->init_val)) {
          // 全局变量初始化为常量
          std::vector<int> init_values = {const_val->value};
          ir.push_back(IR::Global::create(node->symbol->unique_name, 4, init_values));
        } else {
          ir.push_back(IR::Global::create(node->symbol->unique_name, 4));
        }
      } else {
        // 无初始值，初始化为0
        ir.push_back(IR::Global::create(node->symbol->unique_name, 4));
      }
    } else {
      // 处理局部变量
      if (auto init_node = std::dynamic_pointer_cast<AST::VarDefInit>(node)) {
        if (init_node->init_val) {
          std::string place = node->symbol->unique_name;
          auto init_ir = translateExp(init_node->init_val, place);
          std::move(init_ir.begin(), init_ir.end(), std::back_inserter(ir));
        }
      }
    }
  }
  
  return ir;
}

// 递归初始化全局数组
void IRTranslator::initGlobalArrayRecursive(AST::ArrayInitValPtr init, std::vector<int>& values, 
  const std::vector<int>& dims, int& current_pos,
  int dim_index, int start_index) {
  if (!init || static_cast<size_t>(dim_index) >= dims.size()) return;
  
  // 计算子数组大小
  size_t sub_array_size = 1;
  for (size_t i = dim_index + 1; i < dims.size(); i++) {
    sub_array_size *= dims[i];
  }
  
  // 处理初始化列表中的每个元素
  for (size_t i = 0; i < init->values.size() && static_cast<size_t>(current_pos) < values.size(); i++) {
    if (auto nested_init = std::dynamic_pointer_cast<AST::ArrayInitVal>(init->values[i])) {
      // 嵌套初始化 - 递归处理子数组
      int current_dim_start = (current_pos / sub_array_size) * sub_array_size;
      initGlobalArrayRecursive(nested_init, values, dims, current_pos, dim_index + 1, current_dim_start);
      current_pos = current_dim_start + sub_array_size;
    } else if (auto const_val = std::dynamic_pointer_cast<AST::IntConst>(init->values[i])) {
      // 常量值直接赋值
      values[current_pos++] = const_val->value;
    }
  }
}

IR::Code IRTranslator::initLocalArrayRecursive(const std::string& array_name, AST::ArrayInitValPtr init,
  const std::vector<int>& dims, int& current_pos,
  int dim_index, int offset) {
  IR::Code ir;
  
  if (!init || static_cast<size_t>(dim_index) >= dims.size()) return ir;
  
  // 获取每个维度的大小，用于计算填充
  int total_elements = getTotalElements(dims);
  
  // 处理初始化列表中的每个元素
  for (size_t i = 0; i < init->values.size() && current_pos < total_elements; i++) {
    if (auto nested_init = std::dynamic_pointer_cast<AST::ArrayInitVal>(init->values[i])) {
      // 嵌套初始化 - 记录开始位置
      int nested_start = current_pos;
      
      // 计算当前位置对应的各个维度索引
      std::vector<int> indices = computeIndices(nested_start, dims);
      
      // 递归处理嵌套初始化 - 关键：下一级维度
      auto nested_ir = initLocalArrayRecursive(array_name, nested_init, dims, 
                                             current_pos, dim_index + 1, 0);
      std::move(nested_ir.begin(), nested_ir.end(), std::back_inserter(ir));
      
      // 计算嵌套初始化后应该在哪里结束
      // 核心逻辑：计算当前维度这个位置的子数组结束位置
      int next_pos = nested_start;
      
      // 找到最后一个非最低维度索引，增加它到下一个位置
      bool found = false;
      for (int d = dims.size() - 2; d >= dim_index; d--) {
        if (indices[d] < dims[d] - 1) {
          // 在这个维度上移动到下一个位置
          indices[d]++;
          // 重置所有更低维度的索引为0
          for (size_t j = d + 1; j < dims.size(); j++) {
            indices[j] = 0;
          }
          found = true;
          break;
        }
      }
      
      // 如果没找到，说明已经到达数组末尾
      if (found) {
        // 转换回线性索引
        next_pos = computeLinearIndex(indices, dims);
      } else {
        next_pos = total_elements;
      }
      
      // 填充当前子数组剩余元素为0
      if (current_pos < next_pos) {
        std::string zero_temp = new_temp();
        ir.push_back(IR::LoadImm::create(zero_temp, 0));
        
        for (int j = current_pos; j < next_pos; j++) {
          ir.push_back(IR::Store::create(array_name, j * 4, zero_temp));
        }
        current_pos = next_pos;
      }
    } else {
      // 常规初始化 - 处理单个元素
      std::string value_temp = new_temp();
      
      if (auto const_val = std::dynamic_pointer_cast<AST::IntConst>(init->values[i])) {
        ir.push_back(IR::LoadImm::create(value_temp, const_val->value));
      } else {
        auto expr_ir = translateExp(init->values[i], value_temp);
        std::move(expr_ir.begin(), expr_ir.end(), std::back_inserter(ir));
      }
      
      ir.push_back(IR::Store::create(array_name, current_pos * 4, value_temp));
      current_pos++;
    }
  }
  
  return ir;
}

// 辅助函数：计算线性索引对应的多维索引
std::vector<int> IRTranslator::computeIndices(int linear_index, const std::vector<int>& dims) {
  std::vector<int> indices(dims.size(), 0);
  int remaining = linear_index;
  
  for (int i = 0; i < static_cast<int>(dims.size()); i++) {
    int divisor = 1;
    for (int j = i + 1; j < static_cast<int>(dims.size()); j++) {
      divisor *= dims[j];
    }
    
    indices[i] = remaining / divisor;
    remaining %= divisor;
  }
  
  return indices;
}

// 辅助函数：计算多维索引对应的线性索引
int IRTranslator::computeLinearIndex(const std::vector<int>& indices, const std::vector<int>& dims) {
  int linear_index = 0;
  for (int i = 0; i < static_cast<int>(indices.size()); i++) {
    int multiplier = 1;
    for (int j = i + 1; j < static_cast<int>(dims.size()); j++) {
      multiplier *= dims[j];
    }
    linear_index += indices[i] * multiplier;
  }
  return linear_index;
}

// 计算数组总元素数
int IRTranslator::getTotalElements(const std::vector<int>& dims) {
  int total = 1;
  for (int dim : dims) {
    total *= dim;
  }
  return total;
}

IR::Code IRTranslator::translateAssignStmt(AST::AssignStmtPtr node) {
  IR::Code ir;
  auto lnode = node->lval;
  auto rnode = node->exp;

  bool is_global = lnode->symbol && lnode->symbol->scope_level == 1;

  if (lnode->is_array_element) {
    // 计算右值表达式
    std::string rplace = new_temp();
    auto rexp_ir = translateExp(rnode, rplace);
    std::move(rexp_ir.begin(), rexp_ir.end(), std::back_inserter(ir));
    
    // 处理一维数组情况
    if (lnode->indices.size() == 1) {
      std::string base = lnode->symbol->unique_name;

      // 对全局数组，需要先获取地址
      if (is_global) {
        std::string addr_temp = new_temp();
        ir.push_back(IR::GetAddr::create(addr_temp, base));
        base = addr_temp;
      }   
      // 检查索引是否为常量
      if (auto idx_const = std::dynamic_pointer_cast<AST::IntConst>(lnode->indices[0])) {
        // 常量索引情况 - 直接计算偏移量
        int offset = idx_const->value * 4; // 4字节/整数
        ir.push_back(IR::Store::create(base, offset, rplace));
      } else {
        // 变量索引处理部分
        std::string index_place = new_temp();
        auto index_ir = translateExp(lnode->indices[0], index_place);
        std::move(index_ir.begin(), index_ir.end(), std::back_inserter(ir));
        // 计算字节偏移量
        std::string const_four = new_temp();
        ir.push_back(IR::LoadImm::create(const_four, 4));
        std::string offset_place = new_temp();
        ir.push_back(IR::Binary::create(offset_place, index_place, BinaryOp::Mul, const_four));  
        // 计算最终地址
        std::string addr_place = new_temp();
        ir.push_back(IR::Binary::create(addr_place, base, BinaryOp::Add, offset_place));
        ir.push_back(IR::Store::create(addr_place, 0, rplace));
      }
    } else // 多维数组赋值处理部分
    if (lnode->indices.size() > 1) {
      std::string base = lnode->symbol->unique_name;
      
      // 对全局数组，需要先获取地址
      if (is_global) {
        std::string addr_temp = new_temp();
        ir.push_back(IR::GetAddr::create(addr_temp, base));
        base = addr_temp;
      }
      
      // 获取数组维度信息
      std::vector<int> dims;
      if (auto array_type = std::dynamic_pointer_cast<ArrayType>(lnode->symbol->type)) {
        dims = array_type->dims;
      } else {
        // 错误处理：不是数组类型
        return ir;
      }
      
      // 检查所有索引是否都是常量
      bool all_const_indices = true;
      int total_offset = 0;
      
      // 计算偏移量
      int element_size = 4; 
      int stride = 1;
      
      // 从右到左计算每个维度的偏移贡献
      for (int i = lnode->indices.size() - 1; i >= 0; i--) {
        auto idx_const = std::dynamic_pointer_cast<AST::IntConst>(lnode->indices[i]);
        if (idx_const) {
          // 常量索引
          total_offset += idx_const->value * stride;
        } else {
          // 变量索引，无法直接计算
          all_const_indices = false;
          break;
        }
        
        // 更新下一维度的步长
        if (i > 0 && static_cast<size_t>(i - 1) < dims.size()) {
          stride *= dims[i];
        }
      }
      
      if (all_const_indices) {
        // 所有索引都是常量，直接使用计算好的偏移量
        ir.push_back(IR::Store::create(base, total_offset * element_size, rplace));
      } else {
        // 至少有一个索引是变量，需要动态计算
        std::string offset_place = new_temp();
        ir.push_back(IR::LoadImm::create(offset_place, 0)); 
        
        // 计算每个维度的大小乘积（用于偏移量计算）
        std::vector<int> dim_products;
        int product = 1;
        for (int i = dims.size() - 1; i >= 0; --i) {
          dim_products.push_back(product);
          product *= dims[i];
        }
        std::reverse(dim_products.begin(), dim_products.end());
        
        // 计算多维索引的线性偏移量
        for (size_t i = 0; i < lnode->indices.size(); ++i) {
          std::string index_place = new_temp();
          auto index_ir = translateExp(lnode->indices[i], index_place);
          std::move(index_ir.begin(), index_ir.end(), std::back_inserter(ir));
          
          if (i < dim_products.size()) {
            // 如果是常量索引
            if (auto idx_const = std::dynamic_pointer_cast<AST::IntConst>(lnode->indices[i])) {
              int dim_offset = idx_const->value * dim_products[i];
              std::string temp = new_temp();
              ir.push_back(IR::LoadImm::create(temp, dim_offset));
              ir.push_back(IR::Binary::create(offset_place, offset_place, BinaryOp::Add, temp));
            } else {
              // 计算该维度的偏移量贡献
              if (dim_products[i] != 1) {
                std::string mult_temp = new_temp();
                ir.push_back(IR::LoadImm::create(mult_temp, dim_products[i]));
                std::string dim_offset = new_temp();
                ir.push_back(IR::Binary::create(dim_offset, index_place, BinaryOp::Mul, mult_temp));
                ir.push_back(IR::Binary::create(offset_place, offset_place, BinaryOp::Add, dim_offset));
              } else {
                ir.push_back(IR::Binary::create(offset_place, offset_place, BinaryOp::Add, index_place));
              }
            }
          }
        }

        std::string const_four = new_temp();
        ir.push_back(IR::LoadImm::create(const_four, 4));
        std::string byte_offset = new_temp();
        ir.push_back(IR::Binary::create(byte_offset, offset_place, BinaryOp::Mul, const_four));
        
        // 计算最终地址
        std::string addr_place = new_temp();
        ir.push_back(IR::Binary::create(addr_place, base, BinaryOp::Add, byte_offset));
        
        // 使用计算出的地址存储值
        ir.push_back(IR::Store::create(addr_place, 0, rplace));
      }
    }
  }
  // 普通变量赋值
  else {
    if (is_global) {
      // 全局变量需要通过地址赋值
      std::string rplace = new_temp();
      auto rexp_ir = translateExp(rnode, rplace);
      std::move(rexp_ir.begin(), rexp_ir.end(), std::back_inserter(ir));
      
      std::string addr_temp = new_temp();
      ir.push_back(IR::GetAddr::create(addr_temp, lnode->symbol->unique_name));
      ir.push_back(IR::Store::create(addr_temp, 0, rplace));
    } else {
      // 局部变量直接赋值
      std::string place = lnode->symbol->unique_name;
      auto exp_ir = translateExp(rnode, place);
      std::move(exp_ir.begin(), exp_ir.end(), std::back_inserter(ir));
    }
  }

  return ir;
}


IR::Code IRTranslator::translateReturnStmt(AST::ReturnStmtPtr node) {
  IR::Code ir;
  // 翻译返回值
  // 如果有返回值，则：
  // place = new_temp();
  // auto exp_ir = translateExp(node->exp, place);
  // return exp_ir + [RETURN place];
  // 否则：
  // return [RETURN];

  // 如果有返回值，先计算表达式
  if (node->exp) {
    std::string place = "a0";  // 使用a0寄存器存储返回值
    auto exp_ir = translateExp(node->exp, place);
    std::move(exp_ir.begin(), exp_ir.end(), std::back_inserter(ir));
    ir.push_back(IR::Return::create(place));
  } else {
    // 无返回值
    ir.push_back(IR::Return::create());
  }

  return ir;
}

IR::Code IRTranslator::translateLVal(AST::LValPtr node, const std::string &place) {
  IR::Code ir;

  if (!place.empty()) {
    bool is_global = node->symbol && node->symbol->scope_level == 1;
    
    if (node->is_array_element) {
      // 使用新的接口计算数组偏移
      auto offset_ir = calculateArrayOffset(node->symbol->unique_name, 
                                           node->indices, 
                                           node->symbol->type, 
                                           place, 
                                           is_global, 
                                           false,
                                           "");
      std::move(offset_ir.begin(), offset_ir.end(), std::back_inserter(ir));
    } else {
      // 普通变量处理
      if (is_global) {
        std::string addr_temp = new_temp();
        ir.push_back(IR::GetAddr::create(addr_temp, node->symbol->unique_name));
        ir.push_back(IR::Load::create(place, addr_temp, 0));
      } else {
        ir.push_back(IR::Assign::create(place, node->symbol->unique_name));
      }
    }
  }

  return ir;
}

IR::Code IRTranslator::calculateArrayOffset(const std::string& array_name, 
  const std::vector<AST::NodePtr>& indices,
  TypePtr type, const std::string& place,
  bool is_global, bool is_store,
  const std::string& store_value) {
  
  IR::Code ir;
  
  // 获取数组维度
  std::vector<int> dims;
  if (auto array_type = std::dynamic_pointer_cast<ArrayType>(type)) {
    dims = array_type->dims;
  } else {
    return ir;
  }
  
  // 暂存所有索引的值
  std::vector<std::string> index_places;
  for (size_t i = 0; i < indices.size() && i < dims.size(); i++) {
    std::string index_place = new_temp();
    auto index_ir = translateExp(indices[i], index_place);
    std::move(index_ir.begin(), index_ir.end(), std::back_inserter(ir));
    index_places.push_back(index_place);
  }
  
  // 为每个维度计算步长
  std::vector<int> strides(dims.size(), 1);
  for (int i = dims.size() - 2; i >= 0; i--) {
    strides[i] = strides[i+1] * dims[i+1];
  }
  
  // 初始化偏移量为0
  std::string offset = new_temp();
  ir.push_back(IR::LoadImm::create(offset, 0));
  
  // 逐维度计算偏移量
  for (size_t i = 0; i < index_places.size(); i++) {
    // 加载当前维度的步长
    std::string stride_place = new_temp();
    ir.push_back(IR::LoadImm::create(stride_place, strides[i]));
    
    // 当前索引 * 步长
    std::string mult_place = new_temp();
    ir.push_back(IR::Binary::create(mult_place, index_places[i], BinaryOp::Mul, stride_place));
    
    // 加到总偏移量
    std::string new_offset = new_temp();
    ir.push_back(IR::Binary::create(new_offset, offset, BinaryOp::Add, mult_place));
    offset = new_offset;
  }
  
  // 乘以元素大小(4字节)
  std::string elem_size = new_temp();
  ir.push_back(IR::LoadImm::create(elem_size, 4));
  std::string byte_offset = new_temp();
  ir.push_back(IR::Binary::create(byte_offset, offset, BinaryOp::Mul, elem_size));
  
  // 计算最终地址
  std::string base = array_name;
  if (is_global) {
    std::string addr_temp = new_temp();
    ir.push_back(IR::GetAddr::create(addr_temp, base));
    base = addr_temp;
  }
  
  std::string addr = new_temp();
  ir.push_back(IR::Binary::create(addr, base, BinaryOp::Add, byte_offset));
  
  // 读取或存储
  if (is_store) {
    ir.push_back(IR::Store::create(addr, 0, store_value));
  } else {
    ir.push_back(IR::Load::create(place, addr, 0));
  }
  
  return ir;
}


IR::Code IRTranslator::translateBinaryExp(AST::BinaryExpPtr node,
                                          const std::string &place) {
  IR::Code ir;
  auto left_place = new_temp();
  auto right_place = new_temp();

  // 翻译左右子表达式
  auto left_ir = translateExp(node->left, left_place);
  auto right_ir = translateExp(node->right, right_place);

  std::move(left_ir.begin(), left_ir.end(), std::back_inserter(ir));
  std::move(right_ir.begin(), right_ir.end(), std::back_inserter(ir));

  // 添加二元运算指令
  if (!place.empty()) {
    ir.push_back(IR::Binary::create(place, left_place, node->op, right_place));
  }
  return ir;
}


IR::Code IRTranslator::translateUnaryExp(AST::UnaryExpPtr node,
                                         const std::string &place) {
  IR::Code ir;

  if (!place.empty()) {
    // 统计连续的相同单目运算符
    int not_count = 0;
    int neg_count = 0;
    
    AST::NodePtr current_node = node;
    
    // 递归统计连续的单目运算符
    while (auto unary = std::dynamic_pointer_cast<AST::UnaryExp>(current_node)) {
      if (unary->op == BinaryOp::Not) {
        not_count++;
      } else if (unary->op == BinaryOp::Sub) {
        neg_count++;
      } else if (unary->op == BinaryOp::Add) {
        // 加号不影响结果，忽略
      } else {
        break;  // 遇到其他操作符，停止统计
      }
      
      current_node = unary->exp;  // 继续检查子表达式
    }
    
    // 处理最内层的非单目表达式
    std::string exp_place = new_temp();
    auto exp_ir = translateExp(current_node, exp_place);
    std::move(exp_ir.begin(), exp_ir.end(), std::back_inserter(ir));
    
    // 应用单目运算符的复合效果
    std::string temp = exp_place;
    
    // 应用取负运算符(如果有奇数个负号，结果取反)
    if (neg_count % 2 == 1) {
      std::string neg_place = new_temp();
      // 使用0减去值来实现取负，而不是使用Unary指令
      std::string zero_place = new_temp();
      ir.push_back(IR::LoadImm::create(zero_place, 0));
      ir.push_back(IR::Binary::create(neg_place, zero_place, BinaryOp::Sub, temp));
      temp = neg_place;
    }
    
    // 应用逻辑非运算符
    if (not_count % 2 == 1) {
      std::string not_place = new_temp();
      // 使用条件跳转和常量赋值来实现逻辑非
      std::string zero_place = new_temp();
      ir.push_back(IR::LoadImm::create(zero_place, 0));
      std::string one_place = new_temp();
      ir.push_back(IR::LoadImm::create(one_place, 1));
      
      std::string true_label = new_label();
      std::string end_label = new_label();
      
      // 如果值等于0，则结果为1，否则为0
      ir.push_back(IR::If::create(temp, BinaryOp::Eq, zero_place, true_label));
      ir.push_back(IR::Assign::create(not_place, zero_place)); // 原值非0，逻辑非结果为0
      ir.push_back(IR::Goto::create(end_label));
      ir.push_back(IR::Label::create(true_label));
      ir.push_back(IR::Assign::create(not_place, one_place)); // 原值为0，逻辑非结果为1
      ir.push_back(IR::Label::create(end_label));
      
      temp = not_place;
    }
    
    ir.push_back(IR::Assign::create(place, temp));
  }

  return ir;
}


IR::Code IRTranslator::translateFuncCall(AST::FuncCallPtr node,
                                         const std::string &place) {
  IR::Code ir;
  std::vector<std::string> arg_places;

  // 处理每个参数
  for (size_t i = 0; i < node->args.size(); i++) {
    std::string arg_place = new_temp();
    
    // 检查是否为多维数组部分索引
    if (auto lval = std::dynamic_pointer_cast<AST::LVal>(node->args[i])) {
      if (!lval->is_array_element && lval->symbol && 
        std::dynamic_pointer_cast<ArrayType>(lval->symbol->type)) {
      // 对于数组类型，直接获取地址
      bool is_global = lval->symbol && lval->symbol->scope_level == 1;
      if (is_global) {
        // 全局数组已经有地址
        ir.push_back(IR::GetAddr::create(arg_place, lval->symbol->unique_name));
      } else {
        // 局部数组，直接赋值（数组名已经是地址）
        ir.push_back(IR::Assign::create(arg_place, lval->symbol->unique_name));
      }
      arg_places.push_back(arg_place);
      continue;
      }
      else if (lval->is_array_element) {
        
        // 检查是否是多维数组且索引数量少于维度数量（部分索引）
        auto array_type = std::dynamic_pointer_cast<ArrayType>(lval->symbol->type);
        if (array_type && array_type->dims.size() > lval->indices.size()) {
          //子数组（如二维数组的a[2]）
          bool is_global = lval->symbol && lval->symbol->scope_level == 1;
          std::string base = lval->symbol->unique_name;
          
          // 对于全局数组，先获取基地址
          if (is_global) {
            std::string addr_temp = new_temp();
            ir.push_back(IR::GetAddr::create(addr_temp, base));
            base = addr_temp;
          }
          
          // 计算子数组的偏移量
          if (lval->indices.size() == 1) {
            if (auto idx_const = std::dynamic_pointer_cast<AST::IntConst>(lval->indices[0])) {
              // 常量索引
              int index = idx_const->value;
              int second_dim = array_type->dims[1]; 
              int offset = index * second_dim * 4;  
              
              // 生成地址计算
              std::string offset_temp = new_temp();
              ir.push_back(IR::LoadImm::create(offset_temp, offset));
              ir.push_back(IR::Binary::create(arg_place, base, BinaryOp::Add, offset_temp));
            } else {
              // 变量索引
              std::string index_place = new_temp();
              auto index_ir = translateExp(lval->indices[0], index_place);
              std::move(index_ir.begin(), index_ir.end(), std::back_inserter(ir));
              
              // 计算偏移量：索引 * 第二维大小 * 4字节
              std::string row_size = new_temp();
              ir.push_back(IR::LoadImm::create(row_size, array_type->dims[1] * 4));
              std::string offset = new_temp();
              ir.push_back(IR::Binary::create(offset, index_place, BinaryOp::Mul, row_size));
              
              // 将偏移量加到基地址上
              ir.push_back(IR::Binary::create(arg_place, base, BinaryOp::Add, offset));
            }
            
            arg_places.push_back(arg_place);
            continue;
          }
        }
      }
    }
    
    // 常规参数处理
    auto arg_ir = translateExp(node->args[i], arg_place);
    std::move(arg_ir.begin(), arg_ir.end(), std::back_inserter(ir));
    arg_places.push_back(arg_place);
  }
  
  // 将参数传递给函数
  for (size_t i = 0; i < arg_places.size(); i++) {
    ir.push_back(IR::Arg::create(arg_places[i], node->name, i));
  }

  // 函数调用
  if (!place.empty()) {
    ir.push_back(IR::Call::create(place, node->name));
  } else {
    ir.push_back(IR::Call::create(node->name));
  }

  return ir;
}

IR::Code IRTranslator::translateIntConst(AST::IntConstPtr node,
                                         const std::string &place) {
  IR::Code ir;
  // 添加赋值常量指令
  if (!place.empty()) {
    ir.push_back(IR::LoadImm::create(place, node->value));
  }
  return ir;
}

// 添加生成新标签的函数
std::string IRTranslator::new_label() {
  static int label_count = 0;
  return "L" + std::to_string(label_count++);
}

// 添加if语句翻译函数
IR::Code IRTranslator::translateIfStmt(AST::IfStmtPtr node) {
  IR::Code ir;
  
  std::string true_label = new_label();
  std::string false_label = new_label();
  std::string end_label = new_label();
  
  // 翻译条件表达式
  auto cond_ir = translateCond(node->cond, true_label, false_label);
  std::move(cond_ir.begin(), cond_ir.end(), std::back_inserter(ir));
  
  // true分支
  ir.push_back(IR::Label::create(true_label));
  auto then_ir = translate(node->then_stmt);
  std::move(then_ir.begin(), then_ir.end(), std::back_inserter(ir));
  ir.push_back(IR::Goto::create(end_label));
  
  // false分支
  ir.push_back(IR::Label::create(false_label));
  if (node->else_stmt) {
    auto else_ir = translate(node->else_stmt);
    std::move(else_ir.begin(), else_ir.end(), std::back_inserter(ir));
  }
  
  // 结束标签
  ir.push_back(IR::Label::create(end_label));
  
  return ir;
}

// 修复while语句翻译函数
IR::Code IRTranslator::translateWhileStmt(AST::WhileStmtPtr node) {
  IR::Code ir;
  
  std::string cond_label = new_label();
  std::string body_label = new_label();
  std::string end_label = new_label();
  
  // 条件检查标签
  ir.push_back(IR::Label::create(cond_label));
  
  // 翻译条件表达式
  auto cond_ir = translateCond(node->cond, body_label, end_label);
  std::move(cond_ir.begin(), cond_ir.end(), std::back_inserter(ir));
  
  // 循环体
  ir.push_back(IR::Label::create(body_label));
  auto body_ir = translate(node->body);
  std::move(body_ir.begin(), body_ir.end(), std::back_inserter(ir));
  
  // 循环回到条件检查
  ir.push_back(IR::Goto::create(cond_label));
  
  // 循环结束
  ir.push_back(IR::Label::create(end_label));
  
  return ir;
}

IR::Code IRTranslator::translateEmptyStmt(AST::EmptyStmtPtr node) {
  // 空语句不生成任何中间代码，直接返回空的IR::Code
  return IR::Code();
}

// 添加条件表达式翻译函数
IR::Code IRTranslator::translateCond(AST::NodePtr node, 
                                    const std::string &true_label,
                                    const std::string &false_label) {
  IR::Code ir;
  
  // 处理不同类型的条件表达式
  if (auto binary = std::dynamic_pointer_cast<AST::BinaryExp>(node)) {
    // 处理逻辑与(&&)和逻辑或(||)的短路求值
    if (binary->op == BinaryOp::And) {
      // 实现短路求值逻辑
      std::string mid_label = new_label();
      auto left_ir = translateCond(binary->left, mid_label, false_label);
      std::move(left_ir.begin(), left_ir.end(), std::back_inserter(ir));
      
      ir.push_back(IR::Label::create(mid_label));
      auto right_ir = translateCond(binary->right, true_label, false_label);
      std::move(right_ir.begin(), right_ir.end(), std::back_inserter(ir));
    } 
    else if (binary->op == BinaryOp::Or) {
      // 实现短路求值逻辑
      std::string mid_label = new_label();
      auto left_ir = translateCond(binary->left, true_label, mid_label);
      std::move(left_ir.begin(), left_ir.end(), std::back_inserter(ir));
      
      ir.push_back(IR::Label::create(mid_label));
      auto right_ir = translateCond(binary->right, true_label, false_label);
      std::move(right_ir.begin(), right_ir.end(), std::back_inserter(ir));
    } 
    // 处理普通比较运算符的部分
else {
  // 处理普通比较运算符
  std::string left_place = new_temp();
  std::string right_place = new_temp();
  
  auto left_ir = translateExp(binary->left, left_place);
  auto right_ir = translateExp(binary->right, right_place);
  
  std::move(left_ir.begin(), left_ir.end(), std::back_inserter(ir));
  std::move(right_ir.begin(), right_ir.end(), std::back_inserter(ir));
  
  // 检查是否为关系运算符(>, <, ==, !=, >=, <=)
  if (binary->op == BinaryOp::Gt || binary->op == BinaryOp::Lt ||
      binary->op == BinaryOp::Ge || binary->op == BinaryOp::Le ||
      binary->op == BinaryOp::Eq || binary->op == BinaryOp::Ne) {
    // 关系运算符可以直接用于条件判断
    ir.push_back(IR::If::create(left_place, binary->op, right_place, true_label));
    ir.push_back(IR::Goto::create(false_label));
  } else {
    // 算术运算符(+, -, *, /, %)需要先计算结果，然后与0比较
    std::string result_place = new_temp();
    ir.push_back(IR::Binary::create(result_place, left_place, binary->op, right_place));
    
    // 创建常量0的临时变量
    std::string zero_place = new_temp();
    ir.push_back(IR::LoadImm::create(zero_place, 0));
    
    // 与0比较，非0为真
    ir.push_back(IR::If::create(result_place, BinaryOp::Ne, zero_place, true_label));
    ir.push_back(IR::Goto::create(false_label));
  }
  }
  } 
  else {
    // 处理非二元表达式的条件
    std::string place = new_temp();
    auto exp_ir = translateExp(node, place);
    std::move(exp_ir.begin(), exp_ir.end(), std::back_inserter(ir));
    
    // 创建常量0的临时变量
    std::string zero_place = new_temp();
    ir.push_back(IR::LoadImm::create(zero_place, 0));
    
    // 使用临时变量进行比较
    ir.push_back(IR::If::create(place, BinaryOp::Ne, zero_place, true_label));
    ir.push_back(IR::Goto::create(false_label));
  }
  
  return ir;
}