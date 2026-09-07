#pragma once

/// i18n stub (Vaultline 4.8.0) — EN + ES string table for HUD tips / mission names.
/// Cycle language in settings (O). Original Harbor Metro copy only — no third-party IP.

#include <cstddef>
#include <string>

namespace fury {

enum class Lang : int {
  En = 0,
  Es = 1,
  Count = 2
};

inline const char* lang_code(Lang lang) {
  switch (lang) {
    case Lang::En: return "EN";
    case Lang::Es: return "ES";
    case Lang::Count: break;
  }
  return "EN";
}

inline const char* lang_label(Lang lang) {
  switch (lang) {
    case Lang::En: return "English";
    case Lang::Es: return "Espanol";
    case Lang::Count: break;
  }
  return "English";
}

inline Lang lang_from_int(int v) {
  if (v < 0 || v >= static_cast<int>(Lang::Count)) {
    return Lang::En;
  }
  return static_cast<Lang>(v);
}

inline Lang cycle_lang(Lang lang, int dir) {
  const int n = static_cast<int>(Lang::Count);
  int i = static_cast<int>(lang) + dir;
  if (i < 0) i = n - 1;
  if (i >= n) i = 0;
  return static_cast<Lang>(i);
}

/// Subset of localizable keys (tips + mission titles).
enum class StrId : int {
  TipBoard = 0,
  TipTarget = 1,
  TipEscape = 2,
  TipDone = 3,
  MissionMeridian = 4,
  MissionCrown = 5,
  MissionAshcourt = 6,
  MissionDepot = 7,
  MissionNight = 8,
  MissionNorthQuay = 9,
  LabelCash = 10,
  LabelFps = 11,
  SettingsLanguage = 12,
  Count = 13
};

namespace i18n_detail {

inline const char* en(StrId id) {
  switch (id) {
    case StrId::TipBoard: return "Press M — open mission board (1-6)";
    case StrId::TipTarget: return "Follow compass to target — E to breach";
    case StrId::TipEscape: return "Reach green extraction (van F/E)";
    case StrId::TipDone: return "Slice complete — E reset, B fence shop";
    case StrId::MissionMeridian: return "Meridian Mutual Vault";
    case StrId::MissionCrown: return "Crown & Cutler Safe";
    case StrId::MissionAshcourt: return "Ashcourt Market ATM";
    case StrId::MissionDepot: return "Harbor Armored Depot";
    case StrId::MissionNight: return "Meridian Night Vault";
    case StrId::MissionNorthQuay: return "North Quay Container Yard";
    case StrId::LabelCash: return "CASH";
    case StrId::LabelFps: return "FPS";
    case StrId::SettingsLanguage: return "Language";
    case StrId::Count: break;
  }
  return "";
}

inline const char* es(StrId id) {
  switch (id) {
    case StrId::TipBoard: return "Pulsa M — tablero de misiones (1-6)";
    case StrId::TipTarget: return "Sigue la brujula al objetivo — E para entrar";
    case StrId::TipEscape: return "Llega a la extraccion verde (furgon F/E)";
    case StrId::TipDone: return "Slice listo — E reinicia, B tienda";
    case StrId::MissionMeridian: return "Boveda Meridian Mutual";
    case StrId::MissionCrown: return "Caja Crown & Cutler";
    case StrId::MissionAshcourt: return "Cajero Ashcourt Market";
    case StrId::MissionDepot: return "Deposito Blindado Harbor";
    case StrId::MissionNight: return "Boveda Nocturna Meridian";
    case StrId::MissionNorthQuay: return "Patio Contenedores North Quay";
    case StrId::LabelCash: return "EFEC";
    case StrId::LabelFps: return "FPS";
    case StrId::SettingsLanguage: return "Idioma";
    case StrId::Count: break;
  }
  return "";
}

}  // namespace i18n_detail

inline const char* tr(Lang lang, StrId id) {
  switch (lang) {
    case Lang::Es: return i18n_detail::es(id);
    case Lang::En:
    case Lang::Count:
      break;
  }
  return i18n_detail::en(id);
}

inline StrId mission_str_id(std::size_t mission_index) {
  switch (mission_index % 6) {
    case 0: return StrId::MissionMeridian;
    case 1: return StrId::MissionCrown;
    case 2: return StrId::MissionAshcourt;
    case 3: return StrId::MissionDepot;
    case 4: return StrId::MissionNight;
    default: return StrId::MissionNorthQuay;
  }
}

inline const char* mission_title_tr(Lang lang, std::size_t mission_index) {
  return tr(lang, mission_str_id(mission_index));
}

inline StrId onboard_tip_id(int step) {
  switch (step) {
    case 0: return StrId::TipBoard;
    case 1: return StrId::TipTarget;
    case 2: return StrId::TipEscape;
    default: return StrId::TipDone;
  }
}

/// Short HUD abbreviation for onboard tip (bitmap-friendly, ASCII).
inline const char* onboard_tip_abbr(Lang lang, int step) {
  if (lang == Lang::Es) {
    switch (step) {
      case 0: return "TABLERO";
      case 1: return "OBJETIVO";
      case 2: return "ESCAPE";
      default: return "LISTO";
    }
  }
  switch (step) {
    case 0: return "BOARD";
    case 1: return "TARGET";
    case 2: return "ESCAPE";
    default: return "DONE";
  }
}

}  // namespace fury
