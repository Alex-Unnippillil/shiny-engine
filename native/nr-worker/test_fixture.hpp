// SPDX-License-Identifier: MIT
#pragma once
// Synthetic zero-data model layout for tests only; NOT trained weights.
#include "manifest.hpp"
#include <sstream>
namespace shiny::nrpolicy::testing {
std::string fixture(){auto tensors=requiredTensors();uint64_t total=0;for(auto [n,b]:tensors)total+=b;std::ostringstream out;out<<"{\"totals\":{\"blockCount\":71},\"stages\":[{\"id\":\"all\",\"file\":\"stage.bin\",\"sha256\":\""<<std::string(64,'0')<<"\",\"packedByteLength\":"<<total<<"}],\"tensors\":[";uint64_t offset=0;bool first=true;
 for(auto [name,bytes]:tensors){int b=std::stoi(name.substr(5)),l=std::stoi(name.substr(name.find(".layer")+6));out<<(first?"":",")<<"{\"name\":\""<<name<<"\",\"block\":"<<b<<",\"layer\":"<<l<<",\"parameter\":\"layer\",\"stage\":\"all\",\"stageOffset\":"<<offset<<",\"byteLength\":"<<bytes<<"}";offset+=bytes;first=false;}out<<"]}";return out.str();}
}
