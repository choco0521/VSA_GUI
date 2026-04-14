# VSAnalyzer (macOS native)

Apple M4 기반 네이티브 비디오 스트림 분석기의 SwiftUI / SwiftPM 타겟.
기존 `src/codec/` C11 라이브러리를 그대로 재사용하고, Phase I 에서는
단순히 **파일을 열어 파싱된 SPS 정보를 창에 표시** 하는 첫 바이너리를
제공합니다. VideoToolbox / Metal / JM hook 등 고급 기능은 Phase J
이후에 점차 추가됩니다.

## 빌드 요구사항

- macOS 13 Ventura 이상
- Xcode 15 이상 (Swift 5.9+)
- 또는 standalone Swift toolchain 5.9+ 가 PATH 에 있어야 `swift` CLI 사용 가능

## 빌드 및 실행

```bash
cd mac
swift build
swift run VSAnalyzer
```

위 명령은 다음을 수행합니다:

1. `Sources/VSACodec/` 의 C11 소스를 컴파일 (`src/codec/*.c` 를 symlink
   로 참조합니다 — `mac/Sources/VSACodec/codec -> ../../../src/codec`)
2. `Sources/VSAnalyzer/` 의 Swift 파일을 컴파일
3. `@main` 엔트리 `VSAnalyzerApp` 을 실행해 SwiftUI 윈도우를 띄움

## 사용

1. 앱이 열리면 왼쪽 사이드바의 **Open H.264 file…** 버튼 또는 `⌘O`
2. raw H.264 Annex B 파일을 선택 (예: 저장소에 포함된
   `tests/fixtures/tiny_clip.h264`)
3. 오른쪽 패널에 파싱된 SPS 정보 + NAL breakdown 이 즉시 표시

### tiny_clip 기대값

`tests/fixtures/tiny_clip.h264` 는 libx264 baseline profile 1.0
64×48 8 프레임 (2 IDR + 6 P) 샘플입니다. 화면에 표시되어야 할 값:

| Field | Value |
|---|---|
| Profile | Constrained Baseline |
| Level | 1.0 |
| Chroma format | 4:2:0 |
| Bit depth (luma) | 8 |
| Resolution | 64 × 48 |
| MB grid | 4 × 3 |
| frame_mbs_only | yes |
| pic_order_cnt_type | 2 |
| VUI present | yes |
| Total NAL units | 13 |
| SPS / PPS | 2 / 2 |
| IDR / Non-IDR | 2 / 6 |
| SEI / AUD | 1 / 0 |

값이 일치하지 않으면 `src/codec/` 의 버전과 `mac/Sources/VSACodec/codec`
symlink 가 같은 커밋을 가리키고 있는지 먼저 확인하세요.

## Phase I 범위

포함:
- SwiftPM 패키지 매니페스트
- C ↔ Swift module map (`VSACodec` target)
- SwiftUI App + ContentView + StreamSummary 뷰
- H264BridgeParser (C API 래퍼)
- File → Open… 배선, fileImporter, 에러 표시

미포함 (다음 Phase):
- **Phase I.2** — VideoToolbox H.264 디코드 + MTKView 실제 프레임
- **Phase J** — JM ldecod hook 으로 per-MB QP/MV 실시간 추출
- **Phase K** — mmap 기반 background indexer + 타임라인
- **Phase L** — Metal QP heatmap + MV flow 오버레이
- **Phase M** — HEVC (VideoToolbox + HM hook)
- **Phase N** — AV1 (dav1d) + VVC (VVdeC + VTM)
- **Phase O** — Bitstream hex viewer

## 디렉터리 구조

```
mac/
├── Package.swift                      SwiftPM manifest
├── README.md                          (this file)
└── Sources/
    ├── VSACodec/                      C11 target (SwiftPM wrapper)
    │   ├── codec -> ../../../src/codec    symlink to real C sources
    │   └── include/
    │       └── VSACodec.h             umbrella header exported to Swift
    └── VSAnalyzer/                    Swift executable target
        ├── VSAnalyzerApp.swift        @main entry
        ├── Codec/
        │   ├── AnalyzerViewModel.swift  ObservableObject glue
        │   └── H264BridgeParser.swift   unsafe/extern-C confinement
        └── UI/
            └── ContentView.swift      SwiftUI pane layout + StreamSummaryView
```

## 공유 라이브러리와의 관계

`src/codec/` 의 C 소스는 **세 개의 서로 다른 빌드 시스템** 이 각자
컴파일합니다:

1. **CMake** (프로젝트 루트) — Linux CI + 기존 Qt `vsa_gui` + doctest
2. **SwiftPM** (`mac/Package.swift`) — macOS 네이티브 VSAnalyzer
3. **Xcode workspace** (향후 Phase J/L 에서 추가 가능) — Metal / Interface Builder

세 빌드 시스템 모두 동일한 C 소스를 컴파일하므로, 한쪽에서 새 파서
(`h265_sps.c` 등) 를 추가할 때는 **세 곳 모두** 의 빌드 리스트에 등록
해야 합니다:

- CMake: `CMakeLists.txt` 의 `VSA_CODEC_SOURCES` 리스트
- SwiftPM: `mac/Sources/VSACodec/include/VSACodec.h` umbrella header 에
  새 헤더 추가
- Xcode: 해당 파일을 Build Phases / Compile Sources 에 drag-and-drop

SwiftPM 은 `Sources/VSACodec/codec/` 아래의 모든 `.c` 를 자동으로
빌드 대상에 포함하므로 소스 파일 추가만으로 따라오지만, **헤더는
umbrella 에 명시적으로 추가** 해야 Swift 쪽에서 보입니다.

## 알려진 한계 / Phase I 현재 상태

- 이 빌드는 Linux 샌드박스에서 검증할 수 없습니다 (AppKit/SwiftUI
  는 macOS 전용). Phase I 코드는 모두 **blind-written** 이며 사용
  자의 mac 에서 첫 빌드에 컴파일 오류가 발생할 가능성이 있습니다.
  발생 시 에러 메시지를 공유해 주시면 원격에서 수정합니다.
- `fileImporter` 가 raw `.h264` 파일을 인식하기 위해 `UTType(filenameExtension:)`
  에 의존합니다. macOS 13 에서 h264 확장자가 기본 등록돼 있지 않으면
  "All Files" 로 열어야 할 수 있습니다.
- 앱은 sandboxed 아님 (Phase I 기준). App Store 배포 시 entitlements
  설정은 별도 Phase.
