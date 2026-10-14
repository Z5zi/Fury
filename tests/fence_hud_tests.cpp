#include "../apps/vaultline/fence_hud.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
int checks=0; std::size_t most_glyphs=0; float narrowest_gap=1000;
void need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
struct Rect {float x,y,w,h;fury::Color c;};
struct Recorder {std::vector<Rect> rects;void draw_hud_rect(float x,float y,float w,float h,fury::Color c){rects.push_back({x,y,w,h,c});}};
float measured(std::u32string_view text) {
 Recorder r;std::size_t budget=768;vaultline::draw_npc_hud_text(r,0,0,text,{255,255,255,255},1,budget);
 need(budget==768-text.size(),"All glyphs emitted");float right=0;
 for(auto v:r.rects)right=std::max(right,v.x+v.w);
 for(auto c:text){if(!fury::bitmap_detail::glyph5x7(c))std::cerr<<"unsupported="<<int(c)<<"\n";need(fury::bitmap_detail::glyph5x7(c)!=nullptr,"Existing supported glyph");}
 return right;
}
void check_text(const vaultline::FenceHudText& t) {
 std::size_t glyphs=0;
 for(const auto* line:{&t.heading,&t.cash,&t.availability,&t.discount,&t.sales,&t.controls,&t.close}) {
  need(measured(*line)<=284,"Full header/footer within native width");glyphs+=line->size();
 }
 need(vaultline::fence_hud_text_width(t.heading)+vaultline::fence_hud_text_width(t.cash)+8<=284,"Header lanes separated");
 for(const auto& row:t.rows) {
  const float gap=284-vaultline::fence_hud_text_width(row.detail)-vaultline::fence_hud_text_width(row.reason);
  narrowest_gap=std::min(narrowest_gap,gap);need(gap>=8,"Full identity/value and reason retain eight-cell gap");
  need(measured(row.detail)<=vaultline::fence_hud_text_width(row.detail),"Actual ink fits measured detail");
  need(measured(row.reason)<=vaultline::fence_hud_text_width(row.reason),"Actual ink fits measured reason");
  glyphs+=row.detail.size()+row.reason.size();
 }
 most_glyphs=std::max(most_glyphs,glyphs);need(glyphs<=vaultline::kFenceHudGlyphBudget,"No overlay budget truncation");
}
}
int main(){try {
 using R=vaultline::FenceHudReason;
 need(fury::kBitmapFontMaxGlyphs==12,"Shared short font cap unchanged");
 for(auto lang:{fury::Lang::En,fury::Lang::Es})
 for(int value:{std::numeric_limits<int>::min(),-1,0,1,9,std::numeric_limits<int>::max()})
 for(int level:{0,1,2,3,9,std::numeric_limits<int>::max()})
 for(int rep:{19,20,39,40,69,70})
 for(int mode=0;mode<4;++mode) {
  fury::Inventory inv;inv.cash=value;for(int& chip:inv.chips)chip=value;
  fury::PlayerPerks perks{level,level,level};fury::FenceUpgrades upgrades{bool(level%2),bool(level%2)};
  fury::FactionReputations reputation;reputation.pierline=rep;
  const bool open=mode&1,near=mode&2;
  auto text=vaultline::fence_hud_text(lang,inv,perks,upgrades,reputation,1,open,near,93.2f,6.5f);
  check_text(text);
  need(text.cash==vaultline::npc_hud_text("$"+std::to_string(value)),"Full signed cash preserved");
  for(int i=0;i<8;++i) {
   auto& row=text.rows[i];
   if(!open)need(row.state==R::Closed,"Hours precedence");
   else if(!near)need(row.state==R::Approach,"Range precedence");
   if(i<3) {
    need(row.detail.find(vaultline::npc_hud_text(std::to_string(level)+"/3"))!=std::u32string::npos,"Full loaded level preserved");
    need(row.has_price==(level<3),"MAX omits fictitious next price");
   } else if(i<5)need(row.has_price!=bool(level%2),"Owned has no repurchase price");
   else {
    need(row.detail.find(vaultline::npc_hud_text("x"+std::to_string(value)))!=std::u32string::npos,"Full signed stock preserved");
    need(row.amount==fury::loot_chip_sell_price(static_cast<fury::LootChip>(i-5)),"Sale amount unchanged by discount");
    need(row.selected==(i==6),"Selected chip identity retained");
    if(open&&near)need(row.state==(value>0?R::Ready:R::Empty),"Stock reason");
   }
  }
 }
 for(int rep:{19,20,39,40,69,70})for(int level=0;level<3;++level)for(int i=0;i<5;++i) {
  fury::FactionReputations reputation;reputation.pierline=rep;
  const int bases[]={3500,4500,4000,8000,6500};
  const int owner=static_cast<int>(static_cast<float>(bases[i]*(i<3?level+1:1))*reputation.shop_price_mul()+.5f);
  for(int offset:{-1,0,1}) {
   fury::Inventory inv;inv.cash=owner+offset;
   const auto t=vaultline::fence_hud_text(fury::Lang::En,inv,{level,level,level},{},reputation,0,true,true,0,6.5f);
   need(t.rows[i].amount==owner,"Exact integer product then float rounding parity");
   need(t.rows[i].state==(offset<0?R::Cash:R::Ready),"Exact price boundary reason");
  }
 }
 for(auto dims:{std::pair{640,360},std::pair{1280,720}})
 for(auto lang:{fury::Lang::En,fury::Lang::Es})for(float hs:{1.f,1.6f})for(int selected=0;selected<3;++selected) {
  fury::Inventory inv;inv.cash=std::numeric_limits<int>::min();for(int& n:inv.chips)n=std::numeric_limits<int>::min();
  const auto t=vaultline::fence_hud_text(lang,inv,{2147483647,2147483647,2147483647},{true,true},{},selected,true,false,93.2f,6.5f);
  Recorder r;vaultline::draw_fence_hud(r,t,dims.first,dims.second,hs);
  auto box=vaultline::layout_fence_hud(dims.first,dims.second,hs);
  need(box.scale==(dims.first==640?2:3),"Native 14/21 pixel glyph height with HUD fit");
  for(auto v:r.rects)need(v.x>=box.x&&v.y>=box.y&&v.x+v.w<=box.x+box.width+.01f&&v.y+v.h<=box.y+box.height+.01f,"Actual pixels stay in opaque panel");
 }
 for(int mask=0;mask<16;++mask)need(vaultline::fence_hud_visible(mask&1,mask&2,mask&4,mask&8)==((mask&3)==3&&!(mask&12)),"Pure modal visibility gate");
 std::cout<<"PASS checks="<<checks<<" max_glyphs="<<most_glyphs<<" minimum_logical_gap="<<narrowest_gap<<"\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
