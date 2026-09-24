// StatsShot - headless statistics calibration on a real DICOM folder.
//
// Loads a folder, thresholds a HU band into one label, and prints the label-map
// measures plus the closed-surface measures across a sweep of smoothing levels, so
// the closed-surface smoothing can be calibrated against a reference (3D Slicer)
// without the GUI. Not shipped in the app; a dev/calibration tool.
//
//   swift run -c release StatsShot <dicom_folder> <lowHU> <highHU>

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "bridge/mesh_crop.hpp"
#include "io/dicom_loader.h"
#include "segmentation/label_volume.hpp"
#include "segmentation/marching_cubes.hpp"
#include "segmentation/statistics.hpp"

using namespace lumen;

int main(int argc, char** argv) {
    if (argc < 4) {
        std::printf("usage: StatsShot <dicom_folder> <lowHU> <highHU>\n");
        return 1;
    }
    const char* folder = argv[1];
    const float lo = static_cast<float>(std::atof(argv[2]));
    const float hi = static_cast<float>(std::atof(argv[3]));

    LoadResult r = LoadDicomFolder(folder);
    if (!r.ok) {
        std::printf("load failed: %s\n", r.message.c_str());
        return 1;
    }
    const Volume& vol = r.volume;
    std::printf("loaded %dx%dx%d  spacing %.4f/%.4f/%.4f mm  HU[%.0f..%.0f]\n",
                vol.width, vol.height, vol.depth, vol.spacing_x, vol.spacing_y,
                vol.spacing_z, vol.hu_min, vol.hu_max);

    // Threshold the HU band into label 1 (matches the app's Bone-preset segment).
    LabelVolume mask;
    mask.reset_to(vol);
    for (int z = 0; z < vol.depth; ++z)
        for (int y = 0; y < vol.height; ++y)
            for (int x = 0; x < vol.width; ++x) {
                const float hu = vol.voxel_buffer[vol.index(x, y, z)];
                if (hu >= lo && hu <= hi) mask.set(x, y, z, 1);
            }

    const SegmentStats s = compute_label_stats(mask, vol, 1);
    std::printf("\nLabel map (LM):\n");
    std::printf("  voxels        = %ld\n", s.voxel_count);
    std::printf("  volume (LM)   = %.2f cm3  (%.4e mm3)\n", s.volume_mm3 / 1000.0,
                s.volume_mm3);
    std::printf("  voxel-face SA = %.2f cm2  (%.4e mm2)\n",
                s.surface_area_mm2 / 100.0, s.surface_area_mm2);
    std::printf("  HU mean=%.1f  sd=%.1f  range=%.0f..%.0f\n", s.hu_mean,
                s.hu_stddev, s.hu_min, s.hu_max);

    // Closed surface at smooth 0 for the requested band, plus a light-smoothing
    // area sweep. The CS volume being far below the LM volume for a complex band is
    // the marching-cubes thin-structure collapse; the diagnostic below tests a
    // SOLID band to confirm MC volume is faithful when the region is not thin.
    std::vector<std::uint8_t> field;
    auto measure_cs = [&](const LabelVolume& m, int si) {
        const auto reg = lumen_bridge_detail::crop_label_region(
            m, [](std::uint8_t v) { return v == 1; }, field);
        Mesh mesh;
        const int tris = marching_cubes(field.data(), reg.w, reg.h, reg.d,
                                        vol.spacing_x, vol.spacing_y,
                                        vol.spacing_z, si, 1, mesh);
        const MeshMetrics mm = compute_mesh_metrics(mesh);
        std::printf("    smooth=%2d : CS vol=%8.2f cm3   CS surface=%9.2f cm2   tris=%d\n",
                    si, mm.volume_mm3 / 1000.0, mm.surface_area_mm2 / 100.0, tris);
    };
    std::printf("\nClosed surface for the requested band:\n");
    for (int si : {0, 1, 2, 4}) measure_cs(mask, si);
    std::printf("(Slicer reference for this segment: CS vol 3060.72 cm3, "
                "CS surface 18462.10 cm2)\n");

    // DIAGNOSTIC: a large SOLID band (soft tissue + bone, HU >= -300) fills the body
    // interior as one thick mass. If MC volume is faithful, CS vol here should track
    // LM vol; if it also collapses, the bug is not thinness.
    LabelVolume solid;
    solid.reset_to(vol);
    for (int z = 0; z < vol.depth; ++z)
        for (int y = 0; y < vol.height; ++y)
            for (int x = 0; x < vol.width; ++x) {
                const float hu = vol.voxel_buffer[vol.index(x, y, z)];
                if (hu >= -300.0f) solid.set(x, y, z, 1);
            }
    const SegmentStats ss = compute_label_stats(solid, vol, 1);
    std::printf("\nDIAGNOSTIC solid band (HU >= -300):\n");
    std::printf("  LM vol=%.2f cm3  voxels=%ld\n", ss.volume_mm3 / 1000.0,
                ss.voxel_count);
    measure_cs(solid, 0);
    return 0;
}
