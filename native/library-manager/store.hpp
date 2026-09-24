// SPDX-License-Identifier: MIT
#pragma once
#include "package.hpp"
namespace shiny::packages {
using Probe = std::function<void(const fs::path&,const std::string&,std::stop_token)>;
using Fault = std::function<void(std::string_view)>; // Test injection is not exposed by production CLI/IPC.
class Store {
 fs::path root;Policy policy;Fault fault;
 std::unique_ptr<DirectoryPin> rootPin;
 std::unique_ptr<StoreLock> lock;
 std::vector<DirectoryPin> directories;
 std::unique_ptr<Database> database;
 void event(std::string_view id,std::string_view action);
 void point(std::string_view name)const;
 void recover();
 std::string setting(const char* key)const;
 void set(const char* key,std::string_view value);
 Manifest manifest(std::string_view id)const;
 std::string state(std::string_view id)const;
 std::string install(const Manifest&,const std::vector<Bytes>&,std::stop_token);
 void choose(const std::string&,const Probe&,std::stop_token,bool rollback);
 public:
 Store(fs::path directory,Policy trust,Fault injection={});
 ~Store();Store(const Store&)=delete;
 const fs::path& path()const{return root;}
 std::string importFolder(const fs::path&,std::stop_token stop={});
 std::string quarantineFile(const fs::path&,std::stop_token stop={});
 std::string verify(const std::string&,std::stop_token stop={});
 void stage(const std::string&,std::stop_token stop={});
 void activate(const std::string&,const Probe&,std::stop_token stop={});
 void rollback(const Probe&,std::stop_token stop={});
 void original();
 void remove(const std::string&);
 std::string selected()const;
 std::string list()const;
 std::string history()const;
};
// Independent of the database: a worker never accepts cached approval/status as authority.
struct VerifiedLease {
 std::unique_ptr<PackageLease> lease;std::unique_ptr<Snapshot> snapshot;
 VerifiedLease(const fs::path& root,const std::string& id,const Policy& policy,std::stop_token stop={});
};
}
