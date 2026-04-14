//
//  VSAnalyzerApp.swift
//
//  @main entry point for the Apple M4-native VSAnalyzer app. Phase I
//  ships with the minimum SwiftUI scaffold needed to prove the
//  C-codec bridge works: a single-window document-style app with
//  File → Open… and a two-pane layout (file tree / parser result).
//
//  Deliberately minimal — VideoToolbox decoding, Metal rendering,
//  JM hook integration and instant-scrub timeline all land in later
//  Phase I substeps (I.2 / I.3) and Phases J‑O. This file only
//  sets up the window and routes the Open action to a view model
//  that calls into VSACodec.
//

import SwiftUI

@main
struct VSAnalyzerApp: App {
    @StateObject private var viewModel = AnalyzerViewModel()

    var body: some Scene {
        WindowGroup("VSAnalyzer") {
            ContentView()
                .environmentObject(viewModel)
                .frame(minWidth: 720, minHeight: 480)
        }
        .commands {
            // File → Open… replaces the default "New Document" item
            // with an explicit opener. The .openFile() receiver on
            // the main view reads the URL and dispatches to the
            // view model, keeping the UI layer free of parser code.
            CommandGroup(replacing: .newItem) {
                Button("Open…") {
                    viewModel.requestOpenDialog()
                }
                .keyboardShortcut("O", modifiers: .command)
            }
        }
    }
}
