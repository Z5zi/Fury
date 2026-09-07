#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace fury {

/// Talk / bark roles for named NPCs (Harbor Metro original lines — no third-party IP).
enum class DialogueRole {
  Civilian,
  Guard,
  Fence,
  Crew,
};

/// Short 1–3 line bark dialogue stub when the player presses Q near a named NPC.
struct DialogueBarks {
  int rotate{0};

  static const char* role_label(DialogueRole role) {
    switch (role) {
      case DialogueRole::Guard:
        return "Guard";
      case DialogueRole::Fence:
        return "Fence";
      case DialogueRole::Crew:
        return "Crew";
      case DialogueRole::Civilian:
      default:
        return "Civilian";
    }
  }

  /// Fill `out` with 1–3 unique-role lines (rotating). Clears `out` first.
  void pick(DialogueRole role, std::vector<std::string>& out) {
    out.clear();
    struct Bucket {
      const char* const* lines;
      std::size_t count;
    };
    static constexpr const char* kCivilian[] = {
        "Haven't seen anything. Market's quiet today.",
        "You look like you're in a hurry — watch the Metro Watch.",
        "Ashcourt board's got better odds than Meridian, if you ask me.",
        "Keep your head down near the bank. Guards get twitchy.",
        "Pierline kids keep cutting through my block.",
        "Rain's coming. Streets get slick by the loft.",
    };
    static constexpr const char* kGuard[] = {
        "Move along. Meridian Mutual is closed to loiterers.",
        "Badge says I ask the questions. Hands where I can see them.",
        "Heat's on the board — don't give me a reason.",
        "Vault corridor is staff only. Turn around.",
        "I clock faces. Yours just got noted.",
        "Alarm's one button away. Be smart.",
    };
    static constexpr const char* kFence[] = {
        "Cash talks. Named chips talk louder — BearerBond, Sapphire, LedgerDrive.",
        "Pierline discount if your rep's clean. Syndicate's always watching.",
        "Buy crew / heat damp / loot speed — or sell me something shiny.",
        "I don't ask where it came from. I ask what it's worth.",
        "Shop's open. Don't bring Metro Watch to my door.",
        "You fence with me, you stay quiet. That's the deal.",
    };
    static constexpr const char* kCrew[] = {
        "Rook: Comms live. Call it when you need cover.",
        "Sparrow: I'm on lookout — tap Q again if you want a status.",
        "Rook: Bags ready. Say the word and we move.",
        "Sparrow: Street looks clear for a beat. Don't waste it.",
        "Rook: Extract pad's green when you're padded up.",
        "Sparrow: I'll watch heat. You watch the vault.",
    };

    Bucket b{nullptr, 0};
    switch (role) {
      case DialogueRole::Guard:
        b = {kGuard, sizeof(kGuard) / sizeof(kGuard[0])};
        break;
      case DialogueRole::Fence:
        b = {kFence, sizeof(kFence) / sizeof(kFence[0])};
        break;
      case DialogueRole::Crew:
        b = {kCrew, sizeof(kCrew) / sizeof(kCrew[0])};
        break;
      case DialogueRole::Civilian:
      default:
        b = {kCivilian, sizeof(kCivilian) / sizeof(kCivilian[0])};
        break;
    }
    if (!b.lines || b.count == 0) {
      return;
    }
    // 1–3 lines from the rotating pool (unique role tables).
    const int n = 1 + (rotate % 3);
    for (int i = 0; i < n; ++i) {
      const std::size_t idx =
          static_cast<std::size_t>((rotate + i) % static_cast<int>(b.count));
      out.emplace_back(b.lines[idx]);
    }
    ++rotate;
  }
};

}  // namespace fury
