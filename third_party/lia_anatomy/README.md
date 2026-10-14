# Lia Merrow CC0 anatomy inputs

Only pinned numeric MPFB2 graphical morph targets are newly vendored here.
See MANIFEST.json for exact immutable URLs, source paths, hashes, bytes and
Git blobs, and lia_recipe.json for the original weights. Shared base/metadata,
licenses and existing targets are referenced in place. Their graphical asset
license is CC0-1.0; no upstream addon code is included or executed.

Generate with `python3 scripts/build_lia_anatomy.py`; verify without writes
with `python3 scripts/build_lia_anatomy.py --check` from the repository root.
Generated output is engine/include/fury/generated/lia_anatomy.hpp. Statistics
and source fitting are in GENERATED_STATS.json; independent source/UV, shape
and malformed-input audit results are in VALIDATION.json.
