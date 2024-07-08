#ifndef INFIXEXP_H
#define INFIXEXP_H

#include "scanner.h"
#include "prefixExp.h"

ExpTree newExpTreeNode(TokenType tt, Token t, ExpTree tL, ExpTree tR);
int valueIdentifier(List *lp, char **sp);
int isNumerical(ExpTree tr);
double valueExpTree(ExpTree tr);
int treeInfixExpr(List *lp, ExpTree *tp, int *parenthesis);
void printExpTreeInfix(ExpTree tr);
void infixExpTrees();
ExpTree duplicate(ExpTree source);
void differentiate(ExpTree *root, int* differingVar);

typedef struct Stack {
  ExpTree *array;
  int top;
  int size;
} Stack;

#endif
