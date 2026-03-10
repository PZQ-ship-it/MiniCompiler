#include "type.hpp"

#include "common.hpp"

bool PrimitiveType::equals(const TypePtr& other) const {
  auto other_type = std::dynamic_pointer_cast<PrimitiveType>(other);
  return other_type && basic_type == other_type->basic_type;
}


bool ArrayType::equals(const TypePtr& other) const {
   // 判断两个数组类型是否相等
  // tips: 先判断 other 是否是 ArrayType，然后逐个比较 element_type 和 dims
  // tips: 对于 dims，或许可以忽略第 0 维
  // 判断other是否是ArrayType类型
  auto other_array = std::dynamic_pointer_cast<ArrayType>(other);
  if (!other_array) {
    return false;  // 不是数组类型
  }
  
  // 检查元素类型是否相等
  if (!element_type->equals(other_array->element_type)) {
    return false;  // 元素类型不同
  }
  
  // 检查维度数量是否相等
  if (dims.size() != other_array->dims.size()) {
    return false;  // 维度数不同
  }
  
  // 逐一比较每个维度的大小
  for (size_t i = 0; i < dims.size(); i++) {
    // 第一维可能是0（表示函数参数的数组），应特殊处理
    if (i == 0 && (dims[i] == 0 || other_array->dims[i] == 0)) {
      continue;  // 第一维为0时跳过比较
    }
    if (dims[i] != other_array->dims[i]) {
      return false;  // 维度大小不同
    }
  }
  
  return true;  // 数组类型完全相同
}

bool FuncType::equals(const TypePtr& other) const {
  auto other_type = std::dynamic_pointer_cast<FuncType>(other);
  if (!other_type || !return_type->equals(other_type->return_type) ||
      param_types.size() != other_type->param_types.size()) {
    return false;
  }
  for (size_t i = 0; i < param_types.size(); i++) {
    if (!param_types[i]->equals(other_type->param_types[i])) {
      return false;
    }
  }
  return true;
}
