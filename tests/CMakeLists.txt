# 查找 GTest 库
find_package(GTest REQUIRED)
find_package(raylib REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# 创建测试可执行文件
# 注意：这里必须包含你需要测试的源文件 (如 Game.cpp, Ball.cpp)，
# 这样测试程序才能链接到 Ball 和 Game 的函数定义。
add_executable(runTests 
    test_collision.cpp 
    ../Game.cpp 
    ../Ball.cpp 
    ../Paddle.cpp 
    ../Brick.cpp
)

# 包含头文件路径 (指向你的 include 目录)
target_include_directories(runTests PRIVATE ../include)

# 链接 GTest 库
# Windows下通常链接 GTest::gtest_main，Linux下可能需要 pthread
target_link_libraries(runTests ${GTEST_LIBRARIES} raylib gtest_main pthread)