// File: Game.cpp
#include "game.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <iostream>

Game::Game(int width, int height) 
    : screenWidth(width), screenHeight(height), running(true), 
      lives(5), bricksRemaining(0), ball(nullptr), paddle(nullptr) {
    // 构造函数主要进行参数初始化
}

Game::~Game() {
    delete ball;
    delete paddle;
    // vector 会自动释放
}

void Game::InitGame() {
    // 初始化窗口
    SetTargetFPS(60);
    InitWindow(screenWidth, screenHeight, "Refactored Brick Breaker");
    
    // 初始化对象
    ball = new Ball({(float)screenWidth/2, (float)screenHeight/2}, {3, 3}, 10);
    paddle = new Paddle(350, 550, 100, 20); // 假设初始位置

    // 初始化砖块
    float brickWidth = 90;
    float brickHeight = 30;
    bricks.clear(); // 确保清空
    
    for(int row = 0; row < 5; row++) {
        for (int col = 0; col < 7; col++) {
            bricks.emplace_back(20 + col * 110, 50 + row * 50, brickWidth, brickHeight);
        }
    }
    bricksRemaining = bricks.size();
}

void Game::ProcessInput() {
    if (IsKeyDown(KEY_LEFT)) paddle->MoveLeft(5);
    if (IsKeyDown(KEY_RIGHT)) paddle->MoveRight(5);
    if (IsKeyDown(KEY_ESCAPE)) running = false; // 退出游戏
}

void Game::CheckCollisions() {
    Vector2 ballPos = ball->GetPosition();
    float ballRadius = ball->GetRadius();
    Vector2 ballSpeed = ball->GetSpeed();
    Rectangle paddleRect = paddle->GetRect();

    // 1. 球与挡板碰撞
    if (CheckCollisionCircleRec(ballPos, ballRadius, paddleRect)) {
        bool isAbovePaddle = (ballPos.y + ballRadius) >= paddleRect.y;
        bool isMovingDown = ballSpeed.y > 0;
        
        if (isAbovePaddle && isMovingDown) {
            ball->ReverseYSpeed();
        }
    }

    // 2. 球与砖块碰撞
    float prevX = ballPos.x - ballSpeed.x;
    float prevY = ballPos.y - ballSpeed.y;
    
    for (auto& brick : bricks) {
        if (!brick.IsActive()) continue;

        if (CheckCollisionCircleRec(ballPos, ballRadius, brick.GetRect())) {
            brick.SetActive(false);
            bricksRemaining--;

            // 判断碰撞方向
            bool hitSide = (prevX + ballRadius <= brick.GetRect().x) || 
                          (prevX - ballRadius >= brick.GetRect().x + brick.GetRect().width);
            
            if (hitSide) ball->ReverseXSpeed();
            else ball->ReverseYSpeed();
        }
    }

    // 3. 球掉落检测 (生命值逻辑)
    if (ballPos.y > screenHeight) {
        lives--;
        if (lives > 0) {
            ball->Reset({(float)screenWidth/2, (float)screenHeight/2}, {3, 3});
        } else {
            running = false; // 没命了，结束循环
        }
    }
}

void Game::UpdateGame() {
    ball->Move();
    ball->BounceEdge(screenWidth, screenHeight); // 处理墙壁反弹
    CheckCollisions();
}

void Game::Render() {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    ball->Draw();
    paddle->Draw();
    
    // 绘制砖块
    for (auto& brick : bricks) {
        if (brick.IsActive()) brick.Draw();
    }

    // 绘制UI
    DrawText(TextFormat("HP: %i", lives), 10, 10, 20, RED);
    DrawText(TextFormat("Bricks: %i", bricksRemaining), 600, 10, 20, GREEN);

    EndDrawing();
    if (lives <= 0) {
        std::cout << "Game Over!" << std::endl;
        // 这里可以调用一个专门的 ShowGameOverScreen() 函数
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("GAME OVER", 300, 250, 40, RED);
            EndDrawing();
        }
    } else if (bricksRemaining <= 0) {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(GOLD);
            DrawText("YOU WIN!", 300, 250, 50, DARKGREEN);
            EndDrawing();
        }
    }
}

