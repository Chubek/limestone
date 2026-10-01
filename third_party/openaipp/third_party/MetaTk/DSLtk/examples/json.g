%namespace json_parser
%include "DSLtk.hpp"
%start Document
%skip /[ ]+/

Document = Value EOF ;

Value = Object | Array | String | Number | True | False | Null ;

Object = "{" (Member ("," Member)*)? "}" ;
Member = String ":" Value ;

Array = "[" (Value ("," Value)*)? "]" ;

String = "\"" Character* "\"" ;
Character = SafeCharacter | Escape ;
SafeCharacter = /[A-Za-z0-9 _.,:;!?@#$%^&*()_+=<>|~-]+/ ;
Escape = "\\" ("\"" | "\\" | "/" | "b" | "f" | "n" | "r" | "t" | ("u" Hex Hex Hex Hex)) ;
Hex = /[0-9A-Fa-f]/ ;

Number = ("-" | EPSILON) Integer Fraction? Exponent? ;
Integer = "0" | /[1-9][0-9]*/ ;
Fraction = "." /[0-9]+/ ;
Exponent = ("e" | "E") ("+" | "-")? /[0-9]+/ ;

True = "true" ;
False = "false" ;
Null = "null" ;
