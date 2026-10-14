# Vale final visible review

This review is based on the actual captured images, including the complete
final all-side sheets, every frame on the final patrol/talk contact sheets,
the material/clay detail comparison, and the three renderer fixture sheets.
Numerical passes establish constraints; the observations below describe what
still reads visually.

## Head, hair and cap

The frontal head retains broad cheeks tapering into the longer chin. Three-
quarter and side views reveal the nose tip, nostril wing, ear bowl and the turn
under the jaw. The eyes remain seated in their original anatomical apertures.
The source ears and facial planes are the same in Far; the simplified cap and
brow/eye accessories are the noticeable LOD differences in a forced close-up.

The corrected attention frames are materially better than the before witness.
The lower lip now travels with the face instead of pulling diagonally away from
the upper face during yaw or stretching down during upward pitch. The rear
attention frames retain the neck/collar connection. This does not add lip sync,
expressive face controls or independent eye movement.

The nape has a continuous short dark fade, with no old dotted boundary-card
pattern. The cap's crown, raked band and curved bill read as separate forms.
Low-angle details show an opaque underside and an actual thin edge return.
The root/side closure repair survives the close views. Near seam lines remain
thin and can alias at some portrait scales; Far deliberately omits them and
shows stronger crown facets. The small original plain mark retains its color.

## Clothing and extremities

The broader shoulder/deeper torso fit is clear against the tapered waist.
Sleeve roots flow around the armhole rather than ending in separate caps.
The radio, pouch and carrier fronts remain readable in side/rear-quarter
views and move with their supporting areas. The carrier's edges and openings
are still quite rigid and geometric, especially around the shoulder. Their
visible separation and simple edge treatment are art limits, not proof of a
fully simulated or finely finished garment.

The trousers have a connected seat and crotch through stride and crouch. Their
fold treatment is simplified: most visible shaping is concentrated at the
knees and the short trouser break, while thigh/seat surfaces remain plain.
The Far knees and lower legs become angular when forced into these close
views. None of that is hidden by the passing contact tests.

The hands read as palms and separate digits in both LODs. Their wrist lines
remain connected to the cuffs through the reviewed body/gesture poses. Far
reduces fine finger/nail form. Hands retain a static relaxed shape; they do not
change grip or articulate individual fingers.

The raised trouser hem makes the brown shoe upper more legible in the neutral
stage. Soles, toe/heel outlines and a few lace lines are present, but the boots
remain plain in finish and do not match the detailed wear, construction or
material complexity of a high-end reference boot.

The close rear-yoke material view shows a visible woven/ribbed pattern under
foreshortening. Identical-camera clay and vertex-material views remove that
fine pattern while keeping the broader yoke form. It is a material/UV response,
not additional folded geometry. No shared material or renderer was changed to
hide it.

## Actual world and motion

The complete 192-frame live patrol and 36-frame talk/patrol sequences were
reviewed as numbered contact sheets. Body, sleeves, hands, carrier and cap stay
coherent in the visible frames. The scene occludes the actor around the middle
of the patrol sequence; those images remain in the evidence. No claim is made
about invisible body areas behind the building. The known obstacle-blind guard
behavior and its BankWallE/KIT_WallR issue remain deferred.

The original and updated world controls share the same camera and scene.
Both show the existing spawn/platform obscuring the feet. Elsewhere in the
patrol, the boots and trouser break are visible. There was no attempt to remove
scenery, shift the actor, change the route or dress a diagnostic stage as the
game world. The capture helper's historical `patrol-chase` filename is a case
label, not proof of a newly forced chase; the saved final state is non-chasing.
The actual 3.8 m/s gait is independently exercised by the model/renderer tests.

The 96-second real-cycle captures reach a dark sky with the actual lit scene
and tightened night patrol. The face remains readable under the warmer local
light. These are distinct from the earlier orange twilight captures.

## Rendering limits

Software rasterization and Mesa OpenGL show stable pose changes and the same
forms, with different lighting/edge responses. Mesa is software OpenGL, with
some advanced effects disabled by the existing engine.

CPU ray tracing shows the geometry clearly on directly lit areas, but the
hard cap shadow and the shadowed alley flatten facial contrast. The four-frame,
four-spp path-traced portraits add fill but remain visibly noisy. Those results
are included with exact settings and are not presented as cross-backend visual
parity or a polished high-sample render. No image has been cosmetically retouched.

## Final judgment

Vale is a distinct, bounded stylized character improvement with verified source
anatomy, connected hands, coherent attention and explicit performance costs.
Remaining simplified folds, rigid carrier edges, plain boots, coarse forced-Far
silhouettes, renderer/lighting differences and inherited world/navigation
limitations remain visible and documented. Passing suites do not erase them.
