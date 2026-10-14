#include "../apps/vaultline/map_hud.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::size_t checks{}, max_glyphs{}, max_rectangles{};
void need(bool result, const char* message) {
  ++checks;
  if (!result) throw std::runtime_error(message);
}
struct Rect { float x, y, w, h; fury::Color color; };
struct Recorder {
  std::vector<Rect> rects;
  void draw_hud_rect(float x, float y, float w, float h, fury::Color c) {
    rects.push_back({x,y,w,h,c});
  }
};
const vaultline::MapDistrictNames names{
  "Harbor Metro", "Ridge Pier", "Ashcourt Market", "Harbor Depot", "Harbor Loft", "North Quay"};

std::size_t glyphs(const vaultline::MapHudText& t) {
  std::size_t n = t.heading.size() + 6; // Six region numbers share the budget.
  for (const auto& s : t.names) n += s.size();
  for (const auto& s : t.footer) n += s.size();
  return n;
}

float measured_right(std::u32string_view text, float scale) {
  Recorder r;
  std::size_t budget = vaultline::kMapHudGlyphBudget;
  vaultline::draw_npc_hud_text(r, 0, 0, text, {255,255,255,255}, scale, budget);
  need(budget == vaultline::kMapHudGlyphBudget - text.size(), "Actual glyph emission must consume the complete string");
  float right = 0;
  for (const auto& rect : r.rects) right = std::max(right, rect.x + rect.w);
  return right;
}

void text_cases() {
  using R = vaultline::MapHudReason;
  need(fury::kBitmapFontMaxGlyphs == 12, "Global font cap remains untouched");
  for (float cd : {std::numeric_limits<float>::denorm_min(), 0.000001f, 0.001f, .01f, .999f, 1.f})
    need(vaultline::map_hud_cooldown_seconds(cd) == 1, "Every positive subsecond cooldown displays at least 1S");
  need(vaultline::map_hud_cooldown_seconds(1.001f) == 2, "Fractional countdown rounds up");
  need(vaultline::map_hud_cooldown_seconds(44.001f) == 45, "Countdown ceiling is independent of log rounding");
  need(vaultline::map_hud_cooldown_seconds(0.f) == 0, "Expired countdown is zero");
  for (auto lang : {fury::Lang::En, fury::Lang::Es})
  for (int focus = 0; focus < 6; ++focus)
  for (bool safe : {false, true})
  for (int cash : {std::numeric_limits<int>::min(), -1, 0, 249, 250, 15982, std::numeric_limits<int>::max()})
  for (float cd : {0.f, 0.000001f, .01f, 1.f, 1.001f, 10.f, 20.f, 30.f, 45.f}) {
    const auto t = vaultline::map_hud_text(lang, names, focus, safe, cd, cash, 250);
    const auto expected = focus == 4 ? R::LoftFocus : !safe ? R::OutsideLoft :
        cd > 0 ? R::Cooldown : cash < 250 ? R::InsufficientCash : R::Ready;
    need(t.reason == expected, "Live text follows exact existing handler denial priority");
    need(t.focus == focus, "Rendering preserves focus identity");
    need(t.footer[0].find(vaultline::npc_hud_text(names[focus])) != std::u32string::npos,
         "Selected row includes the entire authored destination");
    need(t.footer[1].find(vaultline::npc_hud_text(std::to_string(cash))) != std::u32string::npos,
         "Actual full signed cash value is never abbreviated or clamped");
    need(t.footer[1].find(U"$250") != std::u32string::npos, "Fare is visible in values");
    if (cd > 0) need(t.footer[1].find(U" 0S") == std::u32string::npos, "Blocked subsecond countdown must not say 0S");
    if (focus == 4) need(t.footer[2] == (lang == fury::Lang::Es
        ? U"HARBOR LOFT: ELIGE OTRO DISTRITO" : U"HARBOR LOFT: CHOOSE ANOTHER DISTRICT"),
        "Loft focus does not claim current physical location, including outside the loft");
    need(t.footer[3].find(U"RETURN") != std::u32string::npos &&
         t.footer[3].find(U"TAB/ESC") != std::u32string::npos,
         "Controls advertise the accepted ordinary Return and both close keys");
    need(t.footer[3].find(U"KEYPAD") == std::u32string::npos, "Controls do not promise unsupported keypad Enter");
    auto inspect = [&](const std::u32string& s) {
      for (auto c : s) need(fury::bitmap_detail::glyph5x7(c) != nullptr, "Every composed character uses an existing supported glyph");
      need(measured_right(s, 1.f) <= vaultline::map_hud_text_width(s) + .001f,
           "Measured ink from real glyph rectangles must fit the computed cell width");
    };
    inspect(t.heading);
    for (int i = 0; i < 6; ++i) {
      need(t.names[i] == vaultline::npc_hud_text(std::to_string(i+1) + " " + std::string(names[i])),
           "All six legend names and numbers retain complete source names");
      inspect(t.names[i]);
    }
    for (const auto& s : t.footer) inspect(s);
    max_glyphs = std::max(max_glyphs, glyphs(t));
    need(glyphs(t) <= vaultline::kMapHudGlyphBudget, "Complete composed copy plus six digits must fit one shared budget");
  }
  const auto dynamic = vaultline::map_hud_text(fury::Lang::En, names, 0, true, 0, 300, 301);
  need(dynamic.footer[1].find(U"$301") != std::u32string::npos &&
       dynamic.footer[2] == U"NEED $301 TO TRAVEL", "Display derives the fare rather than hardcoding it");
}

