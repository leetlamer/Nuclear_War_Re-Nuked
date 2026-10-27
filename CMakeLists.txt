cmake_minimum_required(VERSION 3.20)
project(
  NuclearWarReNuked
  VERSION 1.0.0
  LANGUAGES CXX)
include(CTest)
find_package(SDL2 2.0.22 CONFIG REQUIRED)
find_package(PNG 1.6 REQUIRED)
option(NWR_ENABLE_UBSAN "Enable undefined-behavior checks (Clang/GCC)" OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NWR_ENABLE_UBSAN AND NOT MSVC)
  add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all)
  add_link_options(-fsanitize=undefined -fno-sanitize-recover=all)
endif()
add_library(
  nuclear_game
  src/game/game_rules.cpp
  src/game/scoring.cpp
  src/game/turn_plan.cpp
  src/game/ai/ai_controller.cpp
  src/game/combat/bomber_navigation.cpp
  src/game/combat/resolver_markers.cpp
  src/game/combat/turn_resolver.cpp
  src/game/combat/missile_attack.cpp
  src/game/turn_manager.cpp
  src/ui/human_commands.cpp
  src/ui/action_messages.cpp
  src/core/game_text.cpp
  src/ui/player_controller.cpp)
target_include_directories(nuclear_game PUBLIC include src
                                               "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(nuclear_game PUBLIC cxx_std_17)
add_library(nuclear_resources src/platform/resources.cpp)
target_include_directories(nuclear_resources PUBLIC src)
target_compile_definitions(nuclear_resources PUBLIC SDL_MAIN_HANDLED)
if(WIN32)
  target_compile_definitions(nuclear_resources PUBLIC NOMINMAX
                                                      WIN32_LEAN_AND_MEAN)
endif()
target_link_libraries(nuclear_resources PUBLIC SDL2::SDL2 PNG::PNG)
file(
  COPY assets
  DESTINATION "${CMAKE_BINARY_DIR}"
  PATTERN ".DS_Store" EXCLUDE)
add_executable(nuclearwar src/platform/application.cpp)
target_link_libraries(nuclearwar PRIVATE nuclear_game nuclear_resources
                                         SDL2::SDL2 PNG::PNG)
if(TARGET SDL2::SDL2main)
  target_link_libraries(nuclearwar PRIVATE SDL2::SDL2main)
endif()
if(BUILD_TESTING)
  add_executable(state_trace EXCLUDE_FROM_ALL tools/deterministic_trace.cpp)
  target_link_libraries(state_trace PRIVATE nuclear_game)
  add_executable(render_frame_trace EXCLUDE_FROM_ALL
                 tools/render_frame_trace.cpp)
  target_link_libraries(render_frame_trace PRIVATE nuclear_game
                                                   nuclear_resources)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test_output")
  add_executable(nuclearwar_replay src/platform/application.cpp)
  target_compile_definitions(nuclearwar_replay PRIVATE NWR_WITH_REPLAY=1)
  target_link_libraries(nuclearwar_replay PRIVATE nuclear_game
                                                  nuclear_resources)
  function(game_test name source)
    add_executable(${name} ${source} ${ARGN})
    target_include_directories(${name} PRIVATE tests/fixtures)
    target_link_libraries(${name} PRIVATE nuclear_game nuclear_resources
                                          SDL2::SDL2 PNG::PNG)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(
      ${name} PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/test_output"
                         ENVIRONMENT "SDL_VIDEODRIVER=dummy")
  endfunction()
  foreach(name engine_tests ending_tests resolver_marker_tests round_plan_tests)
    game_test(${name} tests/engine/${name}.cpp)
  endforeach()
  game_test(session_tests tests/session/session_tests.cpp)
  game_test(resolver_tests tests/resolver/resolver_core_tests.cpp)
  game_test(missile_tests tests/resolver/single_missile_tests.cpp)
  game_test(text_tests tests/core/game_text_tests.cpp)
  game_test(player_tests tests/frontend/tests.cpp)
  game_test(revenge_tests tests/frontend/revenge_tests.cpp)
  game_test(audio_tests tests/frontend/audio_tests.cpp)
  game_test(render_tests tests/frontend/render_tests.cpp)
  game_test(emoji_renderer_tests tests/frontend/emoji_renderer_tests.cpp)
  game_test(legacy_prototype_tests tests/support/legacy_prototype/tests.cpp
            tests/support/legacy_prototype/game.cpp)
  game_test(missile_demo_tests tests/support/missile_demo/tests.cpp
            tests/support/missile_demo/model.cpp)
endif()

# A normal configure/build never needs Python, original binaries, or a source
# workspace.
function(nwr_verify_file relative expected)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${relative}")
    message(FATAL_ERROR "Missing packaged data: ${relative}")
  endif()
  file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${relative}" actual)
  if(NOT actual STREQUAL expected)
    message(
      FATAL_ERROR
        "Stale generated data/asset: ${relative}. Run python3 tools/generate_data.py after intentional edits."
    )
  endif()
endfunction()
include(cmake/GeneratedData.cmake)
foreach(target nuclearwar nuclearwar_replay)
  if(TARGET ${target})
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        ${CMAKE_COMMAND}
        "-DNWR_ASSET_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "-DNWR_ASSET_DEST=$<TARGET_FILE_DIR:${target}>/assets" -P
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyAssets.cmake")
  endif()
endforeach()
if(MSVC)
  target_compile_options(nuclear_game PRIVATE /W4 /utf-8)
  target_compile_options(nuclear_resources PRIVATE /W4 /utf-8)
else()
  target_compile_options(nuclear_game PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(nuclear_resources PRIVATE -Wall -Wextra -Wpedantic)
endif()
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(Python3_FOUND)
  add_custom_target(
    validate_assets
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_assets.py")
  if(BUILD_TESTING)
    add_test(NAME asset_validation
             COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_assets.py")
  endif()
endif()
include(GNUInstallDirs)
install(TARGETS nuclearwar RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  DIRECTORY assets/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/nuclearwar/assets
  PATTERN ".DS_Store" EXCLUDE)
install(FILES README.md BUILDING.md ARCHITECTURE.md CONTRIBUTING.md CHANGELOG.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/nuclearwar)
set(CPACK_PACKAGE_NAME "NuclearWar-ReNuked")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
set(CPACK_SOURCE_GENERATOR "ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    "/[.]git/;/[.]migration/;/build[^/]*/;/dist/;/_CPack_Packages/;[.]DS_Store$;[.]zip$"
)
include(CPack)
