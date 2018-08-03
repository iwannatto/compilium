#include <stdio.h>
#include <stdlib.h>

#include "compilium.h"

typedef enum {
  kTypeNone,
  kTypePointerOf,
  kTypeChar,
  kTypeInt,
} BasicType;

struct AST_TYPE {
  ASTNodeType type;
  BasicType basic_type;
  ASTType *pointer_of;
};

GenToAST(Type);
GenAllocAST(Type);

ASTType *AllocAndInitBasicType(BasicType basic_type) {
  ASTType *node = AllocASTType();
  node->basic_type = basic_type;
  return node;
}

ASTType *AllocAndInitASTTypePointerOf(ASTType *pointer_of) {
  ASTType *node = AllocASTType();
  node->basic_type = kTypePointerOf;
  node->pointer_of = pointer_of;
  return node;
}

ASTType *AllocAndInitASTType(ASTList *decl_specs, ASTDecltor *decltor) {
  if (GetSizeOfASTList(decl_specs) != 1) {
    Error("decl_specs contains 2 tokens or more is not supported");
  }
  ASTKeyword *kw = ToASTKeyword(GetASTNodeAt(decl_specs, 0));
  BasicType basic_type = kTypeNone;
  if (IsEqualToken(kw->token, "int")) {
    basic_type = kTypeInt;
  } else if (IsEqualToken(kw->token, "char")) {
    basic_type = kTypeChar;
  }
  if (basic_type == kTypeNone) {
    Error("Type %s is not implemented", kw->token->str);
  }
  ASTType *node = AllocAndInitBasicType(basic_type);
  for (ASTPointer *ptr = decltor->pointer; ptr; ptr = ptr->pointer) {
    node = AllocAndInitASTTypePointerOf(node);
  }

  return node;
}

ASTType *GetDereferencedTypeOf(ASTType *node) {
  if (node->basic_type == kTypePointerOf) {
    return node->pointer_of;
  }
  Error("GetDereferencedTypeOf: Not implemented for basic_type %d",
        node->basic_type);
  return NULL;
}

int GetSizeOfType(ASTType *node) {
  if (node->basic_type == kTypePointerOf) {
    return 8;
  } else if (node->basic_type == kTypeChar) {
    return 1;
  } else if (node->basic_type == kTypeInt) {
    // TODO: Change this from 8 to 4
    return 8;
  }
  Error("GetSizeOfType: Not implemented for basic_type %d", node->basic_type);
  return -1;
}

void PrintASTType(ASTType *node) {
  if (node->basic_type == kTypePointerOf) {
    PrintASTType(node->pointer_of);
    putchar('*');
  } else if (node->basic_type == kTypeChar) {
    printf("char");
  } else if (node->basic_type == kTypeInt) {
    printf("int");
  } else {
    Error("PrintASTType: Not implemented for basic_type %d", node->basic_type);
  }
}
