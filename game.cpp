// File: Game.cpp
#include "game.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <fstream>

using json = nlohmann::json;

void Game::LoadConfig() {
    std::ifstream file("config.json"); // 读取同级目录下的文件
    if (!file.is_open()) {
        std::cerr << "无法打开 config.json，使用默认值" << std::endl;
        // 即使文件打不开，构造函数中的默认值也会生效
        return;
    }

    try {
        json data;
        file >> data;

        // 读取 Window 配置
        config.screenWidth = data["window"]["width"];
        config.screenHeight = data["window"]["height"];
        config.title = data["window"]["title"];
        config.targetFps = data["window"]["fps"];

        // 读取 Game 配置
        config.initialLives = data["game"]["initial_lives"];

        // 读取 Ball 配置
        config.ballRadius = data["ball"]["radius"];
        config.ballStartSpeedX = data["ball"]["start_speed_x"];
        config.ballStartSpeedY = data["ball"]["start_speed_y"];

        // 读取 Paddle 配置
        config.paddleWidth = data["paddle"]["width"];
        config.paddleHeight = data["paddle"]["height"];
        config.paddleSpeed = data["paddle"]["speed"];

        // 读取 Bricks 配置
        config.brickRows = data["bricks"]["rows"];
        config.brickCols = data["bricks"]["cols"];
        config.brickWidth = data["bricks"]["width"];
        config.brickHeight = data["bricks"]["height"];
        config.brickSpacing = data["bricks"]["spacing"];

        std::cout << "配置加载成功!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "配置解析错误: " << e.what() << std::endl;
    }
    file.close();
}

Game::Game(int width, int height) 
    : screenWidth(width), screenHeight(height), running(true)
    , bricksRemaining(0), ball(nullptr), paddle(nullptr) {
        LoadConfig();

        screenWidth = config.screenWidth;
    screenHeight = config.screenHeight;
    lives = config.initialLives;

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

void Game::InitWindow() {
    // 只在这里初始化窗口
     ::InitWindow(config.screenWidth, config.screenHeight, config.title.c_str());
    ::SetTargetFPS(config.targetFps);
}

void Game::ResetGame() {
    // 1. 清理旧对象 (防止内存泄漏)
    delete ball;
    delete paddle;

    // 2. 重新创建新对象
    ball = new Ball(
        {(float)screenWidth/2, (float)screenHeight/2}, 
        {config.ballStartSpeedX, config.ballStartSpeedY}, 
        config.ballRadius
    );
    float paddleX = (screenWidth - config.paddleWidth) / 2;
    paddle = new Paddle(
        paddleX, 
        screenHeight - 50, 
        config.paddleWidth, 
        config.paddleHeight,
        config.paddleSpeed // 传入配置的速度
    );

    // 3. 重置砖块
    bricks.clear();
    bricksRemaining = 0;
    
    float startX = (screenWidth - (config.brickCols * (config.brickWidth + config.brickSpacing))) / 2;
    float startY = 50;

    for(int row = 0; row < config.brickRows; row++) {
        for (int col = 0; col < config.brickCols; col++) {
            float x = startX + col * (config.brickWidth + config.brickSpacing);
            float y = startY + row * (config.brickHeight + config.brickSpacing);
            bricks.emplace_back(x, y, config.brickWidth, config.brickHeight);
        }
    }
    
    bricksRemaining = bricks.size();
    lives = config.initialLives; // 重置生命值
}

void Game::ProcessInput() {
    
    switch (state) {
        case MENU:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnStart)) {
                ResetGame(); // <--- 改这里：只重置数据，不碰窗口
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
             if (IsKeyPressed(KEY_ESCAPE)) {
        state = PLAYING;
    }
            break;
        case PLAYING:
            // 游戏中的移动逻辑 (原来的 ProcessInput 内容)
            if (IsKeyDown(KEY_LEFT)) paddle->MoveLeft();
            if (IsKeyDown(KEY_RIGHT)) paddle->MoveRight();
             if (IsKeyPressed(KEY_ESCAPE)) {
        state = PAUSED;
    }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                if (CheckCollisionPointRec(mouse, btnPause)) { // 检测是否点击了 btnPause
                    state = PAUSED;
                }
            }
            break;
        case GAMEOVER:
            // 游戏结束输入
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                if (CheckCollisionPointRec(mouse, btnStart)) {
                    ResetGame();
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
}

void Game::DrawPlaying() {
    // 绘制游戏元素
    ball->Draw();
    paddle->Draw();
    for (auto& brick : bricks) {
        if (brick.IsActive()) brick.Draw();
    }
    
    // 绘制UI
    DrawText(TextFormat("HP: %i", lives), 60, 10, 20, RED);
    DrawText(TextFormat("Bricks: %i", bricksRemaining), 600, 10, 20, GREEN);
    
    // --- 新增：绘制屏幕左上角的暂停按钮 ---
    DrawRectangleRec(btnPause, GRAY);
    DrawText("| |", 20, 18, 30, WHITE);
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
        DrawText("GAME OVER", 250, 150, 50, RED);
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