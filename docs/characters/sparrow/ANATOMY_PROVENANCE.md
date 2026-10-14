# Sparrow anatomy and asset provenance

Sparrow’s original face and hands are generated with Fury-owned Python importers from pinned CC0 graphical data. The upstream MPFB repository is pinned to d0a32e57a7f915cb2f2b95410e2117648c7bbb7e. No MPFB addon, Blender addon, GPL program module or upstream executable is imported or run. The upstream overall program license and the graphical asset license remain separately preserved in third_party/mpfb_assets.

The source manifest records immutable URL, byte length, SHA-256 and Git blob ID for each source. Existing source OBJ and numeric targets are referenced from accepted baseline directories. Two eye-fold numeric .target.gz assets are newly vendored from that same immutable pin. A .target record is a numeric vertex displacement, not executable code. The macro labels describe source shape data and make no claim about Sparrow’s biography or protected identity.

Sparrow uses an independent narrow oval head recipe, small chin/bridge changes, ear placement, modest cheek planes and restrained fold shaping. The original mixed macro experiment could not support properly seated eyeballs; the final recipe was chosen after inspecting actual source aperture triangles and rendered side views. Head correspondence preserves 4,434 source vertices, 8,816 triangles, source UV corners and the full 50-edge neck attachment. The normalization fixes source body ground to -.5 and scalp crown to .493, reserving .007 of actor height for original hair. The actor’s final hair crown remains exactly .5, for full height 1.72 m.

The hands preserve connected thumb and four-finger anatomy, source UV corners and locked wrist correspondence. The Near hand has 3,276 triangles, Far has 520. Their modest static curl and their wrist orientation are original Fury choices. They have no independent finger rig or facial animation.

Reproduction commands, from this source root:

```
python scripts/build_sparrow_anatomy.py --check
python scripts/build_sparrow_hands.py --check
```

The generated headers and statistics are committed-style deliverables, not runtime downloads. Geometry checks compare the actual emitted mesh to these outputs; appearance and gameplay identity are tested separately. Rockstar GTA VI and product reference pixels were viewed privately for quality comparison and are not imported or distributed. The final character remains materially simpler than those references in hair, skin response, cloth detail and deformation.
