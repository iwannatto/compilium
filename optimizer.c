#include "compilium.h"

//
struct Node *CreateNodeFromValue(int value) {
  char s[12];
  snprintf(s, sizeof(s), "%d", value);
  char *ds = strdup(s);  // duplicate because s is allocated on the stack
  int line = 0;
  struct Node *node = AllocNode(kASTExpr);

  if (value < 0) {
    // マイナスだったら一度負じゃない部分のトークンを作る
    node->op = CreateNextToken(ds + 1, ds + 1, &line);
    node->op->begin--;
    node->op->length++;
  } else {
    node->op = CreateNextToken(ds, ds, &line);  // use ds here
  }

  return node;
}

// Strength Reduction
// は式を受け取り、可能であればよりコストの低い演算に書き換える
// 左辺または右辺が負の値である演算には対応していない
// @param expr: 式を表すノード。NULL なら何もせず 0 を返す。
void StrengthReduction(struct Node **exprp) {
  assert(exprp != NULL);
  struct Node *expr = *exprp;
  if (!expr || expr->type != kASTExpr) {
    return;
  }
  if (!expr->left || !expr->right) {
    return;
  }
  if (!expr->left->op || !expr->right->op) {
    return;
  }

  if (expr->right->op->token_type != kTokenIntegerConstant) {
    return;
  }
  int right_var = strtol(expr->right->op->begin, NULL, 10);

  if (strncmp(expr->op->begin, "/", expr->op->length) == 0) {
    if (right_var < 0) {
      return;
    }
    if (__builtin_popcount(right_var) != 1) {
      return;
    }
    int log2_right_var = __builtin_popcount(right_var - 1);
    expr->op->begin = strdup(">>");
    expr->op->length = strlen(">>");
    expr->right = CreateNodeFromValue(log2_right_var);
    return;
  }
  if (strncmp(expr->op->begin, "/=", expr->op->length) == 0) {
    if (right_var < 0) {
      return;
    }
    if (__builtin_popcount(right_var) != 1) {
      return;
    }
    int log2_right_var = __builtin_popcount(right_var - 1);
    expr->op->begin = strdup(">>=");
    expr->op->length = strlen(">>=");
    expr->right = CreateNodeFromValue(log2_right_var);
    return;
  }
}

// ConstantPropagation は式を受け取り，左右が定数値であれば，
// 定数畳み込みを行い，式を定数式に書き換える。
//
// @param expr: 式を表すノード。NULL なら何もせず 0 を返す。
// @return 式が定数式に書き換わったら true
int ConstantPropagation(struct Node **exprp) {
  assert(exprp != NULL);
  struct Node *expr = *exprp;
  if (!expr || expr->type != kASTExpr) {
    return false;
  }

  if (!expr->right || !expr->right->op ||
      expr->right->op->token_type != kTokenIntegerConstant) {
    return false;
  }
  int right_var = strtol(expr->right->op->begin, NULL, 10);
  if (!expr->left) {
    // 単項演算子
    int val;
    if (strncmp(expr->op->begin, "-", expr->op->length) == 0) {
      val = -right_var;
    } else if (strncmp(expr->op->begin, "+", expr->op->length) == 0) {
      val = +right_var;
    } else {
      return false;
    }
    *exprp = CreateNodeFromValue(val);
    return true;
  }

  if (!expr->left || !expr->right) {
    return false;
  }
  if (!expr->left->op || !expr->right->op) {
    return false;
  }

  if (expr->left->op->token_type != kTokenIntegerConstant ||
      expr->right->op->token_type != kTokenIntegerConstant) {
    return false;
  }

  int left_var = strtol(expr->left->op->begin, NULL, 10);
  // PrintASTNode(expr);

  int val;
  if (strncmp(expr->op->begin, "+", expr->op->length) == 0) {
    val = left_var + right_var;
  } else if (strncmp(expr->op->begin, "-", expr->op->length) == 0) {
    val = left_var - right_var;
  } else if (strncmp(expr->op->begin, "*", expr->op->length) == 0) {
    val = left_var * right_var;
  } else if (strncmp(expr->op->begin, "/", expr->op->length) == 0) {
    val = left_var / right_var;
  } else if (strncmp(expr->op->begin, "%", expr->op->length) == 0) {
    val = left_var % right_var;
  } else {
    return false;
  }
  *exprp = CreateNodeFromValue(val);
  return true;
}

