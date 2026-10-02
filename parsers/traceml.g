/* TraceML's existing S-expression frontend; semantic forms are validated later. */
Document: Expr*;
Expr: List | Integer | Symbol;
List: '(' Expr+ ')';
Integer: "-?[0-9]+";
Symbol: "[A-Za-z_+*/=<>!?-][A-Za-z0-9_+*/=<>!?-]*";
whitespace: "([ \t\r\n]|;[^\n]*(\n|$))*";
