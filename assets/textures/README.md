# Vaultline textures (5.3.0)

Original procedural textures — **no third-party / Rockstar / GTA IP**.

| File | Use |
|------|-----|
| `crate_wood.png` / `.ppm` | Wood crates (`TextureSlot::Wood`) albedo |
| `barrel_metal.png` / `.ppm` | Metal barrels (`TextureSlot::BarrelMetal`) albedo |
| `asphalt.png` / `.ppm` | Roads / street ground (`TextureSlot::Asphalt`) albedo |
| `asphalt_n.png` / `.ppm` | Asphalt **normal** map (GL unit 3) |
| `brick_n.png` / `.ppm` | Brick **normal** map (GL unit 3) |

Engine prefers PNG via vendored `stb_image`; PPM is a fallback / edit-friendly format.
Normals are tangent-space RGB (flat = 128,128,255). Procedural height→normal fills in if files are missing.
