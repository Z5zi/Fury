#include "../apps/vaultline/status_hud.hpp"
#include "../apps/vaultline/npc_dialogue_hud.hpp"
#include "../apps/vaultline/mission_guidance_hud.hpp"
#include "../apps/vaultline/result_notice_hud.hpp"
#include "fury/i18n.hpp"

#include <SDL.h>

#include <climits>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks{};
void need(bool ok, const char* message) {
  ++checks;
  if (!ok) throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a-b) < .00001f; }
bool same(fury::Color a, fury::Color b) {
  return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;
}
void valid_text(const char* text) {
  need(std::strlen(text)>0 && std::strlen(text)<=12,"Every status span fits the unchanged font cap");
  for (const char* p=text; *p; ++p)
    need(fury::bitmap_detail::glyph5x7(*p)!=nullptr,"Every displayed character exists in the accepted font");
}
void geometry(const std::array<vaultline::StatusHudRow,6>& rows) {
  for (int width : {640,1280}) for (float scale : {1.f,1.3f,1.6f}) {
    const auto box=vaultline::layout_status_hud(width,width*9/16,scale);
    need(box.glyph>=2.f && box.glyph<=2.f*scale,"Supported windows retain at least 2px readable glyphs");
    need(box.bar_x+box.bar_width <= width-174.f+.001f,"Status content stays left of the unchanged minimap and gap");
    need(box.meter_y(5)+2.f*scale < box.ready_y,
         "Shifted readiness starts below all scaled status content");
    for (const auto& row : rows) {
      valid_text(row.label); valid_text(row.value.data());
      float content = vaultline::status_text_width(row.label,box.glyph) + 12*scale +
          vaultline::status_text_width(row.value.data(),box.glyph);
      if (row.phase) {
        valid_text(row.phase);
        content += 12*scale + vaultline::status_text_width(row.phase,box.glyph);
      }
      need(content <= box.text_right-box.text_left+.001f,"Complete labels, real phase and full values fit with a readable gap");
    }
  }
}
void test_money() {
  const int values[]{INT_MIN,-1,0,49999,50000,50001,79999,80000,80001,999999,INT_MAX};
  for (int money : values) {
    vaultline::StatusHudState s; s.cash=s.job_earnings=money;
    const auto rows=vaultline::status_hud_rows(s,false);
    const auto expected="$"+std::to_string(std::max(0,money));
    need(rows[0].value.data()==expected && rows[2].value.data()==expected,
         "Money stays exact through signed 32-bit maximum; negatives retain legacy zero display");
    need(near(rows[0].fill,std::min(1.f,float(money)/50000.f)),"Cash meter still saturates at 50000");
    need(near(rows[2].fill,std::min(1.f,float(money)/80000.f)),"Job earnings meter still saturates at 80000");
    geometry(rows);
  }
}
void test_ratios_phases() {
  using fury::HeistPhase;
  struct Phase { HeistPhase value; const char* name; fury::Color color; };
  const Phase phases[]{
    {HeistPhase::Idle,"IDLE",{220,180,40,230}},
    {HeistPhase::Approach,"APPROACH",{220,180,40,230}},
    {HeistPhase::Breach,"BREACH",{220,180,40,230}},
    {HeistPhase::Looting,"LOOTING",{220,180,40,230}},
    {HeistPhase::Escape,"ESCAPE",{80,180,255,230}},
    {HeistPhase::Success,"SUCCESS",{90,255,140,230}},
    {HeistPhase::Failed,"FAILED",{220,60,60,230}}
  };
  for (const auto& phase : phases) {
    vaultline::StatusHudState s; s.phase=phase.value;
    // Use production-valid progress for phases outside Looting.
    s.loot=(phase.value==HeistPhase::Escape || phase.value==HeistPhase::Success) ? 1.f :
        (phase.value==HeistPhase::Looting ? .375f : 0.f);
    const auto rows=vaultline::status_hud_rows(s,false);
    need(std::string(rows[1].phase)==phase.name,"All seven heist states use their actual names");
    need(same(rows[1].color,phase.color),"Legacy phase meter colors remain exact");
    const auto remapped=vaultline::status_hud_rows(s,true);
    need(same(remapped[1].color,fury::colorblind_remap(phase.color,true)),"All phase colors keep the accepted accessibility mapping");
    need(std::string(rows[1].value.data())==(s.loot==1.f?"100/100":s.loot==0.f?"0/100":"38/100"),
         "Loot label reflects actual production progress including zero marker and rounding");
    geometry(rows);
  }
  struct Ratio { float value; const char* expected; };
  for (const auto& sample : {Ratio{0,"0/100"},Ratio{.004f,"0/100"},Ratio{.005f,"1/100"},
       Ratio{.33f,"33/100"},Ratio{.33001f,"33/100"},Ratio{.66f,"66/100"},
       Ratio{.66001f,"66/100"},Ratio{.999f,"100/100"},Ratio{1,"100/100"}}) {
    for (bool crouching : {false,true}) for (bool colorblind : {false,true}) {
      vaultline::StatusHudState s; s.loot=s.heat=s.visibility=sample.value;s.crouching=crouching;
      const auto rows=vaultline::status_hud_rows(s,colorblind);
      for (int row : {1,3,4}) {
        need(std::string(rows[row].value.data())==sample.expected,"Ratios are correctly rounded independent of slivers/colors");
        need(near(rows[row].fill,std::max(.02f,sample.value)),"Legacy normalized meters and minimum slivers are preserved");
      }
      need(std::string(rows[4].label)==(crouching?"CROUCH VIS":"VISIBILITY"),"Crouch state remains an explicit label");
      fury::Color heat=sample.value>.66f?fury::Color{255,50,50,240}:
          sample.value>.33f?fury::Color{255,120,30,230}:fury::Color{255,160,40,230};
      fury::Color vis=crouching?fury::Color{60,180,140,230}:sample.value>.66f?fury::Color{255,90,160,240}:
          sample.value>.33f?fury::Color{120,220,255,230}:fury::Color{80,200,220,230};
      need(same(rows[3].color,fury::colorblind_remap(heat,colorblind)),"Heat thresholds and accessibility remap are unchanged");
      need(same(rows[4].color,fury::colorblind_remap(vis,colorblind)),"Visibility/crouch thresholds and remap are unchanged");
      geometry(rows);
    }
  }
  for (const auto& sample : {Ratio{-1,"0/100"},Ratio{2,"100/100"},
       Ratio{std::numeric_limits<float>::quiet_NaN(),"0/100"}}) {
    std::array<char,16> value{};vaultline::format_status_ratio(sample.value,value);
    need(std::string(value.data())==sample.expected,"Malformed ratio input cannot overflow numeric formatting");
  }
}
void test_crew_and_budget() {
  for (int count : {0,1,2,3,32,INT_MAX}) for (int money : {INT_MAX,2000000000}) {
    vaultline::StatusHudState s{money,fury::HeistPhase::Approach,1.f,money,1.f,1.f,false,count};
    const auto before=s;
    const auto rows=vaultline::status_hud_rows(s,false);
    need(std::string(rows[5].value.data())==std::to_string(count),"Crew count is exact and never invents readiness/human count");
    need(near(rows[5].fill,.5f*float(count)),"Existing crew meter normalization is retained, including future-count limitation");
    need(s.cash==before.cash && s.phase==before.phase && s.loot==before.loot &&
         s.job_earnings==before.job_earnings && s.heat==before.heat && s.visibility==before.visibility &&
         s.crouching==before.crouching && s.crew_nearby==before.crew_nearby,"Formatting cannot change gameplay values");
    std::size_t glyphs=0,cells=0,calls=0;
    for (const auto& row : rows) for (const char* text : {row.label,row.value.data(),row.phase}) {
      if (!text) continue;
      ++calls;glyphs+=std::strlen(text);
      for (const char* p=text;*p;++p) {
        const auto* g=fury::bitmap_detail::glyph5x7(*p);
        for (int y=0;y<7;++y) for(int x=0;x<5;++x) cells+=(g[y]>>x)&1u;
      }
    }
    need(calls==13 && glyphs<=104 && cells<=2080,"Status block stays within the reviewed font call/glyph/rectangle budget");
    geometry(rows);
    for(bool cb:{false,true}) {
      const auto colors=vaultline::status_hud_rows(s,cb);
      need(same(colors[0].color,fury::colorblind_remap({50,200,90,230},cb)),"Cash color remains exact");
      need(same(colors[2].color,fury::colorblind_remap({180,120,255,230},cb)),"Earnings color remains exact");
      need(same(colors[5].color,{90,180,255,230}),"Crew blue remains unremapped");
    }
  }
}
void test_real_heist_values() {
  fury::HeistController heist;
  auto check=[&](const char* phase,const char* progress) {
    const auto rows=vaultline::status_hud_rows({heist.inventory().cash,heist.phase(),
        heist.loot_progress(),heist.score().lifetime_cash,0.f,0.f,false,0},false);
    need(std::string(rows[1].phase)==phase && std::string(rows[1].value.data())==progress,
         "Status snapshot displays actual HeistController phase and progress through a real job");
    need(std::string(rows[0].value.data())=="$"+std::to_string(heist.inventory().cash) &&
         std::string(rows[2].value.data())=="$"+std::to_string(heist.score().lifetime_cash),
         "Cash and cumulative job earnings read their separate production counters");
  };
  check("IDLE","0/100");
  heist.update(heist.vault_position,false,0);check("APPROACH","0/100");
  heist.update(heist.vault_position,true,0);check("BREACH","0/100");
  heist.update(heist.vault_position,false,heist.breach_duration);check("LOOTING","0/100");
  heist.update(heist.vault_position,false,heist.loot_duration*.375f);check("LOOTING","38/100");
  heist.update(heist.vault_position,false,heist.loot_duration);check("ESCAPE","100/100");
  heist.update(heist.escape_position,false,0);check("SUCCESS","100/100");
  const int earnings=heist.score().lifetime_cash;
  need(earnings>0,"A completed job has a real payout");
  heist.inventory().cash-=1;check("SUCCESS","100/100");
  need(heist.score().lifetime_cash==earnings,"Spending cash does not change cumulative job earnings");
  heist.reset();heist.update(heist.vault_position,false,0);heist.force_fail();check("FAILED","0/100");
  heist.loot_duration=0;check("FAILED","100/100");
  // Degenerate-duration behavior is intentionally inherited, not inferred from phase.
}
void test_native_text_pixels() {
  SDL_setenv("SDL_VIDEODRIVER","dummy",1);
  need(SDL_Init(SDL_INIT_VIDEO)==0,"Initialize software pixel witness");
  for(int width:{640,1280}) {
    const int height=width*9/16;
    SDL_Window* window=SDL_CreateWindow("Status pixel test",0,0,width,height,SDL_WINDOW_HIDDEN);
    need(window!=nullptr,"Create native test window");
    {
      fury::Renderer renderer;
      need(renderer.create(window,width,height,false,fury::RenderBackendKind::Software),"Create actual Fury software backend");
      for(float scale:{1.f,1.3f,1.6f}) for(bool cb:{false,true}) {
        const vaultline::StatusHudState state{INT_MAX,fury::HeistPhase::Approach,0,INT_MAX,1,1,cb,2};
        renderer.begin_frame(cb?fury::Color{255,255,255,255}:fury::Color{0,0,0,255});
        vaultline::draw_status_hud(renderer,state,width,height,scale,cb);
        std::vector<std::uint8_t> rgb;int rw{},rh{};
        need(renderer.read_rgb_framebuffer(rgb,rw,rh) && rw==width && rh==height,"Read native pixel buffer");
        const auto rows=vaultline::status_hud_rows(state,cb);
        const auto box=vaultline::layout_status_hud(width,width*9/16,scale);
        // Probe the center of every lit font cell in every complete text span.
        // This catches renderer clipping, wrong scales, missing draw calls and
        // meters overwriting text, using the layout consumed by production.
        auto check_text=[&](const char* text,float x,float y) {
          for(const char* p=text;*p;++p,x+=6.f*box.glyph) {
            const auto* glyph=fury::bitmap_detail::glyph5x7(*p);
            for(int gy=0;gy<7;++gy) for(int gx=0;gx<5;++gx) if(glyph[gy]&(1u<<(4-gx))) {
              const int px=int(std::floor(x+(float(gx)+.5f)*box.glyph));
              const int py=int(std::floor(y+(float(gy)+.5f)*box.glyph));
              need(px>=0 && px<width && py>=0 && py<height,"Every text cell is inside the framebuffer");
              const auto i=std::size_t(py*width+px)*3;
              need(rgb[i]==238 && rgb[i+1]==244 && rgb[i+2]==250,"Every lit cell of every complete value/label reaches native pixels");
            }
          }
        };
        for(std::size_t i=0;i<rows.size();++i) {
          const auto& row=rows[i];const float vx=box.text_right-vaultline::status_text_width(row.value.data(),box.glyph);
          check_text(row.label,box.text_left,box.row_y(i));check_text(row.value.data(),vx,box.row_y(i));
          if(row.phase)check_text(row.phase,vx-12.f*box.scale-vaultline::status_text_width(row.phase,box.glyph),box.row_y(i));
        }
        renderer.end_frame();
      }
    }
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
}
void test_onboarding_instructions() {
  const char* expected_bytes[2][3] = {
    {"Press M — open mission board (1-6)",
     "Follow compass to target",
     "Reach green extraction (van F/E)"},
    {"Pulsa M — tablero de misiones (1-6)",
     "Sigue la brujula al objetivo",
     "Llega a la extraccion verde (furgon F/E)"}
  };
  const char32_t* expected_text[2][3] = {
    {U"Press M — open mission board (1-6)",
     U"Follow compass to target",
     U"Reach green extraction (van F/E)"},
    {U"Pulsa M — tablero de misiones (1-6)",
     U"Sigue la brujula al objetivo",
     U"Llega a la extraccion verde (furgon F/E)"}
  };
  struct TextBounds {
    float x, y; std::size_t rectangles{};
    void draw_hud_rect(float px, float py, float w, float h, fury::Color) {
      need(near(h,2.f) && w>=2.f && w<=10.f,"Onboarding keeps fixed 2px glyph rows");
      need(px>=x && px+w<=x+528.f && py>=y && py+h<=y+14.f,
           "Every onboarding glyph rectangle fits the readable single-line lane");
      ++rectangles;
    }
  };
  std::size_t longest{};
  for(int language=0;language<2;++language) for(int stage=0;stage<3;++stage) {
    const char* raw=fury::tr(fury::lang_from_int(language),fury::onboard_tip_id(stage));
    need(std::string(raw)==expected_bytes[language][stage],"Authored EN/ES onboarding bytes stay exact");
    const auto text=vaultline::npc_hud_text(raw);
    need(text==expected_text[language][stage],"Complete onboarding UTF-8 decodes to the authored codepoints");
    need(!text.empty() && text.size()<=44,"Every complete instruction fits the 44-codepoint budget");
    need((float(text.size())*6.f-1.f)*2.f<=528.f,"Complete fixed-2px instruction fits the 528px text lane");
    longest=std::max(longest,text.size());
    for(char32_t c:text) need(fury::bitmap_detail::glyph5x7(c)!=nullptr,
        "Every required onboarding glyph exists without replacement or fallback");
    for(int width:{640,1280}) {
      TextBounds bounds{float(width)*.5f-264.f,float(width*9/16)-72.f};
      std::size_t budget=44;
      vaultline::draw_npc_hud_text(bounds,bounds.x,bounds.y,text,{220,240,255,240},2.f,budget);
      need(budget==44-text.size() && bounds.rectangles>0 && bounds.rectangles<=44*21,
           "Bounded shared renderer consumes the entire authored instruction");
    }
  }
  need(longest==40 && (float(longest)*6.f-1.f)*2.f==478.f,"Longest generic instruction is 478px after neutral target copy");
  need(fury::kBitmapFontMaxGlyphs==12,"Long onboarding does not expand the legacy font cap");
}
void test_adjacent_layout() {
  struct Rect {float x,y,w,h;};
  auto overlap=[](Rect a,Rect b) {
    constexpr float e=.001f;
    return a.x+a.w>b.x+e && b.x+b.w>a.x+e && a.y+a.h>b.y+e && b.y+b.h>a.y+e;
  };
  for(int width:{640,1280}) for(int step=0;step<=60;++step) for(bool vehicle:{false,true}) {
    const int height=width*9/16;const float hs=1.f+float(step)*.01f;
    const auto box=vaultline::layout_status_hud(width,height,hs);
    const auto aux=vaultline::layout_status_auxiliary(box,width,vehicle);
    const Rect body{box.bar_x,box.row_y(0),box.bar_width,box.meter_y(5)+2*hs-box.row_y(0)};
    const Rect ready{16,aux.ready_y,220,22},tier{aux.tier_x,aux.tier_y,200,18},
        crouch{aux.crouch_x,aux.crouch_y,120,18};
    const Rect map{float(width)-166,16,150,150},
        notch{float(width)/2-123,float(height)-118,246,6},
        onboarding{float(width)/2-284,float(height)-78,568,28};
    std::vector<Rect> widgets{ready,tier};
    if(vehicle) {widgets.push_back({16,152+aux.offset,180,22});widgets.push_back({16,178+aux.offset,180,28});}
    else widgets.push_back(crouch);
    for(const auto widget:widgets) {
      need(!overlap(widget,body),"Existing adjacent widgets do not overwrite scaled status text/meters");
      need(!overlap(widget,map) && !overlap(widget,notch) && !overlap(widget,onboarding),
           "Adjacent widgets stay clear of minimap, every compass-notch position and bottom onboarding");
      need(widget.x>=0 && widget.x+widget.w<=width && widget.y>=0 && widget.y+widget.h<=height,
           "Adjacent widget boxes remain within the tested native viewport");
    }
    need(!overlap(ready,tier),"Readiness stays distinct from the payout-tier marker");
    if(!vehicle)need(!overlap(ready,crouch) && !overlap(crouch,tier),"On-foot readiness/tier/crouch markers stay distinct");
    else {
      // The accepted vehicle UI intentionally places readiness over radio.
      need(std::abs(aux.ready_y-(178+aux.offset)-2.f)<.001f,"Inherited readiness-over-radio offset remains exact");
    }
    for(std::size_t row=0;row<6;++row) {
      need(box.row_y(row)+7*box.glyph < box.meter_y(row),"Text and thin meter keep a visible gap");
      if(row<5)need(box.meter_y(row)+2*hs < box.row_y(row+1),"Tightened rows never collide");
    }
    if(step==0)need(near(aux.offset,0) && near(aux.ready_y,vehicle?180.f:148.f) &&
       near(aux.tier_x,16) && near(aux.tier_y,210) && near(aux.crouch_x,16) && near(aux.crouch_y,170),
       "All ordinary-scale auxiliary coordinates remain exact");
  }
}

void test_mission_guidance() {
  using fury::HeistPhase;
  using vaultline::MissionGuidanceKind;
  using vaultline::MissionObjectiveCue;
  const HeistPhase phases[]{HeistPhase::Idle,HeistPhase::Approach,HeistPhase::Breach,
      HeistPhase::Looting,HeistPhase::Escape,HeistPhase::Success,HeistPhase::Failed};
  const MissionGuidanceKind kinds[]{MissionGuidanceKind::Target,MissionGuidanceKind::Approach,
      MissionGuidanceKind::Breach,MissionGuidanceKind::Looting,MissionGuidanceKind::Escape,
      MissionGuidanceKind::Success,MissionGuidanceKind::Failed};
  const char32_t* expected[2][8]{
    {U"Press M — open mission board (1-6)",U"Follow compass to target",
     U"Follow compass to target",U"Breaching — stay near target",
     U"Looting — watch loot meter",U"Reach green extraction (van F/E)",
     U"Job complete — M opens mission board",U"Job failed — M opens mission board"},
    {U"Pulsa M — tablero de misiones (1-6)",U"Sigue la brujula al objetivo",
     U"Sigue la brujula al objetivo",U"Abriendo — quedate cerca del objetivo",
     U"Saqueando — mira el progreso",U"Llega a la extraccion verde (furgon F/E)",
     U"Trabajo completado — M abre el tablero",U"Trabajo fallido — M abre el tablero"}
  };
  struct Bounds {
    float width,height;int panels{},pips{};std::size_t glyph_rects{};
    void draw_hud_rect(float x,float y,float w,float h,fury::Color) {
      if(near(h,28.f)) {
        need(near(x,width*.5f-284.f)&&near(y,height-78.f)&&near(w,568.f),
             "Phase guidance retains the exact existing bar rectangle");++panels;
      } else if(near(h,8.f)) {
        need(near(y,height-42.f)&&near(w,20.f),"Tutorial pips retain their existing lane");++pips;
      } else {
        need(near(h,2.f)&&w>=2.f&&w<=10.f,"Phase guidance retains fixed 2px glyphs");
        need(x>=width*.5f-264.f && x+w<=width*.5f+264.f &&
             y>=height-72.f && y+h<=height-58.f,"Complete phase instruction fits its 528px lane");
        ++glyph_rects;
      }
    }
  };
  for(int pi=0;pi<7;++pi) for(int stage=0;stage<4;++stage) {
    const int before=stage;
    const auto g=vaultline::mission_guidance(phases[pi],stage);
    const bool first_board=pi==0&&stage==0;
    need(g.kind==(first_board?MissionGuidanceKind::Board:kinds[pi]),
         "Current phase overrides every stale tutorial stage");
    const bool terminal=pi>=5;
    const auto objective=terminal?MissionObjectiveCue::None:first_board?MissionObjectiveCue::Board:
        pi==4?MissionObjectiveCue::Escape:MissionObjectiveCue::Target;
    need(g.objective==objective,"Only current-job destinations are suppressed for terminal phases");
    need(g.tutorial_pip==(terminal||stage==3?-1:first_board?0:pi==4?2:1),
         "Terminal/completed tutorials have no pips; reset stages display target progress");
    need(stage==before,"Presentation leaves historical onboarding progress unchanged");
    for(int lang=0;lang<2;++lang) {
      const auto text=vaultline::npc_hud_text(vaultline::mission_guidance_text(fury::lang_from_int(lang),g.kind));
      need(text==expected[lang][int(g.kind)],"Each EN/ES phase instruction retains its complete authored text");
      need(!text.empty()&&text.size()<=44&&(float(text.size())*6.f-1.f)*2.f<=528.f,
           "Every phase instruction fits the unchanged codepoint and width budget");
      for(char32_t c:text)need(fury::bitmap_detail::glyph5x7(c)!=nullptr,"Phase guidance needs no font fallback");
      for(int width:{640,1280}) for(float hud_scale:{1.f,1.6f}) for(bool f6:{false,true}) {
        Bounds bounds{float(width),float(width*9/16)};
        vaultline::draw_mission_guidance(bounds,g,fury::lang_from_int(lang),bounds.width,bounds.height,f6);
        need(bounds.panels==1&&bounds.pips==(!f6&&g.tutorial_pip>=0?3:0),
             "F6 occupies only the pip row and never removes the phase instruction");
        need(bounds.glyph_rects>0&&bounds.glyph_rects<=44*21,"Phase guidance retains the existing bounded draw cost");
        const auto layout=vaultline::layout_status_hud(width,width*9/16,hud_scale);
        need(layout.ready_y+22.f<float(width*9/16)-78.f,"Guidance lane stays below ordinary and maximum-scale status HUD");
      }
    }
  }
  need(fury::kBitmapFontMaxGlyphs==12,"Phase guidance does not expand the legacy font cap");
  // Real controller transitions; these are unit fixtures, not an earned native mission.
  fury::HeistController heist;int historical_stage=2;
  heist.update(heist.vault_position,false,0);heist.update(heist.vault_position,true,0);
  heist.update({100,0,100},false,.1f);
  need(vaultline::mission_guidance(heist.phase(),historical_stage).kind==MissionGuidanceKind::Failed,
       "Real breach-abandon failure overrides stale extraction guidance");
  heist.update({},true,0);
  need(vaultline::mission_guidance(heist.phase(),historical_stage).kind==MissionGuidanceKind::Target,
       "Real terminal reset immediately restores target guidance without changing stage2");
  heist.update(heist.vault_position,false,0);heist.update(heist.vault_position,true,0);
  heist.update(heist.vault_position,false,heist.breach_duration);
  heist.update(heist.vault_position,false,heist.loot_duration);
  heist.update(heist.escape_position,false,0);historical_stage=3;
  need(vaultline::mission_guidance(heist.phase(),historical_stage).kind==MissionGuidanceKind::Success,
       "Terminal success identifies this job without reopening onboarding");
  heist.update({},true,0);heist.update(heist.vault_position,false,0);heist.update(heist.vault_position,true,0);
  const auto next=vaultline::mission_guidance(heist.phase(),historical_stage);
  need(next.kind==MissionGuidanceKind::Breach&&next.tutorial_pip==-1&&historical_stage==3,
       "A later active job has truthful phase guidance while onboarding remains complete");
}

void test_approach_interaction_guidance() {
  using Cue = vaultline::ApproachInteractionCue;
  using Target = vaultline::ApproachSecurityTarget;
  using Context = vaultline::ApproachInteractionContext;
  const char* expected[2][11]{
    {"Follow compass to target", "E: BREACH TARGET", "E: BADGE - OPEN GATE",
     "E: BADGE - OPEN GATE / CAMERAS OFF", "E: BREAKER - DISABLE CAMERAS",
     "E: USE VAULT TOOL", "E: USE VAULT TOOL - REPOSITION TO BREACH",
     "E: USE HATCH", "E: USE HATCH - REPOSITION TO BREACH",
     "E: EMERGENCY PANEL", "E: ACCESS SECURITY TERMINAL"},
    {"Sigue la brujula al objetivo", "E: ABRIR OBJETIVO", "E: TARJETA - ABRIR REJA",
     "E: TARJETA - ABRIR REJA / APAGAR CAMARAS", "E: INTERRUPTOR - APAGAR CAMARAS",
     "E: USAR HERRAMIENTA", "E: USAR HERRAMIENTA - REUBICATE PARA ABRIR",
     "E: USAR ESCOTILLA", "E: USAR ESCOTILLA - REUBICATE PARA ABRIR",
     "E: PANEL DE EMERGENCIA", "E: ACCEDER A TERMINAL DE SEGURIDAD"}
  };
  need(vaultline::approach_interaction_cue(Context{})==Cue::Neutral,
       "Unproved default context never advertises an interaction");
  const Target targets[]{Target::Unknown,Target::None,Target::Badge,Target::Hatch,
                         Target::Tool,Target::Emergency,Target::Terminal};
  const Cue range_cues[]{Cue::Neutral,Cue::Breach,Cue::Badge,Cue::HatchBlocksBreach,
                        Cue::ToolBlocksBreach,Cue::Emergency,Cue::Terminal};
  const Cue far_cues[]{Cue::Neutral,Cue::Neutral,Cue::Badge,Cue::Hatch,
                      Cue::Tool,Cue::Emergency,Cue::Terminal};
  for(int t=0;t<7;++t) for(bool in_range:{false,true}) for(bool breaker:{false,true})
      for(bool site0:{false,true}) for(unsigned gates=0;gates<16;++gates) {
    Context c;c.blocked=(gates&1)!=0;c.vehicle=(gates&2)!=0;
    c.door_snap=(gates&4)!=0;c.connected_join=(gates&8)!=0;
    c.breach_in_range=in_range;c.live_breaker=breaker;c.badge_breaker_site0=site0;
    c.security=targets[t];
    const Context before=c;
    Cue want=in_range?range_cues[t]:far_cues[t];
    if(breaker) want=targets[t]==Target::None?Cue::Breaker:
        targets[t]==Target::Badge&&site0?Cue::BadgeAndBreaker:Cue::Neutral;
    if(gates) want=Cue::Neutral;
    need(vaultline::approach_interaction_cue(c)==want,
         "Known selections, compound actions and every independent context gate choose truthful copy");
    need(c.blocked==before.blocked&&c.vehicle==before.vehicle&&c.door_snap==before.door_snap&&
         c.connected_join==before.connected_join&&c.security==before.security&&
         c.breach_in_range==before.breach_in_range&&c.live_breaker==before.live_breaker&&
         c.badge_breaker_site0==before.badge_breaker_site0,
         "Resolving an interaction is read-only");
  }
  Context unknown;unknown.blocked=false;unknown.security=static_cast<Target>(99);
  unknown.breach_in_range=true;
  need(vaultline::approach_interaction_cue(unknown)==Cue::Neutral,
       "A future or invalid selector value cannot fall through to breach");
  struct Bounds {
    float width,height;int panels{},pips{};
    void draw_hud_rect(float x,float y,float w,float h,fury::Color) {
      if(near(h,28.f)) {
        need(near(x,width*.5f-284.f)&&near(y,height-78.f)&&near(w,568.f),
             "Interaction guidance preserves its existing plate");++panels;
      } else if(near(h,8.f)) {
        need(near(y,height-42.f)&&near(w,20.f),"Interaction pips stay in their accepted lane");++pips;
      } else {
        need(near(h,2.f)&&x>=width*.5f-264.f&&x+w<=width*.5f+264.f&&
             y>=height-72.f&&y+h<=height-58.f,
             "Complete interaction text fits its existing 528px lane with 2px glyphs");
      }
    }
  };
  for(int cue=0;cue<11;++cue) for(int lang=0;lang<2;++lang) {
    const auto c=static_cast<Cue>(cue);const auto l=fury::lang_from_int(lang);
    const auto text=vaultline::npc_hud_text(vaultline::approach_interaction_text(l,c));
    need(text==vaultline::npc_hud_text(expected[lang][cue]),"Exact complete approved EN/ES interaction copy");
    need(text.size()<=44&&(float(text.size())*6.f-1.f)*2.f<=528.f,
         "Interaction copy fits unchanged codepoint and pixel budgets");
    for(char32_t ch:text)need(fury::bitmap_detail::glyph5x7(ch)!=nullptr,"Interaction copy needs no fallback glyph");
    for(int phase=0;phase<8;++phase) {
      const auto kind=static_cast<vaultline::MissionGuidanceKind>(phase);
      if(kind!=vaultline::MissionGuidanceKind::Approach)
        need(std::string(vaultline::mission_guidance_text(l,kind,c))==
             vaultline::mission_guidance_text(l,kind),"Local interaction cue cannot replace another phase's instruction");
    }
    for(int width:{640,1280}) for(int stage:{0,1,2,3}) for(bool f6:{false,true}) {
      const auto g=vaultline::mission_guidance(fury::HeistPhase::Approach,stage);
      Bounds b{float(width),float(width*9/16)};
      vaultline::draw_mission_guidance(b,g,l,b.width,b.height,f6,c);
      need(b.panels==1&&b.pips==(!f6&&stage<3?3:0),
           "Interaction text does not change phase, objective, tutorial history or F6 precedence");
    }
  }
}

void test_native_guidance_pixels() {
  SDL_setenv("SDL_VIDEODRIVER","dummy",1);
  need(SDL_Init(SDL_INIT_VIDEO)==0,"Initialize native guidance pixel witness");
  for(int width:{640,1280}) {
    const int height=width*9/16;
    SDL_Window* window=SDL_CreateWindow("Phase guidance unit fixture",0,0,width,height,SDL_WINDOW_HIDDEN);
    need(window!=nullptr,"Create native guidance fixture window");
    {
      fury::Renderer r;need(r.create(window,width,height,false,fury::RenderBackendKind::Software),"Create native guidance renderer");
      for(int kind=0;kind<8;++kind) for(int lang=0;lang<2;++lang)
          for(int cue=0;cue<(kind==int(vaultline::MissionGuidanceKind::Approach)?11:1);++cue) {
        const auto action=static_cast<vaultline::ApproachInteractionCue>(cue);
        const auto k=static_cast<vaultline::MissionGuidanceKind>(kind);
        const vaultline::MissionGuidance g{k,vaultline::MissionObjectiveCue::None,-1};
        r.begin_frame({0,0,0,255});
        vaultline::draw_mission_guidance(r,g,fury::lang_from_int(lang),float(width),float(height),false,action);
        std::vector<std::uint8_t> rgb;int rw{},rh{};
        need(r.read_rgb_framebuffer(rgb,rw,rh)&&rw==width&&rh==height,"Read actual guidance fixture pixels");
        const auto text=vaultline::npc_hud_text(vaultline::mission_guidance_text(fury::lang_from_int(lang),k,action));
        float x=float(width)*.5f-264.f;const float y=float(height)-72.f;
        for(char32_t c:text) {
          const auto* glyph=fury::bitmap_detail::glyph5x7(c);
          for(int gy=0;gy<7;++gy)for(int gx=0;gx<5;++gx)if(glyph[gy]&(1u<<(4-gx))) {
            const auto i=std::size_t(int(y+float(gy)*2+1)*width+int(x+float(gx)*2+1))*3;
            need(rgb[i]>190&&rgb[i+1]>210&&rgb[i+2]>220,
                 "Every lit cell of every full EN/ES phase instruction reaches the native framebuffer");
          }
          x+=12.f;
        }
        r.end_frame();
      }
    }
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
}
// These are deterministic HUD unit fixtures, not evidence of natural gameplay.
using NoticeState = vaultline::ResultNoticeState;
using NoticeContext = vaultline::ResultNoticeContext;
using NoticeRect = vaultline::ResultNoticeRect;
using NoticeLayout = vaultline::ResultNoticeLayout;
using NoticeKind = vaultline::ResultNoticeKind;

const char32_t* const achievement_titles[]{U"First Score",U"Quiet Withdrawal",
    U"Night Vault Cleared",U"Harbor Millionaire",U"Career Operator",U"Lesson Learned"};

bool overlaps_notice(NoticeRect a,NoticeRect b) {
  return a.x+a.width>b.x && b.x+b.width>a.x &&
         a.y+a.height>b.y && b.y+b.height>a.y;
}
bool contains_notice(NoticeRect outer,NoticeRect inner) {
  constexpr float e=.001f;
  return inner.x>=outer.x-e && inner.y>=outer.y-e &&
      inner.x+inner.width<=outer.x+outer.width+e &&
      inner.y+inner.height<=outer.y+outer.height+e;
}
// Independent oracle for critical rendered content. It intentionally does not
// ask result_notice_obstacles to validate its own reservations.
std::vector<NoticeRect> notice_critical_regions(const NoticeContext& c) {
  const auto s=vaultline::layout_status_hud(c.width,c.height,c.hud_scale);
  const auto a=vaultline::layout_status_auxiliary(s,c.width,c.in_vehicle);
  const float W=float(c.width),H=float(c.height);
  std::vector<NoticeRect> out{{W-166,16,150,150},{16,a.ready_y,220,22},
      {W-56,230,40,40},{16,H-40,160,24},{W-130,H-40,114,24}};
  for(std::size_t row=0;row<6;++row) {
    out.push_back({s.text_left,s.row_y(row),s.text_right-s.text_left,7*s.glyph});
    out.push_back({s.bar_x,s.meter_y(row),s.bar_width,2*s.scale});
  }
  if(c.in_vehicle) {
    out.push_back({16,152+a.offset,180,22});
    out.push_back({16,178+a.offset,180,28});
  }
  if(c.tier_visible)out.push_back({a.tier_x,a.tier_y,200,18});
  if(c.crouching)out.push_back({a.crouch_x,a.crouch_y,120,18});
  if(c.banter)out.push_back({W*.5f-260,H-148,520,26});
  if(c.objective) {
    out.push_back({W*.5f-130,H-114,260,18});
    // Cover every possible compass marker and forward-notch position.
    out.push_back({W*.5f-128,H-110,256,10});
    out.push_back({W*.5f-123,H-118,246,6});
  }
  if(c.pursuit)out.push_back({W-120,178,56,40});
  if(c.alarm)out.push_back({W-56,178,40,40});
  if(c.guidance)out.push_back({W*.5f-284,H-78,568,28});
  if(c.door)out.push_back({W*.5f-90,H-118,180,34});
  else if(c.breaker)out.push_back({W*.5f-110,H-118,220,34});
  else if(c.closed_shop)out.push_back({W*.5f-130,H-118,260,34});
  if(c.interior&&!c.door)out.push_back({W*.5f-60,H-112,120,18});
  if(c.safehouse)out.push_back({W*.5f-200,H-178,400,42});
  out.insert(out.end(),c.protected_rects.begin(),c.protected_rects.end());
  return out;
}
bool same_notice_state(const NoticeState& a,const NoticeState& b) {
  return a.result_timer==b.result_timer && a.success==b.success && a.finale==b.finale &&
      a.achievement_timer==b.achievement_timer && a.achievement==b.achievement &&
      a.phase==b.phase && a.language==b.language;
}
struct NoticeDraws {
  struct Draw { NoticeRect rect;fury::Color color; };
  std::vector<Draw> calls;
  void draw_hud_rect(float x,float y,float w,float h,fury::Color color) {
    calls.push_back({{x,y,w,h},color});
  }
};
void check_notice_layout(const NoticeState& state,const NoticeContext& c,const NoticeLayout& box) {
  bool result=false,achievement=false;
  for(const auto& row:box.rows) {
    need(!row.text.empty(),"A visible notice never substitutes an empty or truncated row");
    const bool is_result=row.kind==NoticeKind::Result;
    need(!(is_result?result:achievement),"Each event has at most one displayed notice row");
    (is_result?result:achievement)=true;
    need((is_result?state.result_timer:state.achievement_timer)>0,
         "Expired events cannot remain in the visible notice rows");
    need(near(row.fade,std::clamp((is_result?state.result_timer/.4f:state.achievement_timer/.45f),0.f,1.f)),
         "Result and achievement retain their distinct original fade windows");
    if(!is_result)need(row.text==achievement_titles[int(state.achievement)],
         "Every achievement keeps its complete authored English title in both languages");
    need(vaultline::result_notice_text_width(row.text,box.glyph)+28<=box.rect.width+.001f,
         "Every visible notice title fits completely inside its padded card");
  }
  need(box.result_suppressed==(state.result_timer>0&&!result) &&
       box.achievement_suppressed==(state.achievement_timer>0&&!achievement),
       "Suppression explicitly describes active events omitted from this frame");
  if(box.rows.empty())return;
  need(!c.blocked && box.glyph>=2.f,"Visible notices honor the HUD gate and at least 2px glyphs");
  need(contains_notice({8,8,float(c.width)-16,float(c.height)-16},box.rect),
       "Every visible notice card stays inside the native viewport margin");
  need(near(box.rect.height,box.row_height*float(box.rows.size())) && box.row_height>=7*box.glyph,
       "Notice rows are separate and retain enough height for complete glyphs");
  for(const auto& critical:notice_critical_regions(c))
    need(!overlaps_notice(box.rect,critical),"No visible notice card intersects critical HUD content");
}

void test_result_notice_semantics() {
  using fury::HeistPhase;
  NoticeContext c;c.width=1280;c.height=720;c.guidance=true;
  std::size_t longest=0;
  for(int id=0;id<int(fury::AchievementId::Count);++id) {
    const auto text=vaultline::npc_hud_text(fury::achievement_title(static_cast<fury::AchievementId>(id)));
    need(text==achievement_titles[id],"All six authored achievement titles remain exact");
    longest=std::max(longest,text.size());
    for(char32_t ch:text)need(fury::bitmap_detail::glyph5x7(ch)!=nullptr,"Achievement titles use existing supported glyphs");
  }
  need(longest==19 && vaultline::result_notice_text_width(achievement_titles[2],2)==226,
       "The longest authored achievement is 19 codepoints and exactly 226px at 2px");
  const char32_t* expected[2][4]{{U"JOB FAILED",U"JOB COMPLETE",U"LAST JOB FAILED",U"LAST JOB COMPLETE"},
      {U"TRABAJO FALLIDO",U"TRABAJO COMPLETADO",U"ANTERIOR: FALLO",U"ANTERIOR: COMPLETADO"}};
  for(int lang=0;lang<2;++lang)for(bool success:{false,true})
      for(HeistPhase phase:{HeistPhase::Failed,HeistPhase::Success,HeistPhase::Idle,HeistPhase::Approach,HeistPhase::Breach}) {
    NoticeState s{2.2f,success,false,3.4f,fury::AchievementId::FirstFail,phase,fury::lang_from_int(lang)};
    const auto before=s;
    const auto box=vaultline::layout_result_notice(s,c);
    need(box.rows.size()==2&&!box.compact,"Both active events receive complete separate rows when room permits");
    const bool current=phase==(success?HeistPhase::Success:HeistPhase::Failed);
    need(box.rows[0].kind==NoticeKind::Result && box.rows[0].text==expected[lang][int(success)+(current?0:2)],
         "Current result is truthful; reset, reselection and mismatched terminal phases label the historical job");
    NoticeDraws draw;vaultline::draw_result_notice(draw,s,box);
    need(same_notice_state(s,before),"Formatting, placement and drawing never mutate phase, event, language or timers");
    check_notice_layout(s,c,box);
  }
  for(int lang=0;lang<2;++lang) {
    NoticeState s{4,true,true,3.4f,fury::AchievementId::FinaleClear,HeistPhase::Success,fury::lang_from_int(lang)};
    auto box=vaultline::layout_result_notice(s,c);
    need(box.rows[0].text==U"Pierline holds the Harbor","Roomy finale result retains its complete authored ending line");
    s.phase=HeistPhase::Idle;box=vaultline::layout_result_notice(s,c);
    need(box.rows[0].text==expected[lang][3],"Reset after finale presents the historical completion rather than a current ending");
  }
  // Real controller resets/reselection with unexpired presentation-only timers.
  fury::HeistController heist;
  heist.update(heist.vault_position,false,0);heist.force_fail();
  NoticeState s{2.2f,false,false,3.4f,fury::AchievementId::FirstFail,heist.phase(),fury::Lang::En};
  need(vaultline::layout_result_notice(s,c).rows[0].text==U"JOB FAILED","A real controller failure is current");
  heist.update({},true,0);s.phase=heist.phase();
  need(vaultline::layout_result_notice(s,c).rows[0].text==U"LAST JOB FAILED","Early real-controller reset retains only historical result wording");
  heist.update(heist.vault_position,false,0);s.phase=heist.phase();
  need(s.phase==HeistPhase::Approach&&vaultline::layout_result_notice(s,c).rows[0].text==U"LAST JOB FAILED",
       "A newly approached job cannot inherit current failure wording from the old timer");
  need(near(s.result_timer,2.2f)&&near(s.achievement_timer,3.4f),"Reset presentation does not extend either original timer");
  need(fury::kBitmapFontMaxGlyphs==12,"Result notices do not expand the accepted legacy font cap");
}

void test_result_notice_geometry() {
  using fury::HeistPhase;
  for(int width:{640,1280})for(float scale:{1.f,1.35f,1.6f})
      for(int lang=0;lang<2;++lang)for(int id=0;id<6;++id)for(int events=1;events<=3;++events)
      for(HeistPhase phase:{HeistPhase::Failed,HeistPhase::Success,HeistPhase::Idle,HeistPhase::Approach})
      for(int variant=0;variant<15;++variant) {
    NoticeState s{events&1?2.2f:0,phase==HeistPhase::Success,id==2,
        events&2?3.4f:0,static_cast<fury::AchievementId>(id),phase,fury::lang_from_int(lang)};
    NoticeContext c;c.width=width;c.height=width*9/16;c.hud_scale=scale;c.guidance=true;
    c.objective=phase!=HeistPhase::Failed&&phase!=HeistPhase::Success;
    c.in_vehicle=variant==1||variant==2||variant==14;c.crouching=variant==3||variant==14;
    c.tier_visible=variant!=2;c.door=variant==4||variant==14;c.breaker=variant==5;
    c.interior=variant==6||variant==14;c.closed_shop=variant==7;c.banter=variant==8||variant==14;
    c.pursuit=variant==9||variant==14;c.alarm=variant==10||variant==14;c.safehouse=variant==11||variant==14;
    if(variant==12)c.protected_rects.push_back({float(width)*.5f-160,56,320,28});
    if(variant==13)c.protected_rects.push_back({float(width)*.5f-110,float(c.height)-160,220,24});
    const auto before=s;const auto box=vaultline::layout_result_notice(s,c);
    check_notice_layout(s,c,box);
    need(same_notice_state(before,s),"Geometry matrix preserves all event state");
    if(variant==0)need(!box.rows.empty(),"Ordinary active notices remain visible at both native sizes and required scales");
  }
  // Dense samples around scale clamping, relocation thresholds and small sizes.
  for(int width:{63,64,320,625,626,639,640,641,959,960,1279,1280,1281})
      for(float scale:{.5f,1.f,1.0001f,1.2999f,1.3f,1.3499f,1.35f,1.3501f,1.5999f,1.6f,1.6001f,2.f}) {
    NoticeContext c;c.width=width;c.height=width<640?360:width*9/16;c.hud_scale=scale;
    c.guidance=true;c.objective=true;c.in_vehicle=true;c.crouching=true;
    NoticeState s{.2f,false,false,.225f,fury::AchievementId::FinaleClear,HeistPhase::Idle,fury::Lang::Es};
    check_notice_layout(s,c,vaultline::layout_result_notice(s,c));
  }
}

void test_result_notice_priority_expiry() {
  using fury::HeistPhase;
  NoticeContext c;c.width=640;c.height=360;c.guidance=true;c.objective=true;
  NoticeState s{2.2f,false,false,3.4f,fury::AchievementId::FinaleClear,HeistPhase::Failed,fury::Lang::En};
  auto box=vaultline::layout_result_notice(s,c);
  need(box.compact&&box.rows.size()==1&&box.rows[0].kind==NoticeKind::Achievement&&box.result_suppressed,
       "When only one compact row fits, current terminal result gives priority to the unique achievement");
  s.phase=HeistPhase::Idle;box=vaultline::layout_result_notice(s,c);
  need(box.compact&&box.rows.size()==1&&box.rows[0].text==U"LAST JOB FAILED"&&box.achievement_suppressed,
       "After early reset the compact lane prioritizes the historical result for its original lifetime");
  s.result_timer=0;box=vaultline::layout_result_notice(s,c);
  need(box.rows.size()==1&&box.rows[0].text==achievement_titles[2],
       "Remaining achievement becomes visible only while its own timer remains active");
  for(int width:{625,626}) {
    c.width=width;box=vaultline::layout_result_notice(s,c);
    need((!box.rows.empty())==(width==626),"Longest compact title requires its full 226px plus 28px padding at the fit boundary");
    check_notice_layout(s,c,box);
  }
  c.width=640;s.result_timer=2.2f;s.achievement_timer=3.4f;
  for(const char* reason:{"Active Q dialogue suppresses transient notices","An open modal suppresses transient notices",
                          "Hidden gameplay HUD suppresses transient notices"}) {
    // The pure helper receives the caller's combined blocked gate; production
    // caller wiring is reviewed separately from this fixture.
    c.blocked=true;const auto before=s;box=vaultline::layout_result_notice(s,c);
    NoticeDraws draw;vaultline::draw_result_notice(draw,s,box);
    need(box.rows.empty()&&box.result_suppressed&&box.achievement_suppressed&&draw.calls.empty(),reason);
    need(same_notice_state(s,before),"Suppression never pauses, extends or consumes the event owners");
  }
  c.blocked=false;c.protected_rects={{0,0,640,360}};box=vaultline::layout_result_notice(s,c);
  need(box.rows.empty()&&box.result_suppressed&&box.achievement_suppressed,
       "If every legal slot is protected, active notices are explicitly suppressed instead of overlapping");
  fury::AchievementBanner banner;banner.trigger(fury::AchievementId::FinaleClear);
  need(near(banner.timer,3.4f),"The original achievement owner retains its 3.4-second duration");
  for(float dt:{.4f,2.8f,.3f}) {
    banner.update(dt);s.achievement_timer=banner.timer;s.result_timer=std::max(0.f,s.result_timer-dt);
    box=vaultline::layout_result_notice(s,c);need(box.rows.empty(),"Hidden notices continue to expire through their original owners");
  }
  c.protected_rects.clear();box=vaultline::layout_result_notice(s,c);
  need(box.rows.empty()&&!box.result_suppressed&&!box.achievement_suppressed,
       "Clearing an obstruction cannot replay or queue notices that expired while hidden");
  for(float result:{-1.f,0.f,.001f,.2f,.4f,2.2f,4.f})
      for(float achievement:{-1.f,0.f,.001f,.225f,.45f,3.4f}) {
    c.width=1280;c.height=720;c.objective=false;
    s.result_timer=result;s.achievement_timer=achievement;
    box=vaultline::layout_result_notice(s,c);check_notice_layout(s,c,box);
    need(box.rows.size()==std::size_t((result>0)+(achievement>0)),"Timer sign alone determines event expiry in an unobstructed layout");
    NoticeDraws draws;vaultline::draw_result_notice(draws,s,box);
    for(std::size_t ri=0;ri<box.rows.size();++ri) {
      const auto& row=box.rows[ri];bool saw_text=false,saw_panel=false;
      const float top=box.rect.y+float(ri)*box.row_height,bottom=top+box.row_height;
      for(const auto& draw:draws.calls)if(draw.rect.y>=top&&draw.rect.y<bottom) {
        if(draw.color.r==238&&draw.color.g==244&&draw.color.b==250) {
          saw_text=true;need(draw.color.a==std::uint8_t(255*row.fade),"Every glyph draw respects its own original timer fade");
        }
        if(draw.color.r==9&&draw.color.g==15&&draw.color.b==24) {
          saw_panel=true;need(draw.color.a==std::uint8_t(235*row.fade),"The complete notice backing fades with its event text");
        }
      }
      need(saw_text&&saw_panel,"Each unexpired fade sample retains complete text and its backing");
    }
  }
  s.result_timer=2.2f;s.achievement_timer=3.4f;s.phase=HeistPhase::Failed;
  box=vaultline::layout_result_notice(s,c);const auto first=box.rect;
  c.protected_rects={{first.x+first.width,first.y,1,first.height}};
  auto touching=vaultline::layout_result_notice(s,c);
  need(near(first.x,touching.rect.x)&&near(first.y,touching.rect.y),"Edge-touching protected rectangles do not invent an intersection");
  c.protected_rects[0].x-=.01f;
  auto blocked=vaultline::layout_result_notice(s,c);check_notice_layout(s,c,blocked);
  need(blocked.rows.empty()||!overlaps_notice(first,blocked.rect),"Even a fractional protected overlap rejects the previously selected slot");
}

void test_native_result_notice_pixels() {
  SDL_setenv("SDL_VIDEODRIVER","dummy",1);
  need(SDL_Init(SDL_INIT_VIDEO)==0,"Initialize native result/achievement unit pixel witness");
  for(int width:{640,1280}) {
    const int height=width*9/16;
    SDL_Window* window=SDL_CreateWindow("Result and achievement unit fixture",0,0,width,height,SDL_WINDOW_HIDDEN);
    need(window!=nullptr,"Create notice unit fixture window");
    {
      fury::Renderer r;need(r.create(window,width,height,false,fury::RenderBackendKind::Software),"Create actual Fury software notice renderer");
      for(float scale:{1.f,1.35f,1.6f})for(int lang=0;lang<2;++lang)for(int id=0;id<6;++id)
          for(int events=1;events<=3;++events) {
        NoticeState s{events&1?2.2f:0,id!=5,id==2,events&2?3.4f:0,
            static_cast<fury::AchievementId>(id),id==5?fury::HeistPhase::Failed:fury::HeistPhase::Success,
            fury::lang_from_int(lang)};
        NoticeContext c;c.width=width;c.height=height;c.hud_scale=scale;c.guidance=true;
        const auto box=vaultline::layout_result_notice(s,c);check_notice_layout(s,c,box);
        need(!box.rows.empty(),"Native baseline fixtures render at least one readable notice");
        r.begin_frame({0,0,0,255});
        for(auto critical:notice_critical_regions(c))r.draw_hud_rect(critical.x,critical.y,critical.width,critical.height,{31,73,109,255});
        std::vector<std::uint8_t> before;int bw{},bh{};
        need(r.read_rgb_framebuffer(before,bw,bh),"Read protected native pixels before result notices");
        vaultline::draw_result_notice(r,s,box);
        std::vector<std::uint8_t> rgb;int rw{},rh{};
        need(r.read_rgb_framebuffer(rgb,rw,rh)&&rw==width&&rh==height,"Read native result/achievement framebuffer");
        for(std::size_t ri=0;ri<box.rows.size();++ri) {
          const auto& row=box.rows[ri];
          float x=box.rect.x+14;const float y=box.rect.y+float(ri)*box.row_height+(box.row_height-7*box.glyph)*.5f;
          for(char32_t ch:row.text) {
            const auto* glyph=fury::bitmap_detail::glyph5x7(ch);
            need(glyph!=nullptr,"Every notice witness character has its actual authored bitmap");
            for(int gy=0;gy<7;++gy)for(int gx=0;gx<5;++gx)if(glyph[gy]&(1u<<(4-gx))) {
              const int px=int(x+(float(gx)+.5f)*box.glyph),py=int(y+(float(gy)+.5f)*box.glyph);
              const auto p=std::size_t(py*width+px)*3;
              need(rgb[p]>220&&rgb[p+1]>230&&rgb[p+2]>240,
                   "Every lit cell of each complete result and achievement reaches the actual software framebuffer");
            }
            x+=6*box.glyph;
          }
          if(row.kind==NoticeKind::Achievement)for(int pip=0;pip<6;++pip) {
            const int px=int(box.rect.x+14+pip*6+1),py=int(box.rect.y+float(ri+1)*box.row_height-1);
            const auto p=std::size_t(py*width+px)*3;
            need((rgb[p]>150)==(pip==id),"Exactly the original achievement ID pip is highlighted in native pixels");
          }
        }
        for(const auto critical:notice_critical_regions(c)) {
          const int left=std::max(0,int(std::ceil(critical.x))),right=std::min(width,int(std::floor(critical.x+critical.width)));
          const int top=std::max(0,int(std::ceil(critical.y))),bottom=std::min(height,int(std::floor(critical.y+critical.height)));
          bool intact=true;
          for(int y=top;y<bottom&&intact;++y)for(int x=left;x<right;++x) {
            const auto p=std::size_t(y*width+x)*3;
            if(rgb[p]!=before[p]||rgb[p+1]!=before[p+1]||rgb[p+2]!=before[p+2]){intact=false;break;}
          }
          need(intact,"Native result/achievement drawing preserves every protected HUD pixel");
        }
        NoticeDraws draws;vaultline::draw_result_notice(draws,s,box);
        for(const auto& draw:draws.calls)need(contains_notice(box.rect,draw.rect),"Text, backgrounds, accents and ID pips stay within the tested notice card");
        r.end_frame();
      }
    }
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
}
} // namespace

int main(int, char**) {
  try {
    test_money();test_ratios_phases();test_crew_and_budget();test_real_heist_values();test_native_text_pixels();test_onboarding_instructions();test_adjacent_layout();test_mission_guidance();test_approach_interaction_guidance();test_native_guidance_pixels();
    test_result_notice_semantics();test_result_notice_geometry();test_result_notice_priority_expiry();test_native_result_notice_pixels();
    std::cout << "status_hud: " << checks << " checks passed; exact formatting, legacy semantics, finite text bounds, result/achievement isolation and native pixels\n";
    return 0;  // SDL2main renames main on Windows; no implicit return 0
  } catch (const std::exception& e) { std::cerr << e.what() << '\n';return 1; }
}
