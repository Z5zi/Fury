#include "../apps/vaultline/mission_board_hud.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
int checks{};
void need(bool result, const char* message) {
  ++checks;
  if (!result) throw std::runtime_error(message);
}
struct Rect { float x,y,w,h; fury::Color color; };
struct Recorder {
  std::vector<Rect> rects;
  void draw_hud_rect(float x,float y,float w,float h,fury::Color color) {
    rects.push_back({x,y,w,h,color});
  }
};
void prerequisites_and_phases() {
  // Exercise the unchanged actual model, including the optional sixth job.
  for(int mask=0;mask<64;++mask) {
    fury::QuestJournal journal;
    for(int i=0;i<6;++i) if(mask & (1<<i)) journal.mark_complete(i);
    need(journal.finale_unlocked(false)==((mask&15)==15),"Only core jobs 1-4 unlock finale; optional/finale flags cannot replace them");
    need(journal.finale_unlocked(true),"Existing explicit unlock-all behavior remains available");
  }
  fury::HeistController heist;
  heist.vault_position={0,0,0};heist.escape_position={20,0,0};
  auto active_help=[&](fury::HeistPhase phase) {
    need(heist.phase()==phase,"Actual heist update must reach the tested phase");
    const auto text=vaultline::mission_board_hud_text(fury::Lang::En,0,true,heist.phase());
    need(text.controls==U"1-6 locked during job / M close","Active actual phase must explain unavailable selection");
  };
  heist.update({0,0,0},false,.1f);active_help(fury::HeistPhase::Approach);
  heist.update({0,0,0},true,.1f);active_help(fury::HeistPhase::Breach);
  heist.update({0,0,0},false,heist.breach_duration+.1f);active_help(fury::HeistPhase::Looting);
  heist.update({0,0,0},false,heist.loot_duration+.1f);active_help(fury::HeistPhase::Escape);
  heist.update({20,0,0},false,.1f);
  need(heist.phase()==fury::HeistPhase::Success,"Actual extraction reaches success");
  need(vaultline::mission_board_hud_text(fury::Lang::En,0,true,heist.phase()).controls==U"1-6 select / M close","Success restores selection help");
}
void coverage() {
  need(fury::kBitmapFontMaxGlyphs==12,"Global short-label cap must remain 12");
  need(fury::bitmap_detail::glyph5x7('&')!=nullptr,"Authored ampersand must render");
  for (auto lang : {fury::Lang::En,fury::Lang::Es})
  for (bool locked : {false,true}) for (int selected=0;selected<6;++selected)
  for(auto phase:{fury::HeistPhase::Idle,fury::HeistPhase::Approach,fury::HeistPhase::Breach,
      fury::HeistPhase::Looting,fury::HeistPhase::Escape,fury::HeistPhase::Success,fury::HeistPhase::Failed}) {
    const auto text=vaultline::mission_board_hud_text(lang,selected,locked,phase);
    std::size_t glyphs{};
    auto inspect=[&](const std::u32string& span) {
      glyphs+=span.size();
      for(auto c:span) need(fury::bitmap_detail::glyph5x7(c)!=nullptr,"Every authored/localized character must have a glyph");
    };
    for(const auto* span:{&text.heading,&text.tier,&text.prerequisite,&text.objective,&text.controls})inspect(*span);
    for(std::size_t i=0;i<6;++i) {
      need(text.names[i]==vaultline::npc_hud_text(std::to_string(i+1)+" "+fury::mission_title_tr(lang,i)),"All six full authored titles and key numbers must be preserved");
      inspect(text.names[i]);inspect(text.states[i]);
      need(text.states[i].empty()==(!(int(i)==selected || (i==4 && locked))),"Only selected or locked jobs receive state labels");
    }
    need(glyphs<=vaultline::kMissionBoardGlyphBudget,"The entire EN/ES chooser must fit its local budget");
    using fury::HeistPhase;
    const bool can_retarget=phase==HeistPhase::Idle || phase==HeistPhase::Success || phase==HeistPhase::Failed;
    need(text.controls==vaultline::npc_hud_text(can_retarget
        ? (lang==fury::Lang::Es?"1-6 seleccionar / M cerrar":"1-6 select / M close")
        : (lang==fury::Lang::Es?"1-6 bloqueado en mision / M cerrar":"1-6 locked during job / M close")),
        "Digit key help must match the existing phase eligibility gate");
    if(phase==HeistPhase::Idle || phase==HeistPhase::Approach || phase==HeistPhase::Escape || phase==HeistPhase::Success)
      need(text.objective==vaultline::npc_hud_text(fury::tr(lang,phase==HeistPhase::Escape?fury::StrId::TipEscape:
        phase==HeistPhase::Success?fury::StrId::TipDone:fury::StrId::TipTarget)),"Applicable objective guidance must remain authored content");
    for (auto dims : {std::pair{640,360},std::pair{800,450},std::pair{1280,720},std::pair{1920,1080}})
    for(float hs:{1.f,1.35f,1.6f}) {
      const auto box=vaultline::layout_mission_board_hud(dims.first,dims.second,hs);
      need(box.scale>=2.f && box.scale<=3.f,"Supported viewports must retain readable glyph size");
      need(box.x>=16.f && box.y>=16.f && box.x+box.width<=dims.first-16.f && box.y+box.height<=dims.second-16.f,"Whole panel must fit inside 16px viewport margins");
      const float lane=box.width-20.f*box.scale;
      for(const auto* span:{&text.prerequisite,&text.objective,&text.controls})
        need(vaultline::mission_board_text_width(*span,box.scale)<=lane,"Full explanatory/key text must fit without truncation");
      for(std::size_t i=0;i<6;++i) {
        need(vaultline::mission_board_text_width(text.names[i],box.scale)+vaultline::mission_board_text_width(text.states[i],box.scale)+8.f*box.scale<=lane,"Full title and state badge must remain separated");
        need(box.row_y(i)+box.row_height()<=box.y+110.f*box.scale,"All six rows must be above the prerequisite line");
      }
      fury::MissionBoard board;board.open=true;board.selected=selected;
      const auto before=board;
      Recorder recorder;
      vaultline::draw_mission_board_hud(recorder,board,locked,lang,phase,dims.first,dims.second,hs);
      need(board.open==before.open && board.selected==before.selected,"Presentation must not mutate selection/open state");
      for(const auto& rect:recorder.rects)
        need(rect.x>=box.x-.001f && rect.y>=box.y-.001f && rect.x+rect.w<=box.x+box.width+.001f && rect.y+rect.h<=box.y+box.height+.001f,"Every actual background, badge and glyph rectangle must fit the panel");
      need(recorder.rects.size()<=21*vaultline::kMissionBoardGlyphBudget+64,"Board rendering must retain bounded rectangle cost");
    }
  }
}
}
int main() {
  try {prerequisites_and_phases();coverage();std::cout<<checks<<" mission-board presentation checks passed\n";return 0;}
  catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
