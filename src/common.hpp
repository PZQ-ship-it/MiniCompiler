#ifndef COMMON_HPP
#define COMMON_HPP

#include <iostream>
#include <memory>
#include <string>

// 基本类型枚举
enum class BasicType { Unknown, Int, Void };
inline constexpr const char *type_to_string(BasicType type) {
  switch (type) {
    case BasicType::Unknown:
      return "unknown";
    case BasicType::Int:
      return "int";
    case BasicType::Void:
      return "void";
  }
  return "unknown";
}

// 二元运算符枚举
enum class BinaryOp { Add, Sub, Mul, Div, Mod, Not,Lt, Le, Gt, Ge, Eq, Ne, And, Or };
inline constexpr const char *op_to_string(BinaryOp op) {
  switch (op) {
    case BinaryOp::Add: return "+";
    case BinaryOp::Sub: return "-";
    case BinaryOp::Mul: return "*";
    case BinaryOp::Div: return "/";
    case BinaryOp::Mod: return "%";
    case BinaryOp::Not: return "!";
    case BinaryOp::Lt: return "<";
    case BinaryOp::Le: return "<=";
    case BinaryOp::Gt: return ">";
    case BinaryOp::Ge: return ">=";
    case BinaryOp::Eq: return "==";
    case BinaryOp::Ne: return "!=";
    case BinaryOp::And: return "&&";
    case BinaryOp::Or: return "||";
    default: return "unknown";
  }
}

// use C++ RTTI to check the type of a shared_ptr
template <typename T, typename U>
inline bool type_of(const std::shared_ptr<U> &node) {
  return std::dynamic_pointer_cast<T>(node) != nullptr;
}

#define ASSERT(expr, msg)                                                \
  do {                                                                   \
    if (!(expr)) {                                                       \
      std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ \
                << " (" << #expr << "): " << msg << std::endl;           \
      std::exit(1);                                                      \
    }                                                                    \
  } while (0)

#endif  // COMMON_HPP