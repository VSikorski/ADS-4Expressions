/* prefixExp.c, Gerard Renardel, 29 January 2014
 *
 * In this file functions are defined for the construction of expression trees
 * from infix expressions generated by the following BNF grammar:
 *
 * <infexp>   ::= <number> | <identifier> | <infexp> '+' <infexp>
 *             | <infexp> '-' <infexp> | <infexp> '*' <infexp> | <infexp> '/' <infexp>
 *
 * <number>      ::= <digit> { <digit> }
 *
 * <identifier> ::= <letter> { <letter> | <digit> }
 *
 * Starting point is the token list obtained from the scanner (in scanner.c).
*/

/* file : infixExp.c */
/* authors : Vrincianu Andrei - Darius (a.vrincianu@student.rug.nl) and Vitalii Sikorski (v.sikorski@student.rug.nl) */
/* date : March 23 2024 */
/* version: 1.0 */

/* Description:
  This program accepts as input an expression, converts it to an expression tree and outputs its 
  infix form, value (if applicable), simplified form and the derivative to x
*/

#include <stdio.h>  /* printf */
#include <stdlib.h> /* malloc, free */
#include <assert.h> /* assert */
#include <string.h>
#include "scanner.h"
#include "recognizeExp.h"
#include "evalExp.h"
#include "prefixExp.h"
#include "infixExp.h"

// Function declaration
Stack newStack(int s);
void doubleStackSize(Stack *stp);
void push(Stack *st, ExpTree x);
int isEmptyStack(Stack st);
void stackEmptyError();
ExpTree pop(Stack *st);
void freeStack(Stack st);
int getPrecedence(char c);
int checkInvalid(char c);
ExpTree simplify(ExpTree t);
void simplifyRec(ExpTree t);

// Transforms the token list in an expression tree
int treeInfixExpr(List *lp, ExpTree *tp, int *paranthesis) {
  int checker = 1;
  double w;
  char *s;
  char c;
  Token t;
  // We use a stack of nodes to handle the tree construction process
  Stack stackNodes = newStack(20);
  ExpTree tempoTree;
  int prio = -1, currentPrio = -1;
  while (*lp != NULL) {
    if ((*lp)->t.symbol == '(') {
      //This keeps count of the number of paranthesis found, finding a right one icreases the counter while a left one decreases
      (*lp) = (*lp)->next;
      *paranthesis = *paranthesis + 1;
      if (treeInfixExpr(lp, &tempoTree, paranthesis)) {
        push(&stackNodes, tempoTree);
        currentPrio = prio + 1;
        if (*lp == NULL) {
          break;
        } 
      } else {
        freeStack(stackNodes);
        return 0;
      }
    }
    if ((*lp)->t.symbol == ')') {
      (*lp) = (*lp)->next;
      *paranthesis = *paranthesis - 1;
      break;
    }

    if (valueOperator(lp, &c)) {
      //Finding an operator immediately after another is considered bad input, thus incorrect input
      if (valueOperator(lp, &c) || stackNodes.top == 0) {
        checker = 0;
        break;
      }
      t.symbol = c;
      // Current priority is based in the precedence of the current character/operand
      currentPrio = getPrecedence(c);
      if (currentPrio > prio) {
        //Current priority being higher pushes the symbol into the stack with the symbol at the top as its child
        ExpTree newChild = newExpTreeNode(Symbol, t, pop(&stackNodes), NULL);
        push(&stackNodes, newChild);
      } else {
        if (currentPrio == prio) {
          //Equal priority case
          tempoTree = pop(&stackNodes);
          ExpTree fullTree = pop(&stackNodes);
          fullTree->right = tempoTree;
          ExpTree newChild = newExpTreeNode(Symbol, t, fullTree, NULL);
          push(&stackNodes, newChild);
        } else {
          //Current priority is lower case
          ExpTree fullTree = pop(&stackNodes);
          while(stackNodes.top!=0) {
            tempoTree = pop(&stackNodes);
            tempoTree->right = fullTree;
            fullTree = tempoTree;
          }
          ExpTree newChild = newExpTreeNode(Symbol, t, fullTree, NULL);
          push(&stackNodes, newChild);
        }
        if (*lp == NULL) {
          //If the list empty after finding an operator, th input is wrong
          ExpTree toRemove;
          while (stackNodes.top > 0) {
            toRemove = pop(&stackNodes);
            freeExpTree(toRemove);
          }
          free(stackNodes.array);
          return 0;
        }
      }

    }

    prio = currentPrio;

    //Finding a number pushes it into the stack
    if (valueNumber(lp, &w)) {
      t.number = w;
      ExpTree newChild = newExpTreeNode(Number, t, NULL, NULL);
      push(&stackNodes, newChild);
    } else {
      //Finding an identifier also pushes it into a stack
      if (valueIdentifier(lp, &s)) {
        t.identifier = s;
        ExpTree newChild = newExpTreeNode(Identifier, t, NULL, NULL);
        push(&stackNodes, newChild);
      }
    }
    
    //Number followed by an identifer, without an operator in-between, is considered bad input 
    if (valueNumber(lp, &w) || valueIdentifier(lp, &s)) {
      checker = 0;
      break;
    }
  }

  //This uses the stack to build the tree
  //by poping the stack's store elements
  if (stackNodes.top != 0) {
    tempoTree = pop(&stackNodes);
    while (stackNodes.top > 0) {
      *tp = pop(&stackNodes);
      (*tp)->right = tempoTree;
      tempoTree = *tp;
    }

    *tp = tempoTree;
  }

  //if the list is NULL and there are still paranthesis left, the input is considered wrong
  if (*lp == NULL && *paranthesis != 0) {
    freeStack(stackNodes);
    return 0;
  }

  freeStack(stackNodes);
  return checker;
}

