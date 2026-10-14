#include "../apps/vaultline/settings_panel_layout.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks{};
void need(bool ok, const char* reason) {
  ++checks;
  if (!ok) throw std::runtime_error(reason);
}
struct Box { float x,y,w,h; };
bool contains(Box outer, Box inner) {
  return inner.w>0 && inner.h>0 && inner.x>=outer.x-.001f && inner.y>=outer.y-.001f &&
      inner.x+inner.w<=outer.x+outer.w+.001f && inner.y+inner.h<=outer.y+outer.h+.001f;
}
bool disjoint(Box a, Box b) {
  return a.x+a.w<=b.x || b.x+b.w<=a.x || a.y+a.h<=b.y || b.y+b.h<=a.y;
}
Box text(float x, float y, const char* value, float scale) {
  const auto n=std::strlen(value);
  need(n>0 && n<=fury::kBitmapFontMaxGlyphs,"Every complete label fits the unchanged glyph cap");
  for(const char* p=value;*p;++p)
    need(fury::bitmap_detail::glyph5x7(*p),"Every label uses supported existing glyphs");
  return {x,y,(float(n)*(fury::kBitmapCellW+fury::kBitmapGlyphGap)-fury::kBitmapGlyphGap)*scale,
      fury::kBitmapCellH*scale};
}
void inspect(int width, int height) {
  const auto layout=vaultline::layout_settings_panel(float(height));
  need(fury::SettingsPanel::kRowCount==14 && fury::SettingsPanel::kPlayerHairRow==13,"All 14 authored controls stay in place");
  const Box panel{width*.5f-300,layout.panel_y,600,layout.panel_height};
  need(contains({0,8,float(width),float(height)-16},panel),"Complete panel fits the supported viewport and margins");
  const Box header{width*.5f-280,layout.header_y,560,18};
  need(contains(panel,header),"Header fits panel");
  need(layout.row_pitch>=18 && layout.row_pitch<=36 && std::floor(layout.row_pitch)==layout.row_pitch,
       "Readable whole-pixel row pitch stays between 18 and 36");
  need(layout.label_scale==1.6f && layout.language_scale==2.f && layout.footer_scale==1.4f,
       "Menu retains its existing fixed fonts independently of gameplay HUD scale");
  const char* labels[]{"SENSITIVITY","FIELD VIEW","VOLUME","DETAIL","SUBTITLES","INVERT Y","COLORBLIND",
      "HUD SIZE","REDUCE FLASH","LANGUAGE","LIGHTING","UPSCALER","UPSCALE MODE","PLAYER HAIR"};
  for(int i=0;i<14;++i) {
    const Box row{width*.5f-270,layout.row_y(i),540,layout.row_height()};
    const Box label=text(width*.5f-255,layout.label_y(i),labels[i],layout.label_scale);
    const Box meter{width*.5f-110,layout.meter_y(i),340,8};
    need(contains(panel,row) && contains(row,label) && contains(row,meter),"Labels and full meters fit each row");
    need(disjoint(label,meter),"Longest labels stay clear of meter and value region");
    need(disjoint(header,row),"Header stays clear of every row");
    if(i<13) need(row.y+row.h<layout.row_y(i+1),"Row backgrounds and glyphs cannot erase the following row");
    for(const auto box : {label,meter})
      need(std::floor(box.y)>=row.y && std::ceil(box.y+box.h)<=row.y+row.h,
           "Conservative raster coverage is contained in row background");
    if(i==9) for(const char* code : {"EN","ES"})
      need(contains(row,text(width*.5f+180,layout.language_y(i),code,layout.language_scale)),"Both language codes fit at unchanged 2px scale");
    if(i>=10 && i<=12) {
      const Box chip{width*.5f+75,layout.chip_y(i),172,layout.chip_height()};
      need(contains(row,chip),"Rendering value chip fits row");
      for(const char* value : {"DX12 ONLY","RAY TRACED","PATH TRACED","NATIVE","AMD FSR","INTEL XESS",
                             "NATIVE AA","QUALITY","BALANCED","PERFORMANCE","ULTRA PERF"})
        need(contains(chip,text(width*.5f+82,layout.label_y(i),value,layout.label_scale)),"Full available/disabled rendering values fit chip");
    }
    if(i==13) {
      const Box chip{width*.5f+55,layout.chip_y(i),195,layout.chip_height()};
      need(contains(row,chip),"Player Hair chip fits row");
      for(const char* value : {"AUTO","ORIGINAL","BALANCED"})
        need(contains(chip,text(width*.5f+65,layout.label_y(i),value,layout.label_scale)),"All Player Hair preferences fit chip");
    }
  }
  for(bool env : {false,true}) for(bool restart : {false,true}) for(bool quality_env : {false,true})
    for(bool supported : {false,true}) for(const char* groom : {"ORIGINAL","BALANCED","DISABLED","CUSTOM"}) {
      std::vector<Box> spans{text(width*.5f-270,layout.footer_line_y(0),"ACTIVE",layout.footer_scale),
                             text(width*.5f-200,layout.footer_line_y(0),groom,layout.footer_scale)};
      if(env) spans.push_back(text(width*.5f-80,layout.footer_line_y(0),"ENV OVERRIDE",layout.footer_scale));
      else {
        if(restart) {
          spans.push_back(text(width*.5f-80,layout.footer_line_y(0),"RESTART",layout.footer_scale));
          spans.push_back(text(width*.5f,layout.footer_line_y(0),groom,layout.footer_scale));
        }
        if(quality_env) spans.push_back(text(width*.5f+100,layout.footer_line_y(0),"QUALITY ENV",layout.footer_scale));
      }
      if(!supported) {
        spans.push_back(text(width*.5f-270,layout.footer_line_y(1),"DX12 GROOM",layout.footer_scale));
        spans.push_back(text(width*.5f-160,layout.footer_line_y(1),"UNVERIFIED",layout.footer_scale));
      }
      for(size_t i=0;i<spans.size();++i) {
        need(contains(panel,spans[i]),"Every reachable complete footer branch fits panel");
        need(spans[i].y>=layout.row_y(13)+layout.row_height()+8,"Footer cannot overwrite final row");
        for(size_t j=0;j<i;++j) need(disjoint(spans[i],spans[j]),"Footer labels never overlap in any branch combination");
      }
    }
}
}
int main() {
  try {
    for(const auto size : {std::pair<int,int>{640,360},{640,480},{960,540},{1280,720},{2560,1440}})
      inspect(size.first,size.second);
    // Check each height through both the compact and maximum-pitch transitions.
    for(int height=360;height<=720;++height) {
      const auto l=vaultline::layout_settings_panel(float(height));
      need(l.panel_y>=8 && l.panel_y+l.panel_height<=height-8,"Every intermediate viewport height contains the full list");
      need(l.row_height()>=fury::kBitmapCellH*l.language_scale,"Language glyph fits at every pitch transition");
    }
    const auto compact=vaultline::layout_settings_panel(360);
    need(compact.panel_y==9 && compact.panel_height==342 && compact.body_y==47 &&
        compact.footer_line_y(0)==307 && compact.footer_line_y(1)==323,"640x360 regression witness uses all available vertical space");
    std::cout<<"settings_panel_layout: "<<checks<<" checks passed\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
