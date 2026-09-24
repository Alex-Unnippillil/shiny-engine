// SPDX-License-Identifier: MIT
#include "store.hpp"
#include "catalog.hpp"
#include <set>
namespace shiny::packages {
namespace {
constexpr std::string_view marker="Shiny library store v1\n";
void idCheck(std::string_view id){if(!hashId(id))throw std::runtime_error("invalid-package-id");
}
std::string readText(const fs::path& p,std::size_t limit){auto b=FilePin(p).read(limit);
return {b.begin(),b.end()};
}
void removePending(Database& db,std::string_view kind,std::string_view id){Statement q(db.get(),"DELETE FROM pending WHERE kind=? AND digest=?");
q.text(1,kind);
q.text(2,id);
q.step();
}
}
Store::Store(fs::path directory,Policy trust,Fault injection):root(std::move(directory)),policy(std::move(trust)),fault(std::move(injection)){
 validateLocal(root);
makeDirectory(root);
rootPin=std::make_unique<DirectoryPin>(root);
 // Stable directory identity, rather than path spelling, serializes Windows aliases.
 lock=std::make_unique<StoreLock>(root);
 if(!fs::exists(root/"store.marker")){
  for(auto& e:fs::directory_iterator(root))if(e.path().filename()!="store.lock")throw std::runtime_error("nonempty-unmanaged-store");
  writeNew(root/"store.marker",marker);
 }else if(readText(root/"store.marker",128)!=marker)throw std::runtime_error("unrecognized-store");
 for(auto name:{"packages","staging","leases"}){makeDirectory(root/name);
directories.emplace_back(root/name);
}
 // Reject redirection/hardlinks before SQLite opens its own writable handle.
 for(auto name:{"catalog.sqlite","catalog.sqlite-journal","catalog.sqlite-wal","catalog.sqlite-shm"})if(fs::exists(root/name)){FilePin check(root/name);
}
 database=std::make_unique<Database>(pathText(root/"catalog.sqlite"));
 database->exec("PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL; PRAGMA foreign_keys=ON; PRAGMA max_page_count=8192;");
 Statement version(database->get(),"PRAGMA user_version");
if(!version.step())throw std::runtime_error("database-version-unavailable");
auto v=version.integer(0);
 if(v!=0&&v!=1)throw std::runtime_error("unsupported-store-schema");
 if(v==0){
  Transaction t(*database);
  database->exec("CREATE TABLE packages(digest TEXT PRIMARY KEY,manifest TEXT NOT NULL,state TEXT NOT NULL);"
   "CREATE TABLE settings(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
   "CREATE TABLE pending(kind TEXT NOT NULL,digest TEXT NOT NULL,manifest TEXT NOT NULL,PRIMARY KEY(kind,digest));"
   "CREATE TABLE history(seq INTEGER PRIMARY KEY AUTOINCREMENT,digest TEXT NOT NULL,event TEXT NOT NULL,at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);"
   "PRAGMA user_version=1;");
t.commit();
 }
 Statement integrity(database->get(),"PRAGMA quick_check");
if(!integrity.step()||integrity.text(0)!="ok")throw std::runtime_error("catalog-corrupt");
 recover();
}
Store::~Store()=default;
void Store::point(std::string_view name)const{if(fault)fault(name);
}
void Store::event(std::string_view id,std::string_view action){
 Statement q(database->get(),"INSERT INTO history(digest,event) VALUES(?,?)");
q.text(1,id);
q.text(2,action);
q.step();
 database->exec("DELETE FROM history WHERE seq < (SELECT COALESCE(MAX(seq),0)-4095 FROM history)");
}
std::string Store::setting(const char* key)const{Statement q(database->get(),"SELECT value FROM settings WHERE key=?");
q.text(1,key);
if(!q.step())return {};
auto s=q.text(0);
if(!s.empty())idCheck(s);
return s;
}
void Store::set(const char* key,std::string_view value){Statement q(database->get(),"INSERT INTO settings(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value");
q.text(1,key);
q.text(2,value);
q.step();
}
std::string Store::selected()const{return setting("selected");
}
Manifest Store::manifest(std::string_view id)const{
 idCheck(id);
Statement q(database->get(),"SELECT manifest FROM packages WHERE digest=?");
q.text(1,id);
if(!q.step())throw std::runtime_error("package-not-found");
auto m=parseManifest(q.text(0));
if(m.sha256!=id)throw std::runtime_error("catalog-manifest-mismatch");
return m;
}
std::string Store::state(std::string_view id)const{Statement q(database->get(),"SELECT state FROM packages WHERE digest=?");
q.text(1,id);
if(!q.step())throw std::runtime_error("package-not-found");
return q.text(0);
}
void Store::recover(){
 struct Pending{std::string kind,id,raw;
};
std::vector<Pending> pending;
 {Statement q(database->get(),"SELECT kind,digest,manifest FROM pending");
while(q.step()){if(pending.size()>=64)throw std::runtime_error("recovery-count-limit");
pending.push_back({q.text(0),q.text(1),q.text(2)});
}}
 for(auto& p:pending){
  idCheck(p.id);
  if(p.kind=="import"){
   auto m=parseManifest(p.raw);
if(m.sha256!=p.id)throw std::runtime_error("recovery-manifest-mismatch");
   auto staged=root/"staging"/p.id,complete=root/"packages"/p.id;
   if(fs::exists(complete)){
    Snapshot snapshot(complete);
if(snapshot.manifest.sha256!=p.id)throw std::runtime_error("recovery-integrity-mismatch");
    Transaction t(*database);
Statement add(database->get(),"INSERT OR IGNORE INTO packages VALUES(?,?,'quarantined')");
add.text(1,p.id);
add.text(2,p.raw);
add.step();
removePending(*database,p.kind,p.id);
event(p.id,"import-recovered");
t.commit();
   }else{
    if(fs::exists(staged))eraseFlat(staged);
    Transaction t(*database);
removePending(*database,p.kind,p.id);
event(p.id,"incomplete-import-removed");
t.commit();
   }
  }else if(p.kind=="select"){
   // Selection and removal of this journal entry commit in the SAME SQLite transaction.
   // A surviving journal therefore means the old selection is still authoritative.
   Transaction t(*database);
removePending(*database,p.kind,p.id);
event(p.id,"selection-recovered-previous-preserved");
t.commit();
  }else if(p.kind=="delete"){
   if(selected()==p.id||setting("previous")==p.id)throw std::runtime_error("recovery-protected-package");
   PackageLease lease(root/"leases"/(p.id+".lock"),true);
   auto folder=root/"packages"/p.id;
if(fs::exists(folder))eraseFlat(folder);
   Transaction t(*database);
Statement q(database->get(),"DELETE FROM packages WHERE digest=?");
q.text(1,p.id);
q.step();
removePending(*database,p.kind,p.id);
event(p.id,"delete-recovered");
t.commit();
  }else throw std::runtime_error("unknown-recovery-operation");
 }
}
std::string Store::install(const Manifest& m,const std::vector<Bytes>& contents,std::stop_token stop){
 cancelled(stop);
auto target=root/"packages"/m.sha256;
 {Statement existing(database->get(),"SELECT digest FROM packages WHERE digest=?");
existing.text(1,m.sha256);
if(existing.step()){Snapshot check(target,stop);
if(check.manifest.sha256!=m.sha256)throw std::runtime_error("stored-integrity-mismatch");
return m.sha256;
}}
 std::uint64_t total=0;
for(auto& f:m.files)total+=f.size;
std::size_t count=0;
 {Statement q(database->get(),"SELECT manifest FROM packages");
while(q.step()){if(++count>=64)throw std::runtime_error("store-package-limit");
for(auto& f:parseManifest(q.text(0)).files)total+=f.size;
}}
 if(total>2ull*1024*1024*1024)throw std::runtime_error("store-size-limit");
 if(contents.size()!=m.files.size())throw std::runtime_error("component-count-mismatch");
 {Transaction t(*database);
Statement q(database->get(),"INSERT INTO pending VALUES('import',?,?)");
q.text(1,m.sha256);
q.text(2,m.raw);
q.step();
event(m.sha256,"import-started");
t.commit();
}
 point("import-journal");
auto staging=root/"staging"/m.sha256;
 if(fs::exists(staging)||fs::exists(target))throw std::runtime_error("untracked-package-directory");
makeDirectory(staging);
point("import-directory");
 writeNew(staging/"manifest.json",m.raw,stop);
point("import-manifest");
 for(std::size_t i=0;i<contents.size();++i){
  if(contents[i].size()!=m.files[i].size||digest(contents[i])!=m.files[i].sha256)throw std::runtime_error("component-integrity-mismatch");
  writeNew(staging/textPath(m.files[i].name),contents[i],stop);
point("import-component");
 }
 {Snapshot verified(staging,stop);
if(verified.manifest.sha256!=m.sha256)throw std::runtime_error("staging-integrity-mismatch");
}
 cancelled(stop);
promote(staging,target);
point("import-promoted");
 {Transaction t(*database);
Statement q(database->get(),"INSERT INTO packages VALUES(?,?,'quarantined')");
q.text(1,m.sha256);
q.text(2,m.raw);
q.step();
removePending(*database,"import",m.sha256);
event(m.sha256,"quarantined");
t.commit();
}
 point("import-committed");
return m.sha256;
}
std::string Store::importFolder(const fs::path& folder,std::stop_token stop){Snapshot snapshot(folder,stop);
return install(snapshot.manifest,snapshot.contents,stop);
}
std::string Store::quarantineFile(const fs::path& file,std::stop_token stop){
 FilePin source(file);
auto bytes=source.read(maxComponent,stop);
auto raw=quarantineManifest(pathText(file.filename()),bytes);
return install(parseManifest(raw),{bytes},stop);
}
std::string Store::verify(const std::string& id,std::stop_token stop){
 auto m=manifest(id);
Snapshot actual(root/"packages"/id,stop);
if(actual.manifest.sha256!=id)throw std::runtime_error("stored-integrity-mismatch");
 auto reason=policy.blockReason(m);
auto next=reason.empty()?"verified":"blocked";
 auto current=state(id);
if(reason.empty()&&(current=="staged"||current=="selected"))next=current.c_str();
 Transaction t(*database);
Statement q(database->get(),"UPDATE packages SET state=? WHERE digest=?");
q.text(1,next);
q.text(2,id);
q.step();
event(id,reason.empty()?"integrity-and-build-policy-verified":"integrity-verified-activation-blocked");
t.commit();
return reason;
}
void Store::stage(const std::string& id,std::stop_token stop){
 auto reason=verify(id,stop);
if(!reason.empty())throw std::runtime_error(reason);
 if(selected()==id)return;
 Transaction t(*database);
Statement q(database->get(),"UPDATE packages SET state='staged' WHERE digest=?");
q.text(1,id);
q.step();
event(id,"staged");
t.commit();
}
void Store::choose(const std::string& id,const Probe& probe,std::stop_token stop,bool rollbackAction){
 auto reason=verify(id,stop);
if(!reason.empty())throw std::runtime_error(reason);
 auto current=state(id);
if(current!="staged"&&current!="selected")throw std::runtime_error("stage-package-first");
 auto old=selected();
if(old==id)return;
 // Leases survive into the probe; every worker independently verifies the same bytes.
 VerifiedLease lease(root,id,policy,stop);
 {Transaction t(*database);
Statement q(database->get(),"INSERT INTO pending VALUES('select',?,'')");
q.text(1,id);
q.step();
event(id,"selection-pending");
t.commit();
}
 point("selection-pending");
 try{cancelled(stop);
if(!probe)throw std::runtime_error("backend-probe-unavailable");
probe(root,id,stop);
cancelled(stop);
}
 catch(...){Transaction t(*database);
removePending(*database,"select",id);
event(id,"activation-failed-previous-preserved");
t.commit();
throw;
}
 point("selection-probed");
 {Transaction t(*database);
database->exec("UPDATE packages SET state='staged' WHERE state='selected'");
  set("selected",id);
set("previous",rollbackAction?std::string_view{}:std::string_view(old));
  Statement q(database->get(),"UPDATE packages SET state='selected' WHERE digest=?");
q.text(1,id);
q.step();
removePending(*database,"select",id);
event(id,rollbackAction?"rolled-back":"selected-for-next-preview");
t.commit();
}
 point("selection-committed");
}
void Store::activate(const std::string& id,const Probe& probe,std::stop_token stop){choose(id,probe,stop,false);
}
void Store::rollback(const Probe& probe,std::stop_token stop){auto previous=setting("previous");
if(previous.empty())return;
choose(previous,probe,stop,true);
}
void Store::original(){
 auto old=selected();
if(old.empty())return;
Transaction t(*database);
set("previous",old);
set("selected","");
database->exec("UPDATE packages SET state='staged' WHERE state='selected'");
event(old,"original-selected");
t.commit();
}
void Store::remove(const std::string& id){
 (void)manifest(id);
if(selected()==id||setting("previous")==id)throw std::runtime_error("selected-or-rollback-package-protected");
 PackageLease lease(root/"leases"/(id+".lock"),true);
 {Transaction t(*database);
Statement q(database->get(),"INSERT INTO pending VALUES('delete',?,'')");
q.text(1,id);
q.step();
event(id,"delete-pending");
t.commit();
}
 point("delete-pending");
auto folder=root/"packages"/id;
if(fs::exists(folder))eraseFlat(folder);
point("delete-files");
 {Transaction t(*database);
Statement q(database->get(),"DELETE FROM packages WHERE digest=?");
q.text(1,id);
q.step();
removePending(*database,"delete",id);
event(id,"removed");
t.commit();
}
}
std::string Store::list()const{
 auto q=libraries::jsonString;
std::string out="{\"schemaVersion\":1,\"selected\":"+q(selected())+",\"previous\":"+q(setting("previous"))+
 ",\"active\":false,\"scope\":\"selection-for-next-independent-sdr-preview\",\"dlssActivationSupported\":false,\"packages\":[";
 Statement rows(database->get(),"SELECT digest,manifest,state FROM packages ORDER BY digest");
bool first=true;
std::size_t count=0;
 while(rows.step()){
  if(++count>64)throw std::runtime_error("catalog-count-limit");
auto m=parseManifest(rows.text(1));
if(m.sha256!=rows.text(0))throw std::runtime_error("catalog-manifest-mismatch");
  if(!first)out+=',';
first=false;
auto reason=policy.blockReason(m);
  out+="{\"digest\":"+q(m.sha256)+",\"id\":"+q(m.id)+",\"version\":"+q(m.version)+",\"family\":"+q(m.family)+",\"architecture\":"+q(m.architecture)+
   ",\"state\":"+q(rows.text(2))+",\"integrity\":\"rechecked-on-every-stage-and-launch\",\"buildCatalogApproved\":"+(policy.approved(m)?"true":"false")+
   ",\"backendCompatible\":"+(reason.empty()?"true":"false")+",\"publisherPolicy\":\"exact-build-pinned-digest-not-authenticode-approval\",\"active\":false,\"blockReason\":"+q(reason)+"}";
 }return out+"]}";
}
std::string Store::history()const{
 auto quote=libraries::jsonString;
std::string out="{\"schemaVersion\":1,\"history\":[";
Statement q(database->get(),"SELECT seq,digest,event,at FROM history ORDER BY seq DESC LIMIT 256");
bool first=true;
 while(q.step()){if(!first)out+=',';
first=false;
out+="{\"sequence\":"+std::to_string(q.integer(0))+",\"digest\":"+quote(q.text(1))+",\"event\":"+quote(q.text(2))+",\"at\":"+quote(q.text(3))+"}";
}return out+"]}";
}
VerifiedLease::VerifiedLease(const fs::path& root,const std::string& id,const Policy& policy,std::stop_token stop){
 idCheck(id);
DirectoryPin storage(root);
if(readText(root/"store.marker",128)!=marker)throw std::runtime_error("unrecognized-store");
 lease=std::make_unique<PackageLease>(root/"leases"/(id+".lock"),false);
snapshot=std::make_unique<Snapshot>(root/"packages"/id,stop);
 if(snapshot->manifest.sha256!=id)throw std::runtime_error("selected-manifest-mismatch");
auto reason=policy.blockReason(snapshot->manifest);
if(!reason.empty())throw std::runtime_error(reason);
}
}
