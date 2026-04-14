// swift-tools-version:5.9
//
// VSAnalyzer — Apple M4 기반 차세대 비디오 분석기의 macOS 네이티브
// 셸. 이 SwiftPM manifest 는 다음을 선언한다:
//
//   1) VSACodec  — 기존 VSA_GUI 리포의 `src/codec/` 순수 C11 라이브
//      러리를 Swift 에서 바로 사용할 수 있는 C target 으로 노출.
//      SwiftPM 의 `.target(..., path: "Sources/VSACodec")` 이
//      `Sources/VSACodec/*.c`(+include) 를 C11 로 컴파일하고,
//      `publicHeadersPath` 가 가리키는 디렉터리의 헤더를 module
//      로 export 한다. 코드 중복을 피하기 위해 실제 소스는
//      `../src/codec/` 의 파일들을 `Sources/VSACodec/` 쪽에서
//      symlink 또는 copy 하는 것이 아니라, `exclude`/`sources` 를
//      명시적으로 써서 상위 디렉터리의 파일을 참조한다. SwiftPM
//      은 패키지 루트 바깥을 컴파일 소스로 직접 지정할 수 없으므
//      로, `Sources/VSACodec/` 에 thin shim (`vsa_codec_umbrella.c`
//      + `include/VSACodec.h`) 만 두고 실제 빌드는 `../src/codec/`
//      의 파일들을 `sources` 파라미터의 절대 경로로 가리킬 수 없
//      다는 제약 때문에 우선 **copy** 전략으로 간다. Phase I 에
//      서는 SwiftPM build script 또는 `prebuild plugin` 으로 매
//      빌드마다 `../src/codec/` 를 `Sources/VSACodec/` 로 sync 하
//      는 방식이 실용적. 프로젝트 크기가 작아서 과잉은 아님.
//
//   2) VSAnalyzer — `@main` SwiftUI App. VSACodec 에 의존한다.
//
// 빌드:
//     cd mac
//     swift build
//     swift run VSAnalyzer
//
// 첫 Phase I 의 목표는 "File → Open → tiny_clip.h264 → SPS 필드
// 가 UI 에 뜬다" 까지이며, VideoToolbox 디코드 / Metal 렌더링은
// Phase I.2 이후에 들어간다.

import PackageDescription

let package = Package(
    name: "VSAnalyzer",
    platforms: [
        .macOS(.v13)        // Ventura 이상 (SwiftUI NavigationSplitView, Observable 등)
    ],
    products: [
        .executable(name: "VSAnalyzer", targets: ["VSAnalyzer"]),
        .library(name: "VSACodec", targets: ["VSACodec"])
    ],
    targets: [
        // ---- C11 codec library ----
        //
        // Sources live at the same path the CMake build uses
        // (../src/codec). SwiftPM cannot reference sources above the
        // package root directly, so the directory is a *symlink* in
        // the repository (`mac/Sources/VSACodec/codec -> ../../../src/codec`).
        // `publicHeadersPath = "include"` exposes our umbrella header
        // to Swift via a generated module map.
        .target(
            name: "VSACodec",
            path: "Sources/VSACodec",
            exclude: [],
            sources: ["codec"],
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("."),
                .headerSearchPath("codec"),
                .define("_POSIX_C_SOURCE", to: "200809L"),
                .unsafeFlags(["-std=c11"], .when(configuration: .debug)),
                .unsafeFlags(["-std=c11", "-O2"], .when(configuration: .release))
            ]
        ),

        // ---- Swift application ----
        .executableTarget(
            name: "VSAnalyzer",
            dependencies: ["VSACodec"],
            path: "Sources/VSAnalyzer",
            swiftSettings: [
                .unsafeFlags(["-parse-as-library"]),
                .define("VSA_PHASE_I")
            ]
        )
    ]
)
