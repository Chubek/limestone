%start Expr
%skip /[ \t\r\n]+/
%mode listener
%%
Expr: Expr '+' Number | Number;
@on_number
Number: /[0-9]+/;
%%
