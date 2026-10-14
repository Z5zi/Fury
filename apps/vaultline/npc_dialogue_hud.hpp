#pragma once

#include <fury/bitmap_font.hpp>
#include <fury/dialogue.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace vaultline {

// Presentation only: the shared Crew pool embeds names unrelated to Q's focused
// actor. Keep the selected/raw/logged bark intact; label it with the real speaker.
inline std::string_view npc_dialogue_body(std::string_view raw, fury::DialogueRole role) {
  if (role == fury::DialogueRole::Crew) {
    for (const std::string_view prefix : {"Rook: ", "Sparrow: "})
      if (raw.substr(0, prefix.size()) == prefix) return raw.substr(prefix.size());
  }
  return raw;
}

inline constexpr std::size_t kNpcHudGlyphBudget = 512;
// Decode before measuring/wrapping. Unknown or malformed characters remain a
// visible replacement cell; UTF-8 punctuation never becomes multiple cells.
inline std::u32string npc_hud_text(std::string_view text) {
  std::u32string out;
  for (std::size_t i = 0; i < text.size() && out.size() < kNpcHudGlyphBudget; ) {
    const auto c = static_cast<unsigned char>(text[i]);
    char32_t value = c;
    std::size_t count = 1;
    if (c >= 0xc2 && c <= 0xdf) { count = 2; value = c & 0x1f; }
    else if (c >= 0xe0 && c <= 0xef) { count = 3; value = c & 0x0f; }
    else if (c >= 0xf0 && c <= 0xf4) { count = 4; value = c & 0x07; }
    else if (c >= 0x80) { out += U'\ufffd'; ++i; continue; }
    bool valid = i + count <= text.size();
    for (std::size_t j = 1; valid && j < count; ++j) {
      const auto next = static_cast<unsigned char>(text[i+j]);
      valid = (next & 0xc0) == 0x80;
      value = (value << 6) | (next & 0x3f);
    }
    valid = valid && !(count == 2 && value < 0x80) &&
        !(count == 3 && value < 0x800) && !(count == 4 && value < 0x10000) &&
        value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff);
    out += valid ? value : U'\ufffd';
    i += valid ? count : 1;
    if (out.size() == kNpcHudGlyphBudget && i < text.size()) out.back() = U'\ufffd';
  }
  return out;
}

inline std::vector<std::u32string> wrap_npc_hud_text(const std::u32string& text,
                                                   std::size_t columns) {
  std::vector<std::u32string> lines;
  columns = std::max(std::size_t{1}, columns);
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t end = std::min(start + columns, text.size());
    if (end < text.size()) {
      const auto space = text.rfind(U' ', end);
      if (space != std::u32string::npos && space > start) end = space;
    }
    lines.push_back(text.substr(start, end - start));
    start = end;
    if (start < text.size() && text[start] == U' ') ++start;
  }
  return lines;
}

struct NpcDialogueLayout {
  float x{}, y{}, width{}, height{}, scale{}, padding{}, line_step{};
  std::u32string speaker;
  std::vector<std::u32string> lines;
};

