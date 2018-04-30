#include "compilium.h"

char *ReadFile(const char *file_name) {
  // file_buf is allocated here.
  FILE *fp = fopen(file_name, "rb");
  if(!fp){
    Error("Failed to open: %s", file_name);
  }
  char *file_buf = malloc(MAX_INPUT_SIZE);
  int file_buf_size = fread(file_buf, 1, MAX_INPUT_SIZE, fp);
  if (file_buf_size >= MAX_INPUT_SIZE) {
    Error("Too large input");
  }
  file_buf[file_buf_size] = 0;
  printf("Input(path: %s, size: %d)\n", file_name, file_buf_size);
  fclose(fp);
  return file_buf;
}

const char *Preprocess(const char *p);
#define IS_IDENT_NODIGIT(c) \
  ((c) == '_' || ('a' <= (c) && (c) <= 'z') || ('A' <= (c) && (c) <= 'Z'))
#define IS_IDENT_DIGIT(c) (('0' <= (c) && (c) <= '9'))
const char *CommonTokenizer(const char *p)
{
  static const char *single_char_punctuators = "[](){}~?:;,%\\";
  const char *begin = NULL;
  if (IS_IDENT_NODIGIT(*p)) {
    begin = p++;
    while (IS_IDENT_NODIGIT(*p) || IS_IDENT_DIGIT(*p)) {
      p++;
    }
    AddToken(begin, p, kIdentifier);
  } else if (IS_IDENT_DIGIT(*p)) {
    begin = p++;
    while (IS_IDENT_DIGIT(*p)) {
      p++;
    }
    AddToken(begin, p, kInteger);
  } else if (*p == '"' || *p == '\'') {
    begin = p++;
    while (*p && *p != *begin) {
      if (*p == '\\') p++;
      p++;
    }
    if (*(p++) != *begin) {
      Error("Expected %c but got char 0x%02X", *begin, *p);
    }
    TokenType type = (*begin == '"' ? kStringLiteral : kCharacterLiteral);
    AddToken(begin + 1, p - 1, type);
  } else if (strchr(single_char_punctuators, *p)) {
    // single character punctuator
    begin = p++;
    AddToken(begin, p, kPunctuator);
  } else if (*p == '#') {
    p++;
    p = Preprocess(p);
  } else if (*p == '|' || *p == '&' || *p == '+' || *p == '/') {
    // | || |=
    // & && &=
    // + ++ +=
    // / // /=
    begin = p++;
    if (*p == *begin || *p == '=') {
      p++;
    }
    AddToken(begin, p, kPunctuator);
  } else if (*p == '-') {
    // - -- -= ->
    begin = p++;
    if (*p == *begin || *p == '-' || *p == '>') {
      p++;
    }
    AddToken(begin, p, kPunctuator);
  } else if (*p == '=' || *p == '!' || *p == '*') {
    // = ==
    // ! !=
    // * *=
    begin = p++;
    if (*p == '=') {
      p++;
    }
    AddToken(begin, p, kPunctuator);
  } else if (*p == '<' || *p == '>') {
    // < << <= <<=
    // > >> >= >>=
    begin = p++;
    if (*p == *begin) {
      p++;
      if (*p == '=') {
        p++;
      }
    } else if (*p == '=') {
      p++;
    }
    AddToken(begin, p, kPunctuator);
  } else if (*p == '.') {
    // .
    // ...
    begin = p++;
    if (p[0] == '.' && p[1] == '.') {
      p += 2;
    }
    AddToken(begin, p, kPunctuator);
  } else {
    Error("Unexpected char '%c'\n", *p);
  }
  return p;
}

void Tokenize(const char *p);
const char *Preprocess(const char *p)
{
  int org_num_of_token = GetNumOfTokens();
  do {
    if (*p == ' ') {
      p++;
    } else if (*p == '\n') {
      p++;
      break;
    } else if(*p == '\\') {
      // if "\\\n", continue parsing beyond the lines.
      // otherwise, raise Error.
      p++;
      if(*p != '\n'){
        Error("Unexpected char '%c'\n", *p);
      }
      p++;
    } else {
      p = CommonTokenizer(p);
    }
  } while(*p);
  const Token *directive = GetTokenAt(org_num_of_token);
  if(IsEqualToken(directive, "include")){
    const Token *file_name = GetTokenAt(org_num_of_token + 1);
    if(!file_name || file_name->type != kStringLiteral){
      Error("Expected string literal but got %s", file_name ? file_name->str : "(null)");
    }
    SetNumOfTokens(org_num_of_token);
    Tokenize(file_name->str);
  } else{
    Error("Unknown preprocessor directive '%s'", directive ? directive->str : "(null)");
  }
  return p;
}

void Tokenize(const char *p) {
  do {
    if (*p == ' ') {
      p++;
    } else if (*p == '\n') {
      p++;
      putchar('\n');
    } else {
      p = CommonTokenizer(p);
    }
  } while (*p);
}

