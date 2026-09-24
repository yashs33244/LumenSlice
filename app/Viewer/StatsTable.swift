import SwiftUI

// The Quantify tab's canvas: a table of per-segment measurements. Reads the
// StatisticsModel the Quantify controls populate; shows an empty state until the
// first measurement and a "stale" banner when the mask changed since. Geometry is
// converted from mm to the friendlier cm here (the CSV export keeps raw mm).
struct StatsTable: View {
    @EnvironmentObject var model: VolumeModel
    @EnvironmentObject var stats: StatisticsModel

    var body: some View {
        ZStack {
            Color(nsColor: .textBackgroundColor).opacity(0.4)
            if !model.hasVolume {
                placeholder("Open a DICOM folder to measure segments.",
                            systemImage: "tablecells")
            } else if stats.rows.isEmpty {
                placeholder(stats.isComputing
                                ? "Measuring…"
                                : "Measure segments to see volume, surface area, "
                                    + "and HU statistics.",
                            systemImage: "function")
            } else {
                VStack(spacing: 0) {
                    if stats.isStale {
                        staleBanner
                    }
                    ScrollView([.vertical, .horizontal]) {
                        table
                            .padding(16)
                    }
                }
            }
        }
    }

    private var staleBanner: some View {
        HStack(spacing: 6) {
            Image(systemName: "exclamationmark.triangle.fill")
            Text("The segmentation changed since these were measured. "
                 + "Measure again to refresh.")
                .font(.caption)
            Spacer()
        }
        .padding(.horizontal, 14)
        .padding(.vertical, 8)
        .background(Color.orange.opacity(0.18))
        .foregroundStyle(.orange)
    }

    private var table: some View {
        Grid(alignment: .leading, horizontalSpacing: 22, verticalSpacing: 8) {
            GridRow {
                header("Segment")
                header("Voxels")
                header("Volume (cm³)")
                header("Surface (cm²)")
                header("HU mean")
                header("HU σ")
                header("HU range")
            }
            Divider().gridCellColumns(7)
            ForEach(stats.rows) { row in
                GridRow {
                    HStack(spacing: 8) {
                        Circle().fill(row.color).frame(width: 12, height: 12)
                        Text(row.name)
                    }
                    numeric(row.voxelCount.formatted())
                    numeric(cubicCentimetres(row.volumeMM3))
                    numeric(squareCentimetres(row.surfaceAreaMM2))
                    numeric(hu(row.huMean))
                    numeric(hu(row.huStdDev))
                    numeric("\(hu(row.huMin)) – \(hu(row.huMax))")
                }
            }
        }
        .font(.system(.body, design: .rounded))
    }

    private func header(_ text: String) -> some View {
        Text(text)
            .font(.caption.weight(.semibold))
            .foregroundStyle(.secondary)
    }

    private func numeric(_ text: String) -> some View {
        Text(text).monospacedDigit()
    }

    private func placeholder(_ text: String, systemImage: String) -> some View {
        VStack(spacing: 12) {
            Image(systemName: systemImage)
                .font(.system(size: 46, weight: .thin))
                .foregroundStyle(.secondary)
            Text(text)
                .font(.callout)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
        }
        .padding()
    }

    // mm^3 -> cm^3, mm^2 -> cm^2, HU rounded to whole units.
    private func cubicCentimetres(_ mm3: Double) -> String {
        String(format: "%.2f", mm3 / 1000.0)
    }
    private func squareCentimetres(_ mm2: Double) -> String {
        String(format: "%.2f", mm2 / 100.0)
    }
    private func hu(_ v: Double) -> String {
        String(format: "%.0f", v)
    }
}
