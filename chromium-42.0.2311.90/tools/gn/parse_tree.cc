// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/parse_tree.h"

#include <string>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "tools/gn/functions.h"
#include "tools/gn/operators.h"
#include "tools/gn/scope.h"
#include "tools/gn/string_utils.h"

namespace {

std::string IndentFor(int value) {
  return std::string(value, ' ');
}

}  // namespace

Comments::Comments() {
}

Comments::~Comments() {
}

void Comments::ReverseSuffix() {
  for (int i = 0, j = static_cast<int>(suffix_.size() - 1); i < j; ++i, --j)
    std::swap(suffix_[i], suffix_[j]);
}

ParseNode::ParseNode() {
}

ParseNode::~ParseNode() {
}

const AccessorNode* ParseNode::AsAccessor() const { return nullptr; }
const BinaryOpNode* ParseNode::AsBinaryOp() const { return nullptr; }
const BlockCommentNode* ParseNode::AsBlockComment() const { return nullptr; }
const BlockNode* ParseNode::AsBlock() const { return nullptr; }
const ConditionNode* ParseNode::AsConditionNode() const { return nullptr; }
const EndNode* ParseNode::AsEnd() const { return nullptr; }
const FunctionCallNode* ParseNode::AsFunctionCall() const { return nullptr; }
const IdentifierNode* ParseNode::AsIdentifier() const { return nullptr; }
const ListNode* ParseNode::AsList() const { return nullptr; }
const LiteralNode* ParseNode::AsLiteral() const { return nullptr; }
const UnaryOpNode* ParseNode::AsUnaryOp() const { return nullptr; }

Comments* ParseNode::comments_mutable() {
  if (!comments_)
    comments_.reset(new Comments);
  return comments_.get();
}

void ParseNode::PrintComments(std::ostream& out, int indent) const {
  if (comments_) {
    std::string ind = IndentFor(indent + 1);
    for (const auto& token : comments_->before())
      out << ind << "+BEFORE_COMMENT(\"" << token.value() << "\")\n";
    for (const auto& token : comments_->suffix())
      out << ind << "+SUFFIX_COMMENT(\"" << token.value() << "\")\n";
    for (const auto& token : comments_->after())
      out << ind << "+AFTER_COMMENT(\"" << token.value() << "\")\n";
  }
}

// AccessorNode ---------------------------------------------------------------

AccessorNode::AccessorNode() {
}

AccessorNode::~AccessorNode() {
}

const AccessorNode* AccessorNode::AsAccessor() const {
  return this;
}

Value AccessorNode::Execute(Scope* scope, Err* err) const {
  if (index_)
    return ExecuteArrayAccess(scope, err);
  else if (member_)
    return ExecuteScopeAccess(scope, err);
  NOTREACHED();
  return Value();
}

LocationRange AccessorNode::GetRange() const {
  if (index_)
    return LocationRange(base_.location(), index_->GetRange().end());
  else if (member_)
    return LocationRange(base_.location(), member_->GetRange().end());
  NOTREACHED();
  return LocationRange();
}

Err AccessorNode::MakeErrorDescribing(const std::string& msg,
                                      const std::string& help) const {
  return Err(GetRange(), msg, help);
}

void AccessorNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "ACCESSOR\n";
  PrintComments(out, indent);
  out << IndentFor(indent + 1) << base_.value() << "\n";
  if (index_)
    index_->Print(out, indent + 1);
  else if (member_)
    member_->Print(out, indent + 1);
}

Value AccessorNode::ExecuteArrayAccess(Scope* scope, Err* err) const {
  Value index_value = index_->Execute(scope, err);
  if (err->has_error())
    return Value();
  if (!index_value.VerifyTypeIs(Value::INTEGER, err))
    return Value();

  const Value* base_value = scope->GetValue(base_.value(), true);
  if (!base_value) {
    *err = MakeErrorDescribing("Undefined identifier.");
    return Value();
  }
  if (!base_value->VerifyTypeIs(Value::LIST, err))
    return Value();

  int64 index_int = index_value.int_value();
  if (index_int < 0) {
    *err = Err(index_->GetRange(), "Negative array subscript.",
        "You gave me " + base::Int64ToString(index_int) + ".");
    return Value();
  }
  size_t index_sizet = static_cast<size_t>(index_int);
  if (index_sizet >= base_value->list_value().size()) {
    *err = Err(index_->GetRange(), "Array subscript out of range.",
        "You gave me " + base::Int64ToString(index_int) +
        " but I was expecting something from 0 to " +
        base::Int64ToString(
            static_cast<int64>(base_value->list_value().size()) - 1) +
        ", inclusive.");
    return Value();
  }

  // Doing this assumes that there's no way in the language to do anything
  // between the time the reference is created and the time that the reference
  // is used. If there is, this will crash! Currently, this is just used for
  // array accesses where this "shouldn't" happen.
  return base_value->list_value()[index_sizet];
}