// Duplicates a given expression tree
ExpTree duplicate(ExpTree source) {
  // Initial check and base case
  if (source == NULL) {
    return source;
  }
  // Recursive block
  ExpTree newNode = newExpTreeNode((source)->tt, (source)->t, NULL, NULL);
  //duplicates the left child
  if ((source)->left != NULL) {
    (newNode)->left = duplicate((source)->left);
  }
  //duplicates the right child
  if ((source)->right != NULL) {
    (newNode)->right = duplicate((source)->right);
  }
  return newNode;
}

// Differentiates the expression tree usign the rules provided in the assignment
void differentiate(ExpTree *root, int* differentVar) {
  // Base case block
  //This would mostly be irrelevant, but helps with incorrect input
  if ((*root) == NULL) {
    return;
  }

  //This derivates a constant/non-identifier to x
  if ((*root)->tt == Number) {
    (*root)->t.number = 0;
    return;
  }

  //This handles the differentiation of an identifier
  if ((*root)->tt == Identifier) {
    //If the identifier is different from x, it is differentiated to 0
    if (strcmp((*root)->t.identifier, "x") != 0) {
      (*root)->tt = Number;
      (*root)->t.number = 0;
    } else {
      //If the identifier is x, it is differentiated to 1
      (*root)->tt = Number;
      (*root)->t.number = 1;
      *differentVar = 0;
    }
    return;
  }

  // Recursive block
  if ((*root)->tt == Symbol) {
    //This derivates using the addition rule
    if ((*root)->t.symbol == '+') {
      differentiate(&(*root)->left, differentVar);
      differentiate(&(*root)->right, differentVar);
      return;
    } else {
      //This derivates using the subtraction rule
      if ((*root)->t.symbol == '-') {
        differentiate(&(*root)->left, differentVar);
        differentiate(&(*root)->right,differentVar);
      } else {
        //This derivates using the multiplication rule
        if ((*root)->t.symbol == '*') {
          (*root)->t.symbol = '+';
          ExpTree ch1 = (*root)->left;
          ExpTree ch2 = (*root)->right;


          Token tok;
          tok.symbol = '*';
          //These are the two multiplications that result from the derivation
          ExpTree multiplication1 = newExpTreeNode(Symbol, tok, NULL, NULL);
          ExpTree multiplication2 = newExpTreeNode(Symbol, tok, NULL, NULL);
          (*root)->left = multiplication1;
          (*root)->right = multiplication2;
          //In order to have both the differentiated version and the original, we must duplicate the elements 
          //of the initial multiplication and differentiate the copies
          ExpTree p1 = duplicate(ch1);
          differentiate(&p1, differentVar);
          ExpTree p2 = duplicate(ch2);
          differentiate(&p2, differentVar);
          //Having the expression (a*b)' the formula used is
          //(a')*b + a*(b')
          (multiplication1)->left = p1;
          (multiplication1)->right = ch2;
          (multiplication2)->left = ch1;
          (multiplication2)->right = p2;
          return;
        } else {
          if ((*root)->t.symbol == '/') {
          ExpTree ch1 = (*root)->left;
          ExpTree ch2 = (*root)->right;

          //Will be used for the subtraction of the quotient
          Token tok;
          tok.symbol = '-';
          
          //Will be used for the multiplications
          Token multSym;
          multSym.symbol = '*';
          //These are all the relevant nodes, the quotient and its multiplications and the denominator
          ExpTree quotient = newExpTreeNode(Symbol, tok, NULL, NULL);
          ExpTree multiplication1 = newExpTreeNode(Symbol, multSym, NULL, NULL);
          ExpTree multiplication2 = newExpTreeNode(Symbol, multSym, NULL, NULL);
          ExpTree denominator = newExpTreeNode(Symbol, multSym, NULL, NULL);

          //Similar process to the multiplication, just with the added bonus of having the quotient keep the multiplications
          ExpTree p1 = duplicate(ch1);
          differentiate(&p1, differentVar);
          ExpTree p2 = duplicate(ch2);
          differentiate(&p2, differentVar);
          //Having the expression (a*b)' the formula used is
          //(a')*b + a*(b')
          (multiplication1)->left = p1;
          (multiplication1)->right = ch2;
          (multiplication2)->left = ch1;
          (multiplication2)->right = p2;

          (quotient)->left = multiplication1;
          (quotient)->right = multiplication2;

          //Since we need the original right element for the denominator, we duplicate it
          ExpTree p3 = duplicate(ch2);
          ExpTree p4 = duplicate(ch2);
          (denominator)->left = p3;
          (denominator)->right = p4;
          
          //This completes the differentiation
          (*root)->left = quotient;
          (*root)->right = denominator;
         
          return;
          }
        }
      }
      
    }
  }
  return;
}

