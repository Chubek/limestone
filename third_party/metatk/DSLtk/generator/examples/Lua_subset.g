# lua_subset.g — Lua 5.x subset
# dsltk-pgen lua_subset.g -o lua_parser.hpp

%namespace lua
%start    Chunk
%skip     /[ \t\r\n]+|--[^\n]*/

# ── Top level ──────────────────────────────────────────────────────────────

Chunk   : Block EOF                { $$ = $1; }
        ;

Block   : StmtList                 { $$ = $1; }
        ;

StmtList : Stmt StmtList           {
                                     auto rest = std::any_cast<std::vector<std::any>>($2);
                                     rest.insert(rest.begin(), $1);
                                     $$ = rest;
                                   }
         | RetStat                 { $$ = std::vector<std::any>(1, $1); }
         |                         { $$ = std::vector<std::any>(); }  # EPSILON
         ;

# ── Statements ─────────────────────────────────────────────────────────────

Stmt    : DoStat                   { $$ = $1; }
        | WhileStat                { $$ = $1; }
        | RepeatStat               { $$ = $1; }
        | IfStat                   { $$ = $1; }
        | NumericFor               { $$ = $1; }
        | FuncDecl                 { $$ = $1; }
        | LocalDecl                { $$ = $1; }
        | AssignOrCall             { $$ = $1; }
        | ";"                      { $$ = std::string("empty"); }
        ;

DoStat  : "do" Block "end"         { $$ = $2; }
        ;

WhileStat : "while" Expr "do" Block "end"
                                   {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("while");
                                     node["cond"] = $2; node["body"] = $4;
                                     $$ = node;
                                   }
          ;

RepeatStat : "repeat" Block "until" Expr
                                   {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("repeat");
                                     node["body"] = $2; node["cond"] = $4;
                                     $$ = node;
                                   }
           ;

IfStat  : "if" Expr "then" Block ElseChain "end"
                                   {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("if");
                                     node["cond"] = $2; node["then"] = $4;
                                     node["else"] = $5;
                                     $$ = node;
                                   }
        ;

ElseChain : "elseif" Expr "then" Block ElseChain
                                   {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("elseif");
                                     node["cond"] = $2; node["then"] = $4;
                                     node["else"] = $5;
                                     $$ = node;
                                   }
          | "else" Block           { $$ = $2; }
          |                        { $$ = std::any{}; }
          ;

NumericFor : "for" Name "=" Expr "," Expr ForStep "do" Block "end"
                                   {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("for");
                                     node["var"]  = $2; node["from"] = $4;
                                     node["to"]   = $6; node["step"] = $7;
                                     node["body"] = $9;
                                     $$ = node;
                                   }
           ;

ForStep : "," Expr                 { $$ = $2; }
        |                          { $$ = std::any(1.0); }
        ;

RetStat : "return" ExprList        {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("return");
                                     node["values"] = $2;
                                     $$ = node;
                                   }
        | "return"                 {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("return");
                                     node["values"] = std::vector<std::any>();
                                     $$ = node;
                                   }
        ;


FuncDecl : "function" Name "(" ParamList ")" Block "end"
                                   {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"]   = std::string("function");
                                     node["name"]   = $2; node["params"] = $4;
                                     node["body"]   = $6;
                                     $$ = node;
                                   }
         ;

LocalDecl : "local" Name "=" Expr  {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("local");
                                     node["name"] = $2; node["init"] = $4;
                                     $$ = node;
                                   }
          | "local" Name           {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("local");
                                     node["name"] = $2;
                                     node["init"] = std::any{};
                                     $$ = node;
                                   }
          ;

# Covers both  x = expr  and  f(args)  — ambiguity resolved by trying assign first
AssignOrCall : Name "=" Expr       {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("assign");
                                     node["name"] = $1; node["value"] = $3;
                                     $$ = node;
                                   }
             | Name CallArgs       {
                                     using M = std::map<std::string, std::any>;
                                     M node; node["kind"] = std::string("call");
                                     node["func"] = $1; node["args"] = $2;
                                     $$ = node;
                                   }
             ;

# ── Expressions (precedence via layering) ─────────────────────────────────

ExprList : Expr "," ExprList       {
                                     auto rest = std::any_cast<std::vector<std::any>>($3);
                                     rest.insert(rest.begin(), $1);
                                     $$ = rest;
                                   }
         | Expr                    { $$ = std::vector<std::any>(1, $1); }
         ;

# or
Expr    : AndExpr "or" Expr        {
                                     using M = std::map<std::string, std::any>;
                                     M n; n["op"] = std::string("or");
                                     n["l"] = $1; n["r"] = $3; $$ = n;
                                   }
        | AndExpr                  { $$ = $1; }
        ;

# and
AndExpr : CmpExpr "and" AndExpr    {
                                     using M = std::map<std::string, std::any>;
                                     M n; n["op"] = std::string("and");
                                     n["l"] = $1; n["r"] = $3; $$ = n;
                                   }
        | CmpExpr                  { $$ = $1; }
        ;