inline NpcDialogueLayout layout_npc_dialogue(int width, int height, float hud_scale,
    float bottom_reserved, std::string_view speaker,
    const std::vector<std::string>& raw_lines, fury::DialogueRole role) {
  NpcDialogueLayout result;
  if (width < 64 || height < 64 || raw_lines.empty()) return result;
  result.width = std::min(800.f, float(width) - 32.f);
  result.x = (width - result.width) * .5f;
  result.scale = std::min(width < 960 ? 1.5f : 2.5f,
                          2.f * std::clamp(hud_scale, 1.f, 1.6f));
  result.speaker = npc_hud_text(speaker);
  // On a short viewport, readable active dialogue has priority over overlapping
  // non-modal tips. Keep the compass lane, rather than shrinking to 1px glyphs.
  if (width < 960) bottom_reserved = std::min(bottom_reserved, 160.f);
  const float available = std::max(32.f, height - bottom_reserved - 8.f);
  // Compact only when a short viewport plus existing transient HUD needs it.
  // Authored 1-3-line barks remain complete; no page/timer/content changes.
  for (;;) {
    result.padding = 6.f * result.scale;
    result.line_step = 10.f * result.scale;
    const auto columns = std::max(1, int((result.width - 2.f * result.padding) /
                                        (6.f * result.scale)));
    result.lines.clear();
    for (const auto& raw : raw_lines) {
      auto wrapped = wrap_npc_hud_text(npc_hud_text(npc_dialogue_body(raw, role)), columns);
      result.lines.insert(result.lines.end(), wrapped.begin(), wrapped.end());
    }
    result.height = 2.f * result.padding + result.line_step * (1.f + result.lines.size());
    if (result.height <= available || result.scale <= 1.5f) break;
    result.scale = std::max(1.5f, result.scale - .25f);
  }
  result.y = std::max(8.f, height - bottom_reserved - result.height);
  return result;
}

struct NpcNameplateAnchor { float x{}, y{}; bool visible{}; };
inline NpcNameplateAnchor project_npc_nameplate(const fury::Mat4& view_projection,
    const fury::Vec3& head, int width, int height) {
  const auto clip = fury::mul(view_projection, fury::Vec4(head));
  if (!std::isfinite(clip.w) || clip.w <= 0.f) return {};
  const float x = (clip.x / clip.w * .5f + .5f) * width;
  const float y = (.5f - clip.y / clip.w * .5f) * height;
  return {x, y, std::isfinite(x) && std::isfinite(y) &&
      x >= 0.f && x <= width && y >= 0.f && y <= height};
}

// Existing 5x7 font, same rect-pixel path. Merge adjacent pixels in each row,
// bounding cost to <=21 rectangles per glyph instead of one call per lit pixel.
// The per-overlay glyph budget is shared by speaker/body (or label/talk hint).
template<class HudRenderer>
inline void draw_npc_hud_text(HudRenderer& renderer, float x, float y,
    std::u32string_view text, fury::Color color, float scale, std::size_t& budget) {
  static constexpr std::uint8_t replacement[7]{0x1f,0x11,0x15,0x15,0x15,0x11,0x1f};
  for (const char32_t c : text) {
    if (!budget) break;
    --budget;
    const auto* glyph = fury::bitmap_detail::glyph5x7(c);
    if (!glyph) glyph = replacement;
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ) {
        if (!(glyph[row] & (1u << (4-column)))) { ++column; continue; }
        const int start = column++;
        while (column < 5 && (glyph[row] & (1u << (4-column)))) ++column;
        renderer.draw_hud_rect(x + start*scale, y + row*scale,
                               (column-start)*scale, scale, color);
      }
    }
    x += 6.f*scale;
  }
}

inline fury::Color npc_dialogue_accent(fury::DialogueRole role, std::uint8_t alpha) {
  switch (role) {
    case fury::DialogueRole::Guard: return {135,180,255,alpha};
    case fury::DialogueRole::Fence: return {255,190,115,alpha};
    case fury::DialogueRole::Crew: return {120,245,190,alpha};
    default: return {190,220,255,alpha};
  }
}

template<class HudRenderer>
inline void draw_npc_dialogue(HudRenderer& renderer, const NpcDialogueLayout& box,
    fury::DialogueRole role, float timer) {
  if (timer <= 0.f || box.lines.empty()) return;
  const float fade = std::clamp(timer / .35f, 0.f, 1.f);
  const auto alpha = static_cast<std::uint8_t>(245 * fade);
  const auto accent = npc_dialogue_accent(role, alpha);
  renderer.draw_hud_rect(box.x, box.y, box.width, box.height,
                         fury::Color{9,15,22,static_cast<std::uint8_t>(255*fade)});
  renderer.draw_hud_rect(box.x, box.y, 3.f, box.height, accent);
  std::size_t budget = kNpcHudGlyphBudget;
  const float x = box.x + box.padding;
  float y = box.y + box.padding;
  draw_npc_hud_text(renderer,x,y,box.speaker,accent,box.scale,budget);
  for (const auto& line : box.lines) {
    y += box.line_step;
    draw_npc_hud_text(renderer,x,y,line,fury::Color{242,245,248,alpha},box.scale,budget);
  }
}

