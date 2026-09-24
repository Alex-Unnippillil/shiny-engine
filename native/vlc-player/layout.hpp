// SPDX-License-Identifier: MIT
// Platform-independent logical-pixel layout; Win32 converts to the monitor DPI.
#pragma once
#include <algorithm>
#include <array>
#include <vector>
namespace shiny::player {
struct Box{int x=0,y=0,w=0,h=0;};
inline std::vector<Box> flow(const std::vector<int>& widths,int width,int y,int height){
 std::vector<Box> out;int x=20;for(int w:widths){if(x+w>width-20&&x>20){x=20;y+=height+8;}out.push_back({x,y,std::min(w,width-40),height});x+=w+8;}return out;
}
struct WorkspaceLayout {
 std::vector<Box> toolbar,transport;
 Box original,comparison,time,seek,status,sidebar;
 bool showSidebar=false;
};
inline WorkspaceLayout workspaceLayout(int width,int height,bool cinema,bool compare){
 width=std::max(640,width);height=std::max(500,height);WorkspaceLayout a;
 a.toolbar=flow({120,138,134,100,174,122,120},width,78,36);
 a.transport=flow({88,60,82,60,96,74,32,32,84,74,128},width,0,34);
 const int rows=a.transport.back().y/42+1,transportTop=height-32-rows*42;
 for(auto& box:a.transport)box.y+=transportTop;
 a.status={20,height-27,width-40,23};a.seek={12,transportTop-32,width-24,26};a.time={20,transportTop-61,width-40,24};
 const int top=a.toolbar.back().y+64,bottom=a.time.y-14,viewHeight=std::max(120,bottom-top);
 a.showSidebar=!cinema&&width>=1000&&viewHeight>=320;
 const int side=a.showSidebar?288:0,viewWidth=width-40-side;
 a.original={20,top,compare?(viewWidth-12)/2:viewWidth,viewHeight};
 a.comparison={a.original.x+a.original.w+12,top,compare?viewWidth-a.original.w-12:0,viewHeight};
 a.sidebar={width-288,top,268,viewHeight};return a;
}
}
