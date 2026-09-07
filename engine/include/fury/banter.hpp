#pragma once

#include "fury/heist.hpp"

#include <cstddef>
#include <string>

namespace fury {

/// Short rotating crew banter lines (Rook / Sparrow) for heist phase changes.
/// Log + HUD tip stub — original Harbor Metro dialogue, no third-party IP.
struct CrewBanter {
  int rotate{0};

  /// Pick next line for `phase`. Empty string for Idle (no chatter).
  const char* next_line(HeistPhase phase) {
    struct Bucket {
      const char* const* lines;
      std::size_t count;
    };
    static constexpr const char* kApproach[] = {
        "Rook: Eyes on the door — keep walking.",
        "Sparrow: Quiet feet. We're just locals.",
        "Rook: Comms check. Sparrow, you copy?",
        "Sparrow: Copy. Street looks clear for now.",
    };
    static constexpr const char* kBreach[] = {
        "Rook: Cutting the lock. Cover me.",
        "Sparrow: Watching the street — hurry it up.",
        "Rook: Soft pop… we're in.",
        "Sparrow: Timer's live. Don't linger.",
    };
    static constexpr const char* kLoot[] = {
        "Rook: Bags moving. Stack and go.",
        "Sparrow: Heat's climbing — keep packing.",
        "Rook: Halfway. Don't get greedy.",
        "Sparrow: Siren's twitchy. Finish the grab.",
    };
    static constexpr const char* kEscape[] = {
        "Rook: Pad's green — move!",
        "Sparrow: Van's warm. Hit the extract.",
        "Rook: Don't look back. Straight to the pad.",
        "Sparrow: Cut through the plaza — I'll watch heat.",
    };
    static constexpr const char* kSuccess[] = {
        "Rook: Clean extract. Nice work.",
        "Sparrow: Split's good. Fence can wait.",
        "Rook: Bags secured. We're ghosts.",
        "Sparrow: Smooth job. Next one's on the board.",
    };
    static constexpr const char* kFailed[] = {
        "Rook: Burn it — scatter!",
        "Sparrow: Too hot. Drop and run.",
        "Rook: Job's dead. Meet at Ashcourt later.",
        "Sparrow: Guards are on us — break contact.",
    };

    Bucket b{nullptr, 0};
    switch (phase) {
      case HeistPhase::Approach:
        b = {kApproach, sizeof(kApproach) / sizeof(kApproach[0])};
        break;
      case HeistPhase::Breach:
        b = {kBreach, sizeof(kBreach) / sizeof(kBreach[0])};
        break;
      case HeistPhase::Looting:
        b = {kLoot, sizeof(kLoot) / sizeof(kLoot[0])};
        break;
      case HeistPhase::Escape:
        b = {kEscape, sizeof(kEscape) / sizeof(kEscape[0])};
        break;
      case HeistPhase::Success:
        b = {kSuccess, sizeof(kSuccess) / sizeof(kSuccess[0])};
        break;
      case HeistPhase::Failed:
        b = {kFailed, sizeof(kFailed) / sizeof(kFailed[0])};
        break;
      case HeistPhase::Idle:
      default:
        return "";
    }
    if (!b.lines || b.count == 0) {
      return "";
    }
    const char* line = b.lines[static_cast<std::size_t>(rotate) % b.count];
    ++rotate;
    return line;
  }

  static std::string hud_label(const char* line) {
    if (!line || !line[0]) {
      return {};
    }
    return std::string("[CREW] ") + line;
  }
};

}  // namespace fury
