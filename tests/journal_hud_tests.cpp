#include "../apps/vaultline/journal_hud.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::size_t checks=0, most_glyphs=0; float minimum_gap=1000, widest_line=0;
void need(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
struct Rect { float x,y,w,h; fury::Color c; };
struct Recorder {
  std::vector<Rect> rects;
  void draw_hud_rect(float x,float y,float w,float h,fury::Color c) { rects.push_back({x,y,w,h,c}); }
};
float measured(std::u32string_view text) {
  Recorder r; std::size_t budget=vaultline::kJournalHudGlyphBudget;
  vaultline::draw_npc_hud_text(r,0,0,text,{255,255,255,255},1,budget);
  need(budget==vaultline::kJournalHudGlyphBudget-text.size(),"Every glyph emitted");
  float right=0;
  for (auto rect:r.rects) {
    need(rect.x>=0 && rect.y>=0 && rect.y+rect.h<=7,"Actual glyph ink bounds");
    right=std::max(right,rect.x+rect.w);
  }
  for (char32_t c:text) {
    need(fury::bitmap_detail::glyph5x7(c)!=nullptr,"Every authored/localized glyph supported");
    need(c!=U'%',"No invented progress percentage");
  }
  need(right<=vaultline::journal_hud_text_width(text),"Measured width bounds actual ink");
  widest_line=std::max(widest_line,right);
  return right;
}
float luminance(fury::Color c) {
  auto linear=[](int n){const float s=n/255.f;return s<=.04045f?s/12.92f:std::pow((s+.055f)/1.055f,2.4f);};
  return .2126f*linear(c.r)+.7152f*linear(c.g)+.0722f*linear(c.b);
}
}
int main() { try {
  need(fury::kBitmapFontMaxGlyphs==12,"Shared short-label limit unchanged");
  need(vaultline::kNpcHudGlyphBudget==512,"Shared text decoder unchanged");
  const int tiers[]={3,2,1,2,4,1};
  for (auto lang:{fury::Lang::En,fury::Lang::Es}) for (int mask=0;mask<64;++mask)
  for (bool override_unlock:{false,true}) {
    fury::QuestJournal journal; journal.open=true; int total=0,core=0;
    for (int i=0;i<6;++i) { journal.complete[i]=(mask>>i)&1; total+=journal.complete[i]; if(i<4)core+=journal.complete[i]; }
    const auto initial=journal; const bool es=lang==fury::Lang::Es;
    const bool locked=!journal.finale_unlocked(override_unlock);
    need(locked==(!override_unlock && (mask&15)!=15),"Actual finale availability uses first four, including override");
    const auto t=vaultline::journal_hud_text(lang,journal,locked);
    need(t.heading==vaultline::npc_hud_text(es?"DIARIO":"JOURNAL"),"Localized journal title");
    need(t.count==vaultline::npc_hud_text(std::string(es?"COMPLETAS ":"COMPLETE ")+std::to_string(total)+"/6"),"Exact distinct completion count");
    need(t.close==vaultline::npc_hud_text(es?"J CERRAR":"J CLOSE"),"Existing J-only close hint");
    need(t.rule.substr(t.rule.size()-3)==vaultline::npc_hud_text(std::to_string(core)+"/4"),"Exact first-four core count");
    need(t.rule.find(U"1-4")!=std::u32string::npos,"Specific first-four prerequisite identified");
    need(t.rule.find(vaultline::npc_hud_text(locked?(es?"BLOQUEADO":"LOCKED"):(es?"DISPONIBLE":"UNLOCKED")))!=std::u32string::npos,"Actual availability described separately from core count");
    std::size_t glyphs=0;
    for (const auto* line:{&t.heading,&t.count,&t.rule,&t.close}) { need(measured(*line)<=284,"Header/rule/footer full ink fits");glyphs+=line->size(); }
    need(vaultline::journal_hud_text_width(t.heading)+vaultline::journal_hud_text_width(t.count)+8<=284,"Header columns separated");
    for (std::size_t i=0;i<6;++i) {
      const auto& row=t.rows[i]; const bool done=journal.complete[i]!=0;
      const auto expected=done?vaultline::JournalHudStatus::Complete:i==4&&locked?vaultline::JournalHudStatus::Locked:vaultline::JournalHudStatus::Incomplete;
      need(row.state==expected,"Done wins over locked, including completed finale with missing core");
      need(row.name==vaultline::npc_hud_text(std::to_string(i+1)+" "+fury::mission_title_tr(lang,i)),"Complete authored localized identity and order");
      need(row.tier==vaultline::npc_hud_text(std::string(es?"NIVEL DE PAGO ":"PAYOUT TIER ")+std::to_string(tiers[i])),"Exact explicitly labeled static payout tier");
      std::string status=done?(es?"COMPLETA":"COMPLETE"):i==4&&locked?(es?"BLOQUEADA":"LOCKED"):(es?"NO COMPLETA":"NOT COMPLETE");
      if(i==5)status+=es?" / OPCIONAL":" / OPTIONAL";
      need(row.status==vaultline::npc_hud_text(status),"Exact completion/lock text and optional North Quay only");
      need(measured(row.name)<=284,"Full long names fit"); measured(row.status); measured(row.tier);
      const float gap=284-vaultline::journal_hud_text_width(row.status)-vaultline::journal_hud_text_width(row.tier);
      minimum_gap=std::min(minimum_gap,gap); need(gap>=8,"Actual status/tier columns remain separated");
      glyphs+=row.name.size()+row.status.size()+row.tier.size();
    }
    most_glyphs=std::max(most_glyphs,glyphs);
    need(glyphs<=vaultline::kJournalHudGlyphBudget,"Complete panel fits J-local budget");
    for (auto dims:{std::pair{640,360},std::pair{1280,720}}) for (float hs:{1.f,1.6f}) {
      Recorder r; vaultline::draw_journal_hud(r,t,dims.first,dims.second,hs);
      const auto b=vaultline::layout_journal_hud(dims.first,dims.second,hs);
      need(b.x>=15.99f&&b.y>=15.99f&&b.x+b.width<=dims.first-15.99f&&b.y+b.height<=dims.second-15.99f,"Sixteen native pixel margins");
      need(b.scale>=2&&b.scale<=3,"Readable native glyph size");
      need(r.rects.front().c.a==255,"Whole panel is opaque");
      for (auto q:r.rects) {
        need(q.x>=b.x-.01f&&q.y>=b.y-.01f&&q.x+q.w<=b.x+b.width+.01f&&q.y+q.h<=b.y+b.height+.01f,"Every emitted rectangle within opaque panel");
        need(q.c.a==255,"All panel and text drawing opaque");
        if(q.h<=b.scale && q.w<30*b.scale)
          need((luminance(q.c)+.05f)/(luminance({23,36,40,255})+.05f)>4.5f,"Text contrast exceeds 4.5 against row and darker panel");
      }
      need(15<20&&27<b.row_y(0),"Header and rule separated from rows");
      for(std::size_t i=0;i<6;++i) {
        need(b.row_y(i)+7<b.row_y(i)+9,"Name/status lines separated");
        need(b.row_y(i)+16<(i<5?b.row_y(i+1):152.f),"Rows and footer separated");
      }
    }
    need(journal.open==initial.open,"Drawing preserves J visibility state");
    for(int i=0;i<6;++i)need(journal.complete[i]==initial.complete[i],"Drawing preserves every completion flag");
  }
  for(int mask=0;mask<16;++mask)need(vaultline::journal_hud_visible(mask&1,mask&2,mask&4,mask&8)==((mask&3)==3&&!(mask&12)),"J visibility truth table");
  std::cout<<"PASS checks="<<checks<<" flag_override_cases=128 languages=2 max_glyphs="<<most_glyphs<<" minimum_logical_gap="<<minimum_gap<<" widest_actual_ink="<<widest_line<<" synthetic_only\n";
  return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<"\n"; return 1; } }
