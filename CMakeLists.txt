cmake_minimum_required(VERSION 3.16)
project(VolchayCleans VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg SvgWidgets Concurrent Multimedia Network)

set(SOURCES
    src/main.cpp
    src/audio/SoundEngine.cpp
    src/core/SizeFormatter.cpp
    src/core/FileType.cpp
    src/core/ScanWorker.cpp
    src/core/HashWorker.cpp
    src/core/DriveInfo.cpp
    src/i18n/I18n.cpp
    src/models/FileTableModel.cpp
    src/models/FolderTreeModel.cpp
    src/models/FolderFilterProxyModel.cpp
    src/ui/ThemeManager.cpp
    src/ui/SpinnerWidget.cpp
    src/ui/RingIndicator.cpp
    src/ui/UsageBar.cpp
    src/ui/Sidebar.cpp
    src/ui/MainWindow.cpp
    src/ui/OverviewView.cpp
    src/ui/DriveCard.cpp
    src/ui/AnalyzerView.cpp
    src/ui/FilesView.cpp
    src/ui/DetailsPanel.cpp
    src/ui/FilterChips.cpp
    src/ui/SettingsView.cpp
    src/ui/RadarOverlay.cpp
    src/ui/RgbStripe.cpp
    src/ui/RgbGlowOverlay.cpp
    src/ui/ToggleSwitch.cpp
    src/platform/ShellOps.cpp
)

set(HEADERS
    src/audio/SoundEngine.h
    src/core/SizeFormatter.h
    src/core/FileType.h
    src/core/ScanResult.h
    src/core/ScanWorker.h
    src/core/HashWorker.h
    src/core/DriveInfo.h
    src/i18n/I18n.h
    src/models/FileTableModel.h
    src/models/FolderTreeModel.h
    src/ui/ThemeManager.h
    src/ui/SpinnerWidget.h
    src/ui/RingIndicator.h
    src/ui/Sidebar.h
    src/ui/MainWindow.h
    src/ui/OverviewView.h
    src/ui/DriveCard.h
    src/ui/AnalyzerView.h
    src/ui/FilesView.h
    src/ui/DetailsPanel.h
    src/ui/FilterChips.h
    src/ui/SettingsView.h
    src/ui/RadarOverlay.h
    src/ui/RgbStripe.h
    src/ui/RgbGlowOverlay.h
    src/ui/ToggleSwitch.h
    src/platform/ShellOps.h
)

set(RESOURCES
    resources/resources.qrc
)

if(WIN32)
    list(APPEND RESOURCES windows/app.rc)
endif()

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
endif()

qt_add_executable(VolchayCleans WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_include_directories(VolchayCleans PRIVATE src)
target_link_libraries(VolchayCleans PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::Network
)

if(WIN32)
    # shlwapi: StrRetToBufW; oleaut32: VariantChangeType/Init/Clear used by
    # ShellOps' restoreFromTrash().
    target_link_libraries(VolchayCleans PRIVATE
        shell32 ole32 oleaut32 advapi32 shlwapi)
    if(MSVC)
        # We embed our own manifest via windows/app.rc — disable MSVC's
        # auto-generated one to avoid duplicate manifest resources at link time
        # (CVT1100 / LNK1123).
        target_link_options(VolchayCleans PRIVATE "/MANIFEST:NO")
    endif()
endif()

# When cross-compiling with the host's qt6-base 6.2.4 rcc against the
# pre-built MinGW Qt 6.2.4 (e.g. installed via aqtinstall), rcc emits a
# reference to qResourceFeatureZstd() that the MinGW libQt6Core does not
# export.  We don't use Zstd resource compression, so a tiny stub
# satisfies the link and returns 0.
if(CMAKE_CROSSCOMPILING AND WIN32)
    set(_zstd_stub "${CMAKE_BINARY_DIR}/qrc_zstd_stub.cpp")
    file(WRITE "${_zstd_stub}"
"// Auto-generated stub — see CMakeLists.txt.\n"
"// rcc emits a *C++-linkage* call to qResourceFeatureZstd(),\n"
"// so we must NOT wrap this in extern \"C\".\n"
"unsigned char qResourceFeatureZstd() { return 0; }\n")
    target_sources(VolchayCleans PRIVATE "${_zstd_stub}")
endif()

set_target_properties(VolchayCleans PROPERTIES
    OUTPUT_NAME "VolchayCleans"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

install(TARGETS VolchayCleans
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(WIN32 AND COMMAND qt_generate_deploy_app_script)
    qt_generate_deploy_app_script(
        TARGET VolchayCleans
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()
