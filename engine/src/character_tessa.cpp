#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/tessa_anatomy.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
namespace fury {
namespace {
using namespace character_detail;
// Generated construction-only ray supports for the accepted Tessa strap.
// Source fingerprint: 6cc15126898ef2bd74bff360ebf360aa3ba7ae52fc33353519731c863d7163d5.
// Exact normalized source coordinates, not a frozen height-1.65 strap mesh.
// Never added to CharacterModel, renderer geometry, or animation state.
#include <array>
#include <cstdint>
namespace tessa_strap_reference {
struct SupportPoint { float x, y, z; };
inline constexpr SupportPoint near_vertices[] = {
    {0x1.7054p-4f, 0x1.33475ep-3f, 0x0p+0f}, // accepted vertex 451
    {0x1.5e6112p-4f, 0x1.33475ep-3f, -0x1.4a3cc4p-6f}, // accepted vertex 489
    {0x1.6bd768p-4f, 0x1.33475ep-3f, -0x1.4e4fbcp-7f}, // accepted vertex 490
    {0x1.79a5ecp-4f, 0x1.4d67dap-3f, 0x0p+0f}, // accepted vertex 492
    {0x1.74d8e4p-4f, 0x1.4d6a5ep-3f, 0x1.57201ep-7f}, // accepted vertex 493
    {0x1.66f878p-4f, 0x1.4d68f6p-3f, 0x1.52d694p-6f}, // accepted vertex 494
    {0x1.50584p-4f, 0x1.4d67c2p-3f, 0x1.f1e20cp-6f}, // accepted vertex 495
    {0x1.31a4e4p-4f, 0x1.4d59f6p-3f, -0x1.42ab4ep-5f}, // accepted vertex 528
    {0x1.50aaccp-4f, 0x1.4d59f6p-3f, -0x1.f28a2p-6f}, // accepted vertex 529
    {0x1.6757dcp-4f, 0x1.4d59f6p-3f, -0x1.53528ap-6f}, // accepted vertex 530
    {0x1.751b98p-4f, 0x1.4d59f6p-3f, -0x1.57749p-7f}, // accepted vertex 531
    {0x1.711618p-4f, 0x1.6878f2p-3f, 0x1.5bd9c2p-6f}, // accepted vertex 535
    {0x1.59de48p-4f, 0x1.686368p-3f, 0x1.ff37eep-6f}, // accepted vertex 536
    {0x1.3a38fap-4f, 0x1.68320ep-3f, 0x1.4b33b4p-5f}, // accepted vertex 537
    {0x1.12d35ep-4f, 0x1.676c8cp-3f, -0x1.8ece7p-5f}, // accepted vertex 568
    {0x1.3a999ap-4f, 0x1.676c8cp-3f, -0x1.4bc01ep-5f}, // accepted vertex 569
    {0x1.5a8b26p-4f, 0x1.676c8cp-3f, -0x1.004c0ap-5f}, // accepted vertex 570
    {0x1.71cefap-4f, 0x1.676c8cp-3f, -0x1.5cca08p-6f}, // accepted vertex 571
    {0x1.43bd66p-4f, 0x1.83caaep-3f, 0x1.53c458p-5f}, // accepted vertex 578
    {0x1.1b5caep-4f, 0x1.82cddcp-3f, 0x1.998bdep-5f}, // accepted vertex 579
    {0x1.1b503ap-4f, 0x1.817f24p-3f, -0x1.9972ep-5f}, // accepted vertex 609
    {0x1.44694p-4f, 0x1.817f24p-3f, -0x1.54be12p-5f}, // accepted vertex 610
    {0x1.24927cp-4f, 0x1.9e1ecep-3f, 0x1.a422cp-5f}, // accepted vertex 620
    {0x1.242b9p-4f, 0x1.9b91bap-3f, -0x1.a354d4p-5f}, // accepted vertex 650
    {0x1.2edf6ap-4f, 0x1.b9bc14p-3f, 0x1.af61d6p-5f}, // accepted vertex 661
    {0x1.f9bca2p-5f, 0x1.b75e62p-3f, 0x1.f0981ep-5f}, // accepted vertex 662
    {0x1.f31c6cp-5f, 0x1.b5a452p-3f, -0x1.e7795cp-5f}, // accepted vertex 690
    {0x1.2d12e2p-4f, 0x1.b5a452p-3f, -0x1.abc8b2p-5f}, // accepted vertex 691
    {0x1.58a4b6p-4f, 0x1.b5a452p-3f, -0x1.63db2ap-5f}, // accepted vertex 692
    {0x1.06124ep-4f, 0x1.d22f52p-3f, 0x1.fd5ddp-5f}, // accepted vertex 703
    {0x1.94c0f6p-5f, 0x1.cffe8p-3f, 0x1.186a12p-4f}, // accepted vertex 704
    {0x1.13c68ap-6f, 0x1.cfb6e8p-3f, 0x1.31d9dep-4f}, // accepted vertex 706
    {0x1.35a976p-4f, 0x1.cfb6e8p-3f, -0x1.b1fb24p-5f}, // accepted vertex 732
    {0x1.0c4696p-4f, 0x1.ed0fcp-3f, 0x1.fff5b6p-5f}, // accepted vertex 744
    {0x1.9b44eep-5f, 0x1.ea2864p-3f, 0x1.16d4d2p-4f}, // accepted vertex 745
    {0x1.15375p-6f, 0x1.e9c98p-3f, 0x1.2b4858p-4f}, // accepted vertex 747
    {-0x1.42ccf4p-28f, 0x1.e9c98p-3f, 0x1.2c28f4p-4f}, // accepted vertex 748
    {0x1.075d7ap-4f, 0x1.e9c98p-3f, -0x1.f2710ep-5f}, // accepted vertex 772
    {0x1.3d7578p-4f, 0x1.e9c98p-3f, -0x1.b4ea06p-5f}, // accepted vertex 773
    {0x1.16c72cp-6f, 0x1.01ee0ap-2f, 0x1.23d07cp-4f}, // accepted vertex 788
    {-0x1.46375p-28f, 0x1.01ee0ap-2f, 0x1.26c28p-4f}, // accepted vertex 789
    {0x1.9d9a58p-5f, 0x1.01ee0ap-2f, -0x1.1028fp-4f}, // accepted vertex 812
    {0x1.0d4af2p-4f, 0x1.01ee0ap-2f, -0x1.f27c4ep-5f}, // accepted vertex 813
    {0x1.43ec82p-4f, 0x1.01ee0ap-2f, -0x1.b39ee6p-5f}, // accepted vertex 814
    {-0x1.4af2dp-28f, 0x1.0ef756p-2f, 0x1.22526cp-4f}, // accepted vertex 830
    {0x1.1c5f46p-5f, 0x1.0ef756p-2f, -0x1.1c8608p-4f}, // accepted vertex 852
    {0x1.a597acp-5f, 0x1.0ef756p-2f, -0x1.0e4bd4p-4f}, // accepted vertex 853
    {0x1.11f70cp-4f, 0x1.0ef756p-2f, -0x1.edb9bp-5f}, // accepted vertex 854
    {-0x1.4ff8cp-28f, 0x1.1c00ap-2f, 0x1.1deb2cp-4f}, // accepted vertex 871
    {-0x1.2180d8p-6f, 0x1.1bffd2p-2f, 0x1.1edf44p-4f}, // accepted vertex 872
    {0x1.20ec9ep-5f, 0x1.1bfd7cp-2f, -0x1.18d38ep-4f}, // accepted vertex 893
    {0x1.ab7dap-5f, 0x1.1bf9dap-2f, -0x1.0a0ecap-4f}, // accepted vertex 894
    {0x1.150d2cp-4f, 0x1.1bf544p-2f, -0x1.e40394p-5f}, // accepted vertex 895
    {-0x1.2527fcp-6f, 0x1.28f0c6p-2f, 0x1.19ca5p-4f}, // accepted vertex 913
    {0x1.73e5a4p-30f, 0x1.2909ecp-2f, -0x1.198608p-4f}, // accepted vertex 932
    {0x1.2527dep-6f, 0x1.28f0c6p-2f, -0x1.19ca28p-4f}, // accepted vertex 933
    {0x1.23aafcp-5f, 0x1.28a7cap-2f, -0x1.129a6p-4f}, // accepted vertex 934
    {0x1.ae1d34p-5f, 0x1.28361cp-2f, -0x1.02d8a4p-4f}, // accepted vertex 935
    {-0x1.270acep-6f, 0x1.35cb7ep-2f, 0x1.12e4ecp-4f}, // accepted vertex 954
    {-0x1.246d3p-5f, 0x1.34fb5ap-2f, 0x1.0a4378p-4f}, // accepted vertex 955
    {-0x1.24837ap-6f, 0x1.35cb8p-2f, -0x1.0ee79p-4f}, // accepted vertex 972
    {0x1.77988ap-30f, 0x1.361338p-2f, -0x1.142646p-4f}, // accepted vertex 973
    {0x1.270acap-6f, 0x1.35cb8p-2f, -0x1.12e4ecp-4f}, // accepted vertex 974
    {0x1.246d2cp-5f, 0x1.34fb5ap-2f, -0x1.0a4374p-4f}, // accepted vertex 975
    {-0x1.23e5e6p-5f, 0x1.413622p-2f, 0x1.018f7ap-4f}, // accepted vertex 996
    {-0x1.acd1a6p-5f, 0x1.3f6ffp-2f, 0x1.e28e04p-5f}, // accepted vertex 997
    {-0x1.224c28p-5f, 0x1.413622p-2f, -0x1.fe31fp-5f}, // accepted vertex 1012
    {-0x1.263c86p-6f, 0x1.429fdep-2f, -0x1.096e22p-4f}, // accepted vertex 1013
    {0x1.78f34ep-30f, 0x1.431c84p-2f, -0x1.0d8bd6p-4f}, // accepted vertex 1014
    {-0x1.abdf58p-5f, 0x1.4b103ep-2f, 0x1.d445b2p-5f}, // accepted vertex 1038
    {-0x1.14e938p-4f, 0x1.4ace3cp-2f, 0x1.a8fa86p-5f}, // accepted vertex 1039
    {-0x1.ab71c8p-5f, 0x1.4a9e7ep-2f, -0x1.d36ec4p-5f}, // accepted vertex 1052
    {-0x1.26ae3ap-6f, 0x1.4f7dc2p-2f, -0x1.033ea6p-4f}, // accepted vertex 1054
    {-0x1.14ddc6p-4f, 0x1.56e7cp-2f, 0x1.9eed36p-5f}, // accepted vertex 1080
    {-0x1.4d121ap-4f, 0x1.57c584p-2f, 0x1.6aa8bep-5f}, // accepted vertex 1081
    {-0x1.abb05ap-5f, 0x1.57103ep-2f, -0x1.c8fa2ep-5f}, // accepted vertex 1093
    {-0x1.231d56p-5f, 0x1.5a5928p-2f, -0x1.e7c686p-5f}, // accepted vertex 1094
    {-0x1.14ddc6p-4f, 0x1.56e7bep-2f, 0x1.9eed36p-5f}, // accepted vertex 1121
    {-0x1.4d121ap-4f, 0x1.57c584p-2f, 0x1.6aa8bep-5f}, // accepted vertex 1122
    {-0x1.14ddcep-4f, 0x1.52ec9cp-2f, -0x1.9eed3p-5f}, // accepted vertex 1133
    {-0x1.4436a8p-4f, 0x1.5b4762p-2f, 0x1.6c7f8ep-5f}, // accepted vertex 1163
    {-0x1.720c5cp-4f, 0x1.4f17b8p-2f, -0x1.21c486p-5f}, // accepted vertex 1172
    {-0x1.42ded2p-4f, 0x1.531b7ep-2f, -0x1.5cc922p-5f}, // accepted vertex 1173
    {-0x1.0c09d8p-4f, 0x1.571f44p-2f, -0x1.8f0818p-5f}, // accepted vertex 1174
    {-0x1.9db86ap-5f, 0x1.5abe72p-2f, -0x1.b77b2p-5f}, // accepted vertex 1175
    {-0x1.2cdcep-4f, 0x1.5b3b26p-2f, 0x1.7158c6p-5f}, // accepted vertex 1204
    {-0x1.58de66p-4f, 0x1.5b4762p-2f, 0x1.37f454p-5f}, // accepted vertex 1205
    {-0x1.7c81f4p-4f, 0x1.5cc33ep-2f, 0x1.ee1394p-6f}, // accepted vertex 1206
    {-0x1.7a8094p-4f, 0x1.511094p-2f, -0x1.8b4086p-6f}, // accepted vertex 1212
    {-0x1.54fb36p-4f, 0x1.542b4cp-2f, -0x1.02efecp-5f}, // accepted vertex 1213
    {-0x1.27fa8ep-4f, 0x1.579c4p-2f, -0x1.3835c4p-5f}, // accepted vertex 1214
    {-0x1.e98788p-5f, 0x1.5b0d32p-2f, -0x1.652082p-5f}, // accepted vertex 1215
    {-0x1.54578ap-4f, 0x1.60d29ep-2f, 0x1.059708p-5f}, // accepted vertex 1247
    {-0x1.6ccd6ap-4f, 0x1.5f0b6cp-2f, 0x1.84b8e8p-6f}, // accepted vertex 1248
    {-0x1.7c0102p-4f, 0x1.5f0b6cp-2f, 0x1.e9c05cp-7f}, // accepted vertex 1249
    {-0x1.8197c8p-4f, 0x1.6076cep-2f, 0x1.7fafc4p-8f}, // accepted vertex 1250
    {-0x1.7ef858p-4f, 0x1.5b048ap-2f, -0x1.b74a6cp-9f}, // accepted vertex 1251
    {-0x1.6cd596p-4f, 0x1.5b3a5cp-2f, -0x1.89d748p-7f}, // accepted vertex 1252
    {-0x1.5047f8p-4f, 0x1.5b3a5cp-2f, -0x1.430f6ap-6f}, // accepted vertex 1253
    {-0x1.2be2f4p-4f, 0x1.58bfe2p-2f, -0x1.aeb2d4p-6f}, // accepted vertex 1254
    {-0x1.01f5ap-4f, 0x1.5b9e02p-2f, -0x1.047fe6p-5f}, // accepted vertex 1255
    {-0x1.3c8a16p-4f, 0x1.63593p-2f, 0x1.ae5a5cp-6f}, // accepted vertex 1289
    {-0x1.4a8cacp-4f, 0x1.64beb6p-2f, 0x1.25db0cp-6f}, // accepted vertex 1290
    {-0x1.500436p-4f, 0x1.64beb6p-2f, 0x1.2f28eep-7f}, // accepted vertex 1291
    {-0x1.4f3c9cp-4f, 0x1.60d6a8p-2f, 0x1.956ecp-12f}, // accepted vertex 1292
    {-0x1.3c96fcp-4f, 0x1.60d6a8p-2f, -0x1.fd08d6p-8f}, // accepted vertex 1293
    {-0x1.1fe19ap-4f, 0x1.606cdap-2f, -0x1.e09b8p-7f}, // accepted vertex 1294
    {-0x1.f98e5p-5f, 0x1.5f1d5ep-2f, -0x1.4ac3ecp-6f}, // accepted vertex 1295
    {0x1.6872bp-7f, 0x1.e68bdap-3f, 0x1.32cfeep-4f}, // accepted vertex 1950
    {0x1.6872bp-7f, 0x1.fd70a4p-3f, 0x1.2c97fep-4f}, // accepted vertex 1959
    {0x1.0624dcp-7f, 0x1.fd70a4p-3f, 0x1.2ec944p-4f}, // accepted vertex 1960
    {0x1.47ae14p-8f, 0x1.fd70a4p-3f, 0x1.3096d2p-4f}, // accepted vertex 1961
    {0x1.47ae14p-8f, 0x1.0a2ab6p-2f, 0x1.2c0966p-4f}, // accepted vertex 1970
    {0x1.0624ep-9f, 0x1.0a2ab6p-2f, 0x1.2d895ep-4f}, // accepted vertex 1971
    {0x1.0624ep-9f, 0x1.159d1cp-2f, 0x1.2994f6p-4f}, // accepted vertex 1980
    {-0x1.0624ep-10f, 0x1.159d1cp-2f, 0x1.2a85dcp-4f}, // accepted vertex 1981
    {-0x1.0624ep-8f, 0x1.159d1cp-2f, 0x1.2aa486p-4f}, // accepted vertex 1982
    {-0x1.0624ep-8f, 0x1.210f82p-2f, 0x1.26bc8ep-4f}, // accepted vertex 1991
    {-0x1.cac08p-8f, 0x1.210f82p-2f, 0x1.26233cp-4f}, // accepted vertex 1992
    {-0x1.47ae14p-7f, 0x1.210f82p-2f, 0x1.24e21ap-4f}, // accepted vertex 1993
    {-0x1.a9fbe8p-7f, 0x1.210f82p-2f, 0x1.232fcap-4f}, // accepted vertex 1994
    {-0x1.a9fbe8p-7f, 0x1.2c81e6p-2f, 0x1.1ea14p-4f}, // accepted vertex 2003
};
inline constexpr std::array<std::uint8_t, 3> near_triangles[] = {
    {0, 3, 4}, // accepted face 880
    {1, 10, 2}, // accepted face 957
    {5, 11, 12}, // accepted face 964
    {6, 12, 13}, // accepted face 966
    {7, 15, 16}, // accepted face 1032
    {8, 17, 9}, // accepted face 1035
    {13, 18, 19}, // accepted face 1048
    {14, 20, 21}, // accepted face 1110
    {22, 24, 25}, // accepted face 1210
    {23, 27, 28}, // accepted face 1270
    {24, 29, 25}, // accepted face 1291
    {26, 32, 27}, // accepted face 1349
    {29, 33, 34}, // accepted face 1372
    {29, 34, 30}, // accepted face 1373
    {31, 35, 36}, // accepted face 1378
    {35, 39, 40}, // accepted face 1458
    {35, 40, 36}, // accepted face 1459
    {37, 43, 38}, // accepted face 1509
    {39, 44, 40}, // accepted face 1539
    {41, 47, 42}, // accepted face 1587
    {44, 48, 49}, // accepted face 1620
    {45, 50, 51}, // accepted face 1664
    {46, 52, 47}, // accepted face 1667
    {48, 53, 49}, // accepted face 1701
    {50, 56, 57}, // accepted face 1744
    {53, 58, 59}, // accepted face 1782
    {54, 61, 62}, // accepted face 1820
    {55, 63, 56}, // accepted face 1823
    {59, 64, 65}, // accepted face 1864
    {60, 68, 61}, // accepted face 1899
    {65, 69, 70}, // accepted face 1946
    {66, 72, 67}, // accepted face 1977
    {70, 73, 74}, // accepted face 2028
    {71, 75, 76}, // accepted face 2054
    {77, 80, 78}, // accepted face 2109
    {79, 83, 84}, // accepted face 2132
    {80, 85, 86}, // accepted face 2190
    {81, 89, 90}, // accepted face 2208
    {82, 91, 83}, // accepted face 2211
    {86, 92, 87}, // accepted face 2273
    {87, 92, 93}, // accepted face 2274
    {88, 98, 99}, // accepted face 2286
    {88, 99, 89}, // accepted face 2287
    {89, 100, 90}, // accepted face 2289
    {93, 101, 102}, // accepted face 2356
    {93, 102, 94}, // accepted face 2357
    {94, 102, 103}, // accepted face 2358
    {95, 103, 104}, // accepted face 2360
    {95, 104, 96}, // accepted face 2361
    {96, 104, 105}, // accepted face 2362
    {96, 105, 97}, // accepted face 2363
    {97, 106, 98}, // accepted face 2365
    {98, 106, 107}, // accepted face 2366
    {108, 109, 110}, // accepted face 3520
    {110, 112, 111}, // accepted face 3539
    {113, 114, 115}, // accepted face 3558
    {116, 117, 118}, // accepted face 3578
    {119, 121, 120}, // accepted face 3599
};
inline constexpr SupportPoint far_vertices[] = {
    {0x1.754a4cp-4f, 0x1.41c37ap-3f, 0x0p+0f}, // accepted vertex 132
    {0x1.6ddd84p-4f, 0x1.41c37ap-3f, 0x1.a6a5c4p-7f}, // accepted vertex 133
    {0x1.58a6d4p-4f, 0x1.41c37ap-3f, 0x1.9e8cc6p-6f}, // accepted vertex 134
    {0x1.364b44p-4f, 0x1.41c37ap-3f, 0x1.2d1238p-5f}, // accepted vertex 135
    {0x1.367666p-4f, 0x1.41c37ap-3f, -0x1.2d4bdep-5f}, // accepted vertex 161
    {0x1.58fe74p-4f, 0x1.41c37ap-3f, -0x1.9f1deep-6f}, // accepted vertex 162
    {0x1.6e342p-4f, 0x1.41c37ap-3f, -0x1.a72f88p-7f}, // accepted vertex 163
    {0x1.8c6bep-4f, 0x1.91c03ap-3f, 0x1.c5b9c4p-7f}, // accepted vertex 166
    {0x1.511274p-4f, 0x1.907cbap-3f, 0x1.445d5ep-5f}, // accepted vertex 168
    {0x1.1f5b98p-4f, 0x1.8ee726p-3f, 0x1.9e3db6p-5f}, // accepted vertex 169
    {0x1.c45df2p-5f, 0x1.8d9598p-3f, 0x1.e8512ap-5f}, // accepted vertex 170
    {0x1.c25fb2p-5f, 0x1.8d15ap-3f, -0x1.e5556cp-5f}, // accepted vertex 192
    {0x1.1f394p-4f, 0x1.8d15ap-3f, -0x1.9df8fp-5f}, // accepted vertex 193
    {0x1.521152p-4f, 0x1.8d15ap-3f, -0x1.45b1f8p-5f}, // accepted vertex 194
    {0x1.775a6ep-4f, 0x1.8d15ap-3f, -0x1.c0359p-6f}, // accepted vertex 195
    {0x1.8da764p-4f, 0x1.8d15ap-3f, -0x1.c7afcap-7f}, // accepted vertex 196
    {0x1.3d826ep-4f, 0x1.dee23cp-3f, 0x1.bda12p-5f}, // accepted vertex 202
    {0x1.f42cbep-5f, 0x1.da2faep-3f, 0x1.06eed8p-4f}, // accepted vertex 203
    {0x1.56a5ap-5f, 0x1.d867c6p-3f, 0x1.21e304p-4f}, // accepted vertex 204
    {0x1.599ap-6f, 0x1.d867c6p-3f, 0x1.2f04dp-4f}, // accepted vertex 205
    {-0x1.417c3cp-28f, 0x1.d867c6p-3f, 0x1.30750cp-4f}, // accepted vertex 206
    {0x1.e8ec74p-5f, 0x1.d867c6p-3f, -0x1.fd06ecp-5f}, // accepted vertex 225
    {0x1.38656ep-4f, 0x1.d867c6p-3f, -0x1.b3670ap-5f}, // accepted vertex 226
    {0x1.6e6f2p-4f, 0x1.d867c6p-3f, -0x1.54e26ep-5f}, // accepted vertex 227
    {0x1.5d406p-5f, 0x1.11dcf4p-2f, 0x1.0e1f0ap-4f}, // accepted vertex 237
    {-0x1.4d125ap-28f, 0x1.11dcf4p-2f, 0x1.223bacp-4f}, // accepted vertex 239
    {-0x1.66b588p-6f, 0x1.11dcf4p-2f, 0x1.220bdcp-4f}, // accepted vertex 240
    {0x1.6b6d3p-30f, 0x1.11dcf4p-2f, -0x1.2230d2p-4f}, // accepted vertex 255
    {0x1.669576p-6f, 0x1.11dcf4p-2f, -0x1.21e37ep-4f}, // accepted vertex 256
    {0x1.645ac8p-5f, 0x1.11dcf4p-2f, -0x1.16b1fep-4f}, // accepted vertex 257
    {0x1.047878p-4f, 0x1.11dcf4p-2f, -0x1.fb048ap-5f}, // accepted vertex 258
    {0x1.4a71ecp-4f, 0x1.11dcf4p-2f, -0x1.ad0a7p-5f}, // accepted vertex 259
    {-0x1.7338dcp-6f, 0x1.370d6p-2f, 0x1.12a34ap-4f}, // accepted vertex 273
    {-0x1.6d281p-5f, 0x1.35b5c4p-2f, 0x1.03fddp-4f}, // accepted vertex 274
    {-0x1.08cb8p-4f, 0x1.360532p-2f, 0x1.d32bd2p-5f}, // accepted vertex 275
    {-0x1.6faff2p-6f, 0x1.370d6p-2f, -0x1.0e31ecp-4f}, // accepted vertex 287
    {0x1.7aba0ep-30f, 0x1.378608p-2f, -0x1.15908p-4f}, // accepted vertex 288
    {0x1.7338e8p-6f, 0x1.370d6p-2f, -0x1.12a34ap-4f}, // accepted vertex 289
    {0x1.6d2812p-5f, 0x1.35b5c4p-2f, -0x1.03fdcap-4f}, // accepted vertex 290
    {0x1.08cb74p-4f, 0x1.33b388p-2f, -0x1.d32ba8p-5f}, // accepted vertex 291
    {-0x1.6c1e4ep-5f, 0x1.58d5cap-2f, 0x1.de934cp-5f}, // accepted vertex 307
    {-0x1.084f18p-4f, 0x1.569ca4p-2f, 0x1.aeb4dap-5f}, // accepted vertex 308
    {-0x1.5066c4p-4f, 0x1.57c584p-2f, 0x1.6e4928p-5f}, // accepted vertex 309
    {-0x1.73407cp-6f, 0x1.5c0dc2p-2f, -0x1.fc0d8cp-5f}, // accepted vertex 320
    {0x1.7cb89p-30f, 0x1.5d2f1ap-2f, -0x1.0300cep-4f}, // accepted vertex 321
    {-0x1.5066c4p-4f, 0x1.57c584p-2f, 0x1.6e4928p-5f}, // accepted vertex 342
    {-0x1.8b90f6p-4f, 0x1.5a4f62p-2f, 0x1.1fc9f2p-5f}, // accepted vertex 343
    {-0x1.8b90f2p-4f, 0x1.48a766p-2f, -0x1.1fc9f6p-5f}, // accepted vertex 349
    {-0x1.5066cp-4f, 0x1.4e5604p-2f, -0x1.6e492cp-5f}, // accepted vertex 350
    {-0x1.084f0ep-4f, 0x1.5404ap-2f, -0x1.aeb4e2p-5f}, // accepted vertex 351
    {-0x1.6c1e36p-5f, 0x1.58d5cap-2f, -0x1.de9352p-5f}, // accepted vertex 352
    {-0x1.1a18bap-4f, 0x1.5ad00ep-2f, 0x1.78712ap-5f}, // accepted vertex 375
    {-0x1.4d1f74p-4f, 0x1.5e10f4p-2f, 0x1.3213eep-5f}, // accepted vertex 376
    {-0x1.73a138p-4f, 0x1.5cc33ep-2f, 0x1.c0938ep-6f}, // accepted vertex 377
    {-0x1.8be89ap-4f, 0x1.5f0b6cp-2f, 0x1.0c50d4p-6f}, // accepted vertex 378
    {-0x1.94837cp-4f, 0x1.5f0b6cp-2f, 0x1.3a634cp-8f}, // accepted vertex 379
    {-0x1.8dc08ap-4f, 0x1.5b048ap-2f, -0x1.c25ac4p-8f}, // accepted vertex 380
    {-0x1.723facp-4f, 0x1.53593ap-2f, -0x1.211824p-6f}, // accepted vertex 381
    {-0x1.472068p-4f, 0x1.568f56p-2f, -0x1.b6ef78p-6f}, // accepted vertex 382
    {-0x1.11fe2p-4f, 0x1.5a5914p-2f, -0x1.19a326p-5f}, // accepted vertex 383
    {-0x1.a9d948p-5f, 0x1.5e22d2p-2f, -0x1.4b0c1ap-5f}, // accepted vertex 384
    {-0x1.2388fcp-5f, 0x1.6158eep-2f, -0x1.6fe016p-5f}, // accepted vertex 385
    {-0x1.089ed8p-4f, 0x1.65fa04p-2f, 0x1.874214p-6f}, // accepted vertex 411
    {-0x1.103ac8p-4f, 0x1.67ec4p-2f, 0x1.c12038p-7f}, // accepted vertex 412
    {-0x1.0de342p-4f, 0x1.67b21p-2f, 0x1.ad6a2p-9f}, // accepted vertex 413
    {-0x1.e32b18p-5f, 0x1.64177cp-2f, -0x1.66c486p-8f}, // accepted vertex 414
    {-0x1.900c8p-5f, 0x1.60ecacp-2f, -0x1.725792p-7f}, // accepted vertex 415
    {-0x1.3c2e84p-5f, 0x1.625e8ap-2f, -0x1.f1bb4ep-7f}, // accepted vertex 416
    {0x1.6872bp-7f, 0x1.fd70a4p-3f, 0x1.2c97fep-4f}, // accepted vertex 575
    {0x1.47ae14p-8f, 0x1.fd70a4p-3f, 0x1.3096d2p-4f}, // accepted vertex 576
    {-0x1.0624ep-10f, 0x1.fd70a4p-3f, 0x1.324d38p-4f}, // accepted vertex 577
    {0x1.47ae14p-8f, 0x1.196de8p-2f, 0x1.267e54p-4f}, // accepted vertex 581
    {-0x1.0624ep-10f, 0x1.196de8p-2f, 0x1.293b04p-4f}, // accepted vertex 582
    {-0x1.cac08p-8f, 0x1.196de8p-2f, 0x1.28c14ap-4f}, // accepted vertex 583
    {-0x1.a9fbe8p-7f, 0x1.196de8p-2f, 0x1.25c4acp-4f}, // accepted vertex 584
    {-0x1.a9fbe8p-7f, 0x1.34238p-2f, 0x1.1b1ffap-4f}, // accepted vertex 589
};
inline constexpr std::array<std::uint8_t, 3> far_triangles[] = {
    {0, 7, 1}, // accepted face 257
    {2, 8, 3}, // accepted face 261
    {4, 13, 14}, // accepted face 314
    {5, 15, 6}, // accepted face 317
    {8, 16, 9}, // accepted face 327
    {9, 17, 10}, // accepted face 329
    {11, 21, 22}, // accepted face 374
    {12, 23, 13}, // accepted face 377
    {17, 24, 18}, // accepted face 395
    {19, 25, 20}, // accepted face 399
    {20, 25, 26}, // accepted face 400
    {21, 30, 31}, // accepted face 438
    {25, 32, 26}, // accepted face 465
    {26, 32, 33}, // accepted face 466
    {27, 36, 37}, // accepted face 496
    {28, 38, 29}, // accepted face 499
    {29, 39, 30}, // accepted face 501
    {32, 40, 33}, // accepted face 531
    {33, 41, 34}, // accepted face 533
    {34, 41, 42}, // accepted face 534
    {35, 43, 44}, // accepted face 558
    {45, 51, 52}, // accepted face 600
    {46, 52, 53}, // accepted face 602
    {47, 58, 59}, // accepted face 614
    {48, 60, 49}, // accepted face 617
    {49, 61, 50}, // accepted face 619
    {53, 62, 54}, // accepted face 669
    {54, 62, 63}, // accepted face 670
    {55, 63, 64}, // accepted face 672
    {55, 64, 56}, // accepted face 673
    {56, 65, 57}, // accepted face 675
    {57, 66, 58}, // accepted face 677
    {58, 67, 59}, // accepted face 679
    {68, 71, 69}, // accepted face 937
    {69, 72, 70}, // accepted face 939
    {73, 75, 74}, // accepted face 951
};
} // namespace fury::character_detail::tessa_strap_reference

// Reconstruct the accepted strap's original projected centerline before b.tube.
// The immutable supports are just the ray-intersected accepted coat facets.
// Preserve the historical scale/unscale operations: bypassing that round-trip
// changes float bits, which then propagate through the parallel-transport frame.
// This is factory-time work only, and produces no additional model geometry.
inline void fit_accepted_tessa_strap(const Sculpt &b, std::vector<Vec3> &strap) {
  auto project = [&](const auto &vertices, const auto &triangles) {
    for (std::size_t point_index = 0; point_index < strap.size(); ++point_index) {
      auto &point = strap[point_index];
      const Vec3 anchor{0, .220f, 0}, direction = normalize(point - anchor);
      float distance = 0;
      auto source_point = [&](std::uint8_t index) {
        const auto &p = vertices[index];
        return Vec3{p.x, p.y, p.z};
      };
      for (const auto &triangle : triangles) {
        Vec3 a = (source_point(triangle[0]) * b.h) * (1 / b.h),
             bv = (source_point(triangle[1]) * b.h) * (1 / b.h),
             c = (source_point(triangle[2]) * b.h) * (1 / b.h);
        Vec3 edge1 = bv - a, edge2 = c - a, p = cross(direction, edge2);
        float det = dot(edge1, p);
        if (std::fabs(det) < 1e-12f)
          continue;
        Vec3 delta = anchor - a;
        float u = dot(delta, p) / det;
        if (u < 0 || u > 1)
          continue;
        Vec3 q = cross(delta, edge1);
        float v = dot(direction, q) / det;
        if (v < 0 || u + v > 1)
          continue;
        distance = std::max(distance, dot(edge2, q) / det);
      }
      if (distance > 0) {
        float t = float(point_index) / (strap.size() - 1),
              fit = smooth(t / .11f) * smooth((1 - t) / .11f);
        point = point * (1 - fit) +
                (anchor + direction * (distance + .0018f)) * fit;
      }
    }
  };
  if (b.near)
    project(tessa_strap_reference::near_vertices,
            tessa_strap_reference::near_triangles);
  else
    project(tessa_strap_reference::far_vertices,
            tessa_strap_reference::far_triangles);
}

// Original integration of the documented CC0 anatomy data. No upstream
// program logic is included. Surface queries fit Fury's own cosmetics/hair.
float anatomy_front(float x, float y) {
  float best = -1.f;
  for (std::size_t i = 0; i < tessa_anatomy::indices.size(); i += 3) {
    const Vec3 a = tessa_anatomy::positions[tessa_anatomy::indices[i]];
    const Vec3 b = tessa_anatomy::positions[tessa_anatomy::indices[i + 1]];
    const Vec3 c = tessa_anatomy::positions[tessa_anatomy::indices[i + 2]];
    const float denominator =
        (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::fabs(denominator) < 1e-12f)
      continue;
    const float u =
        ((x - a.x) * (c.y - a.y) - (y - a.y) * (c.x - a.x)) / denominator;
    const float v =
        ((b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x)) / denominator;
    if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f)
      best = std::max(best, a.z + u * (b.z - a.z) + v * (c.z - a.z));
  }
  return best;
}
float anatomy_radius(float y, float angle, float center_z) {
  const Vec3 origin{0, y, center_z},
      direction{std::cos(angle), 0, std::sin(angle)};
  float best = 0.f;
  for (std::size_t i = 0; i < tessa_anatomy::indices.size(); i += 3) {
    const Vec3 a = tessa_anatomy::positions[tessa_anatomy::indices[i]];
    const Vec3 b = tessa_anatomy::positions[tessa_anatomy::indices[i + 1]];
    const Vec3 c = tessa_anatomy::positions[tessa_anatomy::indices[i + 2]];
    if (y < std::min({a.y, b.y, c.y}) - 1e-6f ||
        y > std::max({a.y, b.y, c.y}) + 1e-6f)
      continue;
    const Vec3 e1 = b - a, e2 = c - a, p = cross(direction, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < 1e-12f)
      continue;
    const Vec3 t = origin - a;
    const float u = dot(t, p) / det;
    if (u < 0.f || u > 1.f)
      continue;
    const Vec3 q = cross(t, e1);
    const float v = dot(direction, q) / det;
    if (v < 0.f || u + v > 1.f)
      continue;
    const float d = dot(e2, q) / det;
    if (d > best)
      best = d;
  }
  return best;
}
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      tessa_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < tessa_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = tessa_anatomy::indices[triangle + corner];
      const auto uv_index = tessa_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = tessa_anatomy::positions[index];
        const Vec2 uv = tessa_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .018f) *
                          g(position.y, .393f, .0040f) *
                          smooth((position.z - .087f) / .012f);
        // Small anatomical pigment zones retain the stable base complexion.
        const float cheeks = g(std::fabs(position.x), .030f, .015f) *
                             g(position.y, .410f, .014f) *
                             smooth((position.z - .067f) / .02f);
        const float nose = g(position.x, 0, .009f) *
                           g(position.y, .411f, .011f) *
                           smooth((position.z - .09f) / .016f);
        const float front = smooth((position.z - .060f) / .025f);
        const float vascular =
            .5f + .25f * std::sin(position.x * 360 + position.y * 97) +
            .25f * std::sin(position.y * 285 - position.z * 190);
        const float lid = g(std::fabs(position.x), .020f, .009f) *
                          g(position.y, .423f, .006f) * front;
        Vec3 color = skin * (1 - .025f * vascular * front);
        color.x *= 1 - .015f * lid;
        color.y *= 1 - .055f * lid;
        color.z *= 1 - .037f * lid;
        color.y *= 1 - .090f * cheeks - .065f * nose;
        color.z *= 1 - .115f * cheeks - .085f * nose;
        color.y *= 1 - .22f * lip;
        color.z *= 1 - .19f * lip;
        const auto vertex = b.vertex(
            {position, color,
             position.y < .355f
                 ? wt(B::Neck, B::Chest, smooth((position.y - .328f) / .027f))
                 : wt(B::Head, B::Neck, smooth((position.y - .355f) / .022f)),
             character_surface_uv(S::Face, uv.x, uv.y)});
        groups[index].push_back(vertex);
        found = corners.emplace(key, vertex).first;
      }
      out[corner] = found->second;
    }
    b.tri(out[0], out[1], out[2]);
  }
  for (auto &group : groups)
    if (group.size() > 1)
      b.normal_groups.push_back(std::move(group));
  for (std::size_t ei = 0; ei < tessa_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = tessa_anatomy::eye_centers[ei] + Vec3{0, -.0005f, -.0025f};
    const Vec3 radius = tessa_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 24 : 12, b.near ? 8 : 6);
    if (b.near) {
      std::vector<Vec3> upper_lid, lower_lid;
      const bool positive = eye.x > 0;
      for (std::size_t i = 0; i < tessa_anatomy::positions.size(); ++i) {
        const auto id = tessa_anatomy::source_vertex_indices[i];
        const bool socket =
            positive
                ? ((id >= 7773 && id <= 7798) || id == 11718 || id == 11719)
                : ((id >= 1081 && id <= 1106) || id == 5103 || id == 5104);
        if (!socket)
          continue;
        if (std::fabs(tessa_anatomy::positions[i].x - eye.x) > .0063f)
          continue;
        Vec3 p = tessa_anatomy::positions[i];
        p.z += .00012f;
        (p.y >= .42973f ? upper_lid : lower_lid).push_back(p);
      }
      for (auto *line : {&upper_lid, &lower_lid})
        std::sort(line->begin(), line->end(),
                  [](Vec3 a, Vec3 c) { return a.x < c.x; });
      const Vec3 rim{skin.x * .79f, skin.y * .69f, skin.z * .67f};
      if (lower_lid.size() > 1)
        b.curve(lower_lid, .00012f, rim, S::Eyes, B::Head, 6, .6f);
      if (upper_lid.size() > 1)
        b.curve(upper_lid, .00010f, rim * .76f, S::Eyes, B::Head, 6, .6f);
    }
    const int iris_columns = b.near ? 24 : 12, iris_rows = b.near ? 5 : 3;
    const auto iris_base = std::uint32_t(b.m.bind_mesh.vertices.size());
    b.grid(
        iris_columns, iris_rows,
        [&](float u, float v) {
          const float near_r[]{.00005f, .00122f, .00140f,
                               .00225f, .00310f, .00355f};
          const float far_r[]{.00005f, .00122f, .00140f, .00355f};
          int row = std::min(iris_rows, int(std::lround(v * iris_rows)));
          float r = b.near ? near_r[row] : far_r[row], a = 2 * pi * u,
                x = r * std::cos(a), y = r * std::sin(a);
          float z = radius.z * std::sqrt(std::max(0.f, 1 - sq(x / radius.x) -
                                                           sq(y / radius.y))) +
                    .00015f;
          Vec3 color{.005f, .006f, .005f};
          if (r > .00130f) {
            float fiber =
                .88f + .16f * std::sin(a * 37 + .4f * std::sin(a * 11));
            color = b.m.appearance.eyes * fiber;
            if (r > .0031f)
              color = color * .38f;
          }
          return P{eye + Vec3{x, y, z}, color, wt(B::Head),
                   character_surface_uv(S::Eyes, u, v)};
        },
        true, false);
    auto center = b.vertex({eye + Vec3{0, 0, radius.z + .00015f},
                            {.005f, .006f, .005f},
                            wt(B::Head),
                            character_surface_uv(S::Eyes, .5f, .5f)});
    for (int i = 0; i < iris_columns; ++i)
      b.tri(center, iris_base + std::uint32_t(i),
            iris_base + std::uint32_t(i + 1));
    std::vector<Vec3> brow;
    std::vector<float> brow_radius;
    const int count = b.near ? 24 : 8;
    for (int i = 0; i <= count; ++i) {
      float t = float(i) / count, x = eye.x + lerp(-.012f, .012f, t),
            outward = eye.x > 0 ? t : 1 - t,
            y = eye.y + .014f + .0027f * std::sin(pi * outward) -
                .0010f * outward;
      float z = anatomy_front(x, y);
      brow.push_back({x, y, z + .0004f});
      brow_radius.push_back(
          .00010f + .00102f * std::pow(std::max(0.f, std::sin(pi * t)), .6f));
    }
    b.tube(
        brow, brow_radius, .35f, b.m.appearance.hair * .48f, S::Hair,
        [](float) { return wt(B::Head); }, b.near ? 6 : 4);
  }
}
CharacterInfluence torso_weight(float y) {
  if (y < .1f)
    return wt(B::Pelvis, B::Spine, 1 - smooth((y - .045f) / .055f));
  if (y < .21f)
    return wt(B::Spine, B::Chest, 1 - smooth((y - .10f) / .11f));
  return wt(B::Chest);
}
float jacket_rx(float y) {
  return .091f + .024f * smooth((y - .085f) / .23f) -
         .006f * g(y, .135f, .072f);
}
float jacket_rz(float y) {
  return .058f + .016f * g(y, .235f, .095f) + .004f * g(y, .035f, .05f);
}
float coat_fold(float y, float a) {
  // Broad tension folds originate at the fastener, armhole and hip pocket.
  const float front = std::max(0.f, std::sin(a));
  return .0036f * g(y, .086f + .018f * std::cos(a * 2), .018f) * front -
         .0022f * g(y, .110f + .023f * std::cos(a * 2), .017f) * front +
         .0028f * g(y, .213f + .036f * std::fabs(std::cos(a)), .023f) * front +
         .0020f * g(y, .27f, .055f) * std::sin(a * 4 + y * 20) +
         .0014f * g(y, .07f, .035f) * std::sin(a * 3 + y * 70);
}
float jacket_z(float x, float y, bool front = true) {
  float lo = front ? 0.f : pi, hi = front ? pi : 2 * pi;
  for (int i = 0; i < 22; ++i) {
    float a = (lo + hi) * .5f,
          xx = (jacket_rx(y) + coat_fold(y, a)) * std::cos(a);
    if ((front && xx > x) || (!front && xx < x))
      lo = a;
    else
      hi = a;
  }
  float a = (lo + hi) * .5f;
  return (jacket_rz(y) + coat_fold(y, a)) * std::sin(a);
}
Vec3 collar_point(float a) {
  float y = .350f - .014f * std::max(0.f, std::sin(a));
  float r = anatomy_radius(y, a, tessa_anatomy::neck_center.z) + .0040f;
  return {r * std::cos(a), y, tessa_anatomy::neck_center.z + r * std::sin(a)};
}
// Construction-only patch membership and shared armhole loops. Nothing persists
// into animation or skinning; the model keeps the existing 17-bone DQ layout.
struct CoatConstruction {
  std::array<std::vector<std::uint32_t>, 2> armholes;
  std::vector<std::uint32_t> surface_triangles;
  std::vector<std::uint32_t> fitted_trim_triangles;
};
CharacterInfluence neck_cloth_weight(float y) {
  return wt(B::Neck, B::Chest, smooth((y - .328f) / .027f));
}
CharacterInfluence blend_cloth_weights(CharacterInfluence a,CharacterInfluence c,float t) {
  std::array<float,character_bone_count> weights{};
  auto add=[&](CharacterInfluence w,float scale) {
    weights[w.first]+=w.first_weight*scale;
    weights[w.second]+=(1-w.first_weight-w.third_weight)*scale;
    weights[w.third]+=w.third_weight*scale;
  };
  add(a,1-t);add(c,t);
  std::array<std::uint8_t,character_bone_count> bones{};
  for(unsigned i=0;i<character_bone_count;++i)bones[i]=std::uint8_t(i);
  std::stable_sort(bones.begin(),bones.end(),[&](auto a,auto c){return weights[a]>weights[c];});
  const float sum=weights[bones[0]]+weights[bones[1]]+weights[bones[2]];
  return {bones[0],bones[1],weights[bones[0]]/sum,bones[2],weights[bones[2]]/sum};
}
CharacterInfluence interpolate_cloth_weights(const Sculpt &b,
    std::uint32_t ia,std::uint32_t ib,std::uint32_t ic,float u,float v) {
  std::array<float,character_bone_count> weights{};
  for(auto pair : {std::make_pair(ia,1-u-v),std::make_pair(ib,u),std::make_pair(ic,v)}) {
    const auto &w=b.m.influences[pair.first];
    weights[w.first]+=w.first_weight*pair.second;
    weights[w.second]+=(1-w.first_weight-w.third_weight)*pair.second;
    weights[w.third]+=w.third_weight*pair.second;
  }
  std::array<std::uint8_t,character_bone_count> bones{};
  for(unsigned i=0;i<character_bone_count;++i)bones[i]=std::uint8_t(i);
  std::stable_sort(bones.begin(),bones.end(),[&](auto a,auto c){return weights[a]>weights[c];});
  const float sum=weights[bones[0]]+weights[bones[1]]+weights[bones[2]];
  return {bones[0],bones[1],weights[bones[0]]/sum,bones[2],weights[bones[2]]/sum};
}
// Exact ray/triangle correspondence, construction-time only. Patch membership
// comes from construction, independent of whatever bones the coat uses.
bool coat_ray(const Sculpt &b,const CoatConstruction &construction,Vec3 origin,
              Vec3 direction,float &distance,CharacterInfluence &weight,Vec3 *surface_normal=nullptr,bool include_trim=false) {
  bool found=false;distance=0;
  const auto count=construction.surface_triangles.size()+
                   (include_trim?construction.fitted_trim_triangles.size():0);
  for(std::size_t query=0;query<count;++query) {
    const auto face=query<construction.surface_triangles.size()?
        construction.surface_triangles[query]:
        construction.fitted_trim_triangles[query-construction.surface_triangles.size()];
    auto i=3*face,ia=b.m.bind_mesh.indices[i],ib=b.m.bind_mesh.indices[i+1],ic=b.m.bind_mesh.indices[i+2];
    Vec3 a=b.m.bind_mesh.vertices[ia].position*(1/b.h),
         bv=b.m.bind_mesh.vertices[ib].position*(1/b.h),
         c=b.m.bind_mesh.vertices[ic].position*(1/b.h);
    Vec3 e1=bv-a,e2=c-a,p=cross(direction,e2);
    float det=dot(e1,p);if(std::fabs(det)<1e-12f)continue;
    Vec3 delta=origin-a;float u=dot(delta,p)/det;if(u<0||u>1)continue;
    Vec3 q=cross(delta,e1);float v=dot(direction,q)/det;if(v<0||u+v>1)continue;
    float d=dot(e2,q)/det;
    if(d>distance){
      distance=d;weight=interpolate_cloth_weights(b,ia,ib,ic,u,v);found=true;
      if(surface_normal){
        *surface_normal=normalize(cross(normalize(e1),normalize(e2)));
        if(dot(*surface_normal,direction)<0)*surface_normal=-*surface_normal;
      }
    }
  }
  return found;
}
void make_torso(Sculpt &b, CoatConstruction &construction) {
  const Vec3 coat = b.m.appearance.shirt;
  const int nc = b.near ? 40 : 32, nr = b.near ? 26 : 9;
  const int frozen_row = b.near ? 11 : 4;
  const int hole_low = b.near ? 18 : 6, hole_high = nr - 1;
  const int hole_half = b.near ? 3 : 2;
  const auto base = std::uint32_t(b.m.bind_mesh.vertices.size());
  auto body_point = [&](int i, int j) {
    const float u = float(i) / nc, v = float(j) / nr;
    float y = lerp(.010f,.341f,v), a=2*pi*u;
    if(!b.near && j==5)y=lerp(y,.219f,std::pow(std::fabs(std::cos(a)),6.f));
    const float fit = smooth(float(j - frozen_row) / (nr - frozen_row));
    float fold = coat_fold(y, a) * (1 - smooth((v - .8f) / .2f));
    float top=.317f+.006f*sq(std::cos(a))-.002f*std::max(0.f,std::sin(a));
    float yy = y - (.341f-top) * fit;
    // Keep preserved lower rows exactly as authored, including their UVs.
    const float radial_scale = 1 + (b.near ? 0.f : .01f) * smooth((v - .65f) / .35f);
    return P{{radial_scale * (jacket_rx(y) + fold + .007f*fit - .024f*g(y,.232f,.031f)*std::pow(std::fabs(std::cos(a)),6.f)*smooth(float(j-frozen_row)/(b.near?3.f:1.f))) * std::cos(a), yy,
              radial_scale * (jacket_rz(y) + fold) * std::sin(a)},
             coat, torso_weight(yy), character_surface_uv(S::Jacket, u, v)};
  };
  for (int j = 0; j <= nr; ++j) {
    for (int i = 0; i <= nc; ++i) {
      auto p = body_point(i == nc ? 0 : i, j);
      p.uv = character_surface_uv(S::Jacket, float(i)/nc, float(j)/nr);
      b.vertex(p);
    }
    b.smooth_seams.push_back({base + j*(nc+1), base+j*(nc+1)+nc});
  }
  auto index = [&](int i, int j) {
    i = (i % nc + nc) % nc;
    return base + std::uint32_t(j*(nc+1)+i);
  };
  for (int side = 0; side < 2; ++side) {
    // Loop begins on the shoulder, then follows front/underarm/back in the
    // same orientation as the matching sleeve's circular cross-section.
    const int center = side == 0 ? nc/2 : 0;
    std::vector<std::pair<int,int>> samples;
    for(int i=0;i<=hole_half;++i) samples.push_back({center+i,hole_high});
    for(int j=hole_high-1;j>=hole_low;--j) samples.push_back({center+hole_half,j});
    for(int i=hole_half-1;i>=-hole_half;--i) samples.push_back({center+i,hole_low});
    for(int j=hole_low+1;j<=hole_high;++j) samples.push_back({center-hole_half,j});
    for(int i=-hole_half+1;i<0;++i) samples.push_back({center+i,hole_high});
    auto &loop = construction.armholes[side];
    const float sign = side == 0 ? -1.f : 1.f;
    for (std::size_t k=0;k<samples.size();++k) {
      const auto [i,j] = samples[k];
      auto id=index(i,j); loop.push_back(id);
      const float angle=2*pi*float(k)/samples.size();
      Vec3 p{sign*(.109f+.009f*std::cos(angle)),
             .276f+.038f*std::cos(angle), sign*.036f*std::sin(angle)};
      auto &v=b.m.bind_mesh.vertices[id];v.position=p*b.h;
      b.m.influences[id]=wt(side==0?B::LeftUpperArm:B::RightUpperArm,
                            B::Chest,.15f*(1-std::cos(angle))*.5f);
      if(i%nc==0) {
        const auto other=base+std::uint32_t(j*(nc+1)+nc);
        b.m.bind_mesh.vertices[other].position=v.position;
        b.m.influences[other]=b.m.influences[id];
      }
    }
  }
  auto add_coat_tri = [&](std::uint32_t a,std::uint32_t c,std::uint32_t d) {
    construction.surface_triangles.push_back(std::uint32_t(b.m.bind_mesh.indices.size()/3));
    b.tri(a,c,d);
  };
  for (int j=0;j<nr;++j) for(int i=0;i<nc;++i) {
    const int d0=std::min(i,nc-1-i),d1=std::abs(i-nc/2);
    bool in_side = i<hole_half || i>=nc-hole_half ||
                   (i>=nc/2-hole_half && i<nc/2+hole_half);
    (void)d0;(void)d1;
    if(j>=hole_low && j<hole_high && in_side) continue;
    auto a=base+std::uint32_t(j*(nc+1)+i),c=a+nc+1;
    add_coat_tri(a,c,c+1);add_coat_tri(a,c+1,a+1);
  }
  // A sloping annular shoulder continues directly into a shared neckline.
  const int shoulder_rows = b.near ? 7 : 4;
  const auto yoke = std::uint32_t(b.m.bind_mesh.vertices.size());
  b.grid(nc, shoulder_rows, [&](float u,float v) {
    int i=int(std::lround(u*nc));
    const Vec3 low=b.m.bind_mesh.vertices[base+nr*(nc+1)+i].position*(1/b.h);
    const float a=2*pi*u;
    const Vec3 high=collar_point(a);
    const float bend=v*v*v*(10+v*(-15+6*v));
    Vec3 p=low*(1-bend)+high*bend;
    p.y=lerp(low.y,high.y,v);
    if(v>0 && v<1) {
      Vec3 radial{p.x,0,p.z-tessa_anatomy::neck_center.z};
      float r=std::sqrt(dot(radial,radial));
      const float angle=std::atan2(radial.z,radial.x);
      float required=anatomy_radius(p.y,angle,tessa_anatomy::neck_center.z)+.007f;
      if(r>1e-6f && required>r)p+=radial*((required-r)/r);
    }
    return P{p,coat,neck_cloth_weight(p.y),character_surface_uv(S::Jacket,u,.8f+.2f*v)};
  },true,false);
  const auto yoke_tri_begin=std::uint32_t(b.m.bind_mesh.indices.size()/3)-2*nc*shoulder_rows;
  for(auto ti=yoke_tri_begin;ti<b.m.bind_mesh.indices.size()/3;++ti) construction.surface_triangles.push_back(ti);
  for(int i=0;i<=nc;++i) {
    auto a=base+std::uint32_t(nr*(nc+1)+i),c=yoke+std::uint32_t(i);
    b.m.bind_mesh.vertices[c].position=b.m.bind_mesh.vertices[a].position;
    b.m.influences[c]=b.m.influences[a];b.normal_groups.push_back({a,c});
  }
  const auto collar=std::uint32_t(b.m.bind_mesh.vertices.size());
  b.grid(nc,b.near?4:1,[&](float u,float v) {
    const float a=2*pi*u;Vec3 p=collar_point(a);
    if(v>0) {
      p.y+=.010f*v;
      float r=anatomy_radius(p.y,a,tessa_anatomy::neck_center.z)+.004f+.0012f*std::sin(pi*v)+.001f*smooth(v);
      p.x=r*std::cos(a);p.z=tessa_anatomy::neck_center.z+r*std::sin(a);
    }
    return P{p,coat*(.91f+.04f*v),neck_cloth_weight(p.y),character_surface_uv(S::Jacket,u,v)};
  },true,false);
  for(int i=0;i<=nc;++i) {
    auto a=yoke+std::uint32_t(shoulder_rows*(nc+1)+i),c=collar+std::uint32_t(i);
    b.m.bind_mesh.vertices[c].position=b.m.bind_mesh.vertices[a].position;
    b.m.influences[c]=b.m.influences[a];b.normal_groups.push_back({a,c});
  }
  auto fitted_detail = [&](P p,bool front,float offset) {
    const float boundary=b.near?.150038f:.157111f;
    if(p.p.y<=boundary)return p;
    Vec3 origin{p.p.x,p.p.y,0},direction{0,0,front?1.f:-1.f};
    float distance;CharacterInfluence weight;
    if(coat_ray(b,construction,origin,direction,distance,weight)) {
      p.p=origin+direction*(distance+offset);p.w=weight;
    }
    return p;
  };
  // Overlapping cloth placket, with recessed edge and four restrained buttons.
  const auto placket_face_begin=std::uint32_t(b.m.bind_mesh.indices.size()/3);
  b.grid(
      b.near ? 8 : 4, b.near ? 28 : 12,
      [&](float u, float v) {
        float y = lerp(.014f, .327f, v), x = lerp(.011f, -.013f, u);
        return fitted_detail(P{{x,y,jacket_z(x,y)+.0014f+.0011f*std::sin(pi*u)},
                  coat*.97f,torso_weight(y),character_surface_uv(S::Jacket,u,v)},
                  true,.0014f+.0011f*std::sin(pi*u));
      },
      false, false);
  // Explicit actual placket membership, discarded after construction. The two
  // lower buttons retain every original attribute and influence bit-for-bit.
  CoatConstruction button_support;
  for(auto f=placket_face_begin;f<b.m.bind_mesh.indices.size()/3;++f)
    button_support.surface_triangles.push_back(f);
  for (float y : {.068f, .141f, .216f, .291f}) {
    const auto button_begin=std::uint32_t(b.m.bind_mesh.vertices.size());
    const auto button_face_begin=std::uint32_t(b.m.bind_mesh.indices.size()/3);
    b.ellipsoid({-.001f, y, jacket_z(-.001f, y) + .0031f},
                {.0037f, .0037f, .0012f}, coat * .34f, S::Rubber,
                y < .11f    ? B::Pelvis
                : y < .235f ? B::Spine
                            : B::Chest,
                b.near ? 16 : 8, b.near ? 6 : 4);
    if(y<.2f)continue;
    Vec3 origin{-.001f,y,0},normal;float distance;CharacterInfluence weight;
    if(!coat_ray(b,button_support,origin,{0,0,1},distance,weight,&normal))
      throw std::runtime_error("Tessa upper button has no placket support");
    const Vec3 old_center{-.001f,y,jacket_z(-.001f,y)+.0031f};
    const Vec3 tangent_x=normalize(cross(Vec3{0,1,0},normal));
    const Vec3 tangent_y=cross(normal,tangent_x);
    const Vec3 center=origin+Vec3{0,0,distance};
    Vec3 low{1,1,1},high{-1,-1,-1};
    for(auto i=button_begin;i<b.m.bind_mesh.vertices.size();++i) {
      auto &vertex=b.m.bind_mesh.vertices[i];
      const Vec3 q=vertex.position*(1/b.h)-old_center;
      const Vec3 p=center+tangent_x*q.x+tangent_y*q.y+normal*q.z;
      vertex.position=p*b.h;
      low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);
      high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);
    }
    // Exact complete-surface +Z separation: clip each projected button facet
    // against each actual placket facet. Their relative height is affine, so
    // its maximum occurs at a clipped-polygon corner, including edge/edge
    // intersections that vertex samples miss. No supporting plane is extended
    // outside its real triangle. The projected footprint stays fixed on lift.
    // The 0.00006-height mounting allowance is 0.099 mm at 1.65 m.
    struct XY {double x,y;};
    auto cross2=[](XY a,XY c,XY d) {
      return (c.x-a.x)*(d.y-a.y)-(c.y-a.y)*(d.x-a.x);
    };
    auto triangle=[&](std::uint32_t face) {
      std::array<Vec3,3> t;
      for(int k=0;k<3;++k)t[k]=b.m.bind_mesh.vertices[
          b.m.bind_mesh.indices[3*face+k]].position*(1/b.h);
      return t;
    };
    auto xy=[](Vec3 v){return XY{v.x,v.y};};
    auto height_at=[&](const std::array<Vec3,3>&t,XY p,double area) {
      const double u=cross2(xy(t[0]),p,xy(t[2]))/area;
      const double v=cross2(xy(t[0]),xy(t[1]),p)/area;
      return double(t[0].z)+u*(double(t[1].z)-t[0].z)+v*(double(t[2].z)-t[0].z);
    };
    double required_lift=0;
    for(auto f:button_support.surface_triangles) {
      const auto cloth=triangle(f);
      const double ca=cross2(xy(cloth[0]),xy(cloth[1]),xy(cloth[2]));
      if(std::fabs(ca)<1e-15)continue;
      const double side=ca>0?1:-1;
      if(std::max({cloth[0].x,cloth[1].x,cloth[2].x})<low.x ||
         std::min({cloth[0].x,cloth[1].x,cloth[2].x})>high.x ||
         std::max({cloth[0].y,cloth[1].y,cloth[2].y})<low.y ||
         std::min({cloth[0].y,cloth[1].y,cloth[2].y})>high.y)continue;
      for(auto g=button_face_begin;g<b.m.bind_mesh.indices.size()/3;++g) {
        const auto button=triangle(g);
        const double ba=cross2(xy(button[0]),xy(button[1]),xy(button[2]));
        if(std::fabs(ba)<1e-15)continue;
        std::array<XY,8> polygon{},next{};
        int count=3;for(int k=0;k<3;++k)polygon[k]=xy(button[k]);
        for(int edge=0;edge<3 && count;++edge) {
          int n=0;
          for(int k=0;k<count;++k) {
            const XY a=polygon[k],c=polygon[(k+1)%count];
            const double da=side*cross2(xy(cloth[edge]),xy(cloth[(edge+1)%3]),a),
                         dc=side*cross2(xy(cloth[edge]),xy(cloth[(edge+1)%3]),c);
            if(da>=0)next[n++]=a;
            if((da>0 && dc<0)||(da<0 && dc>0)) {
              const double t=da/(da-dc);
              next[n++]={a.x+t*(c.x-a.x),a.y+t*(c.y-a.y)};
            }
          }
          polygon=next;count=n;
        }
        for(int k=0;k<count;++k)
          required_lift=std::max(required_lift,
              height_at(cloth,polygon[k],ca)-height_at(button,polygon[k],ba)+.00006);
      }
    }
    const float lift=float(required_lift);
    for(auto i=button_begin;i<b.m.bind_mesh.vertices.size();++i) {
      auto &vertex=b.m.bind_mesh.vertices[i];
      vertex.position.z+=lift*b.h;
      const Vec3 p=vertex.position*(1/b.h);
      if(!coat_ray(b,button_support,{p.x,p.y,0},{0,0,1},distance,weight))
        throw std::runtime_error("Tessa upper button footprint leaves placket");
      b.m.influences[i]=weight;
    }
  }
  // Slanted welt pockets have a shallow actual opening and folded cloth lip.
  for (float side : {-1.f, 1.f}) {
    std::vector<Vec3> pocket;
    int steps = b.near ? 20 : 8;
    for (int i = 0; i <= steps; ++i) {
      float t = float(i) / steps, x = side * lerp(.026f, .081f, t),
            y = lerp(.109f, .142f, t);
      pocket.push_back({x, y, jacket_z(x, y) + .0023f});
    }
    b.tube(
        pocket, std::vector<float>(pocket.size(), .0020f), .48f, coat * .75f,
        S::Jacket, [](float t) { return torso_weight(lerp(.109f, .142f, t)); },
        b.near ? 8 : 4);
    for (auto &p : pocket) {
      p.y += .0018f;
      p.z += .0010f;
    }
    b.tube(
        pocket, std::vector<float>(pocket.size(), .0017f), .65f, coat * 1.02f,
        S::Jacket, [](float t) { return torso_weight(lerp(.109f, .142f, t)); },
        b.near ? 8 : 4);
  }
  b.grid(
      b.near ? 40 : 20, 3,
      [&](float u, float v) {
        float a = 2 * pi * u, y = .010f + .005f * v;
        float r = coat_fold(.010f, a), fold = .0012f * std::sin(pi * v);
        return P{{(jacket_rx(.010f) + r - fold) * std::cos(a), y,
                  (jacket_rz(.010f) + r - fold) * std::sin(a)},
                 coat * .95f,
                 torso_weight(y),
                 character_surface_uv(S::Jacket, u, v)};
      },
      true, false);
  // Back yoke and two long shaped panel seams remain visible on the reverse.
  for (float side : {-1.f, 1.f}) {
    std::vector<Vec3> seam;
    const int steps = b.near ? 20 : 8;
    for (int i = 0; i <= steps; ++i) {
      float t = float(i) / steps, y = lerp(.022f, .298f, t),
            x = side * (.048f + .008f * std::sin(pi * t));
      seam.push_back({x, y, jacket_z(x, y, false) - .0009f});
    }
    const auto seam_base=std::uint32_t(b.m.bind_mesh.vertices.size());
    const auto seam_face=std::uint32_t(b.m.bind_mesh.indices.size()/3);
    b.tube(
        seam, std::vector<float>(seam.size(), .00075f), .35f, coat * .90f,
        S::Jacket, [](float t) { return torso_weight(lerp(.022f, .298f, t)); },
        4);
    for(auto i=seam_base;i<b.m.bind_mesh.vertices.size();++i) {
      auto &vertex=b.m.bind_mesh.vertices[i];
      P p{vertex.position*(1/b.h),vertex.color,b.m.influences[i],vertex.uv};
      // Refit only above the preserved body boundary. Tube relief stays
      // outside the shell, rather than retaining the old detached back path.
      if(p.p.y>(b.near?.150038f:.157111f)) {
        const float original=jacket_z(p.p.x,p.p.y,false);
        const float relief=.001f+std::max(.00015f,original-p.p.z);
        auto q=fitted_detail(p,false,relief);
        vertex.position=q.p*b.h;b.m.influences[i]=q.w;
      }
    }
    for(auto f=seam_face;f<b.m.bind_mesh.indices.size()/3;++f)
      construction.fitted_trim_triangles.push_back(f);
  }

}
void make_limbs(Sculpt &b, const CoatConstruction &construction) {
  auto skin = b.m.appearance.skin;
  Vec3 jacket = b.m.appearance.shirt, pants = b.m.appearance.trousers;
  // A single trouser shell: two leg openings sewn along one crotch saddle.
  const int trouser_columns = b.near ? 24 : 12;
  std::array<std::vector<std::uint32_t>, 2> trouser_top;
  for (float sign : {-1.f, 1.f}) {
    bool right = sign > 0;
    B upper = right ? B::RightUpperArm : B::LeftUpperArm,
      fore = right ? B::RightForearm : B::LeftForearm,
      hand = right ? B::RightHand : B::LeftHand;
    float shoulder = sign * .116f, elbow = sign * .135f, wrist = sign * .155f;
    const int sleeve_nc=b.near?20:10, sleeve_nr=b.near?22:8;
    const int keep=b.near?13:5;
    auto sleeve_point = [&](float u,float v) {
      float y = lerp(-.042f, .313f, v), angle = 2 * pi * u;
      float x = y < .12f ? lerp(wrist, elbow, smooth((y + .042f) / .162f))
                         : lerp(elbow, shoulder, smooth((y - .12f) / .16f));
      x = lerp(x, sign * .094f, smooth((y - .285f) / .028f));
      float radius =
          .018f + .011f * g(y, .025f, .07f) + .013f * g(y, .24f, .09f);
      radius *= 1 - .25f * smooth((y - .285f) / .028f);
      radius += .0015f * g(y, .125f, .035f) * std::sin(y * 210 + angle * 2.f) +
                .0011f * g(y, -.028f, .018f) * std::sin(y * 370 + angle);
      auto weight = y < .10f   ? wt(fore)
                    : y < .15f ? wt(fore, upper, 1 - smooth((y - .10f) / .05f))
                               : wt(upper, B::Chest, smooth((.32f - y) / .08f));
      return P{
          {x + radius * std::cos(angle), y, radius * .91f * std::sin(angle)},
          jacket,
          weight,
          character_surface_uv(S::Jacket, u, v)};
    };
    const auto sleeve_base=std::uint32_t(b.m.bind_mesh.vertices.size());
    // Preserve every original lower sleeve row, including its bottom cap.
    b.grid(sleeve_nc,keep,[&](float u,float v) {
      const int row=int(std::lround(v*keep));
      return sleeve_point(u,float(row)/sleeve_nr);
    },true,false);
    {
      P center=sleeve_point(0,0);center.p={};
      for(int i=0;i<sleeve_nc;++i)center.p+=sleeve_point(float(i)/sleeve_nc,0).p;
      center.p=center.p*(1.f/sleeve_nc);auto c=b.vertex(center);
      auto ring=std::uint32_t(b.m.bind_mesh.vertices.size());
      for(int i=0;i<=sleeve_nc;++i) {
        P p=sleeve_point(float(i)/sleeve_nc,0);
        if(i==sleeve_nc){P first=sleeve_point(0,0);p.p=first.p;p.c=first.c;p.w=first.w;}
        b.vertex(p);
      }
      for(int i=0;i<sleeve_nc;++i)b.tri(c,ring+i,ring+i+1);
    }
    const auto &hole=construction.armholes[right?1:0];
    const int hn=int(hole.size()),sleeve_rows=b.near?5:2;
    // The circular lower sleeve maps into the sewn armhole, without a cap.
    // Ring k=0 corresponds to outward shoulder; pi is the underarm.
    auto sample_hole=[&](float u) {
      float f=u*hn;int k=int(std::floor(f))%hn;float t=f-std::floor(f);
      const auto i=hole[k],j=hole[(k+1)%hn];
      auto p=b.m.bind_mesh.vertices[i].position*(1/b.h)*(1-t)+
             b.m.bind_mesh.vertices[j].position*(1/b.h)*t;
      const float aw=b.m.influences[i].first_weight*(1-t)+b.m.influences[j].first_weight*t;
      return P{p,jacket,wt(upper,B::Chest,aw),character_surface_uv(S::Jacket,u,1)};
    };
    std::vector<std::uint32_t> previous;
    for(int i=0;i<sleeve_nc;++i) previous.push_back(sleeve_base+keep*(sleeve_nc+1)+i);
    for(int row=1;row<=sleeve_rows;++row) {
      float v=float(row)/sleeve_rows;
      const int count=row==sleeve_rows?hn:sleeve_nc;
      std::vector<std::uint32_t> ring;
      for(int i=0;i<count;++i) {
        const float u=float(i)/count;
        // Left sleeve ring starts at its outward (-X) point.
        const float lower_u=right?u:std::fmod(u+.5f,1.f);
        P low=sleeve_point(lower_u,float(keep)/sleeve_nr),high=sample_hole(u);
        P p{low.p*(1-v)+high.p*v,jacket,
            wt(upper,B::Chest,lerp(low.w.first_weight,high.w.first_weight,smooth(v))),
            character_surface_uv(S::Jacket,lower_u,lerp(float(keep)/sleeve_nr,1.f,v))};
        ring.push_back(b.vertex(p));
        if(row==sleeve_rows) {
          b.m.bind_mesh.vertices[ring.back()].position=b.m.bind_mesh.vertices[hole[i]].position;
          b.m.influences[ring.back()]=b.m.influences[hole[i]];
          const unsigned body_nc=b.near?40:32;
          if(hole[i]%(body_nc+1)==0) {
            const auto wrap=hole[i]+body_nc;
            auto &seams=b.smooth_seams;
            seams.erase(std::remove_if(seams.begin(),seams.end(),[&](auto p){
              return p.first==hole[i]&&p.second==wrap;
            }),seams.end());
            b.normal_groups.push_back({ring.back(),hole[i],wrap});
          } else b.normal_groups.push_back({ring.back(),hole[i]});
        }
      }
      // Zipper triangulation also joins differing Far/armhole ring counts.
      if(row==1 && !right) std::rotate(previous.begin(),previous.begin()+sleeve_nc/2,previous.end());
      int i=0,j=0,na=int(previous.size()),nb=int(ring.size());
      while(i<na || j<nb) {
        float a=float(i+1)/na,c=float(j+1)/nb;
        if(i<na && (j==nb || a<c-1e-6f)) {
          b.tri(previous[i%na],ring[j%nb],previous[(i+1)%na]);++i;
        } else if(j<nb && (i==na || c<a-1e-6f)) {
          b.tri(previous[i%na],ring[j%nb],ring[(j+1)%nb]);++j;
        } else {
          b.tri(previous[i%na],ring[j%nb],ring[(j+1)%nb]);
          b.tri(previous[i%na],ring[(j+1)%nb],previous[(i+1)%na]);++i;++j;
        }
      }
      previous=std::move(ring);
    }
    // A turned cuff overlaps the sleeve with a visible folded edge and button.
    b.grid(b.near ? 24 : 10, b.near ? 6 : 3, [&](float u, float v) {
      float y = lerp(-.044f, -.015f, v), a = 2 * pi * u;
      float x = lerp(wrist, elbow, smooth((y + .042f) / .162f));
      float radius =
          .018f + .011f * g(y, .025f, .07f) + .013f * g(y, .24f, .09f);
      radius += .0012f + .0013f * std::sin(pi * v);
      return P{{x + radius * std::cos(a), y, radius * .91f * std::sin(a)},
               jacket * .94f,
               wt(fore),
               character_surface_uv(S::Jacket, u, v)};
    });
    b.ellipsoid({wrist + sign * .025f, -.027f, .004f}, {.0013f, .003f, .003f},
                jacket * .44f, S::Rubber, fore, b.near ? 12 : 8,
                b.near ? 6 : 4);
    // A modest relaxed hand below the preserved coat cuff.
    const auto hand_begin = b.m.bind_mesh.vertices.size();
    b.grid(b.near ? 20 : 8, b.near ? 8 : 3, [&](float u, float v) {
      float angle = 2 * pi * u, lateral = sign * std::cos(angle),
            arc = .0065f * lateral * lateral + .0030f * lateral,
            y = lerp(-.108f, -.036f, v) + arc * (1 - smooth(v / .5f)),
            rx = lerp(.019f, .015f, smooth((v - .5f) / .5f)) +
                 .0015f * (1 - smooth(v / .5f)),
            rz = .0105f + .002f * std::sin(pi * v);
      return P{{wrist + rx * std::cos(angle), y, rz * std::sin(angle) + .0015f -
                    .0020f * (1 - v) * (1 - v) * lateral * lateral *
                    (1 - smooth(v / .5f))},
               skin,
               wt(hand),
               character_surface_uv(S::Skin, u, v)};
    });
    const float offsets[4] = {-.014f, -.0045f, .005f, .014f},
                lengths[4] = {.043f, .050f, .046f, .035f},
                roots[4] = {-.098f, -.099f, -.098f, -.096f},
                curl[4] = {.0045f, .007f, .009f, .011f};
    for (int finger = 0; finger < 4; ++finger) {
      std::vector<Vec3> path;
      std::vector<float> radii;
      int n = b.near ? 6 : 2;
      for (int i = 0; i <= n; ++i) {
        float t = float(i) / n;
        float x = wrist + sign * offsets[finger] * (1 + .14f * t),
              y = roots[finger] - lengths[finger] * t,
              z = .0015f - .0014f * sq(offsets[finger] / .019f) -
                    curl[finger] * t * t -
                    (!b.near && finger == 0 ? .0002f * (1 - t) * (1 - t) : 0.f);
        path.push_back({x, y, z});
        radii.push_back((.0045f - .0014f * t) *
                        (1 + .08f * std::sin(t * 4 * pi)));
      }
      b.tube(
          path, radii, .88f, skin, S::Skin, [&](float) { return wt(hand); },
          b.near ? 8 : 4);
      if (b.near) {
        // Align each existing nail shell to its distal finger surface.
        const float t = 1.f - .006f / lengths[finger];
        const Vec3 tangent = normalize(Vec3{
            sign * offsets[finger] * .14f, -lengths[finger],
            -2.f * curl[finger] * t});
        const Vec3 across = normalize(cross(Vec3{0, 0, 1}, tangent));
        const Vec3 dorsal = normalize(cross(tangent, across));
        const Vec3 p = Vec3{wrist + sign * offsets[finger] * (1 + .14f * t),
                            roots[finger] - lengths[finger] * t,
                            .0015f - .0014f * sq(offsets[finger] / .019f) -
                    curl[finger] * t * t} + dorsal * .0029f;
        b.grid(8, 4, [&](float u, float v) {
          const float a = 2 * pi * u, e = lerp(-pi * .49f, pi * .49f, v);
          return P{p + across * (.0030f * std::cos(a) * std::cos(e)) -
                           tangent * (.0055f * std::sin(e)) +
                           dorsal * (.00055f * std::sin(a) * std::cos(e)),
                   skin * .90f + Vec3{.05f, .03f, .03f}, wt(hand),
                   character_surface_uv(S::Skin, u, v)};
        });
      }
    }
    std::vector<Vec3> thumb{{wrist - sign * .0105f, -.064f, -.001f},
                            {wrist - sign * .022f, -.075f, -.007f},
                            {wrist - sign * .025f, -.089f, -.011f},
                            {wrist - sign * .024f, -.101f, -.012f}};
    b.tube(
        thumb, {.007f, .006f, .0051f, .0039f}, .9f, skin, S::Skin,
        [&](float) { return wt(hand); }, b.near ? 8 : 4);
    // A modest palm-inward rest turn, below the untouched wrist junction.
    // Only construction-time geometry changes; the existing hand bone is kept.
    for (auto i = hand_begin; i < b.m.bind_mesh.vertices.size(); ++i) {
      auto &p = b.m.bind_mesh.vertices[i].position;
      const float turn = sign * (pi * 32.f / 180.f) *
                         smooth((-.065f - p.y / b.h) / .035f);
      const float x = p.x - wrist * b.h, z = p.z;
      p.x = wrist * b.h + std::cos(turn) * x + std::sin(turn) * z;
      p.z = -std::sin(turn) * x + std::cos(turn) * z;
    }
    B thigh = right ? B::RightThigh : B::LeftThigh,
      shin = right ? B::RightShin : B::LeftShin,
      foot = right ? B::RightFoot : B::LeftFoot;
    float x = sign * .060f;
    // Retain leg topology cost while placing rings at the anatomical knee.
    const float near_y[]{-.459f, -.443f, -.425f, -.400f, -.375f, -.350f, -.325f,
                         -.300f, -.285f, -.275f, -.265f, -.255f, -.245f, -.235f,
                         -.225f, -.215f, -.205f, -.195f, -.185f, -.175f, -.160f,
                         -.140f, -.115f, -.090f, -.060f, -.030f, .007f};
    const float far_y[]{-.459f, -.420f, -.370f, -.320f, -.275f, -.250f, -.225f,
                        -.200f, -.175f, -.130f, -.080f, -.035f, .007f};
    const int lower_rows = b.near ? 22 : 9;
    const int upper_rows = b.near ? 10 : 5;
    const int rows = lower_rows + upper_rows;
    const float join_y = b.near ? -.115f : -.130f;
    auto lower_point = [&](float u, float y) {
      float angle = 2 * pi * u;
      float radius = .027f + .009f * g(y, -.34f, .08f) +
                     .018f * smooth((y + .225f) / .22f);
      radius += .0017f * g(y, -.224f, .027f) * std::sin(y * 220 + angle) +
                .0013f * g(y, -.443f, .023f) * std::sin(y * 310 + angle * 2.f);
      // The anterior knee bends locally around the patella. Posterior
      // fabric distributes the compression without hollowing the bind calf.
      const float knee_span = .100f + .120f * std::max(0.f, -std::sin(angle));
      const float knee_low = -.225f - .5f * knee_span;
      const float knee_high = -.225f + .5f * knee_span;
      auto weight =
          y < knee_low ? wt(shin)
          : y < knee_high
              ? wt(shin, thigh, 1 - smooth((y - knee_low) / knee_span))
              : wt(thigh, B::Pelvis, smooth((.045f - y) / .10f));
      return P{
          {x + radius * std::cos(angle), y,
           radius * 1.10f * std::sin(angle) - .001f +
               .013f * g(y, -.225f, .20f) * std::max(0.f, -std::sin(angle))},
          pants,
          weight,
          character_surface_uv(S::Pants, u, (y + .459f) / .466f)};
    };
    const auto base = std::uint32_t(b.m.bind_mesh.vertices.size());
    auto point = [&](int i, int j) {
      const float u = float(i) / trouser_columns;
      if (j <= lower_rows)
        return lower_point(u, b.near ? near_y[j] : far_y[j]);
      const float t = float(j - lower_rows) / upper_rows;
      const int canonical = right ? i % trouser_columns
          : (trouser_columns / 2 - i + trouser_columns) % trouser_columns;
      const float a = 2 * pi * float(canonical) / trouser_columns;
      float c = std::cos(a), sn = std::sin(a);
      if (std::fabs(c) < 1e-6f) c = 0;
      if (std::fabs(sn) < 1e-6f) sn = 0;
      const float inward = std::max(0.f, -c);
      const Vec3 top{sign * .085f * std::max(0.f, c),
                     .055f - .127f * inward,
                     .053f * sn - .004f};
      P p = lower_point(u, join_y);
      p.p = p.p * (1 - t) + top * t;
      // Anchor the waist before it enters the coat hem. The trouser rise
      // still bends below it, while the common saddle uses identical weights.
      const float thigh_weight = std::min(1 - smooth(t),
          1 - smooth((p.p.y - join_y) / (-.005f - join_y)));
      p.w = p.p.y >= -.005f ? torso_weight(p.p.y)
                            : wt(thigh, B::Pelvis, thigh_weight);
      p.uv = character_surface_uv(S::Pants, u, (p.p.y + .459f) / .527f);
      if (j == rows) { p.p = top; p.w = torso_weight(top.y); }
      return p;
    };
    for (int j = 0; j <= rows; ++j) {
      for (int i = 0; i <= trouser_columns; ++i) {
        P p = point(i, j);
        if (i == trouser_columns) {
          const P first = point(0, j);
          p.p = first.p; p.c = first.c; p.w = first.w;
        }
        b.vertex(p);
      }
      if (j != rows || right)
        b.smooth_seams.push_back({base + j * (trouser_columns + 1),
                               base + j * (trouser_columns + 1) + trouser_columns});
    }
    for (int j = 0; j < rows; ++j)
      for (int i = 0; i < trouser_columns; ++i) {
        const auto a = base + j * (trouser_columns + 1) + i;
        const auto next = a + trouser_columns + 1;
        b.tri(a, next, next + 1); b.tri(a, next + 1, a + 1);
      }
    // Preserve the original lower cap; there is no independent upper-leg cap.
    P center = lower_point(0, -.459f); center.p = {};
    for (int i = 0; i < trouser_columns; ++i)
      center.p += lower_point(float(i) / trouser_columns, -.459f).p;
    center.p = center.p * (1.f / trouser_columns);
    const auto cap_center = b.vertex(center);
    const auto cap_ring = std::uint32_t(b.m.bind_mesh.vertices.size());
    for (int i = 0; i <= trouser_columns; ++i) {
      P p = lower_point(float(i) / trouser_columns, -.459f);
      if (i == trouser_columns) p.p = lower_point(0, -.459f).p;
      b.vertex(p);
    }
    for (int i = 0; i < trouser_columns; ++i)
      b.tri(cap_center, cap_ring + i, cap_ring + i + 1);
    auto &top = trouser_top[right ? 1 : 0];
    for (int i = 0; i <= trouser_columns; ++i)
      top.push_back(base + rows * (trouser_columns + 1) + i);
    // Sole and toe box share the animator's exact supported heel/toe envelope.
    b.grid(b.near ? 28 : 14, 3, [&](float u, float v) {
      float a = 2 * pi * u, y = lerp(-.5f, -.483f, v);
      return P{{x + .032f * std::cos(a), y, .033f + .067f * std::sin(a)},
               {.07f, .072f, .068f},
               wt(foot),
               character_surface_uv(S::Rubber, u, v)};
    });
    b.grid(b.near ? 32 : 12, b.near ? 10 : 4, [&](float u, float v) {
      float a = 2 * pi * u, y = lerp(-.483f, -.430f, v),
            rx = lerp(.0315f, .022f, smooth(v)),
            rz = lerp(.066f, .027f, smooth(v)),
            cz = lerp(.033f, 0.f, smooth(v));
      return P{{x + rx * std::cos(a), y, cz + rz * std::sin(a)},
               b.m.appearance.shoes,
               wt(foot),
               character_surface_uv(S::Leather, u, v)};
    });
    for (int lace = 0; lace < (b.near ? 5 : 3); ++lace) {
      float z = .035f + lace * .006f, y = -.445f - lace * .004f;
      std::vector<Vec3> p{
          {x - .012f, y, z}, {x, y + .0018f, z + .002f}, {x + .012f, y, z}};
      b.curve(p, .00085f, {.39f, .37f, .30f}, S::Shirt, foot, b.near ? 6 : 4);
    }
  }
  // Exact shared crotch copies. One group also absorbs the UV seam copies.
  for (int i = trouser_columns / 4; i <= 3 * trouser_columns / 4; ++i) {
    const int other = (trouser_columns / 2 - i + trouser_columns) % trouser_columns;
    std::vector<std::uint32_t> group{trouser_top[1][i], trouser_top[0][other]};
    if (other == 0) group.push_back(trouser_top[0][trouser_columns]);
    b.normal_groups.push_back(std::move(group));
  }
  // One waist cap sits inside the existing jacket. Its perimeter is the two
  // half-waists, not three overlapping independently capped components.
  const auto center = b.vertex({{0, .055f, -.004f}, pants, torso_weight(.055f),
                               character_surface_uv(S::Pants, .5f, 1.f)});
  std::vector<std::uint32_t> waist;
  for (int i = 0; i < trouser_columns; ++i) {
    const int half = (i > trouser_columns / 4 && i < 3 * trouser_columns / 4) ? 0 : 1;
    const auto source = trouser_top[half][i];
    const Vertex v = b.m.bind_mesh.vertices[source];
    const auto copy = b.vertex({{}, pants, b.m.influences[source], v.uv});
    // Copy physical coordinates directly; divide/multiply by height can
    // otherwise leave microscopic open edges at non-unit scales.
    b.m.bind_mesh.vertices[copy].position = v.position;
    waist.push_back(copy);
  }
  for (int i = 0; i < trouser_columns; ++i)
    b.tri(center, waist[(i + 1) % trouser_columns], waist[i]);

}
void closed_trim(Sculpt &b, const std::vector<Vec3> &path, float radius,
                 Vec3 color, S surface, B bone, int sides) {
  const int steps = int(path.size()) - 1;
  const auto base = std::uint32_t(b.m.bind_mesh.vertices.size());
  b.grid(
      sides, steps,
      [&](float u, float v) {
        const int i = int(std::lround(v * steps)) % steps;
        const Vec3 tangent =
            normalize(path[(i + 1) % steps] - path[(i + steps - 1) % steps]);
        const Vec3 normal = normalize(cross(Vec3{0, 0, 1}, tangent));
        const float a = 2 * pi * u;
        return P{path[i] + Vec3{0, 0, radius * .6f * std::cos(a)} +
                     normal * (radius * std::sin(a)),
                 color, wt(bone), character_surface_uv(surface, u, v)};
      },
      true, false);
  for (int i = 0; i <= sides; ++i)
    b.normal_groups.push_back({base + std::uint32_t(i),
                               base + std::uint32_t(steps * (sides + 1) + i)});
}
void make_bag(Sculpt &b, const CoatConstruction &construction) {
  const Vec3 leather{.22f, .145f, .085f};
  // A compact front-slung commuter satchel clears the unchanged limb envelope.
  // Its lower edge stays above the hip crease; this is a fitted wardrobe
  // choice, not collision avoidance hidden inside the animation/controller.
  constexpr float cx = .055f, cy = .195f, cz = .105f;
  b.grid(b.near ? 32 : 16, b.near ? 12 : 6, [&](float u, float v) {
    float a = u == 1.f ? 0.f : 2 * pi * u,
          round = .88f + .12f * std::sin(pi * v);
    float x = cx + .040f * round *
                       std::copysign(std::pow(std::fabs(std::cos(a)), .38f),
                                     std::cos(a));
    float z = cz + .024f * round *
                       std::copysign(std::pow(std::fabs(std::sin(a)), .38f),
                                     std::sin(a));
    return P{{x, lerp(cy - .050f, cy + .050f, v), z},
             leather,
             wt(B::Spine),
             character_surface_uv(S::Leather, u, v)};
  });
  // A separately rolled flap has visible surface thickness at both sides.
  auto flap = [&](float u, float v) {
    float half = .038f - .005f * smooth((v - .78f) / .22f);
    return Vec3{cx + half * (1 - 2 * u),
                .245f - .061f * smooth((v - .22f) / .78f) +
                    .004f * std::sin(pi * std::min(v / .4f, 1.f)),
                .084f + .0465f * smooth(v / .42f)};
  };
  for (bool underside : {false, true})
    b.grid(
        b.near ? 20 : 8, b.near ? 14 : 6,
        [&](float u, float v) {
          const float across = underside ? u : 1 - u;
          Vec3 q = flap(across, v);
          if (underside) {
            const float step = .001f;
            const Vec3 du = flap(std::min(1.f, across + step), v) -
                            flap(std::max(0.f, across - step), v);
            const Vec3 dv = flap(across, std::min(1.f, v + step)) -
                            flap(across, std::max(0.f, v - step));
            // Preserve thickness around the roll; a fixed Z shift leaves the
            // nearly horizontal top sheets almost coplanar. Normalize the
            // tangents first so the small derivative cross does not underflow
            // the normalization tolerance.
            q = q - normalize(cross(normalize(du), normalize(dv))) * .0011f;
          }
          return P{q, leather * (underside ? .70f : 1.02f), wt(B::Spine),
                   character_surface_uv(S::Leather, u, v)};
        },
        false, false);
  for (float edge : {0.f, 1.f}) {
    std::vector<Vec3> path;
    const int n = b.near ? 20 : 10;
    for (int i = 0; i <= n; ++i)
      path.push_back(flap(edge, float(i) / n));
    b.curve(path, .0006f, leather * .72f, S::Leather, B::Spine, b.near ? 6 : 4);
  }
  std::vector<Vec3> bottom;
  for (int i = 0; i <= 16; ++i)
    bottom.push_back(flap(float(i) / 16, 1.f));
  b.curve(bottom, .0006f, leather * .72f, S::Leather, B::Spine, b.near ? 6 : 4);
  b.ellipsoid({cx, .189f, .132f}, {.0044f, .0035f, .0013f}, {.42f, .35f, .20f},
              S::Metal, B::Spine, 10, 5);
  // Continuous shoulder loop: fitted over the left shoulder and around the
  // right torso, with actual upper side tabs on the satchel.
  const std::vector<Vec3> guides{
      {.093f, .248f, .108f},   {.092f, .170f, .038f},  {.088f, .170f, -.039f},
      {.070f, .261f, -.066f},  {.015f, .295f, -.061f}, {-.064f, .326f, -.040f},
      {-.078f, .335f, -.019f}, {-.081f, .338f, .007f}, {-.078f, .333f, .033f},
      {-.055f, .318f, .058f},  {-.012f, .292f, .079f}, {.017f, .248f, .108f}};
  std::vector<Vec3> strap;
  const int subdivisions = b.near ? 5 : 3;
  // Catmull-Rom interpolation avoids segmented zig-zag corners in the loop.
  for (std::size_t i = 0; i + 1 < guides.size(); ++i) {
    Vec3 p0 = guides[i ? i - 1 : i], p1 = guides[i], p2 = guides[i + 1],
         p3 = guides[std::min(i + 2, guides.size() - 1)];
    for (int step = 0; step < subdivisions; ++step) {
      float t = float(step) / subdivisions, t2 = t * t, t3 = t2 * t;
      strap.push_back((p1 * 2 + (p2 - p0) * t +
                       (p0 * 2 - p1 * 5 + p2 * 4 - p3) * t2 +
                       (-p0 + p1 * 3 - p2 * 3 + p3) * t3) *
                      .5f);
    }
  }
  strap.push_back(guides.back());
  // Retain accepted lower-path centers and parallel-transport frames exactly.
  // Only the documented affected body/shoulder rings below are refitted.
  // Exact lower-path preservation is retained outside Near rings3..50 /
  // Far rings1..30; the expanded interval repairs measured edited-coat crossings.
  fit_accepted_tessa_strap(b,strap);
  const auto strap_base=std::uint32_t(b.m.bind_mesh.vertices.size());
  const auto strap_index_base=b.m.bind_mesh.indices.size();
  b.tube(strap, std::vector<float>(strap.size(), .006f), .10f, leather * .86f,
         S::Leather,
         [&](float t) {
           const auto index =
               std::min(strap.size() - 1,
                        std::size_t(std::lround(t * (strap.size() - 1))));
           const float chest = smooth((strap[index].y - .249f) / .064f);
           return wt(B::Spine, B::Chest, 1 - chest);
         },
         b.near ? 8 : 6, {1, 0, 0});
  {
    const int nc=b.near?8:6,lo=b.near?3:1,hi=10*subdivisions;
    // The lower ellipse half traverses physical width in reverse. Mirror its
    // internal quad diagonal so the two thin sheets use matching physical
    // diagonals through bends; otherwise the triangulated sheets can cross.
    // This bounded connectivity repair changes no outer edge or triangle count.
    for(int row=lo;row<hi;++row)for(int col=nc/2;col<nc;++col) {
      const auto offset=strap_index_base+6*std::size_t(row*nc+col);
      const auto a=strap_base+std::uint32_t(row*(nc+1)+col),c=a+nc+1;
      const std::uint32_t corners[]{a,c,a+1,a+1,c,c+1};
      for(int j=0;j<6;++j)b.m.bind_mesh.indices[offset+j]=corners[j];
    }
    const Vec3 anchor{0,.220f,0};
    std::vector<Vec3> centers=strap,normals(strap.size());
    for(int row=lo;row<=hi;++row) {
      const Vec3 direction=normalize(strap[row]-anchor);
      float distance;CharacterInfluence weight;
      if(coat_ray(b,construction,anchor,direction,distance,weight,&normals[row]))
        centers[row]=anchor+direction*distance;
    }
    // A coherent ribbon frame keeps width tangent to the coat and thickness
    // normal to it. Projecting a transported elliptical tube pointwise folds
    // its two thin sheets over each other near the shoulder roll.
    Vec3 previous_width=normalize(b.m.bind_mesh.vertices[strap_base+lo*(nc+1)].position*(1/b.h)-strap[lo]);
    for(int row=lo+1;row<hi;++row) {
      const Vec3 tangent=normalize(centers[row+1]-centers[row-1]);
      Vec3 width=normalize(cross(normals[row],tangent));
      const auto first=strap_base+std::uint32_t(row*(nc+1));
      if(dot(width,previous_width)<0)width=-width;
      const Vec3 old_width=b.m.bind_mesh.vertices[first].position*(1/b.h)-strap[row];
      const Vec3 old_axis=normalize(old_width-tangent*dot(old_width,tangent));
      const float frame_blend=smooth(float(row-lo)/2.f);
      width=normalize(old_axis*(1-frame_blend)+width*frame_blend);
      previous_width=width;
      const Vec3 thickness=normalize(cross(width,tangent));
      const float fit=smooth(float(row-lo))*
                      smooth(float(hi-row)/(.8f*subdivisions));
      float support=0;
      for(int col=0;col<nc;++col) {
        const float angle=2*pi*float(col)/nc;
        const Vec3 query=centers[row]+width*(.006f*std::cos(angle));
        float distance;CharacterInfluence weight;
        if(coat_ray(b,construction,query-normals[row]*.03f,normals[row],distance,weight,nullptr,true))
          support=std::max(support,distance-.03f);
      }
      const Vec3 center=centers[row]+normals[row]*(support+(b.near?.0018f:.0034f));
      // Keep each section planar and convex. A separately projected ellipse
      // has nonplanar thin sheets whose triangulation can cross even when all
      // individual samples clear the coat.
      for(int col=0;col<=nc;++col) {
        auto index=first+std::uint32_t(col);
        auto &vertex=b.m.bind_mesh.vertices[index];
        const Vec3 original=vertex.position*(1/b.h);
        const float angle=2*pi*float(col==nc?0:col)/nc;
        const Vec3 fitted=center+width*(.006f*std::cos(angle))+
                          thickness*(.0006f*std::sin(angle));
        float distance;CharacterInfluence weight;
        const Vec3 direction=normalize(fitted-anchor);
        if(coat_ray(b,construction,anchor,direction,distance,weight)) {
          vertex.position=(original*(1-fit)+fitted*fit)*b.h;
          b.m.influences[index]=blend_cloth_weights(b.m.influences[index],weight,fit);
        }
      }
    }
  }
  // Small side tabs and restrained brass rings explain both strap attachments.
  for (float side : {-1.f, 1.f}) {
    const float x = cx + side * .038f;
    b.ellipsoid({x, .236f, .106f}, {.0035f, .009f, .0018f}, leather * .90f,
                S::Leather, B::Spine, b.near ? 12 : 8, b.near ? 6 : 4);
    if (!b.near)
      continue;
    std::vector<Vec3> ring;
    for (int i = 0; i <= 20; ++i) {
      float a = 2 * pi * float(i) / 20;
      ring.push_back(
          {x + .0037f * std::cos(a), .248f + .0047f * std::sin(a), .108f});
    }
    closed_trim(b, ring, .00065f, {.42f, .35f, .20f}, S::Metal, B::Spine,
                b.near ? 6 : 4);
  }
}
float hairline(float a) {
  float front = std::max(0.f, std::sin(a)), back = std::max(0.f, -std::sin(a));
  return .433f + .031f * front - .029f * back +
         .0026f * std::sin(a * 3) * front + .0010f * std::sin(a * 9) +
         .010f * std::pow(std::fabs(std::cos(a)), 8.f);
}
Vec3 short_hair(float a, float v, float lift = 0) {
  float y = lerp(hairline(a), .499f, std::sin(v * pi * .5f));
  float cz = lerp(tessa_anatomy::head_center.z, tessa_anatomy::crown_joint.z,
                  smooth((y - .46f) / .033f));
  float r = anatomy_radius(std::min(y, .489f), a, cz);
  float layer = .0012f + .0019f * smooth(v / .14f) +
                .00055f * std::sin(a * 9 + v * 6) * std::sin(pi * v);
  r += layer + lift;
  if (y > .489f)
    r = std::max(.0003f,
                 r * std::sqrt(std::max(0.f, 1 - sq((y - .489f) / .010f))));
  return {r * std::cos(a), y, cz + r * std::sin(a)};
}
void make_hair(Sculpt &b) {
  Vec3 hair = b.m.appearance.hair;
  // Split the cap into eight UV sectors: the 192px Hair tile now resolves
  // millimetre-scale bundles instead of stretching 29 bundles over a skull.
  // Adjacent sectors share physical vertices/normals but retain independent
  // UVs.
  const int sectors = 8, columns = b.near ? 8 : 3, rows = b.near ? 18 : 7;
  std::array<std::uint32_t, sectors> starts{};
  for (int sector = 0; sector < sectors; ++sector) {
    starts[sector] = std::uint32_t(b.m.bind_mesh.vertices.size());
    b.grid(
        columns, rows,
        [&](float u, float v) {
          float a = 2 * pi * (float(sector) + u) / sectors;
          float vv = .065f + .935f * v;
          float tone = .82f + .035f * std::sin(a * 7 + vv * 8);
          return P{short_hair(a, vv), hair * tone * .72f, wt(B::Head),
                   character_surface_uv(S::HairCrop, u, v)};
        },
        false, false);
  }
  for (int sector = 0; sector < sectors; ++sector)
    for (int row = 0; row <= rows; ++row) {
      auto first = starts[sector] + std::uint32_t(row * (columns + 1));
      auto last = starts[(sector + sectors - 1) % sectors] +
                  std::uint32_t(row * (columns + 1) + columns);
      b.m.bind_mesh.vertices[last].position =
          b.m.bind_mesh.vertices[first].position;
      b.normal_groups.push_back({first, last});
    }
  const auto pole = b.vertex({{0, .499f, tessa_anatomy::crown_joint.z},
                              hair * (.82f * .72f),
                              wt(B::Head),
                              character_surface_uv(S::HairCrop, .5f, 1.f)});
  for (int sector = 0; sector < sectors; ++sector)
    for (int col = 0; col < columns; ++col) {
      auto a = starts[sector] + std::uint32_t(rows * (columns + 1) + col);
      b.tri(a, pole, a + 1);
    }
  // Short overlapping scissor-cut layers flow from crown to temple and nape.
  // The cutout tips break the scalp outline, not just its painted color.
  const int locks = b.near ? 180 : 20;
  for (int k = 0; k < locks; ++k) {
    const float sequence = float(k) * .754877666f;
    float root = b.near ? float(k) * 2.39996323f : 2 * pi * float(k) / locks;
    float start =
        b.near ? .18f + .73f * (sequence - std::floor(sequence)) : .26f;
    float end = start < .42f ? -.047f : start - .31f;
    float width = (b.near ? .090f : .34f) * (1 + .18f * std::sin(k * 1.7f));
    b.grid(
        2, b.near ? 6 : 4,
        [&](float u, float v) {
          float vv = lerp(start, end, v),
                a = root + .12f * std::sin(pi * v) + (.5f - u) * width;
          Vec3 p = short_hair(a, std::clamp(vv, 0.f, 1.f),
                              -.0010f + .0008f * smooth(v / .40f));
          if (vv < 0)
            p.y += vv * .07f;
          float tone = (.89f + .10f * std::sin(k * 1.31f)) *
                       (.72f + .28f * smooth(v / .35f));
          return P{p, hair * tone * .80f, wt(B::Head),
                   character_surface_uv(S::HairStrands, u, v)};
        },
        false, false);
  }
}
CharacterRig tessa_rig(float height) {
  CharacterRig r;
  r.height = height;
  auto joint = [&](B bone, int parent, Vec3 p) {
    r.joints[bone_index(bone)] = {parent, p * height};
  };
  auto child = [&](B bone, B parent, Vec3 p) {
    joint(bone, int(bone_index(parent)), p);
  };
  joint(B::Pelvis, -1, {0, 0, 0});
  child(B::Spine, B::Pelvis, {0, .13f, 0});
  child(B::Chest, B::Spine, {0, .272f, 0});
  child(B::Neck, B::Chest, tessa_anatomy::neck_center);
  child(B::Head, B::Neck, tessa_anatomy::head_center);
  for (float sign : {-1.f, 1.f}) {
    bool q = sign > 0;
    B upper = q ? B::RightUpperArm : B::LeftUpperArm,
      fore = q ? B::RightForearm : B::LeftForearm,
      hand = q ? B::RightHand : B::LeftHand,
      thigh = q ? B::RightThigh : B::LeftThigh,
      shin = q ? B::RightShin : B::LeftShin,
      foot = q ? B::RightFoot : B::LeftFoot;
    child(upper, B::Chest, {sign * .116f, .310f, 0});
    child(fore, upper, {sign * .135f, .12f, 0});
    child(hand, fore, {sign * .155f, -.044f, 0});
    child(thigh, B::Pelvis, {sign * .060f, 0, 0});
    child(shin, thigh, {sign * .060f, -.225f, 0});
    child(foot, shin, {sign * .060f, -.46f, 0});
  }
  return r;
}
} // namespace
CharacterProfile make_tessa_character_profile(float height, CharacterRole role,
                                              std::string_view identity,
                                              CharacterLod lod) {
  CharacterProfile profile;
  if (!std::isfinite(height) || height <= 0.f ||
      (lod != CharacterLod::Near && lod != CharacterLod::Far))
    throw std::invalid_argument("Invalid individual character request");
  auto &m = profile.model;
  m.role = role;
  m.lod = lod;
  m.seed = character_seed(identity);
  m.appearance = character_appearance(m.seed);
  m.rig = tessa_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 36000 : 12000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 180000 : 60000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt sculpt{m, height, lod == CharacterLod::Near, {}, {}};
  CoatConstruction construction;
  make_torso(sculpt, construction);
  make_limbs(sculpt, construction);
  make_head(sculpt);
  make_hair(sculpt);
  make_bag(sculpt, construction);
  {
    auto &mesh=m.bind_mesh;
    std::vector<std::uint32_t> remap(mesh.vertices.size(),~std::uint32_t(0));
    for(auto i:mesh.indices)remap[i]=0;
    std::uint32_t next=0;
    for(std::uint32_t i=0;i<remap.size();++i)if(remap[i]==0) {
      remap[i]=next;mesh.vertices[next]=mesh.vertices[i];m.influences[next]=m.influences[i];++next;
    }
    for(auto &i:mesh.indices)i=remap[i];
    mesh.vertices.resize(next);m.influences.resize(next);
    auto &seams=sculpt.smooth_seams;
    seams.erase(std::remove_if(seams.begin(),seams.end(),[&](auto p){return remap[p.first]==~std::uint32_t(0)||remap[p.second]==~std::uint32_t(0);}),seams.end());
    for(auto &p:seams){p.first=remap[p.first];p.second=remap[p.second];}
    for(auto &group:sculpt.normal_groups) {
      group.erase(std::remove_if(group.begin(),group.end(),[&](auto i){return remap[i]==~std::uint32_t(0);}),group.end());
      for(auto &i:group)i=remap[i];
    }
  }
  sculpt.finish();
  profile.material = make_character_surface_material();
  profile.material.alpha_cutoff = .4f;
  profile.lod_distance = 12.f;
  profile.individualized = true;
  return profile;
}
} // namespace fury
