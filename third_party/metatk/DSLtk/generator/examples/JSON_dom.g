# JSON_dom.g -- JSON with full bottom-up DOM construction
# Generate with:  dsltk-pgen JSON_dom.g -o json_dom.hpp

%namespace json_dom
%start    Json
%skip     /[ \t\r\n]+/

Json    : Value EOF                { $$ = $1; }
        ;

Value   : Object                   { $$ = $1; }
        | Array                    { $$ = $1; }
        | String                   { $$ = $1; }
        | Number                   { $$ = $1; }
        | "true"                   { $$ = true; }
        | "false"                  { $$ = false; }
        | "null"                   { $$ = nullptr; }
        ;

Object  : "{" Members "}"          { $$ = $2; }
        | "{" "}"                  { $$ = std::vector<std::pair<std::string, std::any>>(); }
        ;

# Right recursion instead of ("," Member)* so $1 and $3 are addressable.
# Evaluation is bottom-up: the tail vector exists before the head prepends.
Members : Member "," Members       {
                                     auto rest = std::any_cast<std::vector<std::pair<std::string, std::any>>>($3);
                                     rest.insert(rest.begin(), std::any_cast<std::pair<std::string, std::any>>($1));
                                     $$ = rest;
                                   }
        | Member                   { $$ = std::vector<std::pair<std::string, std::any>>(1, std::any_cast<std::pair<std::string, std::any>>($1)); }
        ;

Member  : String ":" Value         { $$ = std::make_pair(std::any_cast<std::string>($1), $3); }
        ;

Array   : "[" Elements "]"         { $$ = $2; }
        | "[" "]"                  { $$ = std::vector<std::any>(); }
        ;

Elements: Value "," Elements       {
                                     auto rest = std::any_cast<std::vector<std::any>>($3);
                                     rest.insert(rest.begin(), $1);
                                     $$ = rest;
                                   }
        | Value                    { $$ = std::vector<std::any>(1, $1); }
        ;

# Decodes escapes inline. find('"') skips any whitespace that %skip left
# at the front of $text (it is recorded from _seq_start, before skipping).
String  : /"(\\.|[^"\\])*"/        {
                                     std::string_view t = $text;
                                     std::size_t i = t.find('"') + 1;
                                     std::string out;
                                     while (i < t.size() && t[i] != '"') {
                                       char c = t[i++];
                                       if (c != '\\') { out += c; continue; }
                                       char e = t[i++];
                                       switch (e) {
                                         case 'n': out += '\n'; break;
                                         case 't': out += '\t'; break;
                                         case 'r': out += '\r'; break;
                                         case 'b': out += '\b'; break;
                                         case 'f': out += '\f'; break;
                                         case 'u': out.append(t.substr(i - 2, 6)); i += 4; break;
                                         default:  out += e; break;
                                       }
                                     }
                                     $$ = out;
                                   }
        ;

Number  : /-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?/
                                   { $$ = std::stod(std::string($text)); }
        ;
