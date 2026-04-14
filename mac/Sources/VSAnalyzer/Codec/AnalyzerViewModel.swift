//
//  AnalyzerViewModel.swift
//
//  The view model that bridges SwiftUI to the pure-C parser
//  library. One instance is created by VSAnalyzerApp and injected
//  into every view via @EnvironmentObject. The view model is
//  deliberately thin right now: it holds the path of the currently
//  loaded file and a parsed `StreamSummary` value, plus a couple of
//  methods that run the parser synchronously on the main actor.
//
//  Phase I goal: "open tiny_clip.h264, see SPS fields in the UI."
//  Asynchronous parsing, progress reporting, error dialogs with
//  recovery options etc. all live in later phases; the point of
//  this file is to prove the Swift ↔ C bridge compiles and runs.
//

import AppKit
import Foundation
import Observation
import SwiftUI
import UniformTypeIdentifiers

import VSACodec

/// A snapshot of everything the parser surfaces to the UI for a
/// single H.264 elementary stream. Kept as a plain value type so
/// SwiftUI can diff it cheaply and rebuild views on change.
struct StreamSummary: Equatable {
    var filePath: String
    var fileSizeBytes: UInt64

    // ---- SPS fields ----
    var profileName: String
    var levelString: String        // e.g. "1.0", "3.1"
    var chromaFormat: String
    var bitDepthLuma: Int
    var pictureWidth: Int
    var pictureHeight: Int
    var frameMbsOnly: Bool
    var picOrderCntType: Int
    var picWidthInMbs: Int
    var picHeightInMapUnits: Int
    var vuiPresent: Bool

    // ---- Stream-wide counts (from Annex B scan + slice header pass) ----
    var nalCount: Int
    var spsCount: Int
    var ppsCount: Int
    var idrCount: Int
    var nonIdrSliceCount: Int
    var seiCount: Int
    var audCount: Int

    var resolutionString: String {
        "\(pictureWidth) × \(pictureHeight)"
    }
}

@MainActor
final class AnalyzerViewModel: ObservableObject {
    @Published var summary: StreamSummary?
    @Published var errorMessage: String?
    @Published var isOpenDialogRequested = false

    private let parser = H264BridgeParser()

    var codecVersion: String { parser.version }

    /// Called from the menu command. Flips a @Published flag that the
    /// top-level view observes via .fileImporter, which is SwiftUI's
    /// idiomatic way to chain NSOpenPanel through the view layer.
    func requestOpenDialog() {
        isOpenDialogRequested = true
    }

    /// Open the given file URL and parse it into `summary`. Errors
    /// land in `errorMessage` for display in the UI.
    func openFile(at url: URL) {
        errorMessage = nil
        do {
            let parsed = try parser.parse(path: url.path)
            summary = parsed
        } catch let bridgeError as H264BridgeParser.ParseError {
            summary = nil
            errorMessage = bridgeError.description
        } catch {
            summary = nil
            errorMessage = "Unexpected error: \(error.localizedDescription)"
        }
    }
}
