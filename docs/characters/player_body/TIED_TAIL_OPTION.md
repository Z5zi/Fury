# Player source groom and tied-tail option

Status: the source groom, shortened tied tail and optional detail were integrated and validated on accepted main fingerprint `7b47336d553b060a17a6341c3b47d1d56f53a4674bd09f57202fc14497ab7f2a`. The 2026-10-08 checkpoint passed 132/132 aggregate tests with experimental crouch OFF and 132/132 ON, plus 20 focused sanitizer gates and 23 clean-restore focused gates. Archive SHA-256: `c5398fbfc076e7ca4a6f173e72554d8698d2a06f3c7d00b0871e80da682aad4e`. These counts establish integration coverage; they do not establish FPS parity, performance gain, DX12 validation, or acceptance of experimental movement. LSan was disabled. Generic animation remains the accepted motion path.

Vaultline now uses the integrated Player Hair startup/menu policy described in `PLAYER_HAIR_SETTINGS.md`, on main program fingerprint `db8bdd320b3b0ebf2cd89e53ea02b0df6359a6e989fde928b08be7a5cb0b7757`. Characterlab and other unconfigured tools retain the legacy environment-only default-off behavior.

## Legacy diagnostic environment options

Use separate processes for each configuration; choices are read once and cached.

- `FURY_PLAYER_SOURCE_HAIR=1` selects the fitted CC0 source groom for the individualized `PlayerBody` asset. Absent or `0` retains the original main hair.
- `FURY_PLAYER_SHORT_TAIL=1` selects the shortened tied tail and requires source hair. Absent or `0` leaves the source-groom fallback fold when source hair is enabled.
- `FURY_PLAYER_HAIR_DETAIL=512` optionally enables scoped higher-resolution color/alpha detail and requires source hair. Absent or `0` disables it. `1024` is also supported as a higher-cost comparison; other values are rejected.
- The first two options accept only exact `0`/`1`. In unconfigured tools all options default off; Vaultline uses its saved/preset policy when none of these fields is present. They do not enable or select an animation profile and do not change other characters' authored geometry or material response.

The 512 image and complete mip chain occupy 1,398,100 decoded bytes, excluding renderer-owned copies and small metadata. The 1024 comparison occupies 5,592,404 decoded bytes. The shared atlas dimensions/allocation remain unchanged; three appended cells use previously unused space.

## Geometry and limitations

The guarded main-based model contains 36,916 triangles Near and 15,977 triangles Far with the tied tail enabled (22,620 and 9,662 vertices, respectively). This is not a whole-scene performance result.

Tail deformation blends Head/Neck transforms according to source height. It is a pose-dependent bend, with no temporal simulation, inertia, or independently evolving hair state. Scoped deformed normals and parent-edge attachments follow the current posed geometry.

The Far tail is visibly shorter, simpler, and stiffer. The Near/Far switch can visibly pop; this limitation remains part of the option's acceptance decision. Optional texture detail does not fix the silhouette difference.

Any future experimental movement requires fresh hair/body/garment clearance, deformation, silhouette, and gameplay validation. Keeping `FURY_ENABLE_EXPERIMENTAL_EQUIPPED_CROUCH` OFF is the default. Its existing explicit Player profile assignment is preserved; enabling that build option is not a motion-quality approval.

## Integration and renderer contract

Rebuild all consumers: the required material/model support changes the `Material` and `CharacterModel` layouts. Do not mix old objects or libraries with rebuilt consumers.

Software, CPU ray, and OpenGL have scoped color/alpha-detail support. OpenGL retains main's existing lighting policy. DXR uses the original base atlas as its optional-detail fallback; DXR high-resolution detail is unsupported here, and the fallback still requires platform verification.

The accepted-main checkpoint includes aggregate, dense pose/contact, exact-hold/allocation, focused sanitizer, software/CPU-ray/OpenGL renderer, restored native-pixel, and quiet whole-scene cost evidence. Quiet whole-scene ABBA medians were 318.245/322.108 ms Original and 316.310/320.909 ms enabled on the software host; the noisy measurements support no stable overhead, speedup, FPS, or parity claim. Changes to startup selection or menu policy require a fresh settings-candidate validation pass. DX12 remains unverified.