# ==  ~=  <  >  <=  >=
CmpExpr : AddExpr "==" AddExpr     { using M = std::map<std::string,std::any>; M n; n["op"]=std::string("=="); n["l"]=$1; n["r"]=$3; $$=n; }
        | AddExpr "~=" AddExpr     { using M = std::map<std::string,std::any>; M n; n["op"]=std::string("~="); n["l"]=$1; n["r"]=$3; $$=n; }
        | AddExpr "<"  AddExpr     { using M = std::map<std::string,std::any>; M n; n["op"]=std::string("<");  n["l"]=$1; n["r"]=$3; $$=n; }
        | AddExpr ">"  AddExpr     { using M = std::map<std::string,std::any>; M n; n["op"]=std::string(">");  n["l"]=$1; n["r"]=$3; $$=n; }
        | AddExpr "<=" AddExpr     { using M = std::map<std::string,std::any>; M n; n["op"]=std::string("<="); n["l"]=$1; n["r"]=$3; $$=n; }
        | AddExpr ">=" AddExpr     { using M = std::map<std::string,std::any>; M n; n["op"]=std::string(">="); n["l"]=$1; n["r"]=$3; $$=n; }
        | AddExpr                  { $$ = $1; }
        ;

# +  -  ..  (.. is right-assoc in Lua, so keep right-recursive)
AddExpr : MulExpr "+"  AddExpr     { using M = std::map<std::string,std::any>; M n; n["op"]=std::string("+");  n["l"]=$1; n["r"]=$3; $$=n; }
        | MulExpr "-"  AddExpr     { using M = std::map<std::string,std::any>; M n; n["op"]=std::string("-");  n["l"]=$1; n["r"]=$3; $$=n; }
        | MulExpr ".." AddExpr     { using M = std::map<std::string,std::any>; M n; n["op"]=std::string(".."); n["l"]=$1; n["r"]=$3; $$=n; }
        | MulExpr                  { $$ = $1; }
        ;

# *  /  //  %
MulExpr : UnaryExpr "*"  MulExpr   { using M = std::map<std::string,std::any>; M n; n["op"]=std::string("*");  n["l"]=$1; n["r"]=$3; $$=n; }
        | UnaryExpr "/"  MulExpr   { using M = std::map<std::string,std::any>; M n; n["op"]=std::string("/");  n["l"]=$1; n["r"]=$3; $$=n; }
        | UnaryExpr "//" MulExpr   { using M = std::map<std::string,std::any>; M n; n["op"]=std::string("//"); n["l"]=$1; n["r"]=$3; $$=n; }
        | UnaryExpr "%"  MulExpr   { using M = std::map<std::string,std::any>; M n; n["op"]=std::string("%");  n["l"]=$1; n["r"]=$3; $$=n; }
        | UnaryExpr                { $$ = $1; }
        ;

# unary  -  not  #
UnaryExpr : "-"   UnaryExpr        { using M = std::map<std::s
...>; M n; n["op"]=std::string("-");   n["r"]=$2; $$=n; }
          | "not" UnaryExpr        { using M = std::map<std::s...>; M n; n["op"]=std::string("not"); n["r"]=$2; $$=n; }
          | "#"   UnaryExpr        { using M = std::map<std::s...>; M n; n["op"]=std::string("#");   n["r"]=$2; $$=n; }
          | Primary                { $$ = $1; }
          ;

Primary : CallExpr                 { $$ = $1; }
        | Literal                  { $$ = $1; }
        | "(" Expr ")"             { $$ = $2; }
        ;

CallExpr : Name CallArgs           {
                                     using M = std::map<std::string, std::any>;
                                     M n; n["kind"] = std::string("call_expr");
                                     n["func"] = $1; n["args"] = $2;
                                     $$ = n;
                                   }
         | Name                    {
                                     using M = std::map<std::string, std::any>;
                                     M n; n["kind"] = std::string("variable");
                                     n["name"] = $1;
                                     $$ = n;
                                   }
         ;

CallArgs : "(" ExprList ")"        { $$ = $2; }
         | "(" ")"                 { $$ = std::vector<std::any>(); }
         ;

# ── Terminals & Helpers ────────────────────────────────────────────────────

Literal : Number                   { $$ = $1; }
        | String                   { $$ = $1; }
        | "true"                   { $$ = true; }
        | "false"                  { $$ = false; }
        | "nil"                    { $$ = std::any{}; }
        | TableConstructor         { $$ = $1; }
        ;

TableConstructor : "{" FieldList "}" { $$ = $2; }
                 | "{" "}"           { $$ = std::vector<std::any>(); }
                 ;

FieldList : Field "," FieldList    {
                                     auto rest = std::any_cast<std::vector<std::any>>($3);
                                     rest.insert(rest.begin(), $1);
                                     $$ = rest;
                                   }
          | Field                  { $$ = std::vector<std::any>(1, $1); }
          ;

Field     : Name "=" Expr          {
                                     using M = std::map<std::string, std::any>;
                                     M f; f["key"] = $1; f["value"] = $3;
                                     $$ = f;
                                   }
          | Expr                   { $$ = $1; }
          ;

ParamList : ParamListRaw           { $$ = $1; }
          |                        { $$ = std::vector<std::any>(); }
          ;

ParamListRaw : Name "," ParamListRaw {
                                     auto rest = std::any_cast<std::vector<std::any>>($3);
                                     rest.insert(rest.begin(), $1);
                                     $$ = rest;
                                   }
             | Name                { $$ = std::vector<std::any>(1, $1); }
             ;

Name    : /[a-zA-Z_][a-zA-Z0-9_]*/ { $$ = std::string($text); }
        ;

String  : /"(\\.|[^"\\])*"/        { $$ = std::string($text); }
        | /'(\\.|[^'\\])*'/        { $$ = std::string($text); }
        ;

Number  : /[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?/
                                   { $$ = std::stod(std::string($text)); }
        ;

