cmake_minimum_required(VERSION 3.25)
project(CaeciliaNotchIsland VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Caecilia requires Windows and MSVC. No automatic dependency downloads.")
endif()

add_executable(CaeciliaNotchIsland WIN32
    src/main.cpp
    src/app/App.cpp
    src/app/Config.cpp
    src/window/OverlayWindow.cpp
    src/render/Renderer.cpp
    src/render/IslandGeometry.cpp
    src/render/Painter.cpp
    src/render/TextStyles.cpp
    src/data/DemoSource.cpp
    src/data/MediaSource.cpp
    src/motion/Spring.cpp
    src/state/StateMachine.cpp
    src/input/HoverTracker.cpp
)
target_compile_features(CaeciliaNotchIsland PRIVATE cxx_std_20)
set_target_properties(CaeciliaNotchIsland PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(CaeciliaNotchIsland PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_compile_options(CaeciliaNotchIsland PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
target_include_directories(CaeciliaNotchIsland PRIVATE src)
target_link_libraries(CaeciliaNotchIsland PRIVATE
    d3d11 dxgi d2d1 dcomp dwrite dxguid uuid user32 gdi32 ole32 shell32 runtimeobject windowsapp windowscodecs)

# M2 uses SDK C++/WinRT projections. This optional local override never fetches packages.
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
    add_executable(CaeciliaMotionTests tests/MotionTests.cpp src/motion/Spring.cpp
        src/state/StateMachine.cpp src/input/HoverTracker.cpp)
    target_compile_features(CaeciliaMotionTests PRIVATE cxx_std_20)
    target_compile_definitions(CaeciliaMotionTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(CaeciliaMotionTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    target_include_directories(CaeciliaMotionTests PRIVATE src)
    add_test(NAME motion COMMAND CaeciliaMotionTests)
    add_executable(CaeciliaConfigTests tests/ConfigTests.cpp src/app/Config.cpp
        src/input/HoverTracker.cpp src/render/TextStyles.cpp)
    target_compile_features(CaeciliaConfigTests PRIVATE cxx_std_20)
    target_compile_definitions(CaeciliaConfigTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(CaeciliaConfigTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    target_include_directories(CaeciliaConfigTests PRIVATE src)
    target_link_libraries(CaeciliaConfigTests PRIVATE dwrite ole32 shell32 uuid runtimeobject)
    add_test(NAME config COMMAND CaeciliaConfigTests)
    add_executable(CaeciliaRenderTests tests/RenderTests.cpp src/render/Painter.cpp
        src/render/IslandGeometry.cpp src/render/TextStyles.cpp src/state/StateMachine.cpp)
    target_compile_features(CaeciliaRenderTests PRIVATE cxx_std_20)
    target_compile_definitions(CaeciliaRenderTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(CaeciliaRenderTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    target_include_directories(CaeciliaRenderTests PRIVATE src)
    target_link_libraries(CaeciliaRenderTests PRIVATE d3d11 dxgi d2d1 dwrite windowscodecs dxguid uuid user32 ole32)
    add_test(NAME render COMMAND CaeciliaRenderTests "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/m1-validation/render")
    add_executable(CaeciliaMediaTests tests/MediaTests.cpp src/data/MediaSource.cpp)
    target_compile_features(CaeciliaMediaTests PRIVATE cxx_std_20)
    target_compile_definitions(CaeciliaMediaTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(CaeciliaMediaTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    target_include_directories(CaeciliaMediaTests PRIVATE src)
    target_link_libraries(CaeciliaMediaTests PRIVATE windowsapp windowscodecs ole32)
    add_test(NAME media COMMAND CaeciliaMediaTests)
    find_program(NODE_EXECUTABLE node)
    if(NODE_EXECUTABLE)
        add_test(NAME spec-consistency COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify-spec.cjs")
    endif()
endif()

install(TARGETS CaeciliaNotchIsland RUNTIME DESTINATION .)