struct NpcNameplateLayout {
  float x{}, y{}, width{}, height{}, scale{};
  bool visible{}, prompt{};
  std::u32string text;
};

inline NpcNameplateLayout layout_npc_nameplate(NpcNameplateAnchor anchor,
    int width, int height, std::string_view label, float hud_scale = 1.f,
    bool dialogue_active = false) {
  NpcNameplateLayout plate;
  if (!anchor.visible || label.empty() || width < 64 || height < 64) return plate;
  plate.scale = width < 960 ? 1.5f : 2.f;
  plate.text = npc_hud_text(label);
  plate.width = (std::max(std::size_t{6},plate.text.size())*6.f + 12.f)*plate.scale;
  plate.height = 24.f*plate.scale;
  plate.y = anchor.y - plate.height - 10.f;
  float left = 8.f, right = width - 8.f;
  const float hs = std::clamp(hud_scale,1.f,1.6f);
  // These are the existing status/minimap rectangles, not a new HUD layout.
  if (plate.y < 16.f + 148.f*hs && plate.y + plate.height > 16.f)
    left = std::max(left,16.f + 340.f*hs + 8.f);
  if (plate.y < 166.f && plate.y + plate.height > 16.f)
    right = std::min(right,width - 166.f - 8.f);
  if (plate.y >= 8.f && plate.y + plate.height <= height - 8.f &&
      right - left >= plate.width) {
    plate.x = std::clamp(anchor.x-plate.width*.5f,left,right-plate.width);
    // A nearby horizontal shift keeps the label associated with the focus.
    // Larger detours would point toward a different actor: use the prompt lane.
    if (std::abs(plate.x + plate.width*.5f - anchor.x) <= 110.f) {
      plate.visible = true;
      return plate;
    }
  }
  if (dialogue_active) return {}; // Speaker identity already lives in dialogue.
  // Before Q, preserve identity/discoverability in the existing bottom-center
  // gap between save slots and FPS, below onboarding pips. No legacy HUD hidden.
  plate.prompt = true;
  plate.text += U"  Q TALK";
  plate.scale = 1.5f;
  plate.width = plate.text.size()*6.f*plate.scale + 12.f;
  plate.height = 20.f;
  plate.x = (width-plate.width)*.5f;
  plate.y = height-28.f;
  plate.visible = plate.x >= 184.f && plate.x+plate.width <= width-138.f;
  return plate;
}

template<class HudRenderer>
inline void draw_npc_nameplate(HudRenderer& renderer, NpcNameplateAnchor anchor,
    int width, int height, std::string_view label, fury::DialogueRole role,
    float hud_scale = 1.f, bool dialogue_active = false) {
  const auto plate = layout_npc_nameplate(anchor,width,height,label,hud_scale,dialogue_active);
  if (!plate.visible) return;
  renderer.draw_hud_rect(plate.x,plate.y,plate.width,plate.height,fury::Color{9,15,22,245});
  std::size_t budget = kNpcHudGlyphBudget;
  draw_npc_hud_text(renderer,plate.x+(plate.prompt?6.f:6.f*plate.scale),
      plate.y+(plate.prompt?4.f:4.f*plate.scale),plate.text,
      npc_dialogue_accent(role,255),plate.scale,budget);
  if (!plate.prompt)
    draw_npc_hud_text(renderer,plate.x+6.f*plate.scale,plate.y+14.f*plate.scale,U"Q TALK",
                      fury::Color{245,225,155,255},plate.scale,budget);
}
}  // namespace vaultline
