#include "limestone.hpp"
#include "../bin2bin/bin2bin.hpp"
#include "../metacode/metacode.hpp"
#include "../traceml/traceml.hpp"
#include "../tunah/tunah.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
std::string read_input(const std::string& path) {
  std::ostringstream contents;
  if(path.empty()||path=="-")contents<<std::cin.rdbuf();
  else {std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("cannot open input: "+path);contents<<file.rdbuf();if(file.bad())throw std::runtime_error("cannot read input: "+path);}
  if(std::cin.bad())throw std::runtime_error("cannot read standard input");
  return contents.str();
}
template<class T> T checked(limestone::Result<T> result) {
  if(!result)throw std::runtime_error(result.error().message);return std::move(result.value());
}
size_t number(const std::string& text) {
  size_t value=0;
  auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
  if(error!=std::errc{}||end!=text.data()+text.size())throw std::runtime_error("expected a non-negative integer: "+text);
  return value;
}
void help() {
  std::cout<<"Usage: limestone-cli [options] [input-file|-]\n"
    "  --evaluate                 Evaluate a closed TraceML integer program\n"
    "  --optimize-term            Optimize an S-expression with Tunah\n"
    "  --rules FILE               Load operator/rule declarations (repeatable)\n"
    "  --op-cost OP COST          Set a local extraction cost for an operator\n"
    "  --literal-cost COST        Set the local extraction cost of literals\n"
    "  --iterations N --node-limit N --class-limit N --time-limit MS\n"
    "                             Bound Tunah saturation\n"
    "  --trace                    Print Tunah matches and statistics to stderr\n"
    "  --no-optimize --no-schedule Configure portable MachineIR lowering\n"
    "  --isa FILE                 Parse and summarize an Infobank description\n"
    "  --disassemble ISA          Decode an operand-free fixed8 binary\n"
    "  --decompile ISA            Lift a fixed8 binary to semantic assembly\n"
    "  --translate SOURCE TARGET  Translate between fixed8 descriptions\n"
    "  --cache DIRECTORY          Use a persistent LMDB translation cache\n"
    "  -o, --output FILE          Write output to a file (default: stdout)\n"
    "  --help --version\n";
}
}
int main(int argc,char** argv) {
  try {
    enum class Command { Compile, Evaluate, OptimizeTerm, ISA, Disassemble, Decompile, Translate } command=Command::Compile;
    limestone::PipelineOptions options;std::string input,output,source,target,cache_path;bool mode_set=false;
    limestone::tunah::CostModel costs;limestone::tunah::Limits limits;limits.trace=false;
    std::vector<std::string> rule_files;bool tunah_options=false;
    auto next=[&](int& i){if(++i>=argc)throw std::runtime_error("missing option argument");return std::string(argv[i]);};
    auto mode=[&](Command value){if(mode_set)throw std::runtime_error("choose one operation");command=value;mode_set=true;};
    for(int i=1;i<argc;++i) {
      std::string arg=argv[i];
      if(arg=="--help"||arg=="-h"){help();return 0;}
      if(arg=="--version"){std::cout<<"Limestone 0.1.0\n";return 0;}
      if(arg=="--evaluate")mode(Command::Evaluate);
      else if(arg=="--optimize-term")mode(Command::OptimizeTerm);
      else if(arg=="--rules"){rule_files.push_back(next(i));tunah_options=true;}
      else if(arg=="--op-cost"){
        auto op=next(i);auto cost=number(next(i));
        if(!costs.operators.emplace(op,cost).second)throw std::runtime_error("duplicate operator cost: "+op);
        tunah_options=true;
      }
      else if(arg=="--literal-cost"){costs.literal=number(next(i));tunah_options=true;}
      else if(arg=="--iterations"){limits.iterations=number(next(i));tunah_options=true;}
      else if(arg=="--node-limit"){limits.nodes=number(next(i));tunah_options=true;}
      else if(arg=="--class-limit"){limits.classes=number(next(i));tunah_options=true;}
      else if(arg=="--time-limit"){limits.time_ms=number(next(i));tunah_options=true;}
      else if(arg=="--trace"){limits.trace=true;tunah_options=true;}
      else if(arg=="--isa"){mode(Command::ISA);source=next(i);}
      else if(arg=="--disassemble"){mode(Command::Disassemble);source=next(i);}
      else if(arg=="--decompile"){mode(Command::Decompile);source=next(i);}
      else if(arg=="--translate"){mode(Command::Translate);source=next(i);target=next(i);}
      else if(arg=="--no-optimize")options.optimize=false;
      else if(arg=="--no-schedule")options.schedule=false;
      else if(arg=="--allocate")options.allocate=true;
      else if(arg=="-o"||arg=="--output")output=next(i);
      else if(arg=="--cache")cache_path=next(i);
      else if(arg!="-"&&arg.starts_with('-'))throw std::runtime_error("unknown option: "+arg);
      else {if(!input.empty())throw std::runtime_error("too many input files");input=arg;}
    }
    if(command!=Command::Compile&&(!options.optimize||!options.schedule||options.allocate))throw std::runtime_error("pipeline options require compilation");
    if(command!=Command::Translate&&!cache_path.empty())throw std::runtime_error("--cache requires --translate");
    if(command!=Command::OptimizeTerm&&tunah_options)throw std::runtime_error("Tunah options require --optimize-term");
    std::string result;
    if(command==Command::ISA) {
      if(!input.empty())throw std::runtime_error("--isa does not accept a separate input file");
      result=limestone::metacode::dump(checked(limestone::metacode::load_isa_file(source)));
    }else {
      auto contents=read_input(input);
      if(command==Command::Compile)result=checked(limestone::run_pipeline(contents,options)).machine_ir;
      else if(command==Command::Evaluate)result=std::to_string(checked(limestone::traceml::evaluate(checked(limestone::traceml::compile(contents)))))+"\n";
      else if(command==Command::OptimizeTerm){
        limestone::tunah::Session session;
        for(const auto& path:rule_files)checked(session.load_rules_file(path));
        auto term=checked(limestone::tunah::parse_term(contents,input.empty()||input=="-"?"<stdin>":input));
        auto optimized=checked(session.saturate(term,limits,costs));
        result=optimized.expression+"\n";
        if(limits.trace){
          for(const auto& match:optimized.trace)std::cerr<<"tunah: "<<match<<'\n';
          std::cerr<<"tunah: cost="<<optimized.cost<<" rewrites="<<optimized.rewrites
            <<" nodes="<<optimized.nodes<<" classes="<<optimized.classes<<" iterations="<<optimized.iterations
            <<" saturated="<<optimized.saturated<<" limit_reached="<<optimized.limit_reached<<'\n';
        }
      }
      else {
        auto src=checked(limestone::bin2bin::from_metacode(checked(limestone::metacode::load_isa_file(source))));
        std::span<const uint8_t> bytes{reinterpret_cast<const uint8_t*>(contents.data()),contents.size()};
        if(command==Command::Disassemble)result=limestone::bin2bin::disassemble(checked(limestone::bin2bin::decode(src,bytes)));
        else if(command==Command::Decompile)result=checked(limestone::bin2bin::decompile(src,bytes));
        else {
          auto dst=checked(limestone::bin2bin::from_metacode(checked(limestone::metacode::load_isa_file(target))));
          limestone::bin2bin::TranslationCache cache;if(!cache_path.empty())checked(limestone::bin2bin::open_cache(cache,cache_path));
          auto translated=checked(limestone::bin2bin::translate(src,dst,bytes,cache_path.empty()?nullptr:&cache));
          result.assign(translated.begin(),translated.end());
        }
      }
    }
    if(output.empty()||output=="-"){std::cout.write(result.data(),static_cast<std::streamsize>(result.size()));if(!std::cout)throw std::runtime_error("cannot write standard output");}
    else {std::ofstream file(output,std::ios::binary);if(!file)throw std::runtime_error("cannot open output: "+output);file.write(result.data(),static_cast<std::streamsize>(result.size()));file.close();if(!file)throw std::runtime_error("cannot write output: "+output);}
    return 0;
  }catch(const std::exception& e){std::cerr<<"limestone: "<<e.what()<<'\n';return 1;}
}
