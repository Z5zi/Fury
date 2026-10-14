// Model-specific physical checks. Test geometry is the exact production mesh,
// including the same near/far surfaces and sampled deformation used to render.
struct MetroBounds {
  Vec3 low{1.e9f, 1.e9f, 1.e9f}, high{-1.e9f, -1.e9f, -1.e9f};
  void add(Vec3 p) {
    low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
    high = {std::max(high.x, p.x), std::max(high.y, p.y),
            std::max(high.z, p.z)};
  }
  bool overlaps(const MetroBounds &o) const {
    return !(high.x < o.low.x || o.high.x < low.x || high.y < o.low.y ||
             o.high.y < low.y || high.z < o.low.z || o.high.z < low.z);
  }
};
bool metro_segment_triangle(Vec3 p, Vec3 q, Vec3 a, Vec3 b, Vec3 c, float h) {
  const auto d = q - p, e1 = b - a, e2 = c - a, cross_d = cross(d, e2);
  const double determinant = dot(e1, cross_d);
  if (std::fabs(determinant) < h * h * h * 1.e-12)
    return false;
  const auto o = p - a, cross_o = cross(o, e1);
  const double u = dot(o, cross_d) / determinant,
               v = dot(d, cross_o) / determinant,
               t = dot(e2, cross_o) / determinant;
  constexpr double e = 1.e-5;
  return u > e && v > e && u + v < 1 - e && t > e && t < 1 - e;
}
bool metro_crossing(const Mesh &m, std::size_t a, std::size_t b, float h) {
  Vec3 p[3], q[3];
  for (int i = 0; i < 3; ++i) {
    p[i] = m.vertices[m.indices[a + i]].position;
    q[i] = m.vertices[m.indices[b + i]].position;
  }
  for (auto x : p)
    for (auto y : q)
      if (length(x - y) < h * 1.e-6f)
        return false;
  for (int e = 0; e < 3; ++e)
    if (metro_segment_triangle(p[e], p[(e + 1) % 3], q[0], q[1], q[2], h) ||
        metro_segment_triangle(q[e], q[(e + 1) % 3], p[0], p[1], p[2], h))
      return true;
  return false;
}
void metro_intersection_controls() {
  require(metro_segment_triangle({0, 0, -1}, {0, 0, 1}, {-1, -1, 0}, {1, -1, 0},
                                 {0, 1, 0}, 1),
          "Contact detector misses strict crossing control");
  require(!metro_segment_triangle({0, 0, -1}, {0, 0, -.1f}, {-1, -1, 0},
                                  {1, -1, 0}, {0, 1, 0}, 1),
          "Contact detector incorrectly flags separate control");
  require(!metro_segment_triangle({-1, -1, -1}, {-1, -1, 1}, {-1, -1, 0},
                                  {1, -1, 0}, {0, 1, 0}, 1),
          "Shared edge/vertex contact is not an interior crossing");
}
void metro_closed_layers(const CharacterModel &m) {
  for (int region = 0; region < 5; ++region) {
    // Complete finite carrier pieces and equipment. The shirt/trousers retain
    // anatomical neck/cuff/waist openings and are checked as sewn garments.
    const auto choose = [&](const Vertex &v) {
      const int s = surface(v.uv);
      const float y = v.position.y / m.rig.height;
      return region == 0   ? s == int(CharacterSurface::Jacket)
             : region == 1 ? s == int(CharacterSurface::Metal)
             : region == 2 ? s == int(CharacterSurface::Rubber)
             : region == 3 ? s == int(CharacterSurface::Leather)
                           : s == int(CharacterSurface::Shirt) && y > .44f;
    };
    using Key = std::array<long long, 3>;
    std::map<Key, unsigned> welded;
    std::vector<unsigned> canonical(m.bind_mesh.vertices.size(), ~0u);
    for (std::size_t i = 0; i < canonical.size(); ++i)
      if (choose(m.bind_mesh.vertices[i])) {
        const auto p = m.bind_mesh.vertices[i].position * (1 / m.rig.height);
        canonical[i] =
            welded
                .emplace(Key{std::llround(p.x * 1e7), std::llround(p.y * 1e7),
                             std::llround(p.z * 1e7)},
                         welded.size())
                .first->second;
      }
    std::map<std::pair<unsigned, unsigned>, std::pair<unsigned, int>> edges;
    for (std::size_t t = 0; t < m.bind_mesh.indices.size(); t += 3) {
      if (canonical[m.bind_mesh.indices[t]] == ~0u)
        continue;
      for (int c = 0; c < 3; ++c) {
        auto a = canonical[m.bind_mesh.indices[t + c]],
             b = canonical[m.bind_mesh.indices[t + (c + 1) % 3]];
        require(a != ~0u && b != ~0u && a != b,
                "Finite layer classifier/triangle changed");
        const int direction = a < b ? 1 : -1;
        if (a > b)
          std::swap(a, b);
        auto &e = edges[{a, b}];
        ++e.first;
        e.second += direction;
      }
    }
    require(!edges.empty(), "Finite layer is missing");
    unsigned bad = 0;
    for (const auto &e : edges)
      if (e.second.first != 2 || e.second.second != 0) {
        ++bad;
        if (bad < 25) {
          std::cout << "bad layer=" << region << " count=" << e.second.first
                    << " orientation=" << e.second.second << " points ";
          for (const auto &kv : welded)
            if (kv.second == e.first.first || kv.second == e.first.second)
              std::cout << kv.first[0] << "," << kv.first[1] << ","
                        << kv.first[2] << " / ";
          std::cout << std::endl;
        }
      }
    require(!bad, label(m) + " finite layer " + std::to_string(region) +
                      " has " + std::to_string(bad) +
                      " open, nonmanifold or misoriented edges");
  }
}
struct MetroContacts {
  const CharacterModel &m;
  struct Triangle {
    std::size_t t;
    int group;
  };
  std::vector<Triangle> triangles;
  std::vector<std::pair<unsigned, unsigned>> seams;
  explicit MetroContacts(const CharacterModel &model) : m(model) {
    const auto &mesh = m.bind_mesh;
    const float h = m.rig.height;
    using Key = std::tuple<int, long long, long long, long long>;
    std::map<Key, unsigned> first;
    for (unsigned i = 0; i < mesh.vertices.size(); ++i) {
      const auto &v = mesh.vertices[i];
      const auto p = v.position * (1 / h);
      const int s = surface(v.uv);
      if (s != int(CharacterSurface::Shirt) &&
          s != int(CharacterSurface::Pants))
        continue;
      auto result =
          first.emplace(Key{s, std::llround(p.x * 1e7), std::llround(p.y * 1e7),
                            std::llround(p.z * 1e7)},
                        i);
      if (!result.second &&
          length(v.position - mesh.vertices[result.first->second].position) <
              h * 1e-7f)
        seams.emplace_back(result.first->second, i);
    }
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      const auto a = mesh.vertices[mesh.indices[t]],
                 b = mesh.vertices[mesh.indices[t + 1]],
                 c = mesh.vertices[mesh.indices[t + 2]];
      const auto center =
          (a.position + b.position + c.position) * (1 / (3 * h));
      const int s = surface(a.uv);
      int group = -1;
      if (s == int(CharacterSurface::Pants) && center.y > -.405f &&
          center.y < -.062f)
        group = 0;
      if (s == int(CharacterSurface::Shirt) && center.y > -.05f &&
          center.y < .375f)
        group = 1;
      if ((s == int(CharacterSurface::Face) ||
           s == int(CharacterSurface::Skin)) &&
          center.y > .335f && center.y < .385f)
        group = 2;
      if (s == int(CharacterSurface::Jacket))
        group = 3;
      if (s == int(CharacterSurface::Skin) && center.y < .025f)
        group = 4;
      if ((s == int(CharacterSurface::Leather) ||
           s == int(CharacterSurface::Rubber)) &&
          center.y < -.38f)
        group = 5;
      if (s == int(CharacterSurface::Pants) && center.y < -.38f)
        group = 6;
      if (group >= 0)
        triangles.push_back({t, group});
    }
    require(!seams.empty() && triangles.size() > 1000,
            "Model-specific contact regions are populated");
  }
  void check(const Mesh &mesh, const std::string &context) const {
    for (auto pair : seams)
      require(length(mesh.vertices[pair.first].position -
                     mesh.vertices[pair.second].position) <
                  m.rig.height * 2e-7f,
              context + " sewn seam opens");
    struct Item {
      Triangle triangle;
      MetroBounds b;
    };
    std::vector<Item> items;
    items.reserve(triangles.size());
    for (auto t : triangles) {
      Item item{t, {}};
      for (int c = 0; c < 3; ++c)
        item.b.add(mesh.vertices[mesh.indices[t.t + c]].position);
      items.push_back(item);
    }
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
      return a.b.low.x < b.b.low.x;
    });
    for (std::size_t a = 0; a < items.size(); ++a)
      for (std::size_t b = a + 1;
           b < items.size() && items[b].b.low.x <= items[a].b.high.x; ++b) {
        auto ga = items[a].triangle.group, gb = items[b].triangle.group;
        if (ga > gb)
          std::swap(ga, gb);
        const bool selected = (ga == 0 && gb == 0) || (ga == 1 && gb == 1) ||
                              (ga == 1 && gb == 2) || (ga == 1 && gb == 3) ||
                              (ga == 1 && gb == 4) || (ga == 0 && gb == 4) ||
                              (ga == 5 && gb == 6);
        if (!selected || !items[a].b.overlaps(items[b].b))
          continue;
        if (metro_crossing(mesh, items[a].triangle.t, items[b].triangle.t,
                           m.rig.height)) {
          for (auto t : {items[a].triangle.t, items[b].triangle.t}) {
            std::cout << "cross triangle=" << t / 3;
            for (int c = 0; c < 3; ++c) {
              const auto i = mesh.indices[t + c];
              const auto p =
                  m.bind_mesh.vertices[i].position * (1 / m.rig.height);
              std::cout << " bind " << p.x << "," << p.y << "," << p.z;
            }
            std::cout << std::endl;
          }
          throw std::runtime_error(
              context + " strict contact crossing groups " +
              std::to_string(ga) + "/" + std::to_string(gb) + " triangles " +
              std::to_string(items[a].triangle.t / 3) + "/" +
              std::to_string(items[b].triangle.t / 3));
        }
      }
  }
};