Value AccessorNode::ExecuteScopeAccess(Scope* scope, Err* err) const {
  // We jump through some hoops here since ideally a.b will count "b" as
  // accessed in the given scope. The value "a" might be in some normal nested
  // scope and we can modify it, but it might also be inherited from the
  // readonly root scope and we can't do used variable tracking on it. (It's
  // not legal to const cast it away since the root scope will be in readonly
  // mode and being accessed from multiple threads without locking.) So this
  // code handles both cases.
  const Value* result = nullptr;

  // Look up the value in the scope named by "base_".
  Value* mutable_base_value = scope->GetMutableValue(base_.value(), true);
  if (mutable_base_value) {
    // Common case: base value is mutable so we can track variable accesses
    // for unused value warnings.
    if (!mutable_base_value->VerifyTypeIs(Value::SCOPE, err))
      return Value();
    result = mutable_base_value->scope_value()->GetValue(
        member_->value().value(), true);
  } else {
    // Fall back to see if the value is on a read-only scope.
    const Value* const_base_value = scope->GetValue(base_.value(), true);
    if (const_base_value) {
      // Read only value, don't try to mark the value access as a "used" one.
      if (!const_base_value->VerifyTypeIs(Value::SCOPE, err))
        return Value();
      result =
          const_base_value->scope_value()->GetValue(member_->value().value());
    } else {
      *err = Err(base_, "Undefined identifier.");
      return Value();
    }
  }

  if (!result) {
    *err = Err(member_.get(), "No value named \"" +
        member_->value().value() + "\" in scope \"" + base_.value() + "\"");
    return Value();
  }
  return *result;
}

// BinaryOpNode ---------------------------------------------------------------

BinaryOpNode::BinaryOpNode() {
}

BinaryOpNode::~BinaryOpNode() {
}

const BinaryOpNode* BinaryOpNode::AsBinaryOp() const {
  return this;
}

Value BinaryOpNode::Execute(Scope* scope, Err* err) const {
  return ExecuteBinaryOperator(scope, this, left_.get(), right_.get(), err);
}

LocationRange BinaryOpNode::GetRange() const {
  return left_->GetRange().Union(right_->GetRange());
}

Err BinaryOpNode::MakeErrorDescribing(const std::string& msg,
                                      const std::string& help) const {
  return Err(op_, msg, help);
}

void BinaryOpNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "BINARY(" << op_.value() << ")\n";
  PrintComments(out, indent);
  left_->Print(out, indent + 1);
  right_->Print(out, indent + 1);
}

// BlockNode ------------------------------------------------------------------

BlockNode::BlockNode(bool has_scope) : has_scope_(has_scope) {
}

BlockNode::~BlockNode() {
  STLDeleteContainerPointers(statements_.begin(), statements_.end());
}

const BlockNode* BlockNode::AsBlock() const {
  return this;
}

Value BlockNode::Execute(Scope* containing_scope, Err* err) const {
  if (has_scope_) {
    Scope our_scope(containing_scope);
    Value ret = ExecuteBlockInScope(&our_scope, err);
    if (err->has_error())
      return Value();

    // Check for unused vars in the scope.
    our_scope.CheckForUnusedVars(err);
    return ret;
  }
  return ExecuteBlockInScope(containing_scope, err);
}

LocationRange BlockNode::GetRange() const {
  if (begin_token_.type() != Token::INVALID &&
      end_->value().type() != Token::INVALID) {
    return begin_token_.range().Union(end_->value().range());
  } else if (!statements_.empty()) {
    return statements_[0]->GetRange().Union(
        statements_[statements_.size() - 1]->GetRange());
  }
  return LocationRange();
}

Err BlockNode::MakeErrorDescribing(const std::string& msg,
                                   const std::string& help) const {
  return Err(GetRange(), msg, help);
}

void BlockNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "BLOCK\n";
  PrintComments(out, indent);
  for (const auto& statement : statements_)
    statement->Print(out, indent + 1);
  if (end_ && end_->comments())
    end_->Print(out, indent + 1);
}

