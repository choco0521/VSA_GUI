//
//  ContentView.swift
//
//  Phase I top-level UI. Two panes:
//    - left    : file info / open button / error banner
//    - right   : StreamSummary displayed as a grouped list of rows
//
//  Everything is SwiftUI. No AppKit escape hatches yet. No Metal,
//  no VideoToolbox, no timeline, no decode — those live in later
//  phases. The goal here is purely to prove the C ↔ Swift bridge
//  works end-to-end: click Open, pick tiny_clip.h264, see the same
//  SPS fields the Qt GUI already shows.
//

import SwiftUI
import UniformTypeIdentifiers

struct ContentView: View {
    @EnvironmentObject private var viewModel: AnalyzerViewModel

    var body: some View {
        NavigationSplitView {
            sidebar
        } detail: {
            detail
        }
        .navigationTitle("VSAnalyzer")
        // SwiftUI's file importer is driven by a Bool. The view model
        // flips the flag from the File → Open… command; the importer
        // then calls back with a URL (or an error) that we route to
        // the view model for parsing.
        .fileImporter(
            isPresented: $viewModel.isOpenDialogRequested,
            allowedContentTypes: Self.allowedContentTypes,
            allowsMultipleSelection: false
        ) { result in
            switch result {
            case .success(let urls):
                if let url = urls.first {
                    viewModel.openFile(at: url)
                }
            case .failure(let error):
                viewModel.errorMessage = error.localizedDescription
            }
        }
    }

    // MARK: - Panes

    private var sidebar: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("VSAnalyzer")
                .font(.title2.bold())
            Text("Phase I — macOS native shell")
                .font(.caption)
                .foregroundStyle(.secondary)

            Divider()

            Button {
                viewModel.requestOpenDialog()
            } label: {
                Label("Open H.264 file…", systemImage: "doc.badge.plus")
            }
            .buttonStyle(.borderedProminent)
            .keyboardShortcut("O", modifiers: .command)

            if let err = viewModel.errorMessage {
                Text(err)
                    .font(.caption)
                    .foregroundStyle(.red)
                    .padding(.top, 4)
            }

            Spacer()

            Text("libvsa_codec \(viewModel.codecVersion)")
                .font(.caption2)
                .foregroundStyle(.secondary)
        }
        .padding()
        .frame(minWidth: 220)
    }

    @ViewBuilder
    private var detail: some View {
        if let summary = viewModel.summary {
            StreamSummaryView(summary: summary)
        } else {
            ContentUnavailableView(
                "No file loaded",
                systemImage: "film.stack",
                description: Text("Choose Open… from the File menu or the button on the left.")
            )
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }

    // MARK: - File type filter

    /// UTTypes for the file importer. Raw H.264 Annex B has no
    /// registered UTType, so we lean on the file extension-based
    /// fallback that `UTType(filenameExtension:)` provides and
    /// include the generic `data` type so "All Files" still works.
    private static var allowedContentTypes: [UTType] {
        var types: [UTType] = [.data]
        for ext in ["h264", "264", "avc"] {
            if let t = UTType(filenameExtension: ext) {
                types.append(t)
            }
        }
        return types
    }
}

// MARK: - Detail view

private struct StreamSummaryView: View {
    let summary: StreamSummary

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                headline
                Divider()
                sequenceParameterSet
                Divider()
                nalCountsGrid
            }
            .padding()
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var headline: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(URL(fileURLWithPath: summary.filePath).lastPathComponent)
                .font(.title2.bold())
                .lineLimit(1)
                .truncationMode(.middle)
            Text(summary.filePath)
                .font(.caption)
                .foregroundStyle(.secondary)
                .lineLimit(1)
                .truncationMode(.middle)
            Text("\(summary.fileSizeBytes) bytes")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    private var sequenceParameterSet: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Sequence Parameter Set")
                .font(.headline)
            Grid(alignment: .leading, horizontalSpacing: 24, verticalSpacing: 4) {
                row("Profile",             summary.profileName)
                row("Level",               summary.levelString)
                row("Chroma format",       summary.chromaFormat)
                row("Bit depth (luma)",    "\(summary.bitDepthLuma)")
                row("Resolution",          summary.resolutionString)
                row("MB grid",             "\(summary.picWidthInMbs) × \(summary.picHeightInMapUnits)")
                row("frame_mbs_only",      summary.frameMbsOnly ? "yes" : "no")
                row("pic_order_cnt_type",  "\(summary.picOrderCntType)")
                row("VUI present",         summary.vuiPresent ? "yes" : "no")
            }
        }
    }

    private var nalCountsGrid: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("NAL breakdown")
                .font(.headline)
            Grid(alignment: .leading, horizontalSpacing: 24, verticalSpacing: 4) {
                row("Total NAL units",   "\(summary.nalCount)")
                row("SPS",               "\(summary.spsCount)")
                row("PPS",               "\(summary.ppsCount)")
                row("IDR slices",        "\(summary.idrCount)")
                row("Non-IDR slices",    "\(summary.nonIdrSliceCount)")
                row("SEI",               "\(summary.seiCount)")
                row("AUD",               "\(summary.audCount)")
            }
        }
    }

    private func row(_ label: String, _ value: String) -> some View {
        GridRow {
            Text(label)
                .foregroundStyle(.secondary)
                .gridColumnAlignment(.leading)
            Text(value)
                .fontDesign(.monospaced)
        }
    }
}

// MARK: - Preview

#Preview {
    let vm = AnalyzerViewModel()
    vm.summary = StreamSummary(
        filePath: "/tmp/tiny_clip.h264",
        fileSizeBytes: 4_869,
        profileName: "Constrained Baseline",
        levelString: "1.0",
        chromaFormat: "4:2:0",
        bitDepthLuma: 8,
        pictureWidth: 64,
        pictureHeight: 48,
        frameMbsOnly: true,
        picOrderCntType: 2,
        picWidthInMbs: 4,
        picHeightInMapUnits: 3,
        vuiPresent: true,
        nalCount: 13,
        spsCount: 2,
        ppsCount: 2,
        idrCount: 2,
        nonIdrSliceCount: 6,
        seiCount: 1,
        audCount: 0
    )
    return ContentView()
        .environmentObject(vm)
        .frame(width: 800, height: 500)
}
