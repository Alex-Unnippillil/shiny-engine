// SPDX-License-Identifier: MIT
#pragma once
#include <cctype>
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
namespace shiny::player {
inline constexpr size_t maxQueue = 1000;
inline constexpr const char* version = "0.6.0";
inline constexpr const char* dlssStatus = "Native OpenDLSS graph packaged; trained inference unavailable without a reviewed model.";
inline std::wstring lower(std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c)); return s; }
inline void plain(const std::wstring& s) {
 if (s.empty() || s.size() > 32760 || std::any_of(s.begin(),s.end(),[](wchar_t c){return c<32||c==127;}))throw std::invalid_argument("Empty, oversized or control-containing source.");
}
inline bool network(const std::wstring& s) {
 plain(s);const auto p=s.find(L"://");if(p==std::wstring::npos)return false;
 const auto scheme=lower(s.substr(0,p));const std::vector<std::wstring> allowed{L"http",L"https",L"rtsp",L"rtp",L"udp",L"srt"};
 if(std::find(allowed.begin(),allowed.end(),scheme)==allowed.end()||p+3==s.size()||s.find_first_of(L" \t\r\n")!=std::wstring::npos)throw std::invalid_argument("Use an explicit HTTP(S), RTSP, RTP, UDP or SRT media URL, without whitespace.");return true;
}
// CommandLineToArgvW quoting. Never passed to cmd.exe or PowerShell.
inline std::wstring quote(const std::wstring& s) {
 plain(s);std::wstring out=L"\"";size_t slashes=0;
 for(auto c:s){if(c==L'\\'){++slashes;continue;}out.append(slashes*(c==L'\"'?2:1),L'\\');slashes=0;if(c==L'\"')out+=L'\\';out+=c;}
 out.append(slashes*2,L'\\');return out+L'\"';
}
struct Effects {bool enabled=false;float contrast=1,brightness=1,saturation=1,gamma=1;
 void validate()const{for(float n:{contrast,brightness,saturation,gamma})if(!std::isfinite(n)||n<.1f||n>3.f)throw std::invalid_argument("Invalid live adjustment.");}
};
struct LoopRange {
 std::optional<int64_t> a,b;void clear(){a.reset();b.reset();}
 void start(int64_t t){if(t<0)throw std::invalid_argument("No position.");a=t;b.reset();}
 void end(int64_t t){if(!a||t<=*a+100)throw std::invalid_argument("Set B at least 100 ms after A.");b=t;}
 std::optional<int64_t> target(int64_t t,bool playing,bool seekable)const{return playing&&seekable&&a&&b&&t>=*b?a:std::nullopt;}
};
struct Item{std::wstring source,title;bool remote=false;};
class Queue {
 public:
 std::vector<Item> items;std::optional<size_t> selected;bool repeat=false,shuffle=false;std::mt19937 rng{std::random_device{}()};
 void add(Item item){plain(item.source);if(items.size()>=maxQueue)throw std::invalid_argument("Queue limit: 1000 items.");items.push_back(std::move(item));}
 bool choose(size_t i){if(i>=items.size())return false;selected=i;return true;}
 std::optional<size_t> next(bool automatic=false){if(items.empty())return std::nullopt;if(!selected)return size_t{0};if(automatic&&repeat)return selected;
  if(shuffle&&items.size()>1){size_t n=std::uniform_int_distribution<size_t>(0,items.size()-2)(rng);return n>=*selected?n+1:n;}
  if(*selected+1<items.size())return *selected+1;return automatic?std::nullopt:std::optional<size_t>{0};}
 std::optional<size_t> previous()const{return items.empty()?std::nullopt:std::optional<size_t>{!selected||*selected==0?items.size()-1:*selected-1};}
 void clear(){items.clear();selected.reset();}
 void erase(size_t i){if(i>=items.size())return;items.erase(items.begin()+static_cast<ptrdiff_t>(i));if(selected){if(*selected==i)selected.reset();else if(*selected>i)--*selected;}}
};
inline std::wstring clock(int64_t ms){if(ms<0)return L"--:--";auto sec=ms/1000,m=(sec/60)%60,s=sec%60;return (sec>=3600?std::to_wstring(sec/3600)+L":":L"")+(m<10?L"0":L"")+std::to_wstring(m)+L":"+(s<10?L"0":L"")+std::to_wstring(s);}
inline int64_t parseTime(const std::wstring& text){
 if(text.empty()||text.size()>12)throw std::invalid_argument("Use seconds or HH:MM:SS.");uint64_t value=0,part=0;int separators=0;bool digit=false;
 for(auto c:text){if(c==L':'){if(!digit||(separators&&part>=60)||++separators>2)throw std::invalid_argument("Invalid time.");value=value*60+part;part=0;digit=false;}
 else if(c>=L'0'&&c<=L'9'){part=part*10+static_cast<unsigned>(c-L'0');digit=true;if(part>604800)throw std::invalid_argument("Time exceeds seven days.");}else throw std::invalid_argument("Time must contain digits and colons only.");}
 if(!digit||(separators&&part>=60))throw std::invalid_argument("Invalid seconds.");value=value*60+part;if(value>604800)throw std::invalid_argument("Time exceeds seven days.");return static_cast<int64_t>(value)*1000;
}
inline bool supportedRuntime(const std::string& value){
 int major=0,minor=0,patch=0;size_t pos=0;
 for(int* field:{&major,&minor,&patch}){if(pos>=value.size()||!std::isdigit(static_cast<unsigned char>(value[pos])))return false;while(pos<value.size()&&std::isdigit(static_cast<unsigned char>(value[pos]))){*field=*field*10+(value[pos++]-'0');if(*field>1000)return false;}if(field!=&patch&&(pos>=value.size()||value[pos++]!='.'))return false;}
 return major==3&&minor==0&&patch>=24;
}
}
