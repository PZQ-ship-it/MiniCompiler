#ifndef AST_TREE_HPP
#define AST_TREE_HPP

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <functional>
#include "NodeColor.h"

#include "common.hpp"
// #include "semantic/type.hpp"
#include "semantic/symbol_table.hpp"
extern int yylineno;

namespace AST {

class Node;
using NodePtr = std::shared_ptr<Node>;
class Node {
 public:
  int lineno;
  // TypePtr type;

  virtual std::vector<NodePtr> get_children() { return std::vector<NodePtr>(); }
  void print_tree(std::string prefix = "", std::string info_prefix = "");
  virtual std::string to_string() = 0;

  // 图形化相关方法
  virtual std::string Color() { return NodeColor::DefaultColor; }
  virtual std::string FormalInfo() { 
    return to_string() + " (line " + std::to_string(lineno) + ")"; 
  }

  // 导出为Graphviz dot格式的静态方法
  static void draw_ast(NodePtr root, const std::string& filename);

  Node() : lineno(yylineno) {}
  virtual ~Node() = default;
};

class IntConst;
using IntConstPtr = std::shared_ptr<IntConst>;
class IntConst : public Node {
 public:
  int value;
  IntConst(int value) : value(value) {}
  std::string to_string() override {
    return "IntConst <value: " + std::to_string(value) + ">";
  }
  std::string Color() override { return NodeColor::LtrColor; }
};

class ArrayIndices;
using ArrayIndicesPtr = std::shared_ptr<ArrayIndices>;
class ArrayIndices : public Node {
 public:
  std::vector<NodePtr> indices;  // 存储各维度的索引表达式
  
  void add_index(NodePtr index) { indices.push_back(index); }
  std::string to_string() override { return "ArrayIndices"; }
  std::vector<NodePtr> get_children() override { return indices; }
  std::string Color() override { return NodeColor::ExpColor; }
};

class LVal;
using LValPtr = std::shared_ptr<LVal>;
class LVal : public Node {
  public:
   std::string ident;
   bool is_array_element;  // 是否为数组元素访问
   std::vector<NodePtr> indices;  // 如果是数组元素访问，存储各维度索引表达式
   SymbolPtr symbol;
   LVal(std::string ident) : ident(ident), is_array_element(false) {}
   
   std::string to_string() override { 
     if (is_array_element) {
       return "LVal <ident: " + ident + ", array_element>";
     } else {
       return "LVal <ident: " + ident + ">";
     }
   }
   
   std::vector<NodePtr> get_children() override { 
     return is_array_element ? indices : std::vector<NodePtr>(); 
   }
   std::string Color() override { return NodeColor::IdentColor; }
 };

class UnaryExp;
using UnaryExpPtr = std::shared_ptr<UnaryExp>;
class UnaryExp : public Node {
 public:
  BinaryOp op;
  NodePtr exp;
  UnaryExp(BinaryOp op, NodePtr exp) : op(op), exp(exp) {}
  std::string to_string() override {
    return "UnaryExp <op: " + std::string(op_to_string(op)) + ">";
  }
  std::vector<NodePtr> get_children() override { return {exp}; }
  std::string Color() override { return NodeColor::ExpColor; }
};

class BinaryExp;
using BinaryExpPtr = std::shared_ptr<BinaryExp>;
class BinaryExp : public Node {
 public:
  BinaryOp op;
  NodePtr left, right;