Value BlockNode::ExecuteBlockInScope(Scope* our_scope, Err* err) const {
  for (size_t i = 0; i < statements_.size() && !err->has_error(); i++) {
    // Check for trying to execute things with no side effects in a block.
    const ParseNode* cur = statements_[i];
    if (cur->AsList() || cur->AsLiteral() || cur->AsUnaryOp() ||
        cur->AsIdentifier()) {
      *err = cur->MakeErrorDescribing(
          "This statement has no effect.",
          "Either delete it or do something with the result.");
      return Value();
    }
    cur->Execute(our_scope, err);
  }
  return Value();
}

// ConditionNode --------------------------------------------------------------

ConditionNode::ConditionNode() {
}

ConditionNode::~ConditionNode() {
}

const ConditionNode* ConditionNode::AsConditionNode() const {
  return this;
}

Value ConditionNode::Execute(Scope* scope, Err* err) const {
  Value condition_result = condition_->Execute(scope, err);
  if (err->has_error())
    return Value();
  if (condition_result.type() != Value::BOOLEAN) {
    *err = condition_->MakeErrorDescribing(
        "Condition does not evaluate to a boolean value.",
        std::string("This is a value of type \"") +
            Value::DescribeType(condition_result.type()) +
            "\" instead.");
    err->AppendRange(if_token_.range());
    return Value();
  }

  if (condition_result.boolean_value()) {
    if_true_->ExecuteBlockInScope(scope, err);
  } else if (if_false_) {
    // The else block is optional. It's either another condition (for an
    // "else if" and we can just Execute it and the condition will handle
    // the scoping) or it's a block indicating an "else" in which ase we
    // need to be sure it inherits our scope.
    const BlockNode* if_false_block = if_false_->AsBlock();
    if (if_false_block)
      if_false_block->ExecuteBlockInScope(scope, err);
    else
      if_false_->Execute(scope, err);
  }

  return Value();
}

LocationRange ConditionNode::GetRange() const {
  if (if_false_)
    return if_token_.range().Union(if_false_->GetRange());
  return if_token_.range().Union(if_true_->GetRange());
}

Err ConditionNode::MakeErrorDescribing(const std::string& msg,
                                       const std::string& help) const {
  return Err(if_token_, msg, help);
}

void ConditionNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "CONDITION\n";
  PrintComments(out, indent);
  condition_->Print(out, indent + 1);
  if_true_->Print(out, indent + 1);
  if (if_false_)
    if_false_->Print(out, indent + 1);
}

// FunctionCallNode -----------------------------------------------------------

FunctionCallNode::FunctionCallNode() {
}

FunctionCallNode::~FunctionCallNode() {
}

const FunctionCallNode* FunctionCallNode::AsFunctionCall() const {
  return this;
}

Value FunctionCallNode::Execute(Scope* scope, Err* err) const {
  return functions::RunFunction(scope, this, args_.get(), block_.get(), err);
}

LocationRange FunctionCallNode::GetRange() const {
  if (function_.type() == Token::INVALID)
    return LocationRange();  // This will be null in some tests.
  if (block_)
    return function_.range().Union(block_->GetRange());
  return function_.range().Union(args_->GetRange());
}

Err FunctionCallNode::MakeErrorDescribing(const std::string& msg,
                                          const std::string& help) const {
  return Err(function_, msg, help);
}

void FunctionCallNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "FUNCTION(" << function_.value() << ")\n";
  PrintComments(out, indent);
  args_->Print(out, indent + 1);
  if (block_)
    block_->Print(out, indent + 1);
}

// IdentifierNode --------------------------------------------------------------

IdentifierNode::IdentifierNode() {
}

IdentifierNode::IdentifierNode(const Token& token) : value_(token) {
}

IdentifierNode::~IdentifierNode() {
}

const IdentifierNode* IdentifierNode::AsIdentifier() const {
  return this;
}

Value IdentifierNode::Execute(Scope* scope, Err* err) const {
  const Value* value = scope->GetValue(value_.value(), true);
  Value result;
  if (!value) {
    *err = MakeErrorDescribing("Undefined identifier");
    return result;
  }

  result = *value;
  result.set_origin(this);
  return result;
}

LocationRange IdentifierNode::GetRange() const {
  return value_.range();
}

Err IdentifierNode::MakeErrorDescribing(const std::string& msg,
                                        const std::string& help) const {
  return Err(value_, msg, help);
}

void IdentifierNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "IDENTIFIER(" << value_.value() << ")\n";
  PrintComments(out, indent);
}

// ListNode -------------------------------------------------------------------

ListNode::ListNode() : prefer_multiline_(false) {
}

ListNode::~ListNode() {
  STLDeleteContainerPointers(contents_.begin(), contents_.end());
}

const ListNode* ListNode::AsList() const {
  return this;
}

