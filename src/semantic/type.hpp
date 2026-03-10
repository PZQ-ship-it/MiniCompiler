#ifndef SEMANTIC_TYPE_HPP
#define SEMANTIC_TYPE_HPP

#include <iostream>
#include <string>
#include <vector>

#include "common.hpp"

class Type;
using TypePtr = std::shared_ptr<Type>;
class Type {
 public:
  virtual bool equals(const TypePtr& other) const = 0;
  virtual std::string to_string() const = 0;
  virtual ~Type() = default;  // make the class polymorphic
};

class PrimitiveType;
using PrimitiveTypePtr = std::shared_ptr<PrimitiveType>;
class PrimitiveType : public Type {
 public:
  BasicType basic_type;

  PrimitiveType(BasicType basic_type) : basic_type(basic_type) {}

  static PrimitiveTypePtr create(BasicType basic_type) {
    return std::make_shared<PrimitiveType>(basic_type);
  }

  bool equals(const TypePtr& other) const override;

  std::string to_string() const override { return type_to_string(basic_type); }

  static const TypePtr Int;
  static const TypePtr Void;
};

inline const TypePtr PrimitiveType::Int = PrimitiveType::create(BasicType::Int);
inline const TypePtr PrimitiveType::Void =
    PrimitiveType::create(BasicType::Void);

class ArrayType;
using ArrayTypePtr = std::shared_ptr<ArrayType>;
class ArrayType : public Type {
 public:
  TypePtr element_type;
  std::vector<int> dims;

  ArrayType(TypePtr element_type, std::vector<int> dims)
      : element_type(element_type), dims(dims) {
    ASSERT(dims.size() > 0, "Array dimension should be greater than 0");
  }

  static ArrayTypePtr create(TypePtr element_type, std::vector<int> dims) {
    return std::make_shared<ArrayType>(element_type, dims);
  }

  bool equals(const TypePtr& other) const override;

  std::string to_string() const override {
    std::string result = element_type->to_string() + " (*)";
    for (size_t i = 0; i < dims.size(); i++) {
      result += "[" + std::to_string(dims[i]) + "]";
    }
    return result;
  }
};

class FuncType;
using FuncTypePtr = std::shared_ptr<FuncType>;
class FuncType : public Type {
 public:
  TypePtr return_type;
  std::vector<TypePtr> param_types;

  FuncType(TypePtr return_type, std::vector<TypePtr> param_types)
      : return_type(return_type), param_types(param_types) {}

  static FuncTypePtr create(TypePtr return_type,
                            std::vector<TypePtr> param_types) {
    return std::make_shared<FuncType>(return_type, param_types);
  }

  bool equals(const TypePtr& other) const override;

  std::string to_string() const override {
    std::string result = return_type->to_string() + " (*)(";
    for (size_t i = 0; i < param_types.size(); i++) {
      result += param_types[i]->to_string();
      if (i + 1 < param_types.size()) {
        result += ", ";
      }
    }
    return result + ")";
  }
};

#endif  // SEMANTIC_TYPE_HPP