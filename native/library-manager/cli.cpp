// SPDX-License-Identifier: MIT
#include "client.hpp"
#include "builtins.hpp"
#include "catalog.hpp"
#include <iostream>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace shiny::packages;
int run(const std::vector<std::string>& args){try{
 if(args.size()==1&&args[0]=="--help"){
  std::cout<<"Shiny Library Manager 0.2 (local, unsigned)\n"
   "[--store-dir ABSOLUTE_DIRECTORY] COMMAND [ARGUMENT]\n"
   "--list | --history | --recover | --import-bundled | --rollback | --original\n"
   "--import-folder DIRECTORY | --quarantine-file DLL\n"
   "--verify DIGEST | --stage DIGEST | --activate DIGEST | --remove DIGEST | --probe DIGEST\n"
   "Only exact-build first-party spatial reference packages can run. Not DLSS.\n"
   "Activation selects the next independent SDR preview; it does not replace VLC.\n"
   "Exit 0: completed; inspect blockReason. Exit 2: error. No automatic downloads.\n";
return 0;
 }
 std::size_t at=0;
fs::path root;
 if(args.size()>=2&&args[0]=="--store-dir"){root=textPath(args[1]);
at=2;
}
 if(at>=args.size())throw std::runtime_error("usage-see-help");
auto command=args[at++];
 const std::set<std::string> simple{"--list","--history","--recover","--import-bundled","--rollback","--original"};
 const std::set<std::string> operand{"--import-folder","--quarantine-file","--verify","--stage","--activate","--remove","--probe"};
 bool one=operand.contains(command);
 if((!one&&!simple.contains(command))||args.size()!=at+(one?1:0))throw std::runtime_error("usage-see-help");
 if(root.empty())root=defaultStore();
Store store(root,productionPolicy());
auto arg=one?args[at]:std::string{};
 if(command=="--list"||command=="--recover")std::cout<<store.list();
 else if(command=="--history")std::cout<<store.history();
 else if(command=="--import-folder"||command=="--quarantine-file"){
  auto id=command=="--import-folder"?store.importFolder(textPath(arg)):store.quarantineFile(textPath(arg));
std::cout<<"{\"digest\":"<<shiny::libraries::jsonString(id)<<",\"state\":\"quarantined-or-already-present\",\"active\":false}";
 }else if(command=="--import-bundled"){
  for(auto version:{"1.0.0","1.1.0"})store.importFolder(executableDirectory()/"library-bundles"/version);
std::cout<<store.list();
 }else if(command=="--verify"){
  auto reason=store.verify(arg);
std::cout<<"{\"integrityVerified\":true,\"active\":false,\"blockReason\":"<<shiny::libraries::jsonString(reason)<<"}";
 }else if(command=="--probe"){
  auto reason=store.verify(arg);
if(!reason.empty())throw std::runtime_error(reason);
probePackage(root,arg);
std::cout<<"{\"syntheticFrameProbePassed\":true,\"activePlayback\":false,\"dlss\":false}";
 }else{
  if(command=="--stage")store.stage(arg);
else if(command=="--activate")store.activate(arg,probePackage);
  else if(command=="--rollback")store.rollback(probePackage);
else if(command=="--original")store.original();
else store.remove(arg);
  std::cout<<store.list();
 }std::cout<<'\n';
return std::cout?0:2;
 }catch(const fs::filesystem_error&){std::cerr<<"{\"error\":\"filesystem-operation-failed\"}\n";
}
 catch(const std::runtime_error& e){std::cerr<<"{\"error\":"<<shiny::libraries::jsonString(e.what())<<"}\n";
}
 catch(...){std::cerr<<"{\"error\":\"operation-failed\"}\n";
}return 2;
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
std::vector<std::string> args;
for(int i=1;i<argc;++i)args.push_back(pathText(fs::path(argv[i])));
return run(args);
}
#else
int main(int argc,char** argv){return run(std::vector<std::string>(argv+1,argv+argc));
}
#endif
