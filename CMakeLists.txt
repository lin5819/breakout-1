cmake_minimum_required(VERSION 3.10)
project(breakout_week2)

find_package(raylib REQUIRED)

add_executable(breakout_week2
    main.cpp
    Ball.cpp
    Paddle.cpp
    Brick.cpp
    game.cpp
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ENET REQUIRED libenet)
target_include_directories(breakout_week2 PRIVATE ${ENET_INCLUDE_DIRS})
target_link_libraries(breakout_week2 ${ENET_LIBRARIES})

target_link_libraries(breakout_week2 raylib)

option(BUILD_TESTS "Build the unit tests" ON)

if(BUILD_TESTS)
  #  enable_testing() # 启用 CTest
  #  add_subdirectory(tests) # 引入 tests 目录下的配置
endif()