// Gets the user input and calls the corresponding functions
void infixExpTrees() {
  char *ar;
  int differingVariable = 1;
  List tl, tl1;
  ExpTree t = NULL;
  printf("give an expression: ");
  ar = readInput();
  while (ar[0] != '!') {
    differingVariable = 1;
    // Transforms the user input into a token list
    tl = tokenList(ar);
    // Prints out the token list (initial user input)
    printList(tl);
    tl1 = tl;
    List tl2 = tl;
    int paranthesis = 0;
    if ((acceptExpression(&tl2) && tl2 == NULL) && tl != NULL && treeInfixExpr(&tl1, &t, &paranthesis) && tl1 == NULL) {
      printf("in infix notation: ");
      // Prints out the infix form of the expresion
      printExpTreeInfix(t);
      printf("\n");
      if (isNumerical(t)) {
        printf("the value is %g\n",valueExpTree(t));
      } else {
        printf("this is not a numerical expression\n");
        // 't' holds the simplified expression tree
        t = simplify(t);
        printf("simplified: ");
        printExpTreeInfix(t);
        printf("\n");
        // The expression tree 't' gets differentiated
        differentiate(&t, &differingVariable);
        printf("derivative to x: ");
        // The expression tree gets simplified once more and is printed out
        t = simplify(t);
        printExpTreeInfix(t);
      }
    } else {
      printf("this is not an expression\n");
    }
    // Freeing up the memory
    freeExpTree(t);
    t = NULL;
    freeTokenList(tl);
    free(ar);
    printf("\ngive an expression: ");
    ar = readInput();
  }
  free(ar);
  printf("good bye\n");
}

