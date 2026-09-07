#pragma once

/// Bitmap font stub (Vaultline 4.8.0) — tiny 5x7 glyphs drawn as HUD rect pixels.
/// Used for a few labels (cash / FPS). Falls back to bars if the string is too heavy.

#include "fury/renderer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fury {

/// Cap on glyph draws per call — keep HUD cheap; callers should truncate or bar-fallback.
inline constexpr int kBitmapFontMaxGlyphs = 12;
inline constexpr float kBitmapCellW = 5.f;
inline constexpr float kBitmapCellH = 7.f;
inline constexpr float kBitmapGlyphGap = 1.f;

namespace bitmap_detail {

/// 5x7 packed MSB-left rows for digits, $ , letters used by CASH/FPS/EFEC + tip abbrs.
inline const std::uint8_t* glyph5x7(char c) {
  // Each glyph: 7 rows, low 5 bits used (bit4 = left).
  static const std::uint8_t kSpace[7] = {0, 0, 0, 0, 0, 0, 0};
  static const std::uint8_t k0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
  static const std::uint8_t k1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
  static const std::uint8_t k2[7] = {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F};
  static const std::uint8_t k3[7] = {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E};
  static const std::uint8_t k4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
  static const std::uint8_t k5[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
  static const std::uint8_t k6[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
  static const std::uint8_t k7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
  static const std::uint8_t k8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
  static const std::uint8_t k9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};
  static const std::uint8_t kDollar[7] = {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04};
  static const std::uint8_t kA[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
  static const std::uint8_t kB[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
  static const std::uint8_t kC[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
  static const std::uint8_t kD[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
  static const std::uint8_t kE[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
  static const std::uint8_t kF[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
  static const std::uint8_t kG[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
  static const std::uint8_t kH[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
  static const std::uint8_t kI[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
  static const std::uint8_t kJ[7] = {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E};
  static const std::uint8_t kK[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
  static const std::uint8_t kL[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
  static const std::uint8_t kM[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
  static const std::uint8_t kN[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
  static const std::uint8_t kO[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
  static const std::uint8_t kP[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
  static const std::uint8_t kQ[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
  static const std::uint8_t kR[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
  static const std::uint8_t kS[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
  static const std::uint8_t kT[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
  static const std::uint8_t kU[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
  static const std::uint8_t kV[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
  static const std::uint8_t kW[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
  static const std::uint8_t kX[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
  static const std::uint8_t kY[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
  static const std::uint8_t kZ[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
  static const std::uint8_t kColon[7] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
  static const std::uint8_t kDash[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};

  if (c >= 'a' && c <= 'z') {
    c = static_cast<char>(c - 'a' + 'A');
  }
  switch (c) {
    case ' ': return kSpace;
    case '0': return k0;
    case '1': return k1;
    case '2': return k2;
    case '3': return k3;
    case '4': return k4;
    case '5': return k5;
    case '6': return k6;
    case '7': return k7;
    case '8': return k8;
    case '9': return k9;
    case '$': return kDollar;
    case ':': return kColon;
    case '-': return kDash;
    case 'A': return kA;
    case 'B': return kB;
    case 'C': return kC;
    case 'D': return kD;
    case 'E': return kE;
    case 'F': return kF;
    case 'G': return kG;
    case 'H': return kH;
    case 'I': return kI;
    case 'J': return kJ;
    case 'K': return kK;
    case 'L': return kL;
    case 'M': return kM;
    case 'N': return kN;
    case 'O': return kO;
    case 'P': return kP;
    case 'Q': return kQ;
    case 'R': return kR;
    case 'S': return kS;
    case 'T': return kT;
    case 'U': return kU;
    case 'V': return kV;
    case 'W': return kW;
    case 'X': return kX;
    case 'Y': return kY;
    case 'Z': return kZ;
    default: return nullptr;
  }
}

}  // namespace bitmap_detail

/// Returns false if text is empty / too long / has unsupported glyphs (caller may bar-fallback).
inline bool draw_bitmap_text(Renderer& r, float x, float y, const char* text,
                             const Color& color, float scale = 1.f) {
  if (!text || !text[0]) {
    return false;
  }
  const std::size_t len = std::strlen(text);
  if (len == 0 || static_cast<int>(len) > kBitmapFontMaxGlyphs) {
    return false;
  }
  for (std::size_t i = 0; i < len; ++i) {
    if (!bitmap_detail::glyph5x7(text[i])) {
      return false;
    }
  }
  const float px = (std::max)(1.f, scale);
  float cx = x;
  for (std::size_t i = 0; i < len; ++i) {
    const std::uint8_t* g = bitmap_detail::glyph5x7(text[i]);
    for (int row = 0; row < 7; ++row) {
      const std::uint8_t bits = g[row];
      for (int col = 0; col < 5; ++col) {
        if (bits & (1u << (4 - col))) {
          r.draw_hud_rect(cx + static_cast<float>(col) * px,
                          y + static_cast<float>(row) * px, px, px, color);
        }
      }
    }
    cx += (kBitmapCellW + kBitmapGlyphGap) * px;
  }
  return true;
}

/// Format cash as "$12345" truncated to fit glyph cap; returns false if should use bar only.
inline bool format_cash_label(int cash, char* out, std::size_t out_n) {
  if (!out || out_n < 3) {
    return false;
  }
  if (cash < 0) {
    cash = 0;
  }
  // Prefer "$" + digits; truncate high end if too many digits.
  char digits[16];
  int n = 0;
  int v = cash;
  if (v == 0) {
    digits[n++] = '0';
  } else {
    char rev[16];
    int rn = 0;
    while (v > 0 && rn < 12) {
      rev[rn++] = static_cast<char>('0' + (v % 10));
      v /= 10;
    }
    while (rn > 0) {
      digits[n++] = rev[--rn];
    }
  }
  digits[n] = '\0';
  // "$" + digits must fit kBitmapFontMaxGlyphs
  const int max_digits = kBitmapFontMaxGlyphs - 1;
  int start = 0;
  if (n > max_digits) {
    start = n - max_digits;
  }
  std::size_t oi = 0;
  out[oi++] = '$';
  for (int i = start; i < n && oi + 1 < out_n; ++i) {
    out[oi++] = digits[i];
  }
  out[oi] = '\0';
  return oi > 1;
}

inline bool format_fps_label(float fps, char* out, std::size_t out_n) {
  if (!out || out_n < 6) {
    return false;
  }
  int v = static_cast<int>(fps + 0.5f);
  if (v < 0) {
    v = 0;
  }
  if (v > 999) {
    v = 999;
  }
  // "FPS" + optional space is separate; here just the number "120"
  char tmp[8];
  int n = 0;
  if (v >= 100) {
    tmp[n++] = static_cast<char>('0' + (v / 100));
  }
  if (v >= 10) {
    tmp[n++] = static_cast<char>('0' + ((v / 10) % 10));
  }
  tmp[n++] = static_cast<char>('0' + (v % 10));
  tmp[n] = '\0';
  // Prefixed "F" kept short: "F120" fits
  std::size_t oi = 0;
  out[oi++] = 'F';
  for (int i = 0; i < n && oi + 1 < out_n; ++i) {
    out[oi++] = tmp[i];
  }
  out[oi] = '\0';
  return true;
}

}  // namespace fury