  BinaryExp(BinaryOp op, NodePtr left, NodePtr right)
      : op(op), left(left), right(right) {}
  std::string to_string() override {
    return "BinaryExp <op: " + std::string(op_to_string(op)) + ">";
  }
  std::vector<NodePtr> get_children() override { return {left, right}; }
  std::string Color() override { return NodeColor::ExpColor; }
};

class FuncCall;
using FuncCallPtr = std::shared_ptr<FuncCall>;
class FuncCall : public Node {
 public:
  std::string name;
  std::vector<NodePtr> args;
  SymbolPtr symbol;
  FuncCall(char const *name) : name(name) {}
  FuncCall(NodePtr exp) { add_arg(exp); }
  void add_arg(NodePtr exp) { args.push_back(exp); }
  std::string to_string() override { return "FuncCall <name: " + name + ">"; }
  std::vector<NodePtr> get_children() override { return args; }
  std::string Color() override { return NodeColor::FunColor; }
};

class Block;
using BlockPtr = std::shared_ptr<Block>;
class Block : public Node {
 public:
  std::vector<NodePtr> stmts;
  Block() {}
  Block(NodePtr stmt) { add_stmt(stmt); }
  void add_stmt(NodePtr stmt) { stmts.push_back(stmt); }
  std::string to_string() override { return "Block"; }
  std::vector<NodePtr> get_children() override { return stmts; }
  std::string Color() override { return NodeColor::DefColor; }
};

class AssignStmt;
using AssignStmtPtr = std::shared_ptr<AssignStmt>;
class AssignStmt : public Node {
 public:
  LValPtr lval;
  NodePtr exp;
  AssignStmt(LValPtr lval, NodePtr exp) : lval(lval), exp(exp) {}
  std::string to_string() override { return "AssignStmt"; }
  std::vector<NodePtr> get_children() override { return {lval, exp}; }
  std::string Color() override { return NodeColor::AssignColor; }
};

class ReturnStmt;
using ReturnStmtPtr = std::shared_ptr<ReturnStmt>;
class ReturnStmt : public Node {
 public:
  NodePtr exp;
  ReturnStmt() : exp(nullptr) {}
  ReturnStmt(NodePtr exp) : exp(exp) {}
  std::string to_string() override { return "ReturnStmt"; }
  std::vector<NodePtr> get_children() override {
    return exp ? std::vector<NodePtr>{exp} : std::vector<NodePtr>();
  }
  std::string Color() override { return NodeColor::ReserveColor; }
};

class IfStmt;
using IfStmtPtr = std::shared_ptr<IfStmt>;
class IfStmt : public Node {
 public:
  NodePtr cond;        // 条件表达式
  NodePtr then_stmt;   // if 为真时执行的语句
  NodePtr else_stmt;   // else 部分的语句，可能为 nullptr
  
  IfStmt(NodePtr cond, NodePtr then_stmt, NodePtr else_stmt = nullptr)
      : cond(cond), then_stmt(then_stmt), else_stmt(else_stmt) {}
  
  std::string to_string() override { return "IfStmt"; }
  
  std::vector<NodePtr> get_children() override {
    if (else_stmt) {
      return {cond, then_stmt, else_stmt};
    } else {
      return {cond, then_stmt};
    }
  }
  std::string Color() override { return NodeColor::StmtColor; }
};

class WhileStmt;
using WhileStmtPtr = std::shared_ptr<WhileStmt>;
class WhileStmt : public Node {
 public:
  NodePtr cond;      // 循环条件
  NodePtr body;      // 循环体

  WhileStmt(NodePtr cond, NodePtr body) : cond(cond), body(body) {}
  
  std::string to_string() override { return "WhileStmt"; }
  
  std::vector<NodePtr> get_children() override { return {cond, body}; }
  std::string Color() override { return NodeColor::StmtColor; }
};

class EmptyStmt;
using EmptyStmtPtr = std::shared_ptr<EmptyStmt>;
class EmptyStmt : public Node {
 public:
  EmptyStmt() {}
  std::string to_string() override { return "EmptyStmt"; }
  std::vector<NodePtr> get_children() override { return {}; }
  std::string Color() override { return NodeColor::StmtColor; }
};

class ArrayDims;
using ArrayDimsPtr = std::shared_ptr<ArrayDims>;
class ArrayDims : public Node {
 public:
  std::vector<int> dims;  // 存储各维度大小
  
  void add_dim(int dim) { dims.push_back(dim); }
  std::string to_string() override { return "ArrayDims"; }
  std::string Color() override { return NodeColor::ListColor; }
};

class ArrayInitVal;
using ArrayInitValPtr = std::shared_ptr<ArrayInitVal>;
class ArrayInitVal : public Node {
 public:
  std::vector<NodePtr> values;  // 存储初始值表达式
  
  ArrayInitVal() {}
  void add_val(NodePtr val) { values.push_back(val); }
  std::string to_string() override { return "ArrayInitVal"; }
  std::vector<NodePtr> get_children() override { return values; }
  std::string Color() override { return NodeColor::ListColor; }
};

class VarDef;
using VarDefPtr = std::shared_ptr<VarDef>;
class VarDef : public Node {
  public:
   std::string ident;
   SymbolPtr symbol;
   bool is_array;  // 是否为数组
   std::vector<int> array_dims;  // 如果是数组，存储各维度大小
   