Value ListNode::Execute(Scope* scope, Err* err) const {
  Value result_value(this, Value::LIST);
  std::vector<Value>& results = result_value.list_value();
  results.reserve(contents_.size());

  for (const auto& cur : contents_) {
    if (cur->AsBlockComment())
      continue;
    results.push_back(cur->Execute(scope, err));
    if (err->has_error())
      return Value();
    if (results.back().type() == Value::NONE) {
      *err = cur->MakeErrorDescribing(
          "This does not evaluate to a value.",
          "I can't do something with nothing.");
      return Value();
    }
  }
  return result_value;
}

LocationRange ListNode::GetRange() const {
  return LocationRange(begin_token_.location(),
                       end_->value().location());
}

Err ListNode::MakeErrorDescribing(const std::string& msg,
                                  const std::string& help) const {
  return Err(begin_token_, msg, help);
}

void ListNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "LIST" << (prefer_multiline_ ? " multiline" : "")
      << "\n";
  PrintComments(out, indent);
  for (const auto& cur : contents_)
    cur->Print(out, indent + 1);
  if (end_ && end_->comments())
    end_->Print(out, indent + 1);
}

// LiteralNode -----------------------------------------------------------------

LiteralNode::LiteralNode() {
}

LiteralNode::LiteralNode(const Token& token) : value_(token) {
}

LiteralNode::~LiteralNode() {
}

const LiteralNode* LiteralNode::AsLiteral() const {
  return this;
}

Value LiteralNode::Execute(Scope* scope, Err* err) const {
  switch (value_.type()) {
    case Token::TRUE_TOKEN:
      return Value(this, true);
    case Token::FALSE_TOKEN:
      return Value(this, false);
    case Token::INTEGER: {
      int64 result_int;
      if (!base::StringToInt64(value_.value(), &result_int)) {
        *err = MakeErrorDescribing("This does not look like an integer");
        return Value();
      }
      return Value(this, result_int);
    }
    case Token::STRING: {
      Value v(this, Value::STRING);
      ExpandStringLiteral(scope, value_, &v, err);
      return v;
    }
    default:
      NOTREACHED();
      return Value();
  }
}

LocationRange LiteralNode::GetRange() const {
  return value_.range();
}

Err LiteralNode::MakeErrorDescribing(const std::string& msg,
                                     const std::string& help) const {
  return Err(value_, msg, help);
}

void LiteralNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "LITERAL(" << value_.value() << ")\n";
  PrintComments(out, indent);
}

// UnaryOpNode ----------------------------------------------------------------

UnaryOpNode::UnaryOpNode() {
}

UnaryOpNode::~UnaryOpNode() {
}

const UnaryOpNode* UnaryOpNode::AsUnaryOp() const {
  return this;
}

Value UnaryOpNode::Execute(Scope* scope, Err* err) const {
  Value operand_value = operand_->Execute(scope, err);
  if (err->has_error())
    return Value();
  return ExecuteUnaryOperator(scope, this, operand_value, err);
}

LocationRange UnaryOpNode::GetRange() const {
  return op_.range().Union(operand_->GetRange());
}

Err UnaryOpNode::MakeErrorDescribing(const std::string& msg,
                                     const std::string& help) const {
  return Err(op_, msg, help);
}

void UnaryOpNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "UNARY(" << op_.value() << ")\n";
  PrintComments(out, indent);
  operand_->Print(out, indent + 1);
}

// BlockCommentNode ------------------------------------------------------------

BlockCommentNode::BlockCommentNode() {
}

BlockCommentNode::~BlockCommentNode() {
}

const BlockCommentNode* BlockCommentNode::AsBlockComment() const {
  return this;
}

Value BlockCommentNode::Execute(Scope* scope, Err* err) const {
  return Value();
}

LocationRange BlockCommentNode::GetRange() const {
  return comment_.range();
}

Err BlockCommentNode::MakeErrorDescribing(const std::string& msg,
                                          const std::string& help) const {
  return Err(comment_, msg, help);
}

void BlockCommentNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "BLOCK_COMMENT(" << comment_.value() << ")\n";
  PrintComments(out, indent);
}


// EndNode ---------------------------------------------------------------------

EndNode::EndNode(const Token& token) : value_(token) {
}

EndNode::~EndNode() {
}

const EndNode* EndNode::AsEnd() const {
  return this;
}

Value EndNode::Execute(Scope* scope, Err* err) const {
  return Value();
}

LocationRange EndNode::GetRange() const {
  return value_.range();
}

Err EndNode::MakeErrorDescribing(const std::string& msg,
                                        const std::string& help) const {
  return Err(value_, msg, help);
}

void EndNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "END(" << value_.value() << ")\n";
  PrintComments(out, indent);
}
