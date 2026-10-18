cmake_minimum_required(VERSION 3.25)
project(CaeciliaNotchIsland VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Caecilia requires Windows and MSVC (VS 2022). No automatic dependency downloads.")
endif()

add_executable(CaeciliaNotchIsland WIN32
    src/main.cpp
    src/app/App.cpp
    src/window/OverlayWindow.cpp
    src/render/Renderer.cpp
    src/render/IslandGeometry.cpp
    src/render/Painter.cpp
    src/render/TextStyles.cpp
    src/data/DemoSource.cpp
)
target_compile_features(CaeciliaNotchIsland PRIVATE cxx_std_20)
set_target_properties(CaeciliaNotchIsland PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(CaeciliaNotchIsland PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_compile_options(CaeciliaNotchIsland PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
target_include_directories(CaeciliaNotchIsland PRIVATE src)
target_link_libraries(CaeciliaNotchIsland PRIVATE
    d3d11 dxgi d2d1 dcomp dwrite dxguid uuid user32 gdi32 ole32 shell32)

# M0 uses Windows SDK COM interfaces only. C++/WinRT is needed by the M2
# media implementation; this optional local path does not fetch packages.
set(CAECILIA_CPPWINRT_INCLUDE_DIR "" CACHE PATH "Offline C++/WinRT include folder (contains winrt/base.h)")
if(CAECILIA_CPPWINRT_INCLUDE_DIR)
    if(NOT EXISTS "${CAECILIA_CPPWINRT_INCLUDE_DIR}/winrt/base.h")
        message(FATAL_ERROR "CAECILIA_CPPWINRT_INCLUDE_DIR must contain winrt/base.h")
    endif()
    target_include_directories(CaeciliaNotchIsland SYSTEM PRIVATE "${CAECILIA_CPPWINRT_INCLUDE_DIR}")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(CaeciliaGeometryTests tests/GeometryTests.cpp src/render/IslandGeometry.cpp)
    target_compile_features(CaeciliaGeometryTests PRIVATE cxx_std_20)
    target_compile_definitions(CaeciliaGeometryTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(CaeciliaGeometryTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    target_include_directories(CaeciliaGeometryTests PRIVATE src)
    target_link_libraries(CaeciliaGeometryTests PRIVATE d2d1 user32 ole32)
    add_test(NAME geometry COMMAND CaeciliaGeometryTests)
    find_program(NODE_EXECUTABLE node)
    if(NODE_EXECUTABLE)
        add_test(NAME spec-consistency COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify-spec.cjs")
    endif()
endif()

install(TARGETS CaeciliaNotchIsland RUNTIME DESTINATION .)