void metro_scalp_coverage(const CharacterModel &m) {
  struct T {
    std::size_t t;
    MetroBounds b;
  };
  const auto &mesh = m.bind_mesh;
  const float h = m.rig.height;
  std::vector<T> skin, cover, hair;
  using Point = std::array<float, 3>;
  using Edge = std::pair<Point, Point>;
  std::map<Edge, unsigned> edges;
  const auto point = [](Vec3 p) { return Point{p.x, p.y, p.z}; };
  const auto vec = [](Point p) { return Vec3{p[0], p[1], p[2]}; };
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    const auto &a = mesh.vertices[mesh.indices[t]];
    const int s = surface(a.uv);
    const bool is_hair = s == int(CharacterSurface::HairCrop),
               is_cap =
                   s == int(CharacterSurface::Shirt) && a.position.y > .44f * h;
    if (s != int(CharacterSurface::Face) && !is_hair && !is_cap)
      continue;
    T q{t, {}};
    for (int c = 0; c < 3; ++c)
      q.b.add(mesh.vertices[mesh.indices[t + c]].position);
    if (s == int(CharacterSurface::Face))
      skin.push_back(q);
    else
      cover.push_back(q);
    if (is_hair) {
      hair.push_back(q);
      for (int c = 0; c < 3; ++c) {
        auto a = point(mesh.vertices[mesh.indices[t + c]].position),
             b = point(mesh.vertices[mesh.indices[t + (c + 1) % 3]].position);
        if (b < a)
          std::swap(a, b);
        ++edges[{a, b}];
      }
    }
  }
  std::vector<std::pair<Vec3, Vec3>> boundary;
  float low = h;
  for (auto &e : edges) {
    require(e.second <= 2, "Hair underlay is manifold");
    if (e.second == 1) {
      const auto a = vec(e.first.first), b = vec(e.first.second);
      boundary.emplace_back(a, b);
      low = std::min({low, a.y, b.y});
    }
  }
  require(boundary.size() >= 32 && !cover.empty(),
          "Actual cap/hair and their open roots exist");
  unsigned crossings = 0;
  for (auto a : skin)
    if (a.b.high.y >= low)
      for (auto b : cover)
        if (a.b.overlaps(b.b) && metro_crossing(mesh, a.t, b.t, h))
          ++crossings;
  require(!crossings, label(m) + " cap/hair pierces source scalp at " +
                          std::to_string(crossings) + " triangle pairs");
  const auto ray = [&](Vec3 origin, Vec3 direction) {
    float far = -1.f;
    for (auto t : cover) {
      if (origin.y < t.b.low.y - 1e-7f * h || origin.y > t.b.high.y + 1e-7f * h)
        continue;
      const auto a = mesh.vertices[mesh.indices[t.t]].position,
                 b = mesh.vertices[mesh.indices[t.t + 1]].position,
                 c = mesh.vertices[mesh.indices[t.t + 2]].position;
      const auto e1 = b - a, e2 = c - a, p = cross(direction, e2);
      const float d = dot(e1, p);
      if (std::fabs(d) < h * h * 1e-12f)
        continue;
      const auto o = origin - a, q = cross(o, e1);
      const float u = dot(o, p) / d, v = dot(direction, q) / d,
                  z = dot(e2, q) / d;
      if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f)
        far = std::max(far, z);
    }
    return far;
  };
  float minimum_gap = 1.f;
  unsigned samples = 0, missing = 0;
  for (auto t : skin)
    if (t.b.high.y >= low) {
      const auto a = mesh.vertices[mesh.indices[t.t]].position,
                 b = mesh.vertices[mesh.indices[t.t + 1]].position,
                 c = mesh.vertices[mesh.indices[t.t + 2]].position;
      for (int i = 0; i <= 4; ++i)
        for (int j = 0; j <= 4 - i; ++j) {
          const auto p = a + (b - a) * (i / 4.f) + (c - a) * (j / 4.f);
          if (p.y < low)
            continue;
          const Vec3 origin{metro_watch_anatomy::head_center.x * h, p.y,
                            metro_watch_anatomy::head_center.z * h};
          const auto d = p - origin;
          const float radius = length(d);
          if (radius < 1e-6f * h)
            continue;
          float root = h;
          for (auto e : boundary) {
            const auto a = e.first - origin, b = e.second - e.first;
            const float determinant = d.x * b.z - d.z * b.x;
            if (std::fabs(determinant) < h * h * 1e-12f)
              continue;
            const float u = (a.x * d.z - a.z * d.x) / determinant,
                        along = (a.x * b.z - a.z * b.x) / determinant;
            if (u >= -1e-5f && u <= 1.00001f && along > 0)
              root = std::min(root, e.first.y + u * b.y);
          }
          if (p.y < root + 1e-5f * h)
            continue;
          const float distance = ray(origin, d * (1 / radius));
          ++samples;
          const float gap = (distance - radius) / h;
          minimum_gap = std::min(minimum_gap, gap);
          if (distance < 0 || gap < 0)
            ++missing;
        }
    }
  require(
      samples > 1000,
      "Scalp coverage must test actual vertices, edges and triangle interiors");
  require(!missing,
          label(m) + " cap/hair misses " + std::to_string(missing) +
              " source samples; gap/h=" + std::to_string(minimum_gap));
  std::cout << label(m) << ": " << samples
            << " actual scalp samples, min covering radial gap/h="
            << minimum_gap << ", zero skin crossings\n";
}

