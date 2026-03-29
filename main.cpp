#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "打砖块2D - 第二周");

    // 创建游戏对象
    Ball ball({400, 300}, {2, 2}, 10);
    Paddle paddle(350, 550, 100, 20);

    // 创建砖块（示例：一排5个）
    std::vector<Brick> bricks;
    float brickWidth = 90;
    float brickHeight = 30;
    for(int i1=0;i1<5;i1++){
    for (int i = 0; i <7; i++) {
        bricks.emplace_back(30 + i * 110, 20+i1*50, brickWidth, brickHeight);
    }
    }
    // 绘制左右墙为灰色矩形
    DrawRectangle(0, 0, 5, screenHeight, GRAY);   // 左墙宽5像素
    DrawRectangle(screenWidth-5, 0, 5, screenHeight, GRAY); // 右墙
    // 绘制天花板和地板
    DrawRectangle(0, 0, screenWidth, 5, GRAY);    // 顶墙高5像素
    DrawRectangle(0, screenHeight-5, screenWidth, 5, GRAY); // 底墙

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // 更新
        ball.Move();
        ball.BounceEdge(screenWidth, screenHeight);

        // 板移动
        if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(5);
        if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(5);

        // 绘制
        BeginDrawing();
        ClearBackground(RAYWHITE);

        ball.Draw();
        paddle.Draw();
        for (auto& brick : bricks) brick.Draw();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}