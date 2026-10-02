#include "traceml.hpp"
#include <dparse.h>
#include <charconv>
#include <limits>
#include <map>
#include <set>

extern "C" D_ParserTables parser_tables_traceml;

namespace limestone::traceml {
namespace {
using E=std::shared_ptr<Expr>;
void syntax_error(D_Parser*) {}
D_ParseNode* ambiguity(D_Parser*,int,D_ParseNode** v){return v[0];}
std::string_view symbol(D_ParseNode* n){return parser_tables_traceml.symbols[n->symbol].name;}
std::vector<D_ParseNode*> collect(D_ParseNode* n,std::string_view name) {
  if(symbol(n)==name)return {n};std::vector<D_ParseNode*> out;
  for(int i=0;i<d_get_number_of_children(n);++i){auto part=collect(d_get_child(n,i),name);out.insert(out.end(),part.begin(),part.end());}
  return out;
}
E expression(D_ParseNode* n,const char* start) {
  auto e=std::make_shared<Expr>();e->offset=n->start_loc.s-start;e->line=1;e->column=1;
  for(size_t i=0;i<e->offset;++i){if(start[i]=='\n'){++e->line;e->column=1;}else ++e->column;}
  auto syntax=d_get_child(n,0);auto name=symbol(syntax);
  std::string_view text{syntax->start_loc.s,static_cast<size_t>(syntax->end-syntax->start_loc.s)};
  if(name=="List") {
    e->kind=Expr::Kind::Apply;
    for(int i=0;i<d_get_number_of_children(syntax);++i)
      for(auto c:collect(d_get_child(syntax,i),"Expr"))e->children.push_back(expression(c,start));
  }else {
    int64_t value=0;auto [end,err]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(err==std::errc{}&&end==text.data()+text.size()){e->kind=Expr::Kind::Integer;e->integer=value;}
    else if(name=="Integer")throw Error{Error::Code::Parse,"TraceML integer out of range at line "+std::to_string(e->line)};
    else {e->kind=Expr::Kind::Symbol;e->atom=text;}
  }
  return e;
}
Result<Program> document(std::string_view text) {
  if(text.size()>static_cast<size_t>(std::numeric_limits<int>::max()))return Result<Program>::err({Error::Code::ResourceLimit,"TraceML input too large"});
  // Bound recursion before entering the generated parser/tree conversion.
  size_t depth=0;bool comment=false;
  for(char c:text){if(comment){if(c=='\n')comment=false;continue;}if(c==';'){comment=true;continue;}if(c=='('&&++depth>256)return Result<Program>::err({Error::Code::ResourceLimit,"TraceML nesting limit exceeded"});if(c==')'&&depth)--depth;}
  std::string buffer(text);std::unique_ptr<D_Parser,decltype(&free_D_Parser)> parser(new_D_Parser(&parser_tables_traceml,0),free_D_Parser);
  parser->save_parse_tree=1;parser->syntax_error_fn=syntax_error;parser->ambiguity_fn=ambiguity;
  auto tree=dparse(parser.get(),buffer.data(),static_cast<int>(buffer.size()));
  if(!tree||parser->syntax_errors){if(tree)free_D_ParseNode(parser.get(),tree);return Result<Program>::err({Error::Code::Parse,"TraceML syntax error at "+std::to_string(parser->loc.line)+":"+std::to_string(parser->loc.col+1)});}
  auto deleter=[&](D_ParseNode* n){free_D_ParseNode(parser.get(),n);};std::unique_ptr<D_ParseNode,decltype(deleter)> owned(tree,deleter);
  try {
    Program p;for(auto n:collect(tree,"Expr"))p.forms.push_back(expression(n,buffer.data()));return Result<Program>::ok(std::move(p));
  }catch(const Error& e){return Result<Program>::err(e);}
}
[[noreturn]] void fail(const E& e,std::string message,Error::Code code=Error::Code::InvalidArgument) {
  throw Error{code,"TraceML "+std::to_string(e->line)+":"+std::to_string(e->column)+": "+message};
}
const std::map<std::string,size_t>& primitives() {
  static const std::map<std::string,size_t> table{{"add",2},{"sub",2},{"mul",2},{"eq",2},{"lt",2},{"neg",1}};return table;
}
E normalize(E e) {
  if(e->kind!=Expr::Kind::Apply)return e;
  if(e->children.empty())fail(e,"empty application");
  const auto first=e->children.front();
  if(first->kind==Expr::Kind::Symbol&&first->atom=="lambda") {
    if(e->children.size()!=3||e->children[1]->kind!=Expr::Kind::Symbol)fail(e,"expected (lambda name body)");
    e->kind=Expr::Kind::Lambda;e->atom=e->children[1]->atom;e->children={normalize(e->children[2])};return e;
  }
  if(first->kind==Expr::Kind::Symbol&&first->atom=="if") {
    if(e->children.size()!=4)fail(e,"expected (if condition then else)");
    e->kind=Expr::Kind::If;e->children.erase(e->children.begin());
  }else if(first->kind==Expr::Kind::Symbol&&first->atom=="begin") {
    if(e->children.size()<2)fail(e,"begin requires a result expression");
    e->kind=Expr::Kind::Begin;e->children.erase(e->children.begin());
  }
  for(auto& c:e->children)c=normalize(c);return e;
}
void check(const E& e,std::set<std::string> bound,std::set<const Expr*>& visiting,size_t depth=0) {
  if(!e)throw Error{Error::Code::InvalidArgument,"null TraceLambda node"};
  if(depth>256)fail(e,"TraceLambda nesting limit exceeded",Error::Code::ResourceLimit);
  if(!visiting.insert(e.get()).second)fail(e,"cyclic TraceLambda expression");
  if(e->kind==Expr::Kind::Symbol) {
    if(e->atom.empty()||(!bound.contains(e->atom)&&!primitives().contains(e->atom)))fail(e,"unbound symbol: "+e->atom);
    if(!e->children.empty())fail(e,"symbol has child expressions");
  }else if(e->kind==Expr::Kind::Integer) {
    if(!e->children.empty())fail(e,"integer has child expressions");
  }else if(e->kind==Expr::Kind::Lambda) {
    if(e->atom.empty()||e->children.size()!=1)fail(e,"invalid lambda");bound.insert(e->atom);
  }else if(e->kind==Expr::Kind::If) {
    if(e->children.size()!=3)fail(e,"invalid conditional");
  }else if(e->kind==Expr::Kind::Apply) {
    if(e->children.size()<2)fail(e,"application requires an argument");
    if(e->children[0]&&e->children[0]->kind==Expr::Kind::Symbol&&!bound.contains(e->children[0]->atom)&&primitives().contains(e->children[0]->atom)&&e->children.size()-1!=primitives().at(e->children[0]->atom))fail(e,"wrong primitive arity");
  }else if(e->kind==Expr::Kind::Begin) {
    if(e->children.empty())fail(e,"empty begin");
  }else fail(e,"unknown expression kind");
  for(auto& c:e->children)check(c,bound,visiting,depth+1);visiting.erase(e.get());
}
struct Environment;
struct Closure { E expression; std::shared_ptr<const Environment> environment; };
struct Environment { std::map<std::string,Closure> bindings; };
struct Value { std::optional<int64_t> integer; Closure function; std::string primitive;std::optional<uint32_t> trace_value; };
struct Evaluator {
  size_t remaining;
  const ExecutionOptions& options;
  ExecutionResult execution;
  uint64_t next_value=1;
  size_t events=0;
  std::optional<uint32_t> record(TraceEvent::Kind kind,const E& e,std::string operation={},std::optional<int64_t> result={},std::vector<uint32_t> inputs={},std::optional<bool> taken={}) {
    if(!options.record_trace&&!options.observer)return {};
    if(events++>=options.event_limit)fail(e,"trace event limit reached",Error::Code::ResourceLimit);
    TraceEvent event{kind,execution.steps,e->offset,e->line,e->column,std::move(operation),{},std::move(inputs),result,taken};
    if(result&&(kind==TraceEvent::Kind::Integer||kind==TraceEvent::Kind::Primitive)) {
      if(next_value>UINT32_MAX)fail(e,"trace value identity overflow",Error::Code::ResourceLimit);event.value=static_cast<uint32_t>(next_value++);
    }
    if(options.observer)options.observer(event);
    auto value=event.value;if(options.record_trace)execution.trace.push_back(std::move(event));return value;
  }
  int64_t primitive(const E& e,const std::string& op,const std::vector<int64_t>& args) {
      auto a=args[0],b=args.size()>1?args[1]:0;int64_t result=0;
      const auto min=std::numeric_limits<int64_t>::min(),max=std::numeric_limits<int64_t>::max();
      if(op=="eq")result=a==b;
      else if(op=="lt")result=a<b;
      else if(op=="neg"){if(a==min)fail(e,"integer overflow");result=-a;}
      else if(op=="add"){if((b>0&&a>max-b)||(b<0&&a<min-b))fail(e,"integer overflow");result=a+b;}
      else if(op=="sub"){if((b<0&&a>max+b)||(b>0&&a<min+b))fail(e,"integer overflow");result=a-b;}
      else if(op=="mul") {
        if(a&&b&&((a==-1&&b==min)||(b==-1&&a==min)||(a>0?(b>0?a>max/b:b<min/a):(b>0?a<min/b:a<max/b))))fail(e,"integer overflow");result=a*b;
      }
      return result;
  }
  Value eval(const E& initial,std::shared_ptr<const Environment> initial_env) {
    struct Continuation {
      enum class Kind { Primitive, If, Begin } kind;
      Closure owner;
      std::vector<Closure> pending, operands;
      std::vector<int64_t> values;
      std::string primitive;
      size_t next=0;
      std::vector<uint32_t> trace_inputs;
    };
    Closure control{initial,std::move(initial_env)};
    std::vector<Closure> arguments;
    std::vector<Continuation> continuations;
    std::optional<Value> returned;
    // Krivine transitions use an explicit argument/continuation stack. Even
    // divergent terms consume the step budget without exhausting the C++ stack.
    for(;;) {
      if(options.cancelled&&options.cancelled())fail(control.expression,"execution cancelled",Error::Code::ResourceLimit);
      if(returned) {
        if(continuations.empty())return std::move(*returned);
        auto& frame=continuations.back();
        if(frame.kind==Continuation::Kind::Primitive) {
          if(!returned->integer)fail(frame.owner.expression,"primitive requires integer operands");
          frame.values.push_back(*returned->integer);
          if(returned->trace_value)frame.trace_inputs.push_back(*returned->trace_value);
          if(frame.next<frame.operands.size()){control=frame.operands[frame.next++];returned.reset();continue;}
          auto result=primitive(frame.owner.expression,frame.primitive,frame.values);
          auto traced=record(TraceEvent::Kind::Primitive,frame.owner.expression,frame.primitive,result,std::move(frame.trace_inputs));
          arguments=std::move(frame.pending);continuations.pop_back();returned=Value{result,{},{},traced};
        }else if(frame.kind==Continuation::Kind::If) {
          if(!returned->integer)fail(frame.owner.expression,"conditional requires an integer");
          record(TraceEvent::Kind::Branch,frame.owner.expression,"if",{},returned->trace_value?std::vector<uint32_t>{*returned->trace_value}:std::vector<uint32_t>{},bool(*returned->integer));
          control={frame.owner.expression->children[*returned->integer?1:2],frame.owner.environment};
          arguments=std::move(frame.pending);continuations.pop_back();returned.reset();
        }else {
          control={frame.owner.expression->children[frame.next++],frame.owner.environment};returned.reset();
          if(frame.next==frame.owner.expression->children.size()){arguments=std::move(frame.pending);continuations.pop_back();}
        }
        continue;
      }
      const auto e=control.expression;auto env=control.environment;
      if(!remaining)fail(e,"evaluation step limit reached",Error::Code::ResourceLimit);
      --remaining;
      ++execution.steps;
      if(e->kind==Expr::Kind::Apply) {
        record(TraceEvent::Kind::Apply,e);
        for(size_t i=e->children.size();i>1;--i)arguments.push_back({e->children[i-1],env});
        control={e->children[0],env};continue;
      }
      if(e->kind==Expr::Kind::Symbol) {
        if(env)if(auto it=env->bindings.find(e->atom);it!=env->bindings.end()){record(TraceEvent::Kind::Lookup,e,e->atom);control=it->second;continue;}
        if(arguments.empty()){returned=Value{{},{},e->atom};continue;}
        if(!primitives().contains(e->atom)||arguments.size()!=primitives().at(e->atom))fail(e,"invalid primitive application");
        Continuation frame{Continuation::Kind::Primitive,control,{},{},{},e->atom};
        while(!arguments.empty()){frame.operands.push_back(std::move(arguments.back()));arguments.pop_back();}
        control=frame.operands.front();frame.next=1;continuations.push_back(std::move(frame));continue;
      }
      if(e->kind==Expr::Kind::Lambda) {
        if(arguments.empty()){returned=Value{{},control,{}};continue;}
        record(TraceEvent::Kind::Bind,e,e->atom);
        auto next=std::make_shared<Environment>();if(env)next->bindings=env->bindings;
        next->bindings[e->atom]=std::move(arguments.back());arguments.pop_back();control={e->children[0],std::move(next)};continue;
      }
      if(e->kind==Expr::Kind::If) {
        continuations.push_back({Continuation::Kind::If,control,std::move(arguments)});arguments.clear();
        control={e->children[0],env};continue;
      }
      if(e->kind==Expr::Kind::Begin) {
        record(TraceEvent::Kind::Sequence,e);
        if(e->children.size()>1){Continuation frame{Continuation::Kind::Begin,control,std::move(arguments)};frame.next=1;continuations.push_back(std::move(frame));arguments.clear();}
        control={e->children[0],env};continue;
      }
      if(!arguments.empty())fail(e,"attempt to apply a non-function");
      returned=Value{e->integer,{},{},record(TraceEvent::Kind::Integer,e,"const",e->integer)};
    }
  }
};
}
Result<E> parse(std::string_view text) {
  auto p=document(text);if(!p)return Result<E>::err(p.error());
  if(p.value().forms.size()!=1)return Result<E>::err({Error::Code::Parse,"expected exactly one TraceML expression"});
  return Result<E>::ok(p.value().forms.front());
}
Result<Program> compile(std::string_view text) {
  auto p=document(text);if(!p)return p;
  if(p.value().forms.empty())return Result<Program>::err({Error::Code::Parse,"empty TraceML program"});
  try{for(auto& e:p.value().forms)e=normalize(e);}catch(const Error& e){return Result<Program>::err(e);}
  auto v=verify(p.value());if(!v)return Result<Program>::err(v.error());return p;
}
Result<int> verify(const Program& p) {
  try{if(p.forms.empty())throw Error{Error::Code::InvalidArgument,"empty TraceLambda program"};std::set<const Expr*> visiting;for(auto& e:p.forms)check(e,{},visiting);return Result<int>::ok(0);}catch(const Error& e){return Result<int>::err(e);}
}
Result<int64_t> evaluate(const Program& p,size_t steps) {
  ExecutionOptions options;options.step_limit=steps;options.record_trace=false;
  auto result=execute(p,options);if(!result)return Result<int64_t>::err(result.error());return Result<int64_t>::ok(result.value().value);
}
Result<ExecutionResult> execute(const Program& p,const ExecutionOptions& options) {
  auto v=verify(p);if(!v)return Result<ExecutionResult>::err(v.error());
  try {
    Evaluator evaluator{options.step_limit,options};Value result;
    for(auto& e:p.forms)result=evaluator.eval(e,{});
    if(!result.integer)throw Error{Error::Code::Unsupported,"TraceML program returns a function, not an integer"};
    evaluator.execution.value=*result.integer;evaluator.execution.result_value=result.trace_value;
    evaluator.record(TraceEvent::Kind::Return,p.forms.back(),"return",{},result.trace_value?std::vector<uint32_t>{*result.trace_value}:std::vector<uint32_t>{});
    return Result<ExecutionResult>::ok(std::move(evaluator.execution));
  }catch(const Error& e){return Result<ExecutionResult>::err(e);}
  catch(const std::exception& e){return Result<ExecutionResult>::err({Error::Code::Internal,std::string("TraceML observer: ")+e.what()});}
  catch(...){return Result<ExecutionResult>::err({Error::Code::Internal,"TraceML observer exception"});}
}
std::string print_trace(const ExecutionResult& execution) {
  std::string out;
  for(auto& event:execution.trace) {
    out+=std::to_string(event.step)+" "+std::to_string(event.line)+":"+std::to_string(event.column)+" ";
    const char* names[]={"apply","bind","lookup","integer","primitive","branch","sequence","return"};
    auto index=static_cast<size_t>(event.kind);out+=index<std::size(names)?names[index]:"invalid";
    if(!event.operation.empty())out+=" "+event.operation;
    if(event.value)out+=" %"+std::to_string(*event.value);
    for(auto input:event.inputs)out+=" use %"+std::to_string(input);
    if(event.result)out+=" = "+std::to_string(*event.result);
    if(event.taken)out+=*event.taken?" taken":" not-taken";out+='\n';
  }
  return out;
}
Result<std::string> lower_checked(const Program& p) {
  auto v=verify(p);if(!v)return Result<std::string>::err(v.error());
  // The current machine-facing slice is closed, pure integer computation.
  // Evaluate lazily before emitting so untaken branches/arguments stay unevaluated.
  auto result=evaluate(p);if(!result)return Result<std::string>::err(result.error());
  return Result<std::string>::ok("module traceml {\n  function main {\n    block entry {\n      %0 = const.i64 "+std::to_string(result.value())+"\n      ret %0\n    }\n  }\n}\n");
}
std::string lower_to_machineir(const Program& p) {
  auto r=lower_checked(p);if(!r)throw std::invalid_argument(r.error().message);return r.value();
}
}