// 関数の中で親と同じ名前の関数を呼び出していたら1を返す
// 現在は return func(a) + b の形しか対応してない
// @param fn: 親の関数のノード
// @param np: 再帰で検索する子どものノード
int IsTailRecursiveFunction(struct Node *fn, struct Node *n) {
  assert(fn != NULL);
  if (n == NULL) {
    return false;
  }
  if (n->type == kASTExprFuncCall) {
    struct Node *fexpr = n->func_expr;
    if (fn->func_name_token->length != fexpr->op->length) {
      return false;
    }
    if (strncmp(fn->func_name_token->begin, fexpr->op->begin,
                fexpr->op->length) == 0) {
      return true;
    } else {
      return false;
    }
  }
  if (n->type == kASTExpr) {
    return IsTailRecursiveFunction(fn, n->left) ||
           IsTailRecursiveFunction(fn, n->right);
  }
  if (n->type == kASTExprStmt) {
    return IsTailRecursiveFunction(fn, n->left) ||
           IsTailRecursiveFunction(fn, n->right);
  }
  if (n->type == kASTJumpStmt) {
    if (n->op->token_type != kTokenKwReturn) {
      return false;
    }
    if (!n->right || !n->right->op ||
        strncmp(n->right->op->begin, "+", n->right->op->length) != 0) {
      return false;
    }
    // a + b case
    struct Node *result_expr = n->right;
    // Check if right operand is integer constant
    if (!result_expr->right ||
        !IsTokenWithType(result_expr->right->op, kTokenIntegerConstant)) {
      return false;
    }
    // Check if left operand is func call
    if (!result_expr->left || result_expr->left->type != kASTExprFuncCall) {
      return false;
    }
    // Check if calling the function itself
    struct Node *fexpr = result_expr->left->func_expr;
    if (!IsEqualToken(fn->func_name_token, fexpr->op)) {
      return false;
    }
    fprintf(stderr, "Found Recursive Return Stmt \n");
    return true;
  }
  if (n->type == kASTList) {
    for (int l = 0; l < GetSizeOfList(n); l++) {
      struct Node *stmt = GetNodeAt(n, l);
      if (IsTailRecursiveFunction(fn, stmt)) {
        return true;
      }
    }
    return false;
  }
  if (n->type == kASTForStmt) {
    return IsTailRecursiveFunction(fn, n->body);
  }
  return false;
}

struct Node *ParseStmt();
static struct Node *CreateStmt(const char *s) {
  char* ds = strdup(s);
  struct Node *tokens = Tokenize(ds);
  InitParser(&tokens);
  return ParseStmt();
}

// 末尾最適のASTを中身を実際にOptimizeする
// @param fn: 親の関数のノード
// @param n: 再帰で検索する子どものノード
void SubOptimizeRecursiveFunction(struct Node *fn, struct Node **np) {
  assert(fn != NULL);
  assert(np != NULL);
  struct Node *n = *np;
  if (n == NULL) {
    return;
  }
  if (n->type == kASTSelectionStmt){
    SubOptimizeRecursiveFunction(fn, &n->if_true_stmt);
    if (!n->if_else_stmt){
      SubOptimizeRecursiveFunction(fn, &n->if_else_stmt);
    }
  }
  if (n->type == kASTExprFuncCall) {
    struct Node *fexpr = n->func_expr;
    if (fn->func_name_token->length != fexpr->op->length) {
      return;
    }
    if (strncmp(fn->func_name_token->begin, fexpr->op->begin,
                fexpr->op->length) == 0) {
      return;
    } else {
      return;
    }
  }
  if (n->type == kASTExpr) {
    SubOptimizeRecursiveFunction(fn, &n->left);
    SubOptimizeRecursiveFunction(fn, &n->right);
  }
  if (n->type == kASTExprStmt) {
    SubOptimizeRecursiveFunction(fn, &n->left);
    SubOptimizeRecursiveFunction(fn, &n->right);
  }
  if (n->type == kASTJumpStmt) {
    if (n->op->token_type != kTokenKwReturn) {
      return;
    }

    if (n->right && n->right->op && n->right->op->token_type == kTokenIntegerConstant) {
      const int MAX_LEN = 256;
      char buf[MAX_LEN];

      assert(snprintf(buf, MAX_LEN, "{_X+=(%.*s);break;}", n->right->op->length, n->right->op->begin)>=0);
      *np = CreateStmt(buf);
      return;
    }

    if (!n->right || !n->right->op ||
        strncmp(n->right->op->begin, "+", n->right->op->length) != 0) {
      return;
    }
    // a + b case
    struct Node *result_expr = n->right;
    // Check if right operand is integer constant
    if (!result_expr->right ||
        !IsTokenWithType(result_expr->right->op, kTokenIntegerConstant)) {
      return;
    }
    // Check if left operand is func call
    if (!result_expr->left || result_expr->left->type != kASTExprFuncCall) {
      return;
    }
    // Check if calling the function itself
    struct Node *fexpr = result_expr->left->func_expr;
    if (!IsEqualToken(fn->func_name_token, fexpr->op)) {
      return;
    }

    if(GetSizeOfList(fn->func_type->right) != 1) {
      return;
    }
    
    fprintf(stderr, "Arguments of callee RecursiveFunction\n");
    assert(fn->func_type->right->nodes[0]->left != NULL);
    // PrintASTNode(fn->func_type->right->nodes[0]->left);

    struct Node* call_expr_list = result_expr->left->arg_expr_list;

    fprintf(stderr, "The arguments is {%.*s}\n", fn->func_type->right->nodes[0]->left->length, fn->func_type->right->nodes[0]->left->begin);
    fprintf(stderr, "Call arguments is {%.*s}\n", call_expr_list->nodes[0]->op->length, call_expr_list->nodes[0]->op->begin);
    
    fprintf(stderr, "r: {%.*s}\n",  result_expr->right->op->length, result_expr->right->op->begin);
    
    const int MAX_LEN = 256;
    char buf[MAX_LEN];
    
    // n=n;
    assert(snprintf(buf, MAX_LEN, "{(%.*s) = (%.*s); _X += (%.*s);}",
          fn->func_type->right->nodes[0]->left->length, fn->func_type->right->nodes[0]->left->begin,
          call_expr_list->nodes[0]->op->length, call_expr_list->nodes[0]->op->begin,
          result_expr->right->op->length, result_expr->right->op->begin
    )>=0);
    fprintf(stderr, "%s\n", buf);
    // _X += 1;
    
    *np = CreateStmt(buf);
    PrintASTNode(*np);
    return;
  }
  if (n->type == kASTList) {
    for (int l = 0; l < GetSizeOfList(n); l++) {
      struct Node **stmt = GetNodeReferenceAt(n, l);
      SubOptimizeRecursiveFunction(fn, stmt);
    }
    return;
  }
  if (n->type == kASTForStmt) {
    SubOptimizeRecursiveFunction(fn, &n->body);
  }
  return;
}

