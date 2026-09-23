// SPDX-License-Identifier: MIT
#include "layout.hpp"
#include <iostream>
#include <stdexcept>
using namespace shiny::player;
int assertions=0;
void check(bool value){++assertions;if(!value)throw std::runtime_error("layout invariant failed");}
bool disjoint(Box a,Box b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;}
void bounds(Box b,int w,int h){check(b.x>=0&&b.y>=0&&b.w>0&&b.h>0&&b.x+b.w<=w&&b.y+b.h<=h);}
int main(){try{
 for(int w:{640,720,800,900,999,1000,1100,1280,1440,1920,2560})for(int h:{500,600,720,800,1080,1440})for(bool cinema:{false,true})for(bool compare:{false,true}){
  auto a=workspaceLayout(w,h,cinema,compare);
  check(a.toolbar.size()==7&&a.transport.size()==11);check(!cinema||!a.showSidebar);
  std::vector<Box> boxes=a.toolbar;boxes.insert(boxes.end(),a.transport.begin(),a.transport.end());
  boxes.insert(boxes.end(),{a.original,a.time,a.seek,a.status});if(compare)boxes.push_back(a.comparison);if(a.showSidebar)boxes.push_back(a.sidebar);
  for(auto box:boxes)bounds(box,w,h);
  for(size_t i=0;i<boxes.size();++i)for(size_t j=i+1;j<boxes.size();++j)check(disjoint(boxes[i],boxes[j]));
  if(compare)check(a.original.h==a.comparison.h&&a.original.y==a.comparison.y&&a.comparison.w>0);
 }
 std::cout<<"PASS "<<assertions<<" layout invariants across 264 size/mode combinations (not native rendering tests)\n";
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<" after "<<assertions<<" checks\n";return 1;}}
