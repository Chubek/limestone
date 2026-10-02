#include "detail.hpp"
#include <DSLtk.hpp>
#include <EkippX.hpp>
#include <SExprTk.hpp>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>

namespace limestone::tunah {
namespace detail {
Error diagnostic(Error::Code code, std::string message, const SourceLocation& location) {
  if(!location.file.empty()) {
    message=location.file+(location.expanded?" [expanded]":"")+":"+
      std::to_string(location.line)+":"+std::to_string(location.column)+": "+message;
  }
  return {code,std::move(message)};
}
bool symbol(std::string_view name) {
  if(name.empty()||name.front()=='?'||name.front()==':'||name=="nil"||
     name=="true"||name=="false"||name=="#t"||name=="#f") return false;
  if(std::isdigit(static_cast<unsigned char>(name.front()))||
     ((name.front()=='-'||name.front()=='+')&&name.size()>1&&
      std::isdigit(static_cast<unsigned char>(name[1])))) return false;
  return std::none_of(name.begin(),name.end(),[](unsigned char c) {
    return std::isspace(c)||c=='('||c==')'||c==';'||c=='"'||c=='\''||c=='`';
  });
}
void validate_term(const Term& term, const std::unordered_map<std::string,size_t>& operators,
                   bool pattern, std::set<std::string>& variables, size_t depth) {
  auto fail=[&](Error::Code code,std::string message) { throw diagnostic(code,std::move(message),term.location); };
  if(depth>max_depth) fail(Error::Code::ResourceLimit,"term nesting limit exceeded");
  if(!term.children.empty()) fail(Error::Code::Unsupported,"flat term IDs require an IL adapter; use structured arguments");
  if(term.constant) {
    if(!term.op.empty()||!term.arguments.empty()||term.application)
      fail(Error::Code::InvalidArgument,"constant has an operator or child terms");
    return;
  }
  if(!term.op.empty()&&term.op.front()=='?') {
    if(!pattern||!symbol(term.op.substr(1))||!term.arguments.empty()||term.application)
      fail(Error::Code::InvalidArgument,"invalid pattern variable: "+term.op);
    variables.insert(term.op.substr(1));
    return;
  }
  if(!symbol(term.op)) fail(Error::Code::InvalidArgument,"invalid term operator: "+term.op);
  auto signature=operators.find(term.op);
  if(signature==operators.end()&&(term.application||!term.arguments.empty()))
    fail(Error::Code::InvalidArgument,"unknown operator: "+term.op);
  if(signature!=operators.end()&&signature->second!=term.arguments.size())
    fail(Error::Code::InvalidArgument,"wrong arity for operator: "+term.op);
  for(const auto& child:term.arguments) validate_term(child,operators,pattern,variables,depth+1);
}
}

namespace {
// The lexical pass supplies full-size source coordinates and rejects data
// syntax that the permissive SExprTk parser would otherwise accept. Parsing
// and AST construction use SExprTk's event stream and DSLtk value nodes.
struct Token { SourceLocation location; bool list; std::string_view atom; };
struct Document {
  std::vector<dsl::ASTNode> forms;
  std::vector<Token> tokens;
  std::optional<Term> input;
  const SourceLocation& location(const dsl::ASTNode& node) const {
    return tokens.at(std::stoull(std::string(node.tag()))).location;
  }
};

Term atom(std::string_view text, const SourceLocation& location) {
  Term result;result.location=location;
  int64_t value=0;
  auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
  if(error==std::errc{}&&end==text.data()+text.size()) { result.constant=value; return result; }
  if(!text.empty()&&(std::isdigit(static_cast<unsigned char>(text.front()))||
     ((text.front()=='-'||text.front()=='+')&&text.size()>1&&std::isdigit(static_cast<unsigned char>(text[1])))))
    throw detail::diagnostic(Error::Code::Parse,"invalid or out-of-range integer atom: "+std::string(text),location);
  if(!detail::symbol(text)&&!(text.size()>1&&text.front()=='?'&&detail::symbol(text.substr(1))))
    throw detail::diagnostic(Error::Code::Unsupported,"term adapter supports integer literals and symbols",location);
  result.op=text;
  return result;
}
Term input_term(const sexprtk::Cell& cell, const Document& document, size_t& index) {
  const auto& token=document.tokens.at(index++);
  if(cell.head.is_list()!=token.list) throw Error{Error::Code::Internal,"SExprTk location tree mismatch"};
  if(!token.list) return atom(token.atom,token.location);
  const auto& list=cell.head.as_list();
  if(list.empty()) throw detail::diagnostic(Error::Code::Parse,"term requires an operator symbol",token.location);
  const auto& op=document.tokens.at(index++);
  if(op.list||!detail::symbol(op.atom)) throw detail::diagnostic(Error::Code::Parse,"expected an operator symbol",op.location);
  Term result;result.op=op.atom;result.location=token.location;result.application=true;
  result.arguments.reserve(list.size()-1);
  for(size_t i=1;i<list.size();++i) result.arguments.push_back(input_term(list[i],document,index));
  return result;
}

std::string mask_comments(std::string_view text) {
  std::string masked(text);
  bool quoted=false,escape=false,comment=false;
  for(char& c:masked) {
    if(comment) { if(c=='\n') comment=false; else c=' '; continue; }
    if(quoted) { if(escape) escape=false; else if(c=='\\') escape=true; else if(c=='"') quoted=false; continue; }
    if(c=='"') quoted=true;
    else if(c==';') { c=' '; comment=true; }
  }
  return masked;
}

std::string preprocess(std::string_view specification, std::string_view source) {
  ekippx::Config config;
  config.limits.max_output_size=detail::max_source_size;
  config.runtime.missing_symbol_policy=ekippx::MissingSymbolPolicy::error;
  ekippx::Context context(config);
  // Install the native deterministic macro primitives required by rule specs.
  // Each load gets an isolated context; definitions never leak between loads.
  context.register_directive({.name="define",.min_arity=2,.max_arity=2},
    [](ekippx::Context& ctx,const ekippx::Invocation& invocation) {
      ctx.macros().define_object(invocation.args[0],invocation.args[1]);
    });
  context.register_directive({.name="deflit",.min_arity=2,.max_arity=2},
    [](ekippx::Context& ctx,const ekippx::Invocation& invocation) {
      ctx.macros().define_literal(invocation.args[0],invocation.args[1]);
    });
  return context.expand_text(specification,source);
}

Document parse(std::string_view text, std::string_view source, bool expanded=false, bool input_only=false) {
  if(text.size()>detail::max_source_size)
    throw detail::diagnostic(Error::Code::ResourceLimit,"specification size limit exceeded",{std::string(source)});
  Document document;
  document.tokens.reserve(std::min<size_t>(text.size()/2+1,32));
  SourceLocation location{std::string(source),1,1,0,expanded};
  size_t depth=0,pos=0;
  auto advance=[&] {
    if(text[pos++]=='\n') { ++location.line; location.column=1; } else ++location.column;
    location.offset=pos;
  };
  while(pos<text.size()) {
    const char c=text[pos];
    if(std::isspace(static_cast<unsigned char>(c))) { advance(); continue; }
    if(c==';') { while(pos<text.size()&&text[pos]!='\n') advance(); continue; }
    if(c=='(') {
      document.tokens.push_back({location,true});
      if(++depth>detail::max_depth) throw detail::diagnostic(Error::Code::ResourceLimit,"S-expression nesting limit exceeded",location);
      advance(); continue;
    }
    if(c==')') {
      if(!depth) throw detail::diagnostic(Error::Code::Parse,"unexpected ')'",location);
      --depth; advance(); continue;
    }
    if(c=='\''||c=='`') throw detail::diagnostic(Error::Code::Unsupported,"quoted terms are unsupported",location);
    const auto begin=pos;
    document.tokens.push_back({location,false});
    if(c=='"') {
      const auto start=location;
      advance(); bool closed=false;
      while(pos<text.size()) {
        if(text[pos]=='"') { advance(); closed=true; break; }
        if(text[pos]=='\\') { advance(); if(pos<text.size()) advance(); }
        else advance();
      }
      if(!closed) throw detail::diagnostic(Error::Code::Parse,"unterminated string",start);
    } else {
      while(pos<text.size()&&!std::isspace(static_cast<unsigned char>(text[pos]))&&
            text[pos]!='('&&text[pos]!=')'&&text[pos]!=';') advance();
    }
    document.tokens.back().atom=text.substr(begin,pos-begin);
  }
  if(depth) throw detail::diagnostic(Error::Code::Parse,"unterminated S-expression",location);

  auto parsed=sexprtk::SExprTk{}.parse(sexprtk::Source::from_string(std::string(text),std::string(source)));
  if(!parsed.ok()) throw Error{Error::Code::Parse,parsed.errors.front()};
  if(input_only) {
    if(parsed.root.size()!=1) throw detail::diagnostic(Error::Code::Parse,"expected exactly one input term",{std::string(source)});
    // The input adapter uses SExprTk's existing tree directly; only rewrite
    // specifications need the additional DSLtk compilation AST.
    size_t index=0;document.input=input_term(parsed.root.front(),document,index);
    if(index!=document.tokens.size()) throw Error{Error::Code::Internal,"SExprTk incomplete location tree"};
    return document;
  }
  std::vector<dsl::ASTNode> stack;
  size_t index=0;
  auto append=[&](dsl::ASTNode node) {
    if(stack.empty()) document.forms.push_back(std::move(node));
    else stack.back().children().push_back(std::move(node));
  };
  for(const auto& event:parsed.events) {
    if(event.kind==SEXPRTK_XAS_EVENT_LIST_BEGIN) {
      if(index>=document.tokens.size()||!document.tokens[index].list) throw Error{Error::Code::Internal,"SExprTk location stream mismatch"};
      stack.emplace_back(std::to_string(index++),std::vector<dsl::ASTNode>{});
    } else if(event.kind==SEXPRTK_XAS_EVENT_ATOM) {
      if(index>=document.tokens.size()||document.tokens[index].list) throw Error{Error::Code::Internal,"SExprTk location stream mismatch"};
      append(dsl::ASTNode{std::to_string(index++),event.payload});
    } else if(event.kind==SEXPRTK_XAS_EVENT_LIST_END) {
      if(stack.empty()) throw Error{Error::Code::Internal,"SExprTk unbalanced event stream"};
      auto node=std::move(stack.back()); stack.pop_back(); append(std::move(node));
    }
  }
  if(index!=document.tokens.size()||!stack.empty()) throw Error{Error::Code::Internal,"SExprTk incomplete event stream"};
  return document;
}

std::string name(const dsl::ASTNode& node, const Document& document) {
  if(!node.is_leaf()||!detail::symbol(node.value()))
    throw detail::diagnostic(Error::Code::Parse,"expected a symbol",document.location(node));
  return node.value();
}
Term term(const dsl::ASTNode& node, const Document& document) {
  Term result; result.location=document.location(node);
  if(node.is_leaf()) {
    return atom(node.value(),result.location);
  } else {
    if(node.children().empty()) throw detail::diagnostic(Error::Code::Parse,"term requires an operator symbol",result.location);
    result.op=name(node.child(0),document); result.application=true;
    result.arguments.reserve(node.size()-1);
    for(size_t i=1;i<node.size();++i) result.arguments.push_back(term(node.child(i),document));
  }
  return result;
}
void conditions(const dsl::ASTNode& node, const Document& document, std::vector<Condition>& result) {
  if(node.is_leaf()||node.children().empty())
    throw detail::diagnostic(Error::Code::Parse,"expected a predicate application",document.location(node));
  auto predicate=name(node.child(0),document);
  if(predicate=="and") {
    if(node.size()<2) throw detail::diagnostic(Error::Code::Parse,"empty condition conjunction",document.location(node));
    for(size_t i=1;i<node.size();++i) conditions(node.child(i),document,result);
  } else {
    Condition condition{std::move(predicate),{},document.location(node)};
    for(size_t i=1;i<node.size();++i) condition.arguments.push_back(term(node.child(i),document));
    result.push_back(std::move(condition));
  }
}
}

Result<Term> parse_term(std::string_view expression, std::string_view source) {
  try {
    auto document=parse(expression,source,false,true);
    return Result<Term>::ok(std::move(*document.input));
  } catch(const Error& error) { return Result<Term>::err(error); }
    catch(const std::exception& error) { return Result<Term>::err({Error::Code::Parse,error.what()}); }
}
std::string format_term(const Term& term) {
  if(term.constant) return std::to_string(*term.constant);
  if(!term.application&&term.arguments.empty()) return term.op;
  std::string result="("+term.op;
  for(const auto& child:term.arguments) result+=" "+format_term(child);
  return result+")";
}
Result<int> Session::define_operator(std::string name, size_t arity) {
  if(!detail::symbol(name)) return Result<int>::err({Error::Code::InvalidArgument,"invalid operator name: "+name});
  auto [it,inserted]=operators_.emplace(name,arity);
  if(!inserted&&it->second!=arity) return Result<int>::err({Error::Code::Conflict,"conflicting operator signature: "+name});
  if(inserted) prepared_.reset();
  return Result<int>::ok(inserted?1:0);
}
Result<int> Session::define_predicate(std::string name, size_t arity, Predicate predicate) {
  if(!detail::symbol(name)||name=="and"||!predicate)
    return Result<int>::err({Error::Code::InvalidArgument,"invalid analysis predicate: "+name});
  if(predicates_.contains(name)) return Result<int>::err({Error::Code::Conflict,"duplicate analysis predicate: "+name});
  predicates_.emplace(std::move(name),PredicateInfo{arity,std::move(predicate)});
  return Result<int>::ok(1);
}
void Session::validate_rule(const RewriteRule& rule, const std::unordered_map<std::string,size_t>& operators) const {
  if(!detail::symbol(rule.name)) throw detail::diagnostic(Error::Code::InvalidArgument,"invalid rewrite rule name",rule.location);
  std::set<std::string> lhs,rhs;
  detail::validate_term(rule.lhs,operators,true,lhs); detail::validate_term(rule.rhs,operators,true,rhs);
  for(const auto& variable:rhs) if(!lhs.contains(variable))
    throw detail::diagnostic(Error::Code::InvalidArgument,"unbound replacement variable: ?"+variable,rule.rhs.location);
  for(const auto& condition:rule.conditions) {
    auto predicate=predicates_.find(condition.predicate);
    if(predicate==predicates_.end())
      throw detail::diagnostic(Error::Code::Unsupported,"analysis predicate requires a host adapter: "+condition.predicate,condition.location);
    if(predicate->second.arity!=condition.arguments.size())
      throw detail::diagnostic(Error::Code::InvalidArgument,"wrong predicate arity: "+condition.predicate,condition.location);
    for(const auto& argument:condition.arguments) {
      std::set<std::string> variables;
      detail::validate_term(argument,operators,true,variables);
      if(!argument.constant&&(argument.op.empty()||argument.op.front()!='?'||argument.application||!argument.arguments.empty()))
        throw detail::diagnostic(Error::Code::Unsupported,"predicate arguments must be bound variables or integer literals",argument.location);
      for(const auto& variable:variables) if(!lhs.contains(variable))
        throw detail::diagnostic(Error::Code::InvalidArgument,"unbound predicate variable: ?"+variable,argument.location);
    }
  }
}
Result<int> Session::load_rules(std::string_view specification, std::string_view source) {
  try {
    if(specification.size()>detail::max_source_size) throw Error{Error::Code::ResourceLimit,"specification size limit exceeded"};
    auto masked=mask_comments(specification);
    auto text=preprocess(masked,source);
    auto document=parse(text,source,text!=masked);
    auto operators=operators_;
    // Declarations are visible throughout this load, including before their use.
    for(const auto& form:document.forms) {
      if(form.is_leaf()||form.children().empty()) throw detail::diagnostic(Error::Code::Parse,"expected an operator declaration or rewrite rule",document.location(form));
      if(name(form.child(0),document)!="operator") continue;
      if(form.size()!=3) throw detail::diagnostic(Error::Code::Parse,"expected (operator name arity)",document.location(form));
      auto op=name(form.child(1),document); auto arity=term(form.child(2),document);
      if(!arity.constant||*arity.constant<0) throw detail::diagnostic(Error::Code::InvalidArgument,"operator arity must be a non-negative integer",arity.location);
      auto [it,inserted]=operators.emplace(op,static_cast<size_t>(*arity.constant));
      if(!inserted&&it->second!=static_cast<size_t>(*arity.constant))
        throw detail::diagnostic(Error::Code::Conflict,"conflicting operator signature: "+op,document.location(form));
    }
    std::vector<RewriteRule> compiled;
    std::set<std::string> names; for(const auto& rule:rules_) names.insert(rule.name);
    for(const auto& form:document.forms) {
      auto kind=name(form.child(0),document);
      if(kind=="operator") continue;
      if(kind!="rule"||form.size()<4) throw detail::diagnostic(Error::Code::Parse,"expected (rule name pattern replacement [:where condition])",document.location(form));
      RewriteRule rule{name(form.child(1),document),term(form.child(2),document),term(form.child(3),document),{},document.location(form)};
      if(form.size()!=4) {
        if(form.size()!=6||!form.child(4).is_leaf()||form.child(4).value()!=":where")
          throw detail::diagnostic(Error::Code::Unsupported,"only a single :where clause is supported; extraction costs belong to CostModel",document.location(form));
        conditions(form.child(5),document,rule.conditions);
      }
      if(!names.insert(rule.name).second) throw detail::diagnostic(Error::Code::Conflict,"duplicate rewrite rule: "+rule.name,rule.location);
      validate_rule(rule,operators); compiled.push_back(std::move(rule));
    }
    if(compiled.size()>static_cast<size_t>(std::numeric_limits<int>::max())) throw Error{Error::Code::ResourceLimit,"too many rewrite rules"};
    auto rules=rules_; rules.insert(rules.end(),std::make_move_iterator(compiled.begin()),std::make_move_iterator(compiled.end()));
    auto prepared=prepare_rules(rules,operators);
    const auto count=rules.size()-rules_.size();
    operators_.swap(operators); rules_.swap(rules); prepared_=std::move(prepared);
    return Result<int>::ok(static_cast<int>(count));
  } catch(const Error& error) { return Result<int>::err(error); }
    catch(const ekippx::LimitError& error) { return Result<int>::err(detail::diagnostic(Error::Code::ResourceLimit,error.what(),{std::string(source)})); }
    catch(const std::exception& error) { return Result<int>::err(detail::diagnostic(Error::Code::Parse,error.what(),{std::string(source)})); }
}
Result<int> Session::load_rules_file(const std::filesystem::path& path) {
  try {
    std::ifstream input(path,std::ios::binary);
    if(!input) return Result<int>::err({Error::Code::NotFound,"cannot open rule file: "+path.string()});
    std::string text; char buffer[4096];
    while(input.read(buffer,sizeof(buffer))||input.gcount()) {
      if(text.size()+static_cast<size_t>(input.gcount())>detail::max_source_size)
        return Result<int>::err({Error::Code::ResourceLimit,"rule file size limit exceeded: "+path.string()});
      text.append(buffer,static_cast<size_t>(input.gcount()));
    }
    if(input.bad()) return Result<int>::err({Error::Code::Parse,"cannot read rule file: "+path.string()});
    return load_rules(text,path.string());
  } catch(const std::exception& error) { return Result<int>::err({Error::Code::Parse,path.string()+": "+error.what()}); }
}
}
