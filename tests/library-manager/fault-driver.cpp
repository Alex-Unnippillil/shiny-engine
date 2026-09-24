// SPDX-License-Identifier: MIT
// Test-only crash/lease driver. Never included in application packages.
#include "builtins.hpp"
#include "client.hpp"
#include <windows.h>
#include <iostream>
int wmain(int argc,wchar_t** argv){
 using namespace shiny::packages;
 if(argc!=5)return 2;
 try{
  fs::path root(argv[1]);auto mode=pathText(argv[2]),value=pathText(argv[3]),point=pathText(argv[4]);
  if(mode=="lease"){VerifiedLease lease(root,value,productionPolicy());std::cout<<"ready\n"<<std::flush;Sleep(15000);return 0;}
  Store store(root,productionPolicy(),[&](std::string_view boundary){if(boundary==point)ExitProcess(86);});
  if(mode=="import")store.importFolder(textPath(value));
  else if(mode=="activate")store.activate(value,probePackage);
  else if(mode=="remove")store.remove(value);
  else if(mode=="lock"){std::cout<<"ready\n"<<std::flush;Sleep(15000);}
  else return 2;
  return 0;
 }catch(const std::exception&){return 3;}
}
