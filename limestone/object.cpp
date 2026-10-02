#include "object_internal.hpp"
#include "c_api_internal.hpp"

struct limestone_object_data { std::vector<uint8_t> bytes; };
struct limestone_linked_image { limestone::bin2bin::LinkedImage image; };
namespace {
using namespace limestone;
using namespace limestone::c_api_internal;
void argument(bool valid) {if(!valid)throw Error{Error::Code::InvalidArgument,"invalid object API argument"};}
template<class T> const T& at(const std::vector<T>& items,size_t index) {
  if(index>=items.size())throw Error{Error::Code::NotFound,"object inspection index out of range"};return items[index];
}
template<class F> void change(limestone_object& object,F operation) {
  auto copy=object.file;operation(copy);checked(bin2bin::validate(copy));auto text=bin2bin::print_object(copy);object.file=std::move(copy);object.text=std::move(text);
}
}
extern "C" limestone_object_target* limestone_object_target_load_isa(const char* source,limestone_error* error) {
  return boundary(error,[&]()->limestone_object_target*{argument(source);return new limestone_object_target{checked(bin2bin::object_target(checked(metacode::parse_isa(source))))};});
}
extern "C" void limestone_object_target_destroy(limestone_object_target* target){delete target;}
extern "C" limestone_object* limestone_object_create(const limestone_object_target* target,limestone_error* error) {
  return boundary(error,[&]()->limestone_object*{argument(target);bin2bin::ObjectFile file;file.format=target->target.format;return new limestone_object(std::move(file));});
}
extern "C" limestone_object* limestone_object_load_elf(const uint8_t* bytes,size_t size,const char* source,limestone_error* error) {
  return boundary(error,[&]()->limestone_object*{argument(bytes||!size);return new limestone_object(checked(bin2bin::load_elf({bytes,size},source?source:"<elf>")));});
}
extern "C" limestone_object* limestone_object_from_code(const limestone_object_target* target,const uint8_t* bytes,size_t size,const char* symbol,limestone_error* error) {
  return boundary(error,[&]()->limestone_object*{argument(target&&symbol&&(bytes||!size));return new limestone_object(checked(bin2bin::code_object(target->target,{bytes,size},symbol)));});
}
extern "C" void limestone_object_destroy(limestone_object* object){delete object;}
extern "C" const char* limestone_object_text(const limestone_object* object){return object?object->text.c_str():nullptr;}
extern "C" size_t limestone_object_section_count(const limestone_object* object){return object?object->file.sections.size():0;}
extern "C" size_t limestone_object_symbol_count(const limestone_object* object){return object?object->file.symbols.size():0;}
extern "C" size_t limestone_object_relocation_count(const limestone_object* object){return object?object->file.relocations.size():0;}
extern "C" limestone_status limestone_object_get_section(const limestone_object* object,size_t index,limestone_object_section* out,limestone_error* error) {
  return boundary(error,[&](){argument(object&&out);auto& s=at(object->file.sections,index);*out={s.name.c_str(),s.type,s.flags,s.alignment,s.entry_size,s.bytes.empty()?nullptr:s.bytes.data(),s.bytes.size(),s.zero_fill};return LIMESTONE_OK;});
}
extern "C" limestone_status limestone_object_get_symbol(const limestone_object* object,size_t index,limestone_object_symbol* out,limestone_error* error) {
  return boundary(error,[&](){argument(object&&out);auto& s=at(object->file.symbols,index);*out={s.name.c_str(),s.section,s.value,s.size,static_cast<limestone_symbol_binding>(s.binding),s.type,s.visibility};return LIMESTONE_OK;});
}
extern "C" limestone_status limestone_object_get_relocation(const limestone_object* object,size_t index,limestone_object_relocation* out,limestone_error* error) {
  return boundary(error,[&](){argument(object&&out);auto& r=at(object->file.relocations,index);*out={r.section,r.symbol,r.type,r.offset,r.addend};return LIMESTONE_OK;});
}
extern "C" limestone_status limestone_object_add_section(limestone_object* object,const limestone_object_section* input,uint32_t* id,limestone_error* error) {
  return boundary(error,[&](){argument(object&&input&&id&&input->name&&(input->bytes||!input->byte_count));argument(input->byte_count<=64*1024*1024);
    bin2bin::ObjectSection s{input->name,input->type,input->flags,input->alignment,input->entry_size,{},input->zero_fill};if(input->byte_count)s.bytes.assign(input->bytes,input->bytes+input->byte_count);
    auto index=uint32_t(object->file.sections.size());change(*object,[&](auto& f){f.sections.push_back(std::move(s));});*id=index;return LIMESTONE_OK;});
}
extern "C" limestone_status limestone_object_add_symbol(limestone_object* object,const limestone_object_symbol* input,uint32_t* id,limestone_error* error) {
  return boundary(error,[&](){argument(object&&input&&id&&input->name);bin2bin::ObjectSymbol s{input->name,input->section,input->value,input->size,static_cast<bin2bin::SymbolBinding>(input->binding),input->type,input->visibility};
    auto index=uint32_t(object->file.symbols.size());change(*object,[&](auto& f){f.symbols.push_back(std::move(s));});*id=index;return LIMESTONE_OK;});
}
extern "C" limestone_status limestone_object_add_relocation(limestone_object* object,const limestone_object_relocation* input,limestone_error* error) {
  return boundary(error,[&](){argument(object&&input);bin2bin::ObjectRelocation r{input->section,input->symbol,input->type,input->offset,input->addend};change(*object,[&](auto& f){f.relocations.push_back(r);});return LIMESTONE_OK;});
}
extern "C" limestone_object_data* limestone_object_emit_elf(const limestone_object* object,limestone_error* error) {
  return boundary(error,[&]()->limestone_object_data*{argument(object);return new limestone_object_data{checked(bin2bin::emit_elf(object->file))};});
}
extern "C" void limestone_object_data_destroy(limestone_object_data* data){delete data;}
extern "C" const uint8_t* limestone_object_data_bytes(const limestone_object_data* data){return data&&!data->bytes.empty()?data->bytes.data():nullptr;}
extern "C" size_t limestone_object_data_size(const limestone_object_data* data){return data?data->bytes.size():0;}
extern "C" limestone_linked_image* limestone_object_link(const limestone_object_target* target,const limestone_object* const* objects,size_t count,uint64_t base,uint64_t max_size,const limestone_external_symbol* externals,size_t external_count,limestone_error* error) {
  return boundary(error,[&]()->limestone_linked_image*{argument(target&&count&&objects&&(externals||!external_count));argument(count<=16384&&external_count<=1048576);std::vector<const bin2bin::ObjectFile*> files;files.reserve(count);for(size_t k=0;k<count;++k){argument(objects[k]);files.push_back(&objects[k]->file);}
    bin2bin::LinkOptions options;options.base_address=base;options.max_size=max_size;
    for(size_t k=0;k<external_count;++k){argument(externals[k].name&&*externals[k].name);if(!options.externals.emplace(externals[k].name,externals[k].address).second)throw Error{Error::Code::Conflict,"duplicate external symbol"};}
    return new limestone_linked_image{checked(bin2bin::link_objects(files,target->target,options))};});
}
extern "C" void limestone_linked_image_destroy(limestone_linked_image* image){delete image;}
extern "C" const uint8_t* limestone_linked_image_bytes(const limestone_linked_image* image){return image&&!image->image.bytes.empty()?image->image.bytes.data():nullptr;}
extern "C" size_t limestone_linked_image_size(const limestone_linked_image* image){return image?image->image.bytes.size():0;}
extern "C" uint64_t limestone_linked_image_base(const limestone_linked_image* image){return image?image->image.base_address:0;}
extern "C" size_t limestone_linked_image_symbol_count(const limestone_linked_image* image){return image?image->image.symbols.size():0;}
extern "C" limestone_status limestone_linked_image_get_symbol(const limestone_linked_image* image,size_t index,limestone_external_symbol* out,limestone_error* error) {
  return boundary(error,[&](){argument(image&&out);if(index>=image->image.symbols.size())throw Error{Error::Code::NotFound,"image symbol index out of range"};auto it=image->image.symbols.begin();std::advance(it,index);*out={it->first.c_str(),it->second};return LIMESTONE_OK;});
}
extern "C" limestone_status limestone_linked_image_find_symbol(const limestone_linked_image* image,const char* name,uint64_t* address,limestone_error* error) {
  return boundary(error,[&](){argument(image&&name&&address);auto it=image->image.symbols.find(name);if(it==image->image.symbols.end())throw Error{Error::Code::NotFound,"unknown linked symbol"};*address=it->second;return LIMESTONE_OK;});
}