void layout_cases() {
  // Exact source-defined district centers/extents, solely to independently check
  // new text against unchanged rendered geography and its selected outline.
  const float cx[] = {0,95,-88,58,42,10}, cz[] = {0,8,42,-48,52,96};
  const float hx[] = {36,28,26,20,14,30}, hz[] = {28,22,22,18,12,24};
  for (auto dims : {std::pair{640,360}, std::pair{1280,720}})
  for (auto lang : {fury::Lang::En, fury::Lang::Es})
  for (int focus = 0; focus < 6; ++focus)
  for (int hsi = 100; hsi <= 160; ++hsi) {
    const float w = float(dims.first), h = float(dims.second), hs = hsi / 100.f;
    const float mx = .08f*w, my = .08f*h, mw = .84f*w, mh = .72f*h;
    std::array<Rect,6> regions;
    float top = h;
    for (int i = 0; i < 6; ++i) {
      const float x = mx + 10.f + (cx[i]+120.f)/250.f*(mw-20.f);
      const float y = my + 32.f + (cz[i]+60.f)/180.f*(mh-48.f);
      const float rw = hx[i]*2.f/250.f*(mw-20.f), rh = hz[i]*2.f/180.f*(mh-48.f);
      regions[i] = {x-rw*.5f,y-rh*.5f,rw,rh,{}};
      top = std::min(top, regions[i].y-3.f);
    }
    const auto t = vaultline::map_hud_text(lang,names,focus,true,45.f,std::numeric_limits<int>::min(),250);
    const auto b = vaultline::layout_map_hud(dims.first,dims.second,hs,top,t);
    need(b.legend_scale >= 1.5f && b.footer_scale >= 1.5f, "Both supported viewports retain minimum 1.5px glyph scale");
    need(b.heading_y >= 0 && b.heading_y+7*b.heading_scale <= b.name_y[0]-3.f,
         "Heading remains inside viewport and clears legend");
    for (int i=0;i<6;++i) {
      need(b.name_x[i] >= mx && b.name_x[i]+measured_right(t.names[i],b.legend_scale) <= mx+mw+.001f,
           "Actual full legend ink fits the unchanged map width");
      need(b.name_y[i]+7*b.legend_scale <= top-2.f+.001f,
           "Legend leaves two pixels above all possible selected district outlines");
      if ((i+1)%3 != 0 || dims.first >= 960) {
        if (i<5) need(b.name_y[i]!=b.name_y[i+1] ||
            b.name_x[i]+measured_right(t.names[i],b.legend_scale)+4.f <= b.name_x[i+1]+.001f,
            "Complete adjacent names have visible horizontal separation");
      }
    }
    need(b.footer_y >= my+mh && b.footer_y+b.footer_height <= h-8.f+.001f,
         "Footer remains below unchanged map geography with bottom margin");
    for (const auto& s : t.footer)
      need(measured_right(s,b.footer_scale)+6.f*b.footer_scale <= b.footer_width+.001f,
           "Complete long footer text fits with both horizontal paddings");
    Recorder r;
    std::size_t budget = vaultline::kMapHudGlyphBudget;
    for (int i=0;i<6;++i) {
      const auto begin = r.rects.size();
      const float px=regions[i].x+4.f, py=regions[i].y+4.f;
      vaultline::draw_map_index(r,px,py,i,i==focus,budget);
      need(r.rects.size()>begin,"Every region number actually emits ink");
      for(std::size_t j=begin;j<r.rects.size();++j) {
        const auto q=r.rects[j];
        need(q.x>=px && q.y>=py && q.x+q.w<=px+18.f+.001f && q.y+q.h<=py+12.f+.001f,
             "Actual numeral rectangles fit the existing 18x12 pip");
      }
    }
    vaultline::draw_map_hud(r,b,t,budget);
    const float marker_x=b.name_x[focus]-4.f, marker_y=b.name_y[focus]+2.f*b.legend_scale;
    need(marker_x>=mx && marker_x+2.f<=b.name_x[focus]-2.f+.001f &&
         marker_y+3.f*b.legend_scale<=top-2.f+.001f,
         "Selected marker fits the left gutter without touching text or map geometry");
    int marker_count=0;
    for(const auto& q:r.rects) if(q.x==marker_x && q.y==marker_y && q.w==2.f &&
        q.h==3.f*b.legend_scale) ++marker_count;
    need(marker_count==1,"Exactly one selected legend marker actually renders in its reserved gutter");
    need(budget == vaultline::kMapHudGlyphBudget-glyphs(t), "Actual combined overlay emits every planned glyph without exhausting budget");
    for(const auto& q:r.rects)
      need(q.x>=0 && q.y>=0 && q.x+q.w<=w+.001f && q.y+q.h<=h+.001f,
           "Every emitted glyph and background rectangle remains inside viewport");
    max_rectangles=std::max(max_rectangles,r.rects.size());
    need(r.rects.size()<=21*glyphs(t)+2,"Merged existing glyph path retains its bounded rectangle count");
    if(focus==2 && (hsi==100 || hsi==135 || hsi==160)) {
      std::cout << "MAP_MEASUREMENT {\"width\":"<<w<<",\"height\":"<<h
          <<",\"language\":\""<<(lang==fury::Lang::Es?"es":"en")<<"\",\"hud_scale\":"<<hs
          <<",\"legend_scale\":"<<b.legend_scale<<",\"footer_scale\":"<<b.footer_scale
          <<",\"longest_name_ink\":"<<measured_right(t.names[2],b.legend_scale)
          <<",\"values_ink\":"<<measured_right(t.footer[1],b.footer_scale)
          <<",\"controls_ink\":"<<measured_right(t.footer[3],b.footer_scale)
          <<",\"glyphs\":"<<glyphs(t)<<",\"rectangles\":"<<r.rects.size()<<"}\n";
    }
  }
}
}

int main() {
  try {
    text_cases(); layout_cases();
    std::cout << checks << " map presentation checks passed; max glyphs="<<max_glyphs
              <<", max rectangles="<<max_rectangles<<'\n';
    return 0;
  } catch(const std::exception& e) {
    std::cerr << e.what()<<'\n'; return 1;
  }
}
