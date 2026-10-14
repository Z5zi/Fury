#include "../apps/vaultline/npc_dialogue_hud.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;
void require(bool value, const std::string& message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
bool near(float a, float b) { return std::fabs(a - b) < .001f; }

// Independent authored-content oracle, copied from the accepted v15 source.
// UTF-32 literals ensure an accidentally byte-split em dash cannot pass by
// comparing one UTF-8 decoder against another copy of the same decoder.
struct AuthoredRole {
  fury::DialogueRole role;
  const char* speaker;
  std::array<std::u32string, 6> bodies;
};
const std::array<AuthoredRole, 4> authored{{
  {fury::DialogueRole::Civilian, "Lia Merrow", {{
    U"Haven't seen anything. Market's quiet today.",
    U"You look like you're in a hurry — watch the Metro Watch.",
    U"Ashcourt board's got better odds than Meridian, if you ask me.",
    U"Keep your head down near the bank. Guards get twitchy.",
    U"Pierline kids keep cutting through my block.",
    U"Rain's coming. Streets get slick by the loft."
  }}},
  {fury::DialogueRole::Guard, "Sgt. Hale", {{
    U"Move along. Meridian Mutual is closed to loiterers.",
    U"Badge says I ask the questions. Hands where I can see them.",
    U"Heat's on the board — don't give me a reason.",
    U"Vault corridor is staff only. Turn around.",
    U"I clock faces. Yours just got noted.",
    U"Alarm's one button away. Be smart."
  }}},
  {fury::DialogueRole::Fence, "Cass Vesper", {{
    U"Cash talks. Named chips talk louder — BearerBond, Sapphire, LedgerDrive.",
    U"Pierline discount if your rep's clean. Syndicate's always watching.",
    U"Buy crew / heat damp / loot speed — or sell me something shiny.",
    U"I don't ask where it came from. I ask what it's worth.",
    U"Shop's open. Don't bring Metro Watch to my door.",
    U"You fence with me, you stay quiet. That's the deal."
  }}},
  {fury::DialogueRole::Crew, "Rook", {{
    U"Comms live. Call it when you need cover.",
    U"I'm on lookout — tap Q again if you want a status.",
    U"Bags ready. Say the word and we move.",
    U"Street looks clear for a beat. Don't waste it.",
    U"Extract pad's green when you're padded up.",
    U"I'll watch heat. You watch the vault."
  }}}
}};

std::string encode_utf8(const std::u32string& codepoints) {
  std::string bytes;
  for (char32_t c : codepoints) {
    if (c < 0x80) bytes += static_cast<char>(c);
    else if (c < 0x800) {
      bytes += static_cast<char>(0xc0 | (c >> 6));
      bytes += static_cast<char>(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
      bytes += static_cast<char>(0xe0 | (c >> 12));
      bytes += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
      bytes += static_cast<char>(0x80 | (c & 0x3f));
    } else {
      bytes += static_cast<char>(0xf0 | (c >> 18));
      bytes += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
      bytes += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
      bytes += static_cast<char>(0x80 | (c & 0x3f));
    }
  }
  return bytes;
}

std::string raw_bark(const AuthoredRole& a, std::size_t index) {
  const std::string prefix = a.role == fury::DialogueRole::Crew ?
      (index % 2 == 0 ? "Rook: " : "Sparrow: ") : "";
  return prefix + encode_utf8(a.bodies[index]);
}

struct Rect { float x, y, width, height; fury::Color color; };
struct RecordingHud {
  std::vector<Rect> rects;
  void draw_hud_rect(float x, float y, float width, float height, const fury::Color& color) {
    require(std::isfinite(x) && std::isfinite(y) && std::isfinite(width) &&
                std::isfinite(height) && width > 0.f && height > 0.f,
            "Every emitted rectangle has finite, positive geometry");
    rects.push_back({x, y, width, height, color});
  }
};

void test_exact_display_only_prefixes() {
  using fury::DialogueRole;
  struct Example { const char* raw; const char* crew_body; };
  const Example examples[]{
    {"Rook: Stay. Sparrow: Listen.", "Stay. Sparrow: Listen."},
    {"Sparrow: Rook: Stay.", "Rook: Stay."},
    {"Rook: ", ""}, {"Sparrow: ", ""},
    {"Rook:Stay.", "Rook:Stay."}, {"ROOK: Stay.", "ROOK: Stay."},
    {"rook: Stay.", "rook: Stay."}, {" Sparrow: Stay.", " Sparrow: Stay."},
    {"Marlow: Stay.", "Marlow: Stay."}, {"A Rook: Stay.", "A Rook: Stay."},
    {"Rook:\tStay.", "Rook:\tStay."}, {"Rook", "Rook"}, {"", ""}
  };
  for (const auto& e : examples) {
    std::string raw(e.raw);
    const auto before = raw;
    for (const auto role : {DialogueRole::Civilian, DialogueRole::Guard,
                            DialogueRole::Fence, DialogueRole::Crew}) {
      const auto body = vaultline::npc_dialogue_body(raw, role);
      require(body == (role == DialogueRole::Crew ? e.crew_body : e.raw),
              "Only the exact known Crew prefix is stripped, once, at the start");
      require(raw == before, "Display normalization cannot change the raw/logged string");
    }
  }
}

void test_authored_selection_and_decoding() {
  for (const auto& role : authored) {
    fury::DialogueBarks picker;
    for (int rotation = 0; rotation < 18; ++rotation) {
      std::vector<std::string> raw;
      picker.pick(role.role, raw);
      const auto raw_before = raw;
      const auto rotate_before = picker.rotate;
      require(raw.size() == std::size_t(1 + rotation % 3),
              "Production picker retains its rotating one-to-three bark count");
      for (std::size_t i = 0; i < raw.size(); ++i) {
        const auto index = (rotation + i) % 6;
        require(raw[i] == raw_bark(role, index),
                "All authored/logged bark bytes and production rotation remain exact");
        require(vaultline::npc_hud_text(vaultline::npc_dialogue_body(raw[i], role.role)) ==
                    role.bodies[index],
                "Every display body preserves independent authored Unicode wording/punctuation");
      }
      require(picker.rotate == rotate_before && raw == raw_before,
              "Presentation leaves raw dialogue and future picker rotation untouched");
    }
  }
  require(vaultline::npc_hud_text("A—B") == U"A—B",
          "An em dash decodes as one Unicode cell rather than three UTF-8 bytes");
  require(vaultline::npc_hud_text("\xc2\xa3\xf0\x9f\x8c\x8a") == U"£🌊",
          "Valid two- and four-byte Unicode decode without content loss");
  require(vaultline::npc_hud_text(std::string("A\xe2\x28\xa1Z", 5)) == U"A\ufffd(\ufffdZ",
          "Malformed UTF-8 remains visible and does not eat adjacent valid characters");
  for (const std::string malformed : {std::string("\xc0\xaf", 2),
       std::string("\xed\xa0\x80", 3), std::string("\xf4\x90\x80\x80", 4),
       std::string("\xe2\x80", 2)}) {
    const auto text = vaultline::npc_hud_text(malformed);
    require(!text.empty() && std::all_of(text.begin(), text.end(),
                [](char32_t c) { return c == U'\ufffd'; }),
            "Overlong, surrogate, out-of-range and incomplete UTF-8 produce visible replacement");
  }
}

// Match each displayed span against the independently authored string. Only a
// space consumed at a visual wrap boundary may be omitted; punctuation, case,
// and all non-space characters must survive exactly and in order.
void check_complete_wrapping(const std::u32string& expected,
                             const std::vector<std::u32string>& lines,
                             std::size_t columns) {
  std::size_t consumed = 0;
  require(!lines.empty(), "Nonempty authored bark always has visible rows");
  for (const auto& line : lines) {
    require(!line.empty() && line.size() <= columns, "Wrapped rows are nonempty and fit the cell width");
    require(expected.substr(consumed, line.size()) == line,
            "Wrapped rows preserve every source codepoint in source order");
    consumed += line.size();
    if (consumed < expected.size() && expected[consumed] == U' ') ++consumed;
  }
  require(consumed == expected.size(), "Wrapping renders the complete body without truncation");
}

void test_word_wrapping() {
  for (const auto& role : authored)
    for (const auto& body : role.bodies)
      for (const std::size_t columns : {1u, 2u, 5u, 12u, 32u, 48u, 80u})
        check_complete_wrapping(body, vaultline::wrap_npc_hud_text(body, columns), columns);
  require(vaultline::wrap_npc_hud_text(U"alpha beta gamma", 10) ==
              std::vector<std::u32string>{U"alpha beta", U"gamma"},
          "Words wrap at a available space instead of splitting a fitting word");
  require(vaultline::wrap_npc_hud_text(U"AB—CDEFG", 3) ==
              std::vector<std::u32string>{U"AB—", U"CDE", U"FG"},
          "A long unbroken word wraps by whole Unicode cells without inserting text");
  require(vaultline::wrap_npc_hud_text(U"AB", 0) ==
              std::vector<std::u32string>{U"A", U"B"},
          "A zero-column caller remains bounded and makes progress");
}

void test_layout_and_draws() {
  std::size_t cases = 0, maximum_rects = 0, minimum_scale_cases = 0;
  const std::array<float, 11> reserves{{130, 140, 160, 172, 190, 204, 208, 232, 264, 296, 0}};
  for (const auto dimensions : {std::array<int, 2>{1280, 720}, {640, 360}}) {
    const auto width = dimensions[0], height = dimensions[1];
    for (const float scale : {1.f, 1.35f, 1.6f})
      for (const float reserved : reserves)
        for (const auto& role : authored)
          for (std::size_t first = 0; first < 6; ++first)
            for (std::size_t count = 1; count <= 3; ++count) {
              std::vector<std::string> raw;
              for (std::size_t i = 0; i < count; ++i) raw.push_back(raw_bark(role, (first+i)%6));
              const auto before = raw;
              const auto box = vaultline::layout_npc_dialogue(width, height, scale, reserved,
                  role.speaker, raw, role.role);
              const auto context = " width=" + std::to_string(width) + " reserved=" +
                  std::to_string(reserved) + " first=" + std::to_string(first) +
                  " count=" + std::to_string(count);
              require(raw == before, "Layout does not mutate selected/raw/logged lines" + context);
              require(box.speaker == vaultline::npc_hud_text(role.speaker),
                      "Dialogue header always uses the supplied actual speaker" + context);
              const float effective_reserve = width < 960 ? std::min(reserved,160.f) : reserved;
              require(box.x >= 8.f && box.y >= 8.f && box.x+box.width <= width-8.f &&
                          box.y+box.height <= height-effective_reserve+.001f,
                      "Dialogue fits the viewport and protects the approved bottom lane" + context);
              require(box.scale >= 1.5f, "Authored dialogue never shrinks below the readable 1.5px floor" + context);
              if (box.scale <= 1.f) ++minimum_scale_cases;
              const auto columns = std::size_t((box.width - 2*box.padding) / (6*box.scale));
              std::vector<std::u32string> expected_lines;
              for (std::size_t i = 0; i < count; ++i) {
                const auto& body = role.bodies[(first+i)%6];
                auto lines = vaultline::wrap_npc_hud_text(body, columns);
                check_complete_wrapping(body, lines, columns);
                expected_lines.insert(expected_lines.end(), lines.begin(), lines.end());
              }
              require(box.lines == expected_lines, "Every requested bark reaches the actual display layout" + context);
              std::size_t glyphs = box.speaker.size();
              for (const auto& line : box.lines) glyphs += line.size();
              require(glyphs < vaultline::kNpcHudGlyphBudget,
                      "Authored content fits fully within the shared render budget" + context);
              RecordingHud drawn;
              vaultline::draw_npc_dialogue(drawn, box, role.role, 4.2f);
              require(drawn.rects.size() > 2 && drawn.rects.size() <= 2 + 21*glyphs,
                      "Readable text is actually drawn with bounded per-glyph rectangle cost" + context);
              require(drawn.rects.front().color.a == 255,
                      "Opaque dialogue background blocks intersecting ordinary HUD detail" + context);
              // Independently count lit font pixels, without duplicating the
              // renderer's row-run grouping. This catches a silently omitted
              // word/row even when the layout itself still contains its text.
              std::size_t expected_pixels = 0;
              const auto count_pixels = [&](const std::u32string& text) {
                for (const char32_t c : text) {
                  const auto* glyph = fury::bitmap_detail::glyph5x7(c);
                  require(glyph != nullptr, "Authored fixture has no unsupported glyph" + context);
                  for (int y = 0; y < 7; ++y)
                    for (int x = 0; x < 5; ++x)
                      if (glyph[y] & (1u << x)) ++expected_pixels;
                }
              };
              count_pixels(box.speaker);
              for (const auto& line : box.lines) count_pixels(line);
              double drawn_pixels = 0;
              for (std::size_t i = 2; i < drawn.rects.size(); ++i)
                drawn_pixels += double(drawn.rects[i].width)*drawn.rects[i].height /
                    (double(box.scale)*box.scale);
              require(std::fabs(drawn_pixels-double(expected_pixels)) < .01,
                      "Draw output contains every lit pixel of every speaker/body glyph" + context);
              for (const auto& r : drawn.rects)
                require(r.x >= box.x-.001f && r.y >= box.y-.001f &&
                            r.x+r.width <= box.x+box.width+.001f &&
                            r.y+r.height <= box.y+box.height+.001f,
                        "Every text/panel rectangle remains inside its reserved dialogue bounds" + context);
              maximum_rects = std::max(maximum_rects, drawn.rects.size());
              ++cases;
            }
  }
  std::cout << "dialogue layout: " << cases << " authored combinations; maximum rectangles="
            << maximum_rects << "; compact one-pixel cases=" << minimum_scale_cases << '\n';
  require(minimum_scale_cases == 0, "No tested dialogue requires one-pixel fallback text");
}

std::array<unsigned, 7> raster_glyph(char32_t c) {
  RecordingHud hud;
  std::size_t budget = 1;
  vaultline::draw_npc_hud_text(hud, 0, 0, std::u32string(1, c), {255,255,255,255}, 1, budget);
  require(budget == 0, "Each Unicode glyph consumes exactly one shared budget cell");
  require(hud.rects.size() <= 21, "Row-run rendering costs at most 21 rectangles per glyph");
  std::array<unsigned, 7> rows{};
  for (const auto& rect : hud.rects) {
    require(near(rect.height,1) && rect.x >= 0 && rect.y >= 0 && rect.x+rect.width <= 5 && rect.y < 7,
            "Each glyph stays within its 5x7 cell");
    for (int x = int(rect.x); x < int(rect.x+rect.width); ++x)
      rows[std::size_t(rect.y)] |= 1u << (4-x);
  }
  return rows;
}

void test_font_fallback_and_budget() {
  const auto replacement = raster_glyph(U'\ufffd');
  require(std::any_of(replacement.begin(), replacement.end(), [](unsigned row) { return row != 0; }),
          "Unsupported text has a visible replacement glyph");
  require(raster_glyph(U'\u0141') == replacement && raster_glyph(U'\u0141') != raster_glyph(U'A'),
          "Unknown Unicode never narrows to an unrelated ASCII glyph with the same low byte");
  for (const char32_t c : U".,'/—") {
    if (!c) continue;
    const auto glyph = raster_glyph(c);
    require(glyph != replacement && std::any_of(glyph.begin(), glyph.end(), [](unsigned row) { return row != 0; }),
            "Every punctuation mark in authored barks has a distinct supported visible glyph");
  }
  const auto dash = raster_glyph(U'\u2014');
  require(dash[3] == 31 && dash[0] == 0 && dash[6] == 0,
          "Authored em dash is visibly a horizontal midline stroke");
  for (const auto& role : authored)
    for (const auto& body : role.bodies)
      for (char32_t c : body)
        require(c == U' ' || raster_glyph(c) != replacement,
                "All 24 authored barks render complete supported glyphs, not replacement boxes");
  const auto bounded = vaultline::npc_hud_text(std::string(100000, 'W'));
  require(bounded.size() == vaultline::kNpcHudGlyphBudget && bounded.back() == U'\ufffd',
          "Oversize input is bounded and visibly marks clipping");
  RecordingHud hud;
  std::size_t budget = vaultline::kNpcHudGlyphBudget;
  vaultline::draw_npc_hud_text(hud,0,0,std::u32string(10000,U'W'),{255,255,255,255},1,budget);
  const auto first_count = hud.rects.size();
  require(!budget && first_count > 0 && first_count <= 21*vaultline::kNpcHudGlyphBudget,
          "Long input obeys the hard total glyph/draw budget");
  vaultline::draw_npc_hud_text(hud,0,0,U"MORE",{255,255,255,255},1,budget);
  require(hud.rects.size() == first_count, "Later rows cannot reset the per-overlay budget");
}

void test_nameplate_projection_and_inactive_draws() {
  const auto overlaps = [](const Rect& a, const Rect& b) {
    return a.x < b.x+b.width && a.x+a.width > b.x &&
        a.y < b.y+b.height && a.y+a.height > b.y;
  };
  for (const auto dimensions : {std::array<int,2>{1280,720}, {640,360}}) {
    const auto width = dimensions[0], height = dimensions[1];
    const auto identity = fury::Mat4::identity();
    for (const float ndc_x : {-.98f, -.3f, 0.f, .7f, .98f}) {
      const auto anchor = vaultline::project_npc_nameplate(identity,{ndc_x,0,0},width,height);
      require(anchor.visible && near(anchor.x,(ndc_x+1)*.5f*width) && near(anchor.y,height*.5f),
              "Nameplate anchor follows the projected head, including horizontal edge positions");
      for (const float hud_scale : {1.f,1.35f,1.6f})
        for (const bool dialogue_active : {false,true})
          for (const char* label : {"Rook", "Sparrow", "Cass Vesper", "Mira Vale", "Jon Keel",
              "Tessa Quill", "Sgt. Hale", "Lia Merrow", "Ofc. Renn", "Owen Pike",
              "Ofc. Vale", "Nell Ash", "Pax Wren", "Rina Holt"}) {
            const auto plate = vaultline::layout_npc_nameplate(anchor,width,height,label,hud_scale,dialogue_active);
            RecordingHud hud;
            vaultline::draw_npc_nameplate(hud,anchor,width,height,label,fury::DialogueRole::Crew,
                                         hud_scale,dialogue_active);
            require(plate.visible == !hud.rects.empty(), "Nameplate draw visibility follows collision-aware layout");
            if (!dialogue_active)
              require(plate.visible, "Before Q, every roster name preserves identity and talk discoverability");
            if (!plate.visible) continue;
            require(hud.rects.size() > 1 && plate.scale >= 1.5f,
                    "Visible focus has readable text at the minimum glyph scale");
            const Rect status{16,16,340*hud_scale,148*hud_scale,{}};
            const Rect minimap{float(width)-166,16,150,150,{}};
            for (const auto& r : hud.rects) {
              require(r.x >= 8.f-.001f && r.x+r.width <= width-8.f+.001f && r.y >= 8.f &&
                          r.y+r.height <= height-8.f+.001f,
                      "Every emitted nameplate rectangle stays in the viewport");
              require(!overlaps(r,status) && !overlaps(r,minimap),
                      "Nameplate/text never collides with the scaled status panel or minimap");
              require(r.x >= plate.x-.001f && r.y >= plate.y-.001f &&
                          r.x+r.width <= plate.x+plate.width+.001f &&
                          r.y+r.height <= plate.y+plate.height+.001f,
                      "Every identity and talk-hint glyph fits its collision-free plate");
            }
            if (plate.prompt) {
              require(!dialogue_active && plate.text == vaultline::npc_hud_text(label)+U"  Q TALK",
                      "Pre-Q fallback preserves exact focused identity plus explicit Q TALK hint");
              require(plate.x >= 184.f && plate.x+plate.width <= width-138.f &&
                          plate.y >= height-28.f && plate.y+plate.height <= height-8.f,
                      "Fallback occupies only the bottom-center gap between save slots, onboarding and FPS");
            } else {
              require(plate.text == vaultline::npc_hud_text(label) && plate.y+plate.height <= anchor.y-10.f &&
                          std::fabs(plate.x+plate.width*.5f-anchor.x) <= 110.f,
                      "Overhead identity remains above and near the actual projected head");
            }
          }
    }
    require(!vaultline::project_npc_nameplate(identity,{2,0,0},width,height).visible,
            "A head outside the viewport does not create a visible nameplate");
    auto behind = identity;
    behind.m[15] = -1.f;
    require(!vaultline::project_npc_nameplate(behind,{0,0,0},width,height).visible,
            "A head behind the camera cannot create a nameplate");
    auto invalid = identity;
    invalid.m[15] = std::numeric_limits<float>::quiet_NaN();
    require(!vaultline::project_npc_nameplate(invalid,{0,0,0},width,height).visible,
            "A nonfinite projection suppresses the nameplate");
    RecordingHud inactive;
    vaultline::draw_npc_nameplate(inactive,{},width,height,"Rook",fury::DialogueRole::Crew);
    vaultline::draw_npc_nameplate(inactive,{width*.5f,height*.5f,true},width,height,"",fury::DialogueRole::Crew);
    vaultline::draw_npc_nameplate(inactive,{width*.5f,8.f,true},width,height,"Rook",fury::DialogueRole::Crew,1,true);
    const auto box = vaultline::layout_npc_dialogue(width,height,1,130,"Rook",
        {"Rook: Comms live."},fury::DialogueRole::Crew);
    for (const float timer : {0.f,-1.f})
      vaultline::draw_npc_dialogue(inactive,box,fury::DialogueRole::Crew,timer);
    vaultline::draw_npc_dialogue(inactive,{},fury::DialogueRole::Crew,4.2f);
    require(inactive.rects.empty(), "Inactive, empty, redundant/clipped and expired overlays add zero HUD draws");
  }

  // Concrete regression geometry from the 640px capture: the short Crew labels
  // fit the narrow gap, while Cass Vesper must not cover the minimap or status.
  const vaultline::NpcNameplateAnchor center{320,180,true};
  for (const char* label : {"Rook","Sparrow"}) {
    const auto shifted = vaultline::layout_npc_nameplate(center,640,360,label,1,true);
    require(shifted.visible && !shifted.prompt && shifted.x > center.x &&
                shifted.x >= 364.f && shifted.x+shifted.width <= 466.f,
            "Short Crew identity shifts into the 640px gap beside the status panel and minimap");
  }
  const auto suppressed = vaultline::layout_npc_nameplate(center,640,360,"Cass Vesper",1,true);
  require(!suppressed.visible, "Active dialogue suppresses a redundant Cass nameplate that cannot fit");
  const auto fallback = vaultline::layout_npc_nameplate(center,640,360,"Cass Vesper",1,false);
  require(fallback.visible && fallback.prompt && fallback.text == U"Cass Vesper  Q TALK",
          "Before Q, Cass retains her exact identity and interaction hint in the safe prompt lane");
  for (const float scale : {1.35f,1.6f}) {
    const auto active = vaultline::layout_npc_nameplate(center,640,360,"Rook",scale,true);
    const auto idle = vaultline::layout_npc_nameplate(center,640,360,"Rook",scale,false);
    require(!active.visible && idle.visible && idle.prompt,
            "Larger status HUDs suppress redundant active labels while preserving pre-Q identity");
  }

  const auto box = vaultline::layout_npc_dialogue(640,360,1.6f,296,"Cass Vesper",
      {"Cash talks. Named chips talk louder — BearerBond, Sapphire, LedgerDrive."},fury::DialogueRole::Fence);
  for (const float timer : {4.2f,.175f,.01f}) {
    RecordingHud hud;
    vaultline::draw_npc_dialogue(hud,box,fury::DialogueRole::Fence,timer);
    const auto expected_alpha = static_cast<std::uint8_t>(255*std::clamp(timer/.35f,0.f,1.f));
    require(!hud.rects.empty() && hud.rects.front().color.a == expected_alpha,
            "Dialogue is opaque during normal display and retains its authored final fade timing");
  }
}
}  // namespace

int main() {
  try {
    test_exact_display_only_prefixes();
    test_authored_selection_and_decoding();
    test_word_wrapping();
    test_layout_and_draws();
    test_font_fallback_and_budget();
    test_nameplate_projection_and_inactive_draws();
    std::cout << "npc_dialogue_hud: " << checks << " checks passed; authored content, Unicode, wrapping, "
                 "viewport/exclusion bounds, projected nameplates, visible fallback, budget and inactive draws\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "npc_dialogue_hud FAILED after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
