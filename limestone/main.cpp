#include "limestone.hpp"
#include "../bin2bin/bin2bin.hpp"
#include "../metacode/metacode.hpp"
#include "../traceml/traceml.hpp"
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
void help() {
  std::cout<<"Usage: limestone-cli [options] [input-file|-]\n"
    "  --evaluate                 Evaluate a closed TraceML integer program\n"
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
    enum class Command { Compile, Evaluate, ISA, Disassemble, Decompile, Translate } command=Command::Compile;
    limestone::PipelineOptions options;std::string input,output,source,target,cache_path;bool mode_set=false;
    auto next=[&](int& i){if(++i>=argc)throw std::runtime_error("missing option argument");return std::string(argv[i]);};
    auto mode=[&](Command value){if(mode_set)throw std::runtime_error("choose one operation");command=value;mode_set=true;};
    for(int i=1;i<argc;++i) {
      std::string arg=argv[i];
      if(arg=="--help"||arg=="-h"){help();return 0;}
      if(arg=="--version"){std::cout<<"Limestone 0.1.0\n";return 0;}
      if(arg=="--evaluate")mode(Command::Evaluate);
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
    std::string result;
    if(command==Command::ISA) {
      if(!input.empty())throw std::runtime_error("--isa does not accept a separate input file");
      result=limestone::metacode::dump(checked(limestone::metacode::load_isa_file(source)));
    }else {
      auto contents=read_input(input);
      if(command==Command::Compile)result=checked(limestone::run_pipeline(contents,options)).machine_ir;
      else if(command==Command::Evaluate)result=std::to_string(checked(limestone::traceml::evaluate(checked(limestone::traceml::compile(contents)))))+"\n";
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
