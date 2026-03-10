#ifndef IR_IR_TRANSLATOR_HPP
#define IR_IR_TRANSLATOR_HPP

#include <memory>
#include <vector>

#include "ast/tree.hpp"
#include "ir/ir.hpp"

class IRTranslator {
 public:
  IR::Code translate(AST::NodePtr node);
  IR::Code translateExp(AST::NodePtr node, const std::string &place = "");

 private:
  IR::Code translateCompUnit(AST::CompUnitPtr node);
  IR::Code translateFuncDef(AST::FuncDefPtr node);
  IR::Code translateBlock(AST::BlockPtr node);
  IR::Code translateVarDecl(AST::VarDeclPtr node);
  IR::Code translateVarDef(AST::VarDefPtr node);
  IR::Code translateAssignStmt(AST::AssignStmtPtr node);
  IR::Code translateReturnStmt(AST::ReturnStmtPtr node);
  IR::Code translateLVal(AST::LValPtr node, const std::string &place = "");
  IR::Code translateBinaryExp(AST::BinaryExpPtr node,
                              const std::string &place = "");
  IR::Code translateUnaryExp(AST::UnaryExpPtr node,
                             const std::string &place = "");
  IR::Code translateFuncCall(AST::FuncCallPtr node,
                             const std::string &place = "");
  IR::Code translateIntConst(AST::IntConstPtr node,
                             const std::string &place = "");
  IR::Code translateIfStmt(AST::IfStmtPtr node);
  IR::Code translateWhileStmt(AST::WhileStmtPtr node);
  IR::Code translateEmptyStmt(AST::EmptyStmtPtr node);
  IR::Code translateCond(AST::NodePtr node, const std::string &true_label,
                        const std::string &false_label);
  std::string new_label();

  std::string new_temp();
  void initGlobalArrayRecursive(AST::ArrayInitValPtr init, std::vector<int>& values, 
    const std::vector<int>& dims, int& current_pos,
    int dim_index = 0, int start_index = 0);
    
  IR::Code initLocalArrayRecursive(const std::string& array_name, AST::ArrayInitValPtr init,
        const std::vector<int>& dims, int& current_pos,
        int dim_index = 0, int offset = 0);
        
  int getTotalElements(const std::vector<int>& dims);

  IR::Code calculateArrayOffset(const std::string& array_name, 
    const std::vector<AST::NodePtr>& indices,
    TypePtr type, const std::string& place,
    bool is_global, bool is_store,
    const std::string& store_value = "");

  std::vector<int> computeIndices(int linear_index, const std::vector<int>& dims);
  int computeLinearIndex(const std::vector<int>& indices, const std::vector<int>& dims);
  
};

#endif  // IR_IR_TRANSLATOR_HPP