   VarDef(char const *ident) : ident(ident), is_array(false) {}
   std::string to_string() override { 
     if (is_array) {
       return "VarDef <ident: " + ident + ", array>";
     } else {
       return "VarDef <ident: " + ident + ">";
     }
   }
   std::string Color() override { return NodeColor::DefColor; }
 };

class VarDefInit;
using VarDefInitPtr = std::shared_ptr<VarDefInit>;
class VarDefInit : public VarDef {
  public:
   NodePtr init_val;
   VarDefInit(char const *ident, NodePtr init_val) : VarDef(ident), init_val(init_val) {}
   std::string to_string() override { 
     if (is_array) {
       return "VarDefInit <ident: " + ident + ", array>";
     } else {
       return "VarDefInit <ident: " + ident + ">";
     }
   }
   std::vector<NodePtr> get_children() override { return {init_val}; }
   std::string Color() override { return NodeColor::DefColor; }
 };

class VarDecl;
using VarDeclPtr = std::shared_ptr<VarDecl>;
class VarDecl : public Node {
 public:
  BasicType btype;
  std::vector<VarDefPtr> defs;
  VarDecl(VarDefPtr def) : btype(BasicType::Unknown) { add_def(def); }
  void add_def(VarDefPtr def) { defs.push_back(def); }
  std::string to_string() override {
    return "VarDecl <btype: " + std::string(type_to_string(btype)) + ">";
  }
  std::vector<NodePtr> get_children() override {
    return std::vector<NodePtr>(defs.begin(), defs.end());
  }
  std::string Color() override { return NodeColor::DefColor; }
};

class FuncFParam;
using FuncFParamPtr = std::shared_ptr<FuncFParam>;
// 修改FuncFParam类，支持数组类型参数
class FuncFParam : public Node {
 public:
  BasicType btype;
  std::string ident;
  bool is_array;                // 是否为数组类型参数
  std::vector<int> array_dims;  // 如果是多维数组，存储除第一维外的各维大小
  SymbolPtr symbol;             // 添加符号表引用

  FuncFParam(BasicType btype, char const *ident) 
      : btype(btype), ident(ident), is_array(false) {}
  
  std::string to_string() override {
    if (is_array) {
      return "FuncFParam <btype: " + std::string(type_to_string(btype)) + 
             ", ident: " + ident + ", array>";
    } else {
      return "FuncFParam <btype: " + std::string(type_to_string(btype)) + 
             ", ident: " + ident + ">";
    }
  }

  std::vector<NodePtr> get_children() override { return {}; }
  std::string Color() override { return NodeColor::IdentColor; }
};

class FuncFParams;
using FuncFParamsPtr = std::shared_ptr<FuncFParams>;
class FuncFParams : public Node {
 public:
  std::vector<FuncFParamPtr> params;
  FuncFParams(FuncFParamPtr param) { add_param(param); }
  void add_param(FuncFParamPtr param) { params.push_back(param); }
  std::string to_string() override { return "FuncFParams"; }
  std::vector<NodePtr> get_children() override {
    return std::vector<NodePtr>(params.begin(), params.end());
  }
  std::string Color() override { return NodeColor::ListColor; }
};

class FuncDef;
using FuncDefPtr = std::shared_ptr<FuncDef>;
class FuncDef : public Node {
 public:
  BasicType return_btype;
  std::string name;
  std::vector<FuncFParamPtr> params;  // 添加参数列表
  BlockPtr block;
  SymbolPtr symbol;
  FuncDef(BasicType return_btype, char const *name, BlockPtr block)
      : return_btype(return_btype), name(name), block(block) {}
  std::string to_string() override {
    return "FuncDef <return_btype: " +
           std::string(type_to_string(return_btype)) + ", name: " + name + ">";
  }
  std::vector<NodePtr> get_children() override {
    std::vector<NodePtr> children;
    for (auto &param : params) {
      children.push_back(param);
    }
    children.push_back(block);
    return children;
  }
  std::string Color() override { return NodeColor::FunColor; }
};

class CompUnit;
using CompUnitPtr = std::shared_ptr<CompUnit>;
class CompUnit : public Node {
 public:
  std::vector<NodePtr> units;  // FuncDef or VarDecl
  CompUnit(NodePtr unit) { add_unit(unit); }
  void add_unit(NodePtr unit) { units.push_back(unit); }
  std::string to_string() override { return "CompUnit"; }
  std::vector<NodePtr> get_children() override { return units; }
  std::string Color() override { return NodeColor::ProgramColor; }
};

// #warning More AST nodes are needed

}  // namespace AST

#endif  // AST_TREE_HPP