// SPDX-License-Identifier: MIT
#include "workspace_model.hpp"
#include <iostream>
#include <stdexcept>
using namespace shiny::player;
int main(){try{
 int count=0;auto check=[&](bool value){++count;if(!value)throw std::runtime_error("workspace assertion "+std::to_string(count));};
 Queue q;q.add({L"C:/private/password/one.mp4",L"First Film.mp4",false});q.add({L"C:/different/two.mp4",L"Second Film.mp4",false});q.add({L"https://secret.invalid/?token=hidden",L"Network stream",true});q.choose(0);
 check(queueMatches(q,L"")==std::vector<std::size_t>({0,1,2}));
 check(queueMatches(q,L"  FILM  second ")==std::vector<std::size_t>({1}));
 check(queueMatches(q,L"password").empty());check(queueMatches(q,L"token").empty());
 check(queueMatches(q,L"\t\n").size()==3);check(queueMatches(q,L"not a title").empty());
 auto filtered=queueMatches(q,L"second");check(mappedQueueIndex(filtered,0,q.items.size())==1);
 check(!mappedQueueIndex(filtered,-1,q.items.size()));check(!mappedQueueIndex(filtered,1,q.items.size()));
 check(!mappedQueueIndex({99},0,q.items.size()));
 q.erase(*mappedQueueIndex(filtered,0,q.items.size()));check(q.items[0].title==L"First Film.mp4"&&q.items[1].remote&&q.selected==0);
 check(matchesTitle(L"énergie 🎬",L"🎬"));check(matchesTitle(L"Queue workspace",L"workspace queue"));
 check(!matchesTitle(L"Play",L"player"));check(matchesTitle(L"Open media",L"open"));
 for(int i=0;i<997;++i)q.add({L"local",L"Item "+std::to_wstring(i),false});
 auto all=queueMatches(q,L"");check(all.size()==999);check(mappedQueueIndex(all,998,q.items.size())==998);
 q.clear();check(queueMatches(q,L"").empty()&&!q.selected);
 std::cout<<count<<" workspace checks: multiword title filtering, index mapping, privacy and bounds passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
