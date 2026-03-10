#ifndef SEMANTIC_TYPE_CHECKER_HPP
#define SEMANTIC_TYPE_CHECKER_HPP

#include <memory>

#include "ast/tree.hpp"
#include "symbol_table.hpp"

class TypeChecker {
 public:
  TypeChecker();

  TypePtr check(AST::NodePtr node);
 // 打印符号表内容的辅助方法
  void print_symbol_table() const;
  bool checkArrayInitList(const std::vector<int>& dims, const AST::ArrayInitValPtr& init_val, 
    int& used_elems, int line_no, TypePtr expected_element_type = nullptr);

 private:
  /// @brief The symbol table
  SymbolTable symbol_table;
  TypePtr current_function_return_type;
  

  TypePtr checkIntConst(AST::IntConstPtr node);
  TypePtr checkLVal(AST::LValPtr node);
  TypePtr checkUnaryExp(AST::UnaryExpPtr node);
  TypePtr checkBinaryExp(AST::BinaryExpPtr node);
  TypePtr checkFuncCall(AST::FuncCallPtr node);
  TypePtr checkBlock(AST::BlockPtr node, bool new_scope = true);
  TypePtr checkAssignStmt(AST::AssignStmtPtr node);
  TypePtr checkReturnStmt(AST::ReturnStmtPtr node);
  TypePtr checkVarDef(AST::VarDefPtr node, BasicType var_type);
  TypePtr checkVarDecl(AST::VarDeclPtr node);
  TypePtr checkFuncDef(AST::FuncDefPtr node);
  TypePtr checkCompUnit(AST::CompUnitPtr node);
  TypePtr checkIfStmt(AST::IfStmtPtr node);
  TypePtr checkWhileStmt(AST::WhileStmtPtr node);

  // bool checkArrayInitList(const std::vector<int>& dims, const AST::ArrayInitValPtr& init_val, 
    // int& used_elems, int line_no, TypePtr expected_element_type);
};

#endif  // SEMANTIC_TYPE_CHECKER_HPP