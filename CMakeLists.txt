cmake_minimum_required(VERSION 3.16)
project(BuraiVibeGame CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)

include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS} ${SDL2_MIXER_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} src/main.cpp src/stage.cpp
    src/player_sprite.cpp
    src/effect_sprite.cpp
    src/player_sprite_contract.cpp
    src/multi_part_entity.cpp
    src/boss_entity.cpp
    src/orbital_shield_enemy.cpp
    src/orbital_enemy_field.cpp
    src/orbital_sprite.cpp
    src/oam_sdl_bridge.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${SDL2_LIBRARIES} 
    ${SDL2_IMAGE_LIBRARIES} 
    ${SDL2_TTF_LIBRARIES} 
    ${SDL2_MIXER_LIBRARIES}
    mingw32
    SDL2main
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/enemy_tests.cpp")
    add_executable(EnemyTests tests/enemy_tests.cpp src/enemy.cpp)
    target_compile_definitions(EnemyTests PRIVATE SDL_MAIN_HANDLED)
    target_link_libraries(EnemyTests PRIVATE ${SDL2_LIBRARIES} mingw32)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/orbital_shield_tests.cpp")
    add_executable(OrbitalShieldTests tests/orbital_shield_tests.cpp
        src/orbital_shield_enemy.cpp src/multi_part_entity.cpp)
endif()

add_executable(OamTileTests tests/oam_tile_tests.cpp
    src/oam_sdl_bridge.cpp src/orbital_sprite.cpp src/orbital_enemy_field.cpp
    src/orbital_shield_enemy.cpp src/multi_part_entity.cpp)
target_link_libraries(OamTileTests PRIVATE ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} mingw32)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# テスト実行時も最新アセットを使えるようにする
foreach(test_target EnemyTests OamTileTests BgmTests)
    if(TARGET ${test_target})
        add_custom_command(TARGET ${test_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/assets
                $<TARGET_FILE_DIR:${test_target}>/assets)
    endif()
endforeach()