struct Node *ParseDecl();
static struct Node *CreateDecl(const char *s) {
  struct Node *tokens = Tokenize(s);
  InitParser(&tokens);
  return ParseDecl();
}

void OptimizeRecursiveFunction(struct Node **fnp) {
  assert(fnp != NULL);
  struct Node *fn = *fnp;
  assert(fn != NULL);
  assert(fn->type == kASTFuncDef);

  if (GetSizeOfList(fn->func_type->right) != 1) {
    return;
  }

  if (!IsTailRecursiveFunction(fn, fn->func_body)) {
    return;
  }
  fprintf(stderr, "OptimizeRecusiveFunction %.*s\n",
          fn->func_name_token->length, fn->func_name_token->begin);

  SubOptimizeRecursiveFunction(fn, &fn->func_body);

  struct Node *for_stmt = AllocNode(kASTForStmt);
  for_stmt->op = CreateToken("for");
  for_stmt->body = fn->func_body;

  struct Node *new_fn_body = AllocList();
  new_fn_body->op = CreateToken("{");

  PushToList(new_fn_body, CreateDecl("int _X = 0;"));
  PushToList(new_fn_body, for_stmt);
  PushToList(new_fn_body, CreateStmt("return _X;"));

  fn->func_body = new_fn_body;
}

void Optimize(struct Node **np) {
  if (!np) {
    return;
  }
  struct Node *n = *np;
  if (!n) {
    return;
  }
  fprintf(stderr, "AST before optimization:\n");
  if (n->type == kASTFuncDef) {
    //関数の再起呼び出しの検知
    OptimizeRecursiveFunction(np);
    Optimize(&n->func_body);
    return;
  }
  if (n->type == kASTExprFuncCall) {
    Optimize(&n->left);
    Optimize(&n->arg_expr_list);
    return;
  }
  if (n->type == kASTExpr) {
    fprintf(stderr, "Optimize:\n");
    // PrintASTNode(n);
    Optimize(&n->left);
    Optimize(&n->right);
    // left と right が定数なら，ここで定数の計算
    ConstantPropagation(np);
    StrengthReduction(np);
    return;
  }
  if (n->type == kASTExprStmt || n->type == kASTJumpStmt) {
    if (n->left) {
      Optimize(&n->left);
    }
    if (n->right) {
      Optimize(&n->right);
    }
    return;
  }
  if (n->type == kASTList) {
    for (int l = 0; l < GetSizeOfList(n); l++) {
      struct Node *stmt = GetNodeAt(n, l);
      Optimize(&stmt);
    }
    return;
  }
  if (n->type == kASTForStmt) {
    Optimize(&n->body);
    return;
  }
  // Show the result
  fprintf(stderr, "AST after optimization:\n");
  // PrintASTNode(ast);
  // fputs("Optimization end\n", stderr);
}
