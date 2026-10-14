#!/usr/bin/env python3
"""Verify generic numerical source preservation and identity-free dispatch."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
original = 'b260a337ab82dbe47a1c0ac8caa365c815823acc56fb81ec38c3c9031f8e0ce9'

def digest(text):
    return hashlib.sha256(text.encode()).hexdigest()

s = (root / 'engine/src/character_animation_generic.cpp').read_text()
s = s.replace('namespace fury { namespace animation_detail { namespace generic {', 'namespace fury {', 1)
s = s.replace('}}}  // namespace fury::animation_detail::generic', '}  // namespace fury')
assert digest(s) == original, 'Generic arithmetic changed from the frozen validated source'
r = (root / 'tests/fixtures/character_animation_generic_reference.cpp').read_text()
r = r.replace('namespace fury { namespace animation_reference {', 'namespace fury {', 1)
r = r.replace('}} // namespace fury::animation_reference', '}  // namespace fury')
assert digest(r) == original, 'Independent reference source changed'
for path in ['engine/src/character_animation.cpp',
             'engine/src/character_animation_generic.cpp',
             'engine/src/character_animation_equipped_crouch.cpp']:
    source = (root / path).read_text()
    for forbidden in ['PlayerBody', 'GhostLoop', 'CrewRook', 'CrewSparrow',
                      'CharacterRole', 'character_seed(', 'FURY_ENABLE_EXPERIMENTAL_EQUIPPED_CROUCH']:
        assert forbidden not in source, (path, forbidden)
print(json.dumps({'passed': True, 'generic_source_sha256': original,
                  'reference_source_sha256': digest(r), 'dispatch': 'enum/state only'}))
