#include "fury/character_profile.hpp"

namespace fury {
CharacterProfile make_mira_character_profile(float height, CharacterRole role,
                                             std::string_view identity,
                                             CharacterLod lod);

CharacterProfile make_jon_character_profile(float height, CharacterRole role,
                                            std::string_view identity,
                                            CharacterLod lod);

CharacterProfile make_tessa_character_profile(float height, CharacterRole role,
                                              std::string_view identity,
                                              CharacterLod lod);

CharacterProfile make_owen_character_profile(float height, CharacterRole role,
                                             std::string_view identity,
                                             CharacterLod lod);

CharacterProfile make_lia_character_profile(float height, CharacterRole role,
                                            std::string_view identity, CharacterLod lod);

CharacterProfile make_nell_character_profile(float height, CharacterRole role,
                                             std::string_view identity, CharacterLod lod);

CharacterProfile make_pax_character_profile(float height, CharacterRole role,
                                            std::string_view identity, CharacterLod lod);

CharacterProfile make_rina_character_profile(float height, CharacterRole role,
                                             std::string_view identity, CharacterLod lod);

CharacterProfile make_hale_character_profile(float height, CharacterRole role,
                                             std::string_view identity, CharacterLod lod);

CharacterProfile make_vale_character_profile(float height, CharacterRole role,
                                             std::string_view identity, CharacterLod lod);

CharacterProfile make_metro_watch_character_profile(float height, CharacterRole role,
                                                    std::string_view identity, CharacterLod lod);

CharacterProfile make_cass_character_profile(float height, CharacterRole role,
                                             std::string_view identity, CharacterLod lod);

CharacterProfile make_renn_character_profile(float height, CharacterRole role,
                                             std::string_view identity, CharacterLod lod);

CharacterProfile make_rook_character_profile(float height, CharacterRole role,
                                             std::string_view identity, CharacterLod lod);

CharacterProfile make_sparrow_character_profile(float height, CharacterRole role,
                                                std::string_view identity, CharacterLod lod);

CharacterProfile make_enforcer_character_profile(float height, CharacterRole role,
                                                 std::string_view identity, CharacterLod lod);

CharacterProfile make_ghost_character_profile(float height, CharacterRole role,
                                             std::string_view identity, CharacterLod lod);

CharacterProfile make_player_body_character_profile(float height, CharacterRole role,
    std::string_view identity, CharacterLod lod);

CharacterProfile make_character_profile(float height, CharacterRole role,
                                        std::string_view identity,
                                        CharacterLod lod) {
  if (identity == "NpcCivA" && role == CharacterRole::Commuter)
    return make_mira_character_profile(height, role, identity, lod);
  if (identity == "NpcCivB" && role == CharacterRole::Market)
    return make_jon_character_profile(height, role, identity, lod);
  if (identity == "NpcCivC" && role == CharacterRole::Commuter)
    return make_tessa_character_profile(height, role, identity, lod);
  if (identity == "NpcBankCust" && role == CharacterRole::Commuter)
    return make_owen_character_profile(height, role, identity, lod);
  if (identity == "NpcTeller" && role == CharacterRole::BankStaff)
    return make_lia_character_profile(height, role, identity, lod);
  if (identity == "NpcCivAsh" && role == CharacterRole::Market)
    return make_nell_character_profile(height, role, identity, lod);
  if (identity == "NpcCivD" && role == CharacterRole::Dock)
    return make_pax_character_profile(height, role, identity, lod);
  if (identity == "NpcCivE" && role == CharacterRole::Dock)
    return make_rina_character_profile(height, role, identity, lod);
  if (identity == "NpcGuard" && role == CharacterRole::Security)
    return make_hale_character_profile(height, role, identity, lod);
  if (identity == "NpcAlleyHmpd" && role == CharacterRole::Security)
    return make_vale_character_profile(height, role, identity, lod);
  if (identity == "NpcExtraGuard" && role == CharacterRole::Security)
    return make_metro_watch_character_profile(height, role, identity, lod);
  if (identity == "NpcFence" && role == CharacterRole::Fence)
    return make_cass_character_profile(height, role, identity, lod);
  if (identity == "NpcDeskGuard" && role == CharacterRole::Security)
    return make_renn_character_profile(height, role, identity, lod);
  if (identity == "CrewRook" && role == CharacterRole::CrewTech)
    return make_rook_character_profile(height, role, identity, lod);
  if (identity == "CrewSparrow" && role == CharacterRole::CrewScout)
    return make_sparrow_character_profile(height, role, identity, lod);
  if (identity == "NpcEnforcer" && role == CharacterRole::Enforcer)
    return make_enforcer_character_profile(height, role, identity, lod);
  if (identity == "GhostLoop" && role == CharacterRole::Ghost)
    return make_ghost_character_profile(height, role, identity, lod);
  if (identity == "PlayerBody" && role == CharacterRole::Player)
    return make_player_body_character_profile(height, role, identity, lod);
  CharacterProfile profile;
  profile.model =
      make_character_model(height, role, character_seed(identity), lod);
  return profile;
}
} // namespace fury
