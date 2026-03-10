#include "symbol_table.hpp"
#include "common.hpp"

SymbolPtr SymbolTable::add_symbol(std::string name, TypePtr type) {
  // 实现符号表的插入操作
  // 并设置 symbol 的 unique_name 属性（你也可以等到 IR Translation 阶段再设置）
  // 对于局部变量和数组，最好为该标识符重新生成一个唯一名称
  // 对于全局变量和函数，直接使用原名称即可
  // 最后，如果插入成功，返回新的符号
  // 如果符号已经存在，返回 nullptr
  if (scopes.empty()) {
    enter_scope();  // 确保至少有一个作用域
  }
  
  // 创建新的符号
  auto symbol = std::make_shared<Symbol>(name, type);
  // 设置作用域级别 - 作用域大小即为作用域级别
  symbol->scope_level = scopes.size();
  
  // 检查当前作用域是否已存在同名符号
  auto& current_scope = scopes.back();
  if (current_scope.find(name) != current_scope.end()) {
    throw std::runtime_error("Symbol '" + name + "' already defined in current scope");
  }
  
  // 设置唯一名称
  if (scopes.size() == 1) {
    // 全局作用域，直接使用原名称
    symbol->unique_name = name;
  } else {
    // 局部作用域，生成唯一名称
    static int unique_counter = 0;
    symbol->unique_name = name + "_" + std::to_string(unique_counter++);
  }
  
  // 将符号添加到当前作用域
  current_scope[name] = symbol;
  
  return symbol;
}


SymbolPtr SymbolTable::find_symbol(std::string name,
  bool in_current_scope) const {

  // 实现符号表的查找操作
  // 找到了返回对应的符号，否则返回 nullptr
  // in_current_scope 为 true 时，只在当前作用域查找

  // 从内层作用域向外层作用域查找
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }

  return nullptr;
}

void SymbolTable::enter_scope() {
  // 实现符号表的进入作用域操作
  // 需要创建一个新的作用域

  // 创建新的作用域
  scopes.push_back({});
}

void SymbolTable::exit_scope() {
  // 实现符号表的退出作用域操作
  // 需要删除当前作用域中的所有符号

  // 删除当前作用域
  if (!scopes.empty()) {
    scopes.pop_back();
  } else {
    throw std::runtime_error("Cannot exit from global scope");
  }
}
