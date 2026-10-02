cmake_minimum_required(VERSION 3.18)
project(facet-display-power VERSION 0.2.0 LANGUAGES CXX)
set(DP_VERSION_SUFFIX "alpha")  # pre-release tag; empty for final releases; keep manifest.json in sync

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(DP_STATIC "Link fully static (portable release binaries)" OFF)

if(DP_VERSION_SUFFIX)
    set(DP_VERSION "${PROJECT_VERSION}-${DP_VERSION_SUFFIX}")
else()
    set(DP_VERSION "${PROJECT_VERSION}")
endif()

# The plugin SDK lives in facet-core: a local checkout next to this project
# (or FACET_CORE_DIR) is used when present, otherwise it is downloaded.
set(FACET_CORE_DIR "" CACHE PATH "facet-core checkout providing the SDK (default: ../facet-core, else download)")
set(FACET_CORE_REF "v0.4.0-alpha" CACHE STRING "facet-core git ref to download the SDK from")
if(NOT FACET_CORE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../facet-core/sdk/CMakeLists.txt")
    set(FACET_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../facet-core")
endif()
if(FACET_CORE_DIR)
    add_subdirectory(${FACET_CORE_DIR}/sdk ${CMAKE_BINARY_DIR}/facet_sdk)
else()
    include(FetchContent)
    FetchContent_Declare(facet_core
        GIT_REPOSITORY https://github.com/siakinnik/facet-core.git
        GIT_TAG ${FACET_CORE_REF}
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR sdk)
    FetchContent_MakeAvailable(facet_core)
endif()

add_library(dp_logic STATIC src/policy.cpp src/presence.cpp)
target_include_directories(dp_logic PUBLIC src)
target_link_libraries(dp_logic PUBLIC facet_sdk)

add_executable(display-power src/main.cpp src/camera_v4l2.cpp src/watcher.cpp src/i18n/ru.cpp)
target_link_libraries(display-power PRIVATE dp_logic pthread)
target_compile_options(display-power PRIVATE -Wall -Wextra)
target_compile_definitions(display-power PRIVATE DP_VERSION="${DP_VERSION}")
if(DP_STATIC)
    target_link_options(display-power PRIVATE -static -s)
endif()

# The build dir doubles as a plugin dir: FACET_PLUGIN_PATH=<build>
set(PLUGIN_OUT ${CMAKE_BINARY_DIR}/display-power.plugin)
set_target_properties(display-power PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PLUGIN_OUT})
facet_plugin_manifest(${CMAKE_CURRENT_SOURCE_DIR}/manifest.json ${PLUGIN_OUT}/manifest.json)

# Dev tool: live camera view on the framebuffer (not installed).
add_executable(camview tools/camview.cpp src/camera_v4l2.cpp)
target_include_directories(camview PRIVATE src)
target_link_libraries(camview PRIVATE facet_sdk)

enable_testing()
add_executable(logic_test tests/logic_test.cpp)
target_link_libraries(logic_test PRIVATE dp_logic)
add_test(NAME logic COMMAND logic_test)

install(TARGETS display-power RUNTIME DESTINATION lib/facet/plugins/display-power)
install(FILES ${PLUGIN_OUT}/manifest.json DESTINATION lib/facet/plugins/display-power)
