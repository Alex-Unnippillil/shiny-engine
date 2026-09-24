// SPDX-License-Identifier: MIT
#include "package.hpp"
#include "catalog.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace shiny::packages {
namespace {
struct Value {std::string type,text;
std::int64_t number=0;
};
using Object=std::map<std::string,Value>;
Object object(Database& db,std::string_view raw,std::initializer_list<std::string_view> expected){
 Statement q(db.get(),"SELECT key,type,value FROM json_each(?)");
q.text(1,raw);
Object out;
 while(q.step()){
  auto key=q.text(0);
if(!out.emplace(key,Value{q.text(1),q.text(2),q.integer(2)}).second)throw std::runtime_error("duplicate-manifest-key");
 }
 if(out.size()!=expected.size())throw std::runtime_error("manifest-schema-mismatch");
 for(auto key:expected)if(!out.contains(std::string(key)))throw std::runtime_error("manifest-schema-mismatch");
return out;
}
std::string field(const Object& o,const char* key,std::string_view allowed,std::size_t max=96){
 auto& v=o.at(key);
if(v.type!="text"||v.text.empty()||v.text.size()>max||v.text.find_first_not_of(allowed)!=v.text.npos)throw std::runtime_error("invalid-manifest-text");
return v.text;
}
std::int64_t number(const Object& o,const char* key,std::int64_t low,std::int64_t high){auto& v=o.at(key);
if(v.type!="integer"||v.number<low||v.number>high)throw std::runtime_error("invalid-manifest-number");
return v.number;
}
constexpr std::string_view safe="abcdefghijklmnopqrstuvwxyz0123456789-._";
}
Manifest parseManifest(std::string raw){
 if(raw.empty()||raw.size()>maxManifest||raw.find('\0')!=raw.npos)throw std::runtime_error("manifest-size-or-encoding");
 // Schema identifiers are ASCII. JSON escape sequences still go through SQLite.
 for(unsigned char c:raw)if(c>127)throw std::runtime_error("manifest-ascii-required");
 Database db;
Statement valid(db.get(),"SELECT json_valid(?)");
valid.text(1,raw);
if(!valid.step()||valid.integer(0)!=1)throw std::runtime_error("invalid-json");
 auto o=object(db,raw,{"schemaVersion","id","version","family","architecture","abi","license","source","dependencySet","files"});
 if(number(o,"schemaVersion",1,1)!=1)throw std::runtime_error("unsupported-schema");
 Manifest m;
m.raw=std::move(raw);
m.sha256=digest(m.raw);
m.id=field(o,"id",safe);
m.version=field(o,"version",safe,32);
m.family=field(o,"family",safe);
 m.architecture=field(o,"architecture",safe,16);
m.abi=static_cast<int>(number(o,"abi",0,16));
 m.license=field(o,"license","ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._",64);
 m.source=field(o,"source","ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._:/",200);
 m.dependencySet=field(o,"dependencySet",safe);
 if(o.at("files").type!="array")throw std::runtime_error("manifest-files-required");
 Statement files(db.get(),"SELECT type,value FROM json_each(?)");
files.text(1,o.at("files").text);
std::set<std::string> names;
std::uint64_t total=0;
 while(files.step()){
  if(m.files.size()>=16||files.text(0)!="object")throw std::runtime_error("manifest-file-count");
  auto f=object(db,files.text(1),{"name","sha256","size"});
Component c;
c.name=field(f,"name",safe,80);
c.sha256=field(f,"sha256","0123456789abcdef",64);
  if(!hashId(c.sha256)||!names.insert(c.name).second)throw std::runtime_error("duplicate-or-invalid-component");
  auto* slot=libraries::candidateSlot(c.name);
  if(c.name!="shiny_spatial.dll"&&(!slot||!slot->dll))throw std::runtime_error("unrecognized-component-name");
  c.size=static_cast<std::uint64_t>(number(f,"size",1,static_cast<std::int64_t>(maxComponent)));
total+=c.size;
  if(total>maxPackage)throw std::runtime_error("package-size-limit");
m.files.push_back(std::move(c));
 }
 if(m.files.empty())throw std::runtime_error("empty-package");
return m;
}
bool Policy::approved(const Manifest& m)const{return std::find(approvedDigests.begin(),approvedDigests.end(),m.sha256)!=approvedDigests.end();
}
std::string Policy::blockReason(const Manifest& m)const{
 if(!approved(m))return "not-in-build-pinned-catalog";
 if(m.family!="shiny-spatial-reference"||m.id!="shiny-spatial"||m.abi!=1||m.files.size()!=1||m.files[0].name!="shiny_spatial.dll")return "no-compatible-backend";
 if(m.architecture!=hostArchitecture()||m.architecture!="x64")return "architecture-mismatch";
 return {};
}
Snapshot::Snapshot(const fs::path& folder,std::stop_token stop):directory(folder){
 FilePin sourceManifest(folder/"manifest.json");
auto bytes=sourceManifest.read(maxManifest,stop);
 manifest=parseManifest(std::string(bytes.begin(),bytes.end()));
pins.push_back(std::move(sourceManifest));
 std::set<std::string> expected{"manifest.json"};
for(auto& c:manifest.files)expected.insert(c.name);
 std::set<std::string> actual;
 for(auto& e:fs::directory_iterator(folder)){
  cancelled(stop);
auto name=pathText(e.path().filename());
if(!expected.contains(name)||!actual.insert(name).second)throw std::runtime_error("unexpected-package-contents");
 }
 if(actual!=expected)throw std::runtime_error("incomplete-package");
 for(auto& c:manifest.files){
  FilePin pin(folder/textPath(c.name));
auto data=pin.read(maxComponent,stop);
  if(data.size()!=c.size||digest(data)!=c.sha256)throw std::runtime_error("component-integrity-mismatch");
  auto pe=libraries::inspectPe(data);
if(!pe.dll||pe.architecture!=manifest.architecture)throw std::runtime_error("component-architecture-or-kind-mismatch");
  contents.push_back(std::move(data));
pins.push_back(std::move(pin));
 }
}
std::string quarantineManifest(std::string name,const Bytes& bytes){
 name=libraries::lowerAscii(std::move(name));
auto* slot=libraries::candidateSlot(name);
 if(!slot||!slot->dll||bytes.empty()||bytes.size()>maxComponent)throw std::runtime_error("unrecognized-or-oversized-library");
 auto pe=libraries::inspectPe(bytes);
if(!pe.dll)throw std::runtime_error("dll-required");
 auto q=libraries::jsonString;
 return "{\"schemaVersion\":1,\"id\":\"local-inspection\",\"version\":\"unverified\",\"family\":"+q(libraries::familyName(slot->family))+
 ",\"architecture\":"+q(pe.architecture)+",\"abi\":0,\"license\":\"Unknown\",\"source\":\"local-user-selection\",\"dependencySet\":\"unapproved\",\"files\":[{\"name\":"+q(name)+",\"sha256\":"+q(digest(bytes))+",\"size\":"+std::to_string(bytes.size())+"}]}";
}
}
