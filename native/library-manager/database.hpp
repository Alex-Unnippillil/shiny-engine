// SPDX-License-Identifier: MIT
#pragma once
#include "platform.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>
namespace shiny::packages {
class Statement {
 sqlite3_stmt* stmt=nullptr;
 public:
 Statement(sqlite3* db,const char* sql){if(sqlite3_prepare_v2(db,sql,-1,&stmt,nullptr)!=SQLITE_OK)throw std::runtime_error("database-statement-failed");
}
 ~Statement(){sqlite3_finalize(stmt);
}Statement(const Statement&)=delete;
 void text(int n,std::string_view s){if(sqlite3_bind_text(stmt,n,s.empty()?"":s.data(),static_cast<int>(s.size()),SQLITE_TRANSIENT)!=SQLITE_OK)throw std::runtime_error("database-bind-failed");
}
 void integer(int n,std::int64_t v){if(sqlite3_bind_int64(stmt,n,v)!=SQLITE_OK)throw std::runtime_error("database-bind-failed");
}
 bool step(){auto rc=sqlite3_step(stmt);
if(rc==SQLITE_ROW)return true;
if(rc==SQLITE_DONE)return false;
throw std::runtime_error("database-operation-failed");
}
 std::string text(int n) const {auto* s=sqlite3_column_text(stmt,n);
return s?std::string(reinterpret_cast<const char*>(s),static_cast<std::size_t>(sqlite3_column_bytes(stmt,n))):std::string{};
}
 std::int64_t integer(int n)const{return sqlite3_column_int64(stmt,n);
}
 int type(int n)const{return sqlite3_column_type(stmt,n);
}
};
class Database {
 sqlite3* value=nullptr;
 public:
 explicit Database(std::string name=":memory:"){
  int flags=SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX;
#ifdef SQLITE_OPEN_NOFOLLOW
  flags|=SQLITE_OPEN_NOFOLLOW;
#endif
  if(sqlite3_open_v2(name.c_str(),&value,flags,nullptr)!=SQLITE_OK){if(value)sqlite3_close(value);
value=nullptr;
throw std::runtime_error("database-open-failed");
}
  sqlite3_db_config(value,SQLITE_DBCONFIG_DEFENSIVE,1,nullptr);
  sqlite3_db_config(value,SQLITE_DBCONFIG_TRUSTED_SCHEMA,0,nullptr);
  sqlite3_limit(value,SQLITE_LIMIT_LENGTH,1024*1024);
  sqlite3_limit(value,SQLITE_LIMIT_SQL_LENGTH,16384);
  sqlite3_limit(value,SQLITE_LIMIT_EXPR_DEPTH,40);
  sqlite3_busy_timeout(value,1000);
 }
 ~Database(){if(value)sqlite3_close(value);
}Database(const Database&)=delete;
 sqlite3* get()const{return value;
}
 void exec(const char* sql){if(sqlite3_exec(value,sql,nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error("database-operation-failed");
}
};
class Transaction {
 Database& db;
bool committed=false;
 public:
 explicit Transaction(Database& d):db(d){db.exec("BEGIN IMMEDIATE");
}
 ~Transaction(){if(!committed)try{db.exec("ROLLBACK");
}catch(...) {}}
 void commit(){db.exec("COMMIT");
committed=true;
}
};
}