void metro_contact_classification_controls(const CharacterModel &m) {
  MetroContacts audit(m);
  audit.check(m.bind_mesh, label(m) + " positive control");
  for (auto pair : {std::pair<int, int>{1, 3}, {1, 4}, {5, 6}, {0, 0}}) {
    std::size_t target = ~std::size_t(0), moving = target;
    float largest = 0;
    for (auto item : audit.triangles) {
      if (item.group == pair.second && moving == ~std::size_t(0))
        moving = item.t;
      if (item.group != pair.first)
        continue;
      const auto
          a = m.bind_mesh.vertices[m.bind_mesh.indices[item.t]].position,
          b = m.bind_mesh.vertices[m.bind_mesh.indices[item.t + 1]].position,
          c = m.bind_mesh.vertices[m.bind_mesh.indices[item.t + 2]].position;
      const float area = length(cross(b - a, c - a));
      if (area > largest) {
        largest = area;
        target = item.t;
      }
    }
    require(target != ~std::size_t(0) && moving != ~std::size_t(0) &&
                target != moving,
            "Every contact class has independent control triangles");
    Mesh injected = m.bind_mesh;
    const auto a = injected.vertices[injected.indices[target]].position,
               b = injected.vertices[injected.indices[target + 1]].position,
               c = injected.vertices[injected.indices[target + 2]].position;
    const auto center = (a + b + c) * (1.f / 3),
               normal = normalize(cross(b - a, c - a)),
               along = normalize(b - a);
    const float size =
        std::min(length(b - a), std::min(length(c - a), length(c - b))) * .1f;
    const std::array<Vec3, 3> positions{
        center - normal * size, center + normal * size + along * size * .3f,
        center + along * size + normal * size};
    for (int corner = 0; corner < 3; ++corner) {
      auto vertex = injected.vertices[injected.indices[moving + corner]];
      vertex.position = positions[corner];
      injected.indices[moving + corner] = injected.vertices.size();
      injected.vertices.push_back(vertex);
    }
    bool detected = false;
    try {
      audit.check(injected, "injected contact");
    } catch (const std::exception &e) {
      detected = std::string(e.what()).find("strict contact crossing") !=
                 std::string::npos;
    }
    require(detected, "A selected garment/hand/boot/knee crossing escaped its "
                      "negative control");
  }
  std::cout << label(m)
            << ": four injected contact-class crossing controls rejected\n";
}
