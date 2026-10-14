#include "../apps/vaultline/inventory_hud.hpp"
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
 Recorder r;std::size_t budget=256;vaultline::draw_npc_hud_text(r,0,0,text,{255,255,255,255},1,budget);
 need(budget==256-text.size(),"All glyphs emitted");float right=0;
 for(auto v:r.rects){need(v.x>=0&&v.y>=0&&v.y+v.h<=7,"Glyph ink bounds");right=std::max(right,v.x+v.w);}
 for(auto c:text)need(fury::bitmap_detail::glyph5x7(c)!=nullptr,"Every label/value glyph supported");
 need(right<=vaultline::inventory_hud_text_width(text),"Actual glyph ink within measured width");return right;
}
}
int main(){try {
 need(fury::kBitmapFontMaxGlyphs==12,"Shared short-label limit unchanged");
 need(vaultline::kNpcHudGlyphBudget==512,"Shared decoder limit unchanged");
 // Negative chip values below are helper-only formatting fixtures, never loaded gameplay claims.
 for(auto lang:{fury::Lang::En,fury::Lang::Es})
 for(int cash:{std::numeric_limits<int>::min(),-1,0,49999,50000,50001,std::numeric_limits<int>::max()})
 for(int value:{std::numeric_limits<int>::min(),-1,0,1,7,8,9,std::numeric_limits<int>::max()}) {
  fury::Inventory inv;inv.cash=cash;for(int& chip:inv.chips)chip=value;const auto initial=inv;
  const auto t=vaultline::inventory_hud_text(lang,inv);const bool es=lang==fury::Lang::Es;
  need(t.heading==vaultline::npc_hud_text(es?"INVENTARIO":"INVENTORY"),"Local title");
  need(t.chips==vaultline::npc_hud_text(es?"FICHAS":"CHIPS"),"Local section");
  need(t.close==vaultline::npc_hud_text(es?"I CERRAR":"I CLOSE"),"Existing I-only close hint");
  need(t.rows[0].label==vaultline::npc_hud_text(es?"EFECTIVO":"CASH"),"Local cash label");
  need(t.rows[0].value==vaultline::npc_hud_text("$"+std::to_string(cash)),"Full signed cash preserved");
  const char* canonical[]={"BearerBond","Sapphire","LedgerDrive"};
  std::size_t glyphs=0;
  for(const auto* text:{&t.heading,&t.chips,&t.close}){need(measured(*text)<=244,"Heading/footer fit");glyphs+=text->size();}
  for(std::size_t i=0;i<t.rows.size();++i){const auto& row=t.rows[i];
   if(i){need(row.label==vaultline::npc_hud_text(canonical[i-1]),"Canonical identity and order");need(row.value==vaultline::npc_hud_text(std::to_string(value)),"Full signed helper quantity preserved");}
   measured(row.label);measured(row.value);
   const float gap=244-vaultline::inventory_hud_text_width(row.label)-vaultline::inventory_hud_text_width(row.value);
   narrowest_gap=std::min(narrowest_gap,gap);need(gap>=8,"Measured label/value columns remain separated");glyphs+=row.label.size()+row.value.size();
  }
  most_glyphs=std::max(most_glyphs,glyphs);need(glyphs<=vaultline::kInventoryHudGlyphBudget,"All text fits I-local budget");
  for(auto dims:{std::pair{640,360},std::pair{1280,720}})for(float hs:{1.f,1.6f}){
   Recorder r;vaultline::draw_inventory_hud(r,t,dims.first,dims.second,hs);const auto box=vaultline::layout_inventory_hud(dims.first,dims.second,hs);
   need(box.x>=16-.01f&&box.y>=16-.01f&&box.x+box.width<=dims.first-16+.01f&&box.y+box.height<=dims.second-16+.01f,"Sixteen native pixel viewport margin");
   need(box.scale>=2&&box.scale<=3,"Readable native glyph scale");
   for(auto v:r.rects)need(v.x>=box.x-.01f&&v.y>=box.y-.01f&&v.x+v.w<=box.x+box.width+.01f&&v.y+v.h<=box.y+box.height+.01f,"Actual emitted rectangles within opaque panel");
   need(box.row_y(0)+7<40&&47<box.row_y(1)&&box.row_y(1)+7<box.row_y(2)&&box.row_y(2)+7<box.row_y(3)&&box.row_y(3)+7<92,"All text rows vertically separated");
  }
  need(inv.cash==initial.cash,"Rendering preserves cash");for(int i=0;i<3;++i)need(inv.chips[i]==initial.chips[i],"Rendering preserves chip quantity");
 }
 for(int mask=0;mask<16;++mask)need(vaultline::inventory_hud_visible(mask&1,mask&2,mask&4,mask&8)==((mask&3)==3&&!(mask&12)),"I-local visibility truth table");
 std::cout<<"PASS checks="<<checks<<" max_glyphs="<<most_glyphs<<" minimum_logical_gap="<<narrowest_gap<<" signed_min=helper_only\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
