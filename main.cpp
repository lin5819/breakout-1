#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>
#include <iostream> // 用于输出游戏结束信息（可选）

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "打砖块2D - 带生命值系统");

    // 创建游戏对象
    Ball ball({400, 300}, {3, 3}, 10);
    Paddle paddle(350, 550, 100, 20);
    
    // --- 新增：生命值系统 ---
    int lives = 5; 

    // 创建砖块（示例：一排5个）
    std::vector<Brick> bricks;
    float brickWidth = 90;
    float brickHeight = 30;
    for(int i1=0; i1<5; i1++){
        for (int i = 0; i < 7; i++) {
            bricks.emplace_back(20 + i * 110, 50+i1*50, brickWidth, brickHeight);
        }
    }

    SetTargetFPS(60);
    int bricksRemaining = bricks.size();
    
    // --- 修改：循环条件增加了 lives > 0 ---
    while (!WindowShouldClose() && lives > 0 && bricksRemaining > 0) { 
        ball.Move();
    ball.BounceEdge(screenWidth, screenHeight); // 这个函数目前只处理左右和上边界

    // 板移动
    if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(5);
    if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(5);

    // --- 新增：球与板子的碰撞检测 ---
    // 1. 获取球的当前位置和半径
        // --- 球与板子的碰撞检测 (优化版) ---
    
    // 1. 获取数据
    Vector2 ballPos = ball.GetPosition();
    float ballRadius = ball.GetRadius();
    Vector2 ballSpeed = ball.GetSpeed(); // 假设你添加了 GetSpeed，或者直接用 public 变量
    Rectangle paddleRect = paddle.GetRect();

    // 2. 基础碰撞检测：球和板子是否发生重叠
    if (CheckCollisionCircleRec(ballPos, ballRadius, paddleRect)) {
        
        // 条件 A: 球必须在板子的上方 (球的底部 y + 半径 >= 板子的顶部 y)
        // 注意：这里允许一点点的穿透，只要球心还在板子之上或附近
        bool isAbovePaddle = (ballPos.y + ballRadius) >= paddleRect.y;
        
        // 条件 B: 球必须是向下运动的 (速度的 y 分量 > 0)
        bool isMovingDown = ballSpeed.y > 0;

        // 只有同时满足：碰撞发生 + 球在板子上方 + 球向下运动 -> 才反弹
        if (isAbovePaddle && isMovingDown) {
            ball.ReverseYSpeed(); // 垂直速度反向
            
            // 可选优化：强制将球修正到板子上方，防止粘连
            // ball.SetY(paddleRect.y - ballRadius); 
        }
    }
        // --- 球与砖块的碰撞检测 ---
    
    // 1. 计算球上一帧的位置 (用于判断撞击方向)
    float prevX = ball.GetPosition().x - ball.GetSpeed().x;
    float prevY = ball.GetPosition().y - ball.GetSpeed().y;
    float radius = ball.GetRadius();

    // 2. 遍历所有砖块
    for (int i = 0; i < bricks.size(); i++) {
        // 只检测还活着的砖块
        if (bricks[i].IsActive()) {
            
            // 3. 检测圆(球)与矩形(砖块)是否重叠
            if (CheckCollisionCircleRec(ball.GetPosition(), radius, bricks[i].GetRect())) {
                
                // 4. 标记砖块消失
                bricks[i].SetActive(false);
                bricksRemaining--; 

                // 5. 计算反弹方向
                // 逻辑：看球上一帧是在砖块的“侧面”还是“上面/下面”
                
                // 判断是否在水平方向上发生了碰撞（即上一帧球在砖块左边或右边）
                bool hitSide = (prevX + radius <= bricks[i].GetRect().x) || 
                               (prevX - radius >= bricks[i].GetRect().x + bricks[i].GetRect().width);
                
                if (hitSide) {
                    ball.ReverseXSpeed(); // 碰到左右侧，水平反弹
                } else {
                    ball.ReverseYSpeed(); // 碰到上下侧，垂直反弹
                }
                
                // 注意：这里没有 break，允许一帧内撞碎多个紧贴的砖块（可选）
            }
        }
    }
        if (ball.GetPosition().y > screenHeight) { // 假设 Ball 类有 GetPosition() 或直接访问 position
            lives--; // 生命值减一
            if (lives > 0) {
                // 重置球的位置（仅在还有命时重置，否则停留在屏幕外等待游戏结束）
                ball.Reset({400, 300}, {2, 2}); // 需要在 Ball 类中添加 Reset 函数
                // 或者直接赋值：ball.position = {400, 300}; ball.speed = {2, 2};
            }
        }

        // --- 绘制逻辑 ---
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        ball.Draw();
        paddle.Draw();
        
        // 绘制砖块
        for (auto& brick : bricks) if (brick.IsActive()) brick.Draw();

        // --- 新增：绘制UI（生命值） ---
        DrawText(TextFormat("HP: %i", lives), 10, 10, 20, RED);
        DrawText(TextFormat("breaks left: %i", bricksRemaining), 600, 10, 20, GREEN);

        EndDrawing();
    }

    // --- 新增：游戏结束画面 ---
    if (lives <= 0) {
        // 简单的控制台输出，你也可以绘制一个全屏画面
        std::cout << "Game End,your score is" << std::endl; 
        // 这里可以加一个暂停，等待用户关闭窗口
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("game end", 300, 250, 40, RED);
            DrawText("push ESC to quit", 320, 320, 20, WHITE);
            EndDrawing();
        }
    }
    else if (bricksRemaining <= 0) {
        // --- 新增：游戏胜利逻辑 ---
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(GOLD); // 胜利背景用金色
            DrawText("win!", 280, 250, 50, DARKGREEN);
            DrawText("all completed!", 260, 320, 20, DARKGREEN);
            DrawText("push ESC to quit", 320, 380, 20, BLACK);
            EndDrawing();
        }
    }

    CloseWindow();
    return 0;
}