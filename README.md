# Kaleidoscope Language Specification

## 1. Introduction

This document describes the syntax and semantics of Kaleidoscope, a simple programming language.

## 2. Syntax

### 2.1 Lexical Elements

- **Keywords**: `def`, `if`, `then`, `else`
- **Identifiers**: Begin with a letter, followed by any number of letters, digits, or underscores
- **Numbers**: Floating-point numbers (doubles)
- **Operators**: `+`, `-`, `*`, `/`, `<`
- **Delimiters**: `(`, `)`, newline

### 2.2 Grammar

The language grammar in EBNF notation:

```ebnf
program     ::= function_def* expression
function_def::= "def" identifier "(" parameter ")" newline body
parameter   ::= identifier
body        ::= expression | if_statement
if_statement::= "if" condition "then" newline expression newline "else" newline expression
condition   ::= expression "<" expression
expression  ::= term | expression operator term
term        ::= identifier | number | function_call
function_call::= identifier "(" expression ")"
operator    ::= "+" | "-" | "*" | "/"
```

## 3. Semantics

### 3.1 Program Structure

A program consists of zero or more function definitions followed by an expression that serves as the entry point.

### 3.2 Function Definition

- Functions are defined using the `def` keyword, followed by the function name and a single parameter in parentheses.
- The function body is an expression or an if-statement.
- Functions are recursive and can call themselves or other defined functions.

### 3.3 If Statement

- The if statement uses the keywords `if`, `then`, and `else`.
- The condition must be a comparison using the `<` operator.
- Both the `then` and `else` clauses are required.

### 3.4 Expressions

- Expressions can be simple terms (identifiers, numbers, or function calls) or arithmetic operations (`+`, `-`, `*`, and `/`).
- Function calls are evaluated by replacing the call with the body of the function, substituting the argument for the parameter.

### 3.5 Types

- The language uses double-precision floating-point numbers (doubles) for all values.
- Integers are supported as a subset of doubles.
- There are no explicit type declarations or type checking.

### 3.6 Scope

- The language has a global scope for function definitions.
- Function parameters are local to the function body.

### 3.7 Evaluation

- The program is evaluated by first processing all function definitions, then evaluating the final expression.
- Arithmetic is performed using floating-point mathematics.
- The language follows standard operator precedence: multiplication and division have higher precedence than addition and subtraction.

## 4. Example

```
# Compute the x'th fibonacci number.
def fib(x)
  if x < 3 then
    1
  else
    fib(x-1) + fib(x-2)

# This expression will compute the 40th number.
fib(40)
```

This program defines a function `fib` that computes the x'th Fibonacci number recursively. The program then calls this function with the argument 40.
