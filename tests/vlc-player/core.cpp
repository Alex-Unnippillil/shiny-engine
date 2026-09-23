#include "core.hpp"
#include <iostream>
using namespace shiny::player;
static int checks=0;
#define CHECK(x) do {++checks; if(!(x)) throw std::runtime_error(#x);} while(0)
template<class F> void rejects(F fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}CHECK(failed);}
int main(){try{
 CHECK(network(L"https://example.test/video.mp4"));CHECK(network(L"udp://@239.0.0.1:1234"));CHECK(!network(L"C:\\Movies\\a.mkv"));CHECK(!network(L"/tmp/movie.mkv"));
 for(auto s:{L"javascript://alert",L"file://C:/x",L"https://",L"https://a/\nx",L"https://a b",L""})rejects([&]{network(s);});
 CHECK(quote(L"C:\\a b\\c.mkv")==L"\"C:\\a b\\c.mkv\"");CHECK(quote(L"C:\\folder\\")==L"\"C:\\folder\\\\\"");CHECK(quote(L"a\"b")==L"\"a\\\"b\"");rejects([]{quote(L"bad\nargument");});
 CHECK(supportedRuntime("3.0.24 Vetinari"));CHECK(supportedRuntime("3.0.25"));for(auto s:{"3.0.23","4.0.0","x","3.0.","999999999.0.0"})CHECK(!supportedRuntime(s));
 Effects e;e.validate();e.gamma=NAN;rejects([&]{e.validate();});e.gamma=4;rejects([&]{e.validate();});LoopRange l;CHECK(!l.target(10000,true,true));rejects([&]{l.end(10);});l.start(1000);rejects([&]{l.end(1050);});l.end(2000);CHECK(l.target(2000,true,true)==1000);CHECK(!l.target(2000,false,true));CHECK(!l.target(2000,true,false));l.clear();CHECK(!l.a);
 Queue q;CHECK(!q.next());CHECK(!q.previous());q.add({L"A",L"A"});q.add({L"B",L"B"});q.add({L"C",L"C"});CHECK(q.next()==0);CHECK(q.choose(0));CHECK(q.next()==1);CHECK(q.previous()==2);CHECK(!q.choose(99));q.choose(2);CHECK(!q.next(true));CHECK(q.next(false)==0);q.repeat=true;CHECK(q.next(true)==2);q.repeat=false;q.shuffle=true;for(int i=0;i<20;++i)CHECK(q.next()!=2);q.erase(0);CHECK(q.selected==1);q.erase(1);CHECK(!q.selected);q.clear();CHECK(q.items.empty());for(size_t i=0;i<maxQueue;++i)q.add({L"A",L"A"});rejects([&]{q.add({L"B",L"B"});});
 CHECK(parseTime(L"1:02:03")==3723000);CHECK(parseTime(L"90")==90000);CHECK(parseTime(L"01:30")==90000);for(auto t:{L"",L"-2",L"1:99:02",L"1:2:3:4",L"1:",L":1",L"9999999",L"nan"})rejects([&]{parseTime(t);});CHECK(clock(0)==L"00:00");CHECK(clock(3661000)==L"1:01:01");CHECK(clock(-1)==L"--:--");std::cout<<"PASS "<<checks<<" native player policy/queue/loop assertions\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
