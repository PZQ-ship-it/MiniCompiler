#ifndef SEMANTIC_SYMBOL_TABLE_HPP
#define SEMANTIC_SYMBOL_TABLE_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "type.hpp"

class Symbol;
using SymbolPtr = std::shared_ptr<Symbol>;
class Symbol {
 public:
  /// @brief The name of the symbol
  std::string name;
  /// @brief The unique name of the symbol
  std::string unique_name;
  /// @brief The type of the symbol
  TypePtr type;
  int scope_level;
  
  // 删除或注释掉这个构造函数
  // Symbol(std::string name, TypePtr type) : name(name), type(type) {}
  
  // 保留这个更完整的构造函数
  Symbol(const std::string& name, TypePtr type, int scope_level = 0) 
      : name(name), unique_name(name), type(type), scope_level(scope_level) {}
  
  static SymbolPtr create(std::string name, TypePtr type) {
    return std::make_shared<Symbol>(name, type);
  }
};

class SymbolTable {
 public:
  /// @brief Add a symbol to the table and return the unique name of the symbol
  /// @param name The name of the symbol
  /// @param type The type of the symbol
  /// @return The added symbol if added successfully, nullptr otherwise
  SymbolPtr add_symbol(std::string name, TypePtr type);

  /// @brief Find a symbol by name
  /// @param name The name of the symbol
  /// @param in_current_scope Whether to search only in the current scope
  /// @return The symbol if found, nullptr otherwise
  SymbolPtr find_symbol(std::string name, bool in_current_scope = false) const;

  /// @brief Enter a new scope
  void enter_scope();

  /// @brief Exit the current scope
  void exit_scope();

  friend class TypeChecker;

 private:
 // 使用嵌套的map来存储符号，外层vector表示作用域的嵌套
 std::vector<std::unordered_map<std::string, SymbolPtr>> scopes;
// #warning Not implemented: SymbolTable
};

#endif  // SEMANTIC_SYMBOL_TABLE_HPP