// Creates an empty stack of size s
Stack newStack(int s) {
  Stack st;
  st.array = malloc(s*sizeof(ExpTree));
  assert(st.array != NULL);
  st.top = 0;
  st.size = s;
  return st;
}

// Doubles the stack size
void doubleStackSize(Stack *stp) {
  int newSize = 2 * stp->size;
  stp->array = realloc(stp->array, newSize * sizeof(*stp->array));
  assert(stp->array != NULL);
  stp->size = newSize;
  return;
}

// Pushes a new value to the stack
void push(Stack *st, ExpTree x) {
  if (st->top == st->size) {
    doubleStackSize(st);
  }
  st->array[st->top] = x;
  st->top++;
  return;
}

// Checks if the stack is empty
int isEmptyStack(Stack st) {
  return (st.top == 0);
}

// Quits the program if an error has occured
void stackEmptyError() {
  printf("stack empty\n");
  abort();
  return;
}

// Pops the stack
ExpTree pop(Stack *st) {
  if (isEmptyStack(*st)) {
    stackEmptyError();
  }
  st->top--;
  return (st->array)[st->top];
}

// Frees up the allocated space
void freeStack(Stack st) {
  while (st.top > 0) {
    ExpTree toFree = pop(&st);
    freeExpTree(toFree);
  }
  free(st.array);
}

// Returns the precedence of the operator
int getPrecedence(char c) {
  switch (c) {
    case '+':
      return 1;
    case '-':
      return 1;
    case '*':
      return 2;
    case '/':
      return 2;
    default: return 0;
  }
}

