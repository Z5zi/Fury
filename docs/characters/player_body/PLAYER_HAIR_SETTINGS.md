# Player Hair settings candidate

Status: integrated and validated on main program fingerprint `db8bdd320b3b0ebf2cd89e53ea02b0df6359a6e989fde928b08be7a5cb0b7757`, preserving accepted base `7b47336d553b060a17a6341c3b47d1d56f53a4674bd09f57202fc14497ab7f2a`. The existing 132 OFF aggregate cases passed in the isolated candidate, followed by 28/28 affected integrated-main gates. Experimental crouch remains OFF.

## O / Start menu

Player Hair has three saved requested choices:

- Auto (default): Original at Low quality; fitted source groom, short tied tail and 512 color/alpha detail at Med/High.
- Original: original main hair regardless of quality.
- Balanced: fitted source groom, short tied tail and 512 detail regardless of quality.

Balanced is supported by software raster, CPU ray and OpenGL. DX12 remains Original for Auto and saved Balanced requests; Balanced is excluded from DX12 menu cycling because hardware validation is still missing. A saved Balanced request is retained so it works again on a supported backend. Explicit diagnostic environment bundles retain their legacy DX12 behavior, including its unverified detail fallback.

Use Up/Down or W/S to select, Left/Right or A/D to change, and Enter/Space to cycle Player Hair. Controller D-pad and A work in this settings modal; Start opens/closes it. O, Esc or Start closes and saves. Held inputs do not repeat or change a value merely because the menu is reopened.

The footer shows the active groom and, if it differs, the groom that will apply after restart. Models, material instances, atlas and detail textures are not rebuilt in mid-session. F6 and the quality row still change existing render quality live; Auto's hair change waits for restart. Returning to a choice/quality that resolves to the active groom clears the restart notice. `QUALITY ENV` means `FURY_QUALITY` will win again at restart, and the preview accounts for it.

## Precedence and validation

Any presence of `FURY_PLAYER_SOURCE_HAIR`, `FURY_PLAYER_SHORT_TAIL`, or `FURY_PLAYER_HAIR_DETAIL` selects the entire legacy environment bundle. It takes priority over the saved request and Auto. Missing bundle fields are zero; no preset fills them in. Empty values are invalid. Source and tail accept exact `0`/`1`; detail accepts exact `0`/`512`/`1024`. Tail or nonzero detail without source throws. The footer says `ENV OVERRIDE`; editing the menu still saves the requested preference, never the forced/resolved result.

`FURY_QUALITY > saved quality > Med` is unchanged. Hair policy is resolved once after actual renderer fallback and before surfaces/models are created, using an explicit API rather than environment mutation. `FURY_NPC_DETAIL=0` or `FURY_INDIVIDUAL_CHARACTERS=0` disables the individualized hair and detail allocation. Malformed explicit hair bundles still fail validation before these gates.

The JSON field is `player_hair` (`0` Auto, `1` Original, `2` Balanced). Legacy files without it use Auto. Invalid, fractional, nested-only or out-of-range values use Auto. Unrelated existing and future top-level JSON fields are retained when settings are saved.

Characterlab and other tools that do not call the configuration API retain the legacy cached environment-only defaults. Configuration and legacy lazy initialization are thread-safe and immutable once selected.

## Limits

This policy does not change Player geometry, texture content, motion, lighting, or other characters. See `TIED_TAIL_OPTION.md` for accepted-main evidence and the remaining Far-tail pop, pose-dependent bend, DX12, performance and sanitizer limits. No new FPS or renderer-parity claim is made.
