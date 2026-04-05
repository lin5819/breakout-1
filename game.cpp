// File: Game.cpp
#include "game.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <iostream>

Game::Game(int width, int height) 
    : screenWidth(width), screenHeight(height), running(true), 
      lives(5), bricksRemaining(0), ball(nullptr), paddle(nullptr) {

         state = MENU;

         btnStart = { (float)screenWidth/2 - 100, 200, 200, 50 };
    btnExit = { (float)screenWidth/2 - 100, 300, 200, 50 };
    btnResume = { (float)screenWidth/2 - 100, 200, 200, 50 };
    btnPause = { 10, 10, 40, 40 }; // 屏幕左上角的暂停按钮
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
      if (IsKeyPressed(KEY_ESCAPE)) {
        if (state == PLAYING) state = PAUSED;
        else if (state == PAUSED) state = PLAYING;
    }

    switch (state) {
        case MENU:
            // 菜单输入处理
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                if (CheckCollisionPointRec(mouse, btnStart)) {
                    // 开始游戏，重置数据
                    InitGame(); 
                    state = PLAYING;
                } else if (CheckCollisionPointRec(mouse, btnExit)) {
                    running = false;
                }
            }
            break;
        case PAUSED:
            // 暂停菜单输入
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                if (CheckCollisionPointRec(mouse, btnResume)) {
                    state = PLAYING;
                } else if (CheckCollisionPointRec(mouse, btnExit)) {
                    state = MENU;
                }
            }
            break;
        case PLAYING:
            // 游戏中的移动逻辑 (原来的 ProcessInput 内容)
            if (IsKeyDown(KEY_LEFT)) paddle->MoveLeft(5);
            if (IsKeyDown(KEY_RIGHT)) paddle->MoveRight(5);
            break;
        case GAMEOVER:
            // 游戏结束输入
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                if (CheckCollisionPointRec(mouse, btnStart)) {
                    InitGame();
                    state = PLAYING;
                } else if (CheckCollisionPointRec(mouse, btnExit)) {
                    state = MENU;
                }
            }
            break;
    }
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
            // --- 修改：设置状态为 GAMEOVER，而不是设置 running = false ---
            state = GAMEOVER; 
            // 注意：不要在这里设置 running = false，否则主循环会退出，看不到结束画面
        }
    }
    // 检查是否胜利
if (bricksRemaining == 0) {
    state = GAMEOVER; // 或者可以设置一个 WIN 状态
    // 注意：如果要显示 "YOU WIN"，需要在 GAMEOVER 界面判断 lives > 0
}
}

void Game::UpdateGame() {
     if (state == PLAYING) {
        ball->Move();
        ball->BounceEdge(screenWidth, screenHeight);
        CheckCollisions(); // 这里会修改 state 为 GAMEOVER (如果 lives <= 0)
    }
}

void Game::Render() {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // 根据状态绘制画面
    switch (state) {
        case MENU:
            DrawMenu();
            break;
        case PLAYING:
            DrawPlaying();
            break;
        case PAUSED:
            DrawPlaying(); // 先绘制背后的游戏画面
            DrawPaused();  // 再绘制暂停UI
            break;
        case GAMEOVER:
            DrawPlaying(); // 如果你想在结束画面看到最终状态，否则可以 DrawGameOver()
            DrawGameOver();
            break;
    }

    EndDrawing();
}

void Game::DrawMenu() {
    // 绘制标题
    DrawText("BRICK BREAKER", 250, 100, 40, DARKBLUE);
    
    // 绘制按钮
    DrawRectangleRec(btnStart, LIGHTGRAY);
    DrawText("START GAME", 320, 215, 20, DARKBLUE);
    
    DrawRectangleRec(btnExit, PINK);
    DrawText("EXIT", 370, 315, 20, DARKBLUE);
    
    // 绘制提示
    DrawText("Click Buttons or Press ESC to Pause", 10, screenHeight - 30, 20, GRAY);
}

void Game::DrawPlaying() {
    // 绘制游戏元素
    ball->Draw();
    paddle->Draw();
    for (auto& brick : bricks) {
        if (brick.IsActive()) brick.Draw();
    }
    
    // 绘制UI
    DrawText(TextFormat("HP: %i", lives), 10, 10, 20, RED);
    DrawText(TextFormat("Bricks: %i", bricksRemaining), 600, 10, 20, GREEN);
    
    // --- 新增：绘制屏幕左上角的暂停按钮 ---
    DrawRectangleRec(btnPause, GRAY);
    DrawText("||", 25, 20, 30, WHITE);
}

void Game::DrawPaused() {
    // 半透明遮罩
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
    
    // 恢复按钮
    DrawRectangleRec(btnResume, YELLOW);
    DrawText("RESUME", 330, 215, 20, DARKBLUE);
    
    // 退出到菜单按钮
    DrawRectangleRec(btnExit, ORANGE);
    DrawText("MAIN MENU", 310, 315, 20, DARKBLUE);
}

void Game::DrawGameOver() {
    // 半透明遮罩
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.8f));
    
    if (lives <= 0) {
        DrawText("GAME OVER", 300, 150, 50, RED);
    } else {
        DrawText("YOU WIN!", 300, 150, 50, GOLD);
    }
    
    // 重新开始
    DrawRectangleRec(btnStart, GREEN);
    DrawText("RESTART", 340, 265, 20, DARKBLUE);
    
    // 返回菜单
    DrawRectangleRec(btnExit, BLUE);
    DrawText("MENU", 370, 365, 20, DARKBLUE);
}