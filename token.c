#include "compilium.h"

void InternalCopyTokenStr(Token *token, const char *s, size_t len) {
  if (len >= MAX_TOKEN_LEN) {
    Error("Too long token");
  }
  strncpy(token->str, s, len);
  token->str[len] = 0;
}

Token *AllocateToken(const char *s, TokenType type) {
  if (!s) {
    Error("Trying to allocate a token with a null string");
  }
  Token *token = malloc(sizeof(Token));
  InternalCopyTokenStr(token, s, strlen(s));
  token->type = type;
  return token;
}

Token *AllocateTokenWithSubstring(const char *begin, const char *end,
                                  TokenType type, const char *filename,
                                  int line) {
  Token *token = malloc(sizeof(Token));
  InternalCopyTokenStr(token, begin, end - begin);
  token->type = type;
  token->filename = filename;
  token->line = line;
  return token;
}

int IsEqualToken(const Token *token, const char *s) {
  if (!token) return 0;
  return strcmp(token->str, s) == 0;
}

static const char *keyword_list[] = {
    "auto",       "break",    "case",     "char",   "const",   "continue",
    "default",    "do",       "double",   "else",   "enum",    "extern",
    "float",      "for",      "goto",     "if",     "inline",  "int",
    "long",       "register", "restrict", "return", "short",   "signed",
    "sizeof",     "static",   "struct",   "switch", "typedef", "union",
    "unsigned",   "void",     "volatile", "while",  "_Bool",   "_Complex",
    "_Imaginary", NULL};

int IsKeyword(const Token *token) {
  for (int i = 0; keyword_list[i]; i++) {
    if (IsEqualToken(token, keyword_list[i])) return 1;
  }
  return 0;
}

int IsTypeToken(const Token *token) {
  return IsEqualToken(token, "int") || IsEqualToken(token, "char");
}

// TokenList

struct TOKEN_LIST {
  int capacity;
  int size;
  const Token *tokens[];
};

TokenList *AllocateTokenList(int capacity) {
  TokenList *list =
      malloc(sizeof(TokenList) + sizeof(const Token *) * capacity);
  list->capacity = capacity;
  list->size = 0;
  return list;
}

int IsTokenListFull(TokenList *list) { return (list->size >= list->capacity); }

void AppendTokenToList(TokenList *list, const Token *token) {
  if (IsTokenListFull(list)) {
    Error("No more space in TokenList");
  }
  list->tokens[list->size++] = token;
}

const Token *GetTokenAt(const TokenList *list, int index) {
  if (!list || index < 0 || list->size <= index) return NULL;
  return list->tokens[index];
}

int GetSizeOfTokenList(const TokenList *list) { return list->size; }
void SetSizeOfTokenList(TokenList *list, int size) { list->size = size; }

void DebugPrintToken(const Token *token) {
  printf("(Token: '%s' type %d at %s:%d)\n", token->str, token->type,
         token->filename, token->line);
}
void PrintToken(const Token *token) { printf("%s", token->str); }

void PrintTokenList(const TokenList *list) {
  for (int i = 0; i < list->size; i++) {
    if (i) putchar(' ');
    PrintToken(list->tokens[i]);
  }
}

// TokenStream

struct TOKEN_STREAM {
  const TokenList *list;
  int pos;
};

TokenStream *AllocAndInitTokenStream(const TokenList *list) {
  TokenStream *stream = malloc(sizeof(TokenStream));
  stream->list = list;
  stream->pos = 0;
  return stream;
}

const Token *PopToken(TokenStream *stream) {
  if (stream->pos >= stream->list->size) return NULL;
  return GetTokenAt(stream->list, stream->pos++);
}

const Token *PeekToken(TokenStream *stream) {
  return GetTokenAt(stream->list, stream->pos);
}

int IsNextToken(TokenStream *stream, const char *str) {
  return IsEqualToken(GetTokenAt(stream->list, stream->pos), str);
}

const Token *ConsumeToken(TokenStream *stream, const char *str) {
  if (!IsNextToken(stream, str)) return NULL;
  return PopToken(stream);
}
