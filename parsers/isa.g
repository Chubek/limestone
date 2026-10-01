%{
/* This file must be fed to `scripts/aurocks.pl`
 * Recognition-only result. The returned pointer must not be freed.
 */
static char document_success;
%}

%start Document

%skip /([ \t\r\n]|#[^\n]*(\n|$))+/

%%

/*
 * --------------------------------------------------------------------------
 * Document
 * --------------------------------------------------------------------------
 */

Document:
    Tops
    { $$ = &document_success; }
    ;

Tops:
      Top Tops
    | %empty
    ;

Top:
      Arch
    | Profile
    | Compiler
    | Tooling
    | Regclass
    | Alias
    | Encoding
    | Op
    | Legacy
    ;


/*
 * --------------------------------------------------------------------------
 * Architecture
 * --------------------------------------------------------------------------
 */

Arch:
    /arch([ \t\r\n]|#[^\n]*(\n|$))+/ Ident '{' Fields '}'
    ;


/*
 * --------------------------------------------------------------------------
 * Architecture profile
 * --------------------------------------------------------------------------
 */

Profile:
    'profile' '{' Fields '}'
    ;


/*
 * --------------------------------------------------------------------------
 * Compiler description
 *
 * Example:
 *
 * compiler {
 *     instruction_selection {
 *         strategy = "target_pattern_matching";
 *         ...
 *     }
 *
 *     instruction_scheduling {
 *         ...
 *     }
 *
 *     register_allocation {
 *         ...
 *     }
 * }
 * --------------------------------------------------------------------------
 */

Compiler:
    'compiler' '{' CompilerSections '}'
    ;

CompilerSections:
      CompilerSection CompilerSections
    | %empty
    ;

CompilerSection:
    Ident '{' Fields '}'
    ;


/*
 * --------------------------------------------------------------------------
 * Global tooling description
 *
 * Example:
 *
 * tooling {
 *     register_classes = [...];
 *     ...
 * }
 * --------------------------------------------------------------------------
 */

Tooling:
    'tooling' '{' Fields '}'
    ;


/*
 * --------------------------------------------------------------------------
 * Legacy compatibility section
 * --------------------------------------------------------------------------
 */

Legacy:
    'legacy' '{' Fields '}'
    ;


/*
 * --------------------------------------------------------------------------
 * Register classes
 *
 * Example:
 *
 * regclass GPR {
 *     x0(64)=0,
 *     x1(64)=1,
 *     ...
 * }
 * --------------------------------------------------------------------------
 */

Regclass:
    /regclass([ \t\r\n]|#[^\n]*(\n|$))+/ Ident '{' RegisterEntries '}'
    ;

RegisterEntries:
      RegisterEntry RegisterEntries
    | %empty
    ;

RegisterEntry:
    Ident '(' Number ')' '=' Number ','
    ;


/*
 * --------------------------------------------------------------------------
 * Register aliases
 *
 * Example:
 *
 * alias fp = x29;
 * alias lr = x30;
 * --------------------------------------------------------------------------
 */

Alias:
    'alias' Ident '=' AliasTarget ';'
    ;

AliasTarget:
      Ident
    | String
    | Number
    ;


/*
 * --------------------------------------------------------------------------
 * Instruction encodings
 * --------------------------------------------------------------------------
 */

Encoding:
    /encoding([ \t\r\n]|#[^\n]*(\n|$))+/ Ident '{' Fields '}'
    ;


/*
 * --------------------------------------------------------------------------
 * Instructions
 * --------------------------------------------------------------------------
 */

Op:
    /op([ \t\r\n]|#[^\n]*(\n|$))+/ Ident '{' OpFields '}'
    ;

OpFields:
      OpField OpFields
    | %empty
    ;

OpField:
      SemanticsField
    | Field
    ;

SemanticsField:
    'semantics' '=' Semantic ';'
    ;


/*
 * --------------------------------------------------------------------------
 * Generic fields
 * --------------------------------------------------------------------------
 */

Fields:
      Field Fields
    | %empty
    ;

Field:
    Ident '=' Value ';'
    ;


/*
 * --------------------------------------------------------------------------
 * Values
 *
 * The new tooling model deliberately uses the existing generic Value
 * mechanism. Therefore constructs such as:
 *
 *     scheduling = {
 *         latency = "unknown";
 *         resources = ["ALU0", "ALU1"];
 *         ...
 *     };
 *
 * and nested structures such as:
 *
 *     scheduling = {
 *         resource_model = {
 *             ports = [0, 1];
 *         };
 *     };
 *
 * are accepted without special-purpose grammar productions.
 * --------------------------------------------------------------------------
 */

Value:
      String
    | Number
    | Boolean
    | Array
    | Object
    | CallList
    | BareList
    ;


/*
 * --------------------------------------------------------------------------
 * Arrays
 *
 * Examples:
 *
 *     ["GPR", "FPR"]
 *     [0, 1, 2]
 *     []
 * --------------------------------------------------------------------------
 */

Array:
    '[' OptionalValues ']'
    ;

OptionalValues:
      Value MoreValues
    | %empty
    ;

MoreValues:
      ',' Value MoreValues
    | %empty
    ;


/*
 * --------------------------------------------------------------------------
 * Objects
 *
 * Objects are recursive so the tooling schema can grow without requiring
 * a grammar production for every individual subsystem.
 * --------------------------------------------------------------------------
 */

Object:
    '{' Fields '}'
    ;


/*
 * --------------------------------------------------------------------------
 * Call lists
 *
 * Example:
 *
 *     foo(a, b), bar(c)
 * --------------------------------------------------------------------------
 */

CallList:
    Call MoreCalls
    ;

MoreCalls:
      ',' Call MoreCalls
    | %empty
    ;

Call:
    Ident '(' OptionalBareArgs ')'
    ;

OptionalBareArgs:
      BareArgs
    | %empty
    ;

BareArgs:
    Value MoreValues
    ;


/*
 * --------------------------------------------------------------------------
 * Semantic expressions
 * --------------------------------------------------------------------------
 */

Semantic:
      String
    | SExpr
    ;

SExpr:
    '(' SExprItems ')'
    ;

SExprItems:
      SExprItem SExprItems
    | %empty
    ;

SExprItem:
      String
    | Number
    | Ident
    | SExpr
    ;


/*
 * --------------------------------------------------------------------------
 * Primitive values
 * --------------------------------------------------------------------------
 */

Boolean:
      'true'
    | 'false'
    ;

Number:
      Hex
    | Decimal
    ;

Hex:
    /0x[0-9A-Fa-f]+/
    ;

Decimal:
    /-?[0-9]+/
    ;


/*
 * --------------------------------------------------------------------------
 * Bare lists
 * --------------------------------------------------------------------------
 */

BareList:
      BareAtom MoreBareAtoms
    | %empty
    ;

MoreBareAtoms:
      ',' BareAtom MoreBareAtoms
    | %empty
    ;


/*
 * First byte: neither a delimiter nor whitespace.
 * Remaining bytes: anything except a delimiter.
 *
 * Whitespace remains part of this terminal once matching starts.
 */
BareAtom:
    /[^;}\]), \t\r\n][^;}\]),]*/
    ;


/*
 * --------------------------------------------------------------------------
 * Strings
 *
 * Backslash escapes any byte, including newline.
 * --------------------------------------------------------------------------
 */

String:
    /"([^"\\]|\\(.|\n))*"/
    ;


/*
 * --------------------------------------------------------------------------
 * Identifiers
 * --------------------------------------------------------------------------
 */

Ident:
    /[A-Za-z_][A-Za-z0-9_.-]*/
    ;

%%

/* Application code may follow here. */
