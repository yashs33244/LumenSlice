// LumenSlice - bridge-private label-region cropping, shared by the 3D snapshot and
// the statistics closed-surface path.
//
// Both need the same thing before marching cubes: the labelled region binarized to
// a 0/1 field, cropped to its bounding box plus a one-voxel zero margin (so the
// field has a closing border), together with the crop's origin in the full volume.
// Factoring it here keeps the two callers byte-for-byte identical and lets the stats
// path crop into its OWN buffer without disturbing the handle's live mesh snapshot.
//
// This header lives under src/bridge (not bridge/include): it is internal to the
// bridge translation units and pulls in a C++ core type (LabelVolume), so it never
// crosses the C language line.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "segmentation/label_volume.hpp"

namespace lumen_bridge_detail {

// Geometry of a crop: the cropped dimensions and the origin (in full-volume voxels)
// of the cropped box. w == 0 means nothing matched.
struct CropRegion {
    int w = 0, h = 0, d = 0;
    int ox = 0, oy = 0, oz = 0;
};

// Crop `mask` to the voxels for which keep(id) is true, binarized to 0/1 into `out`
// with a one-voxel zero margin (clamped to the volume). Returns the cropped
// geometry; when nothing matches, clears `out` and returns a zero region.
template <class KeepFn>
CropRegion crop_label_region(const lumen::LabelVolume& mask, KeepFn keep,
                             std::vector<std::uint8_t>& out) {
    const int W = mask.width(), H = mask.height(), D = mask.depth();
    const std::uint8_t* src = mask.data();

    // One linear pass to find the inclusive bounding box of kept voxels.
    int x0 = W, y0 = H, z0 = D, x1 = -1, y1 = -1, z1 = -1;
    for (int z = 0; z < D; ++z) {
        for (int y = 0; y < H; ++y) {
            const std::uint8_t* row =
                src + (static_cast<std::size_t>(z) * H + y) * W;
            for (int x = 0; x < W; ++x) {
                if (!keep(row[x])) continue;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
                if (z < z0) z0 = z;
                if (z > z1) z1 = z;
            }
        }
    }

    CropRegion r;
    if (x1 < 0) { // nothing labelled
        out.clear();
        return r;
    }

    // One-voxel margin (clamped to the volume) so the field has a zero border.
    x0 = std::max(0, x0 - 1);
    y0 = std::max(0, y0 - 1);
    z0 = std::max(0, z0 - 1);
    x1 = std::min(W - 1, x1 + 1);
    y1 = std::min(H - 1, y1 + 1);
    z1 = std::min(D - 1, z1 + 1);

    r.w = x1 - x0 + 1;
    r.h = y1 - y0 + 1;
    r.d = z1 - z0 + 1;
    r.ox = x0;
    r.oy = y0;
    r.oz = z0;
    out.resize(static_cast<std::size_t>(r.w) * r.h * r.d);

    std::size_t o = 0;
    for (int z = z0; z <= z1; ++z)
        for (int y = y0; y <= y1; ++y) {
            const std::uint8_t* row =
                src + (static_cast<std::size_t>(z) * H + y) * W;
            for (int x = x0; x <= x1; ++x)
                out[o++] = keep(row[x]) ? 1 : 0;
        }
    return r;
}

} // namespace lumen_bridge_detail
