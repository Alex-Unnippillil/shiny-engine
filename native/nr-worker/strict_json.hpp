// SPDX-License-Identifier: MIT
// Bounded JSON validation BEFORE the permissive upstream model parser is invoked.
#pragma once
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace shiny::nrpolicy::strict {
struct Value {
 enum Kind {Null, Bool, Number, String, Array, Object} kind=Null;
 bool boolean=false; double number=0; std::string string;
 std::vector<Value> array; std::map<std::string,Value> object;
 const Value& at(std::string_view key) const {
  if(kind!=Object) throw std::runtime_error("Manifest member is not an object.");
  auto it=object.find(std::string(key)); if(it==object.end()) throw std::runtime_error("Missing manifest field: "+std::string(key));
  return it->second;
 }
 std::string text() const {if(kind!=String)throw std::runtime_error("Expected JSON string.");return string;}
 uint64_t integer(uint64_t maximum) const {
  if(kind!=Number||!std::isfinite(number)||number<0||std::floor(number)!=number||number>static_cast<double>(maximum))
   throw std::runtime_error("Manifest integer is outside its supported range.");
  return static_cast<uint64_t>(number);
 }
};
class Parser {
 std::string_view input; size_t pos=0,nodes=0;
 [[noreturn]] void fail() const {throw std::runtime_error("Invalid or over-limit JSON near byte "+std::to_string(pos)+".");}
 void ws(){while(pos<input.size()&&(input[pos]==' '||input[pos]=='\t'||input[pos]=='\r'||input[pos]=='\n'))++pos;}
 char peek(){ws();return pos<input.size()?input[pos]:'\0';}
 void expect(char c){if(peek()!=c)fail();++pos;}
 unsigned hex4(){unsigned n=0;for(int i=0;i<4;++i){if(pos>=input.size())fail();char c=input[pos++];n*=16;if(c>='0'&&c<='9')n+=c-'0';else if(c>='A'&&c<='F')n+=10+c-'A';else if(c>='a'&&c<='f')n+=10+c-'a';else fail();}return n;}
 static void codepoint(std::string& s,unsigned cp){
  if(cp<=0x7f)s+=static_cast<char>(cp);
  else if(cp<=0x7ff){s+=static_cast<char>(0xc0|(cp>>6));s+=static_cast<char>(0x80|(cp&63));}
  else if(cp<=0xffff){s+=static_cast<char>(0xe0|(cp>>12));s+=static_cast<char>(0x80|((cp>>6)&63));s+=static_cast<char>(0x80|(cp&63));}
  else {s+=static_cast<char>(0xf0|(cp>>18));s+=static_cast<char>(0x80|((cp>>12)&63));s+=static_cast<char>(0x80|((cp>>6)&63));s+=static_cast<char>(0x80|(cp&63));}
 }
 std::string string(){
  expect('"');std::string s;
  while(pos<input.size()){
   unsigned char c=input[pos++];if(c=='"')return s;if(c<32)fail();
   if(c=='\\'){
    if(pos>=input.size())fail();char e=input[pos++];
    switch(e){case '"':case '\\':case '/':s+=e;break;case 'b':s+='\b';break;case 'f':s+='\f';break;case 'n':s+='\n';break;case 'r':s+='\r';break;case 't':s+='\t';break;
     case 'u':{auto cp=hex4();if(cp>=0xd800&&cp<=0xdbff){if(input.substr(pos,2)!="\\u")fail();pos+=2;auto low=hex4();if(low<0xdc00||low>0xdfff)fail();cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);}else if(cp>=0xdc00&&cp<=0xdfff)fail();codepoint(s,cp);break;}default:fail();}
   }else if(c<128)s+=static_cast<char>(c);
   else{
    unsigned cp=0;int count=0;unsigned min=0;
    if(c>=0xc2&&c<=0xdf){cp=c&31;count=1;min=0x80;}else if(c>=0xe0&&c<=0xef){cp=c&15;count=2;min=0x800;}else if(c>=0xf0&&c<=0xf4){cp=c&7;count=3;min=0x10000;}else fail();
    for(int i=0;i<count;++i){if(pos>=input.size())fail();unsigned char n=input[pos++];if((n&0xc0)!=0x80)fail();cp=(cp<<6)|(n&63);}
    if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))fail();codepoint(s,cp);
   }
   if(s.size()>4096)fail();
  }fail();
 }
 Value value(unsigned depth){
  if(depth>16||++nodes>100000)fail();Value v;char c=peek();
  if(c=='{'){
   ++pos;v.kind=Value::Object;if(peek()=='}'){++pos;return v;}
   for(;;){auto key=string();expect(':');auto child=value(depth+1);if(!v.object.emplace(std::move(key),std::move(child)).second)fail();char end=peek();++pos;if(end=='}')return v;if(end!=',')fail();}
  }else if(c=='['){
   ++pos;v.kind=Value::Array;if(peek()==']'){++pos;return v;}
   for(;;){v.array.push_back(value(depth+1));char end=peek();++pos;if(end==']')return v;if(end!=',')fail();}
  }else if(c=='"'){v.kind=Value::String;v.string=string();return v;}
  else if(input.substr(pos,4)=="true"){pos+=4;v.kind=Value::Bool;v.boolean=true;return v;}
  else if(input.substr(pos,5)=="false"){pos+=5;v.kind=Value::Bool;return v;}
  else if(input.substr(pos,4)=="null"){pos+=4;return v;}
  else{
   v.kind=Value::Number;size_t start=pos;if(c=='-')++pos;
   if(pos>=input.size())fail();if(input[pos]=='0')++pos;else{if(input[pos]<'1'||input[pos]>'9')fail();while(pos<input.size()&&input[pos]>='0'&&input[pos]<='9')++pos;}
   if(pos<input.size()&&input[pos]=='.'){++pos;size_t first=pos;while(pos<input.size()&&input[pos]>='0'&&input[pos]<='9')++pos;if(pos==first)fail();}
   if(pos<input.size()&&(input[pos]=='e'||input[pos]=='E')){++pos;if(pos<input.size()&&(input[pos]=='+'||input[pos]=='-'))++pos;size_t first=pos;while(pos<input.size()&&input[pos]>='0'&&input[pos]<='9')++pos;if(pos==first)fail();}
   auto result=std::from_chars(input.data()+start,input.data()+pos,v.number);
   if(result.ec!=std::errc{}||result.ptr!=input.data()+pos||!std::isfinite(v.number))fail();return v;
  }
 }
 public:
 explicit Parser(std::string_view text):input(text){if(text.empty()||text.size()>2*1024*1024)fail();}
 Value parse(){auto result=value(0);ws();if(pos!=input.size())fail();return result;}
};
inline Value parse(std::string_view text){return Parser(text).parse();}
}