// Checks for invalid input
int checkInvalid(char c) {
  if ((c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || isOperator(c) || c == ' ') {
    return 1;
  }
  return 0;
}

// Returns the simplified tree
ExpTree simplify(ExpTree t) {
  simplifyRec(t);
  return t;
}

// Recursively simplifies an expression of the expression tree t
void simplifyRec(ExpTree t) {
  // Base case
  if (t == NULL) {
    return;
  }
  // Recursive block
  simplifyRec(t->left);
  simplifyRec(t->right);
  // Simplifying the expression
  if (t->tt == Symbol && (t->t.symbol == '*' || t->t.symbol == '/' || t->t.symbol == '+' || t->t.symbol == '-')) {
    if (t->t.symbol == '*') {
      if (t->left->tt == Number && t->right->tt == Number) {
        // Recognizes and simplifies n * 1
        if (t->right->t.number == 1) {
          t->tt = Number;
          t->t.number = t->left->t.number;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
        // Recognizes and simplifies n * 0
        else if (t->right->t.number == 0) {
          t->tt = Number;
          t->t.number = 0;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
        // Recognizes and simplifies 1 * n
        else if (t->left->t.number == 1) {
          t->tt = Number;
          t->t.number = t->right->t.number;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
        // Recognizes and simplifies 0 * n
        else if (t->left->t.number == 0) {
          t->tt = Number;
          t->t.number = 0;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Number && t->right->tt == Identifier) {
        // Recognizes and simplifies 1 * x
        if (t->left->t.number == 1) {
          t->tt = Identifier;
          t->t.identifier = t->right->t.identifier;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
        // Recognizes and simplifies 0 * x
        else if (t->left->t.number == 0) {
          t->tt = Number;
          t->t.number = 0;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Number && t->right->tt == Symbol) {
        // Recognizes and simplifies 1 * exp
        if (t->left->t.number == 1) {
          t->tt = Symbol;
          t->t.symbol = t->right->t.symbol;
          free(t->left);
          t->left = NULL;
          ExpTree newNode = t->right;
          t->left = newNode->left;
          t->right = newNode->right;
          free(newNode); 
        }
        // Recognizes and simplifies 0 * exp
        else if (t->left->t.number == 0) {
          t->tt = Number;
          t->t.number = 0;
          free(t->left);
          freeExpTree(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Identifier && t->right->tt == Number) {
        // Recognizes and simplifies x * 1
        if (t->right->t.number == 1) {
          t->tt = Identifier;
          t->t.identifier = t->left->t.identifier;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
        // Recognizes and simplifies 0 * x
        else if (t->right->t.number == 0) {
          t->tt = Number;
          t->t.number = 0;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Symbol && t->right->tt == Number) {
        // Recognizes and simplifies exp * 1
        if (t->right->t.number == 1) {
          t->tt = Symbol;
          t->t.symbol = t->left->t.symbol;
          free(t->right);
          t->right = NULL;
          ExpTree newNode = t->left;
          t->left = newNode->left;
          t->right = newNode->right;
          free(newNode);      
        }
        // Recognizes and simplifies exp * 0
        else if (t->right->t.number == 0) {
          t->tt = Number;
          t->t.number = 0;
          freeExpTree(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
    }
    else if (t->t.symbol == '/') {
      if (t->left->tt == Number && t->right->tt == Number) {
        // Recognizes and simplifies n / 1
        if (t->right->t.number == 1) {
          t->tt = Number;
          t->t.number = t->left->t.number;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Identifier && t->right->tt == Number) {
        // Recognizes and simplifies x / 1
        if (t->right->t.number == 1) {
          t->tt = Identifier;
          t->t.identifier = t->left->t.identifier;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Symbol && t->right->tt == Number) {
        // Recognizes and simplifies exp / 1
        if (t->right->t.number == 1) {
          t->tt = Symbol;
          t->t.symbol = t->left->t.symbol;
          free(t->right);
          t->right = NULL;
          ExpTree newNode = t->left;
          t->left = newNode->left;
          t->right = newNode->right;
          free(newNode);
        }
      }
    }
    else if (t->t.symbol == '+') {
      if (t->left->tt == Number && t->right->tt == Number) {
        if (t->left->t.number == 0) {
          // Recognizes and simplifies 0 + n
          t->tt = Number;
          t->t.number = t->right->t.number;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
        else if (t->right->t.number == 0) {
          // Recognizes and simplifies n + 0
          t->tt = Number;
          t->t.number = t->left->t.number;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Number && t->right->tt == Identifier) {
        // Recognizes and simplifies 0 + x
        if (t->left->t.number == 0) {
          t->tt = Identifier;
          t->t.identifier = t->right->t.identifier;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Identifier && t->right->tt == Number) {
        // Recognizes and simplifies x + 0
        if (t->right->t.number == 0) {
          t->tt = Identifier;
          t->t.identifier = t->left->t.identifier;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Number && t->right->tt == Symbol) {
        // Recognizes and simplifies 0 + exp
        if (t->left->t.number == 0) {
          t->tt = Symbol;
          t->t.symbol = t->right->t.symbol;
          free(t->left);
          t->left = NULL;
          ExpTree newNode = t->right;
          t->left = newNode->left;
          t->right = newNode->right;
          free(newNode);
        }
      }
      else if (t->left->tt == Symbol && t->right->tt == Number) {
        // Recognizes and simplifies exp + 0
        if (t->right->t.number == 0) {
          t->tt = Symbol;
          t->t.symbol = t->left->t.symbol;
          free(t->right);
          t->right = NULL;
          ExpTree newNode = t->left;
          t->left = newNode->left;
          t->right = newNode->right;
          free(newNode);
        }
      }
    }
    else if (t->t.symbol == '-') {
      if (t->left->tt == Number && t->right->tt == Number) {
        // Recognizes and simplifies n - 0
        if (t->right->t.number == 0) {
          t->tt = Number;
          t->t.number = t->left->t.number;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Identifier && t->right->tt == Number) {
        // Recognizes and simplifies x - 0
        if (t->right->t.number == 0) {
          t->tt = Identifier;
          t->t.identifier = t->left->t.identifier;
          free(t->left);
          free(t->right);
          t->left = NULL;
          t->right = NULL;
        }
      }
      else if (t->left->tt == Symbol && t->right->tt == Number) {
        // Recognizes and simplifies exp - 0
        if (t->right->t.number == 0) {
          t->tt = Symbol;
          t->t.symbol = t->left->t.symbol;
          free(t->right);
          t->right = NULL;
          ExpTree newNode = t->left;
          t->left = newNode->left;
          t->right = newNode->right;
          free(newNode);
        }
      }
    }
  } 
}
