// File: Game.cpp
#include "game.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <fstream>

float Lerp(float a, float b, float t)
{
    return (1 - t) * a + t * b;
}

using json = nlohmann::json;

PowerUpConfig PowerUpFactory::cfg;

void Game::LoadConfig()
{
    std::ifstream file("../config.json");
    if (!file.is_open())
    {
        std::cerr << "无法打开 config.json，使用默认值" << std::endl;
        // 即使文件打不开，构造函数中的默认值也会生效
        return;
    }

    try
    {
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

        PowerUpFactory::cfg.spawnChance = data["powerup"]["spawn_chance"];
        PowerUpFactory::cfg.fallSpeed = data["powerup"]["fall_speed"];
        PowerUpFactory::cfg.growDuration = data["powerup"]["duration"]["grow_paddle"];
        PowerUpFactory::cfg.growAnimationTime = data["powerup"]["animation"]["grow_time"];
        PowerUpFactory::cfg.growFactor = data["powerup"]["values"]["grow_factor"];
        PowerUpFactory::cfg.extraLives = data["powerup"]["values"]["extra_lives"];
        PowerUpFactory::cfg.splitCount = data["powerup"]["values"]["split_count"];
        PowerUpFactory::cfg.size = data["graphics"]["powerups"]["size"];

        // 加载颜色 (JSON数组转Color)
        auto &g = data["graphics"]["powerups"]["grow"]["color"];
        PowerUpFactory::cfg.colorGrow = {(unsigned char)g[0], (unsigned char)g[1], (unsigned char)g[2], 255};
        auto &s = data["graphics"]["powerups"]["split"]["color"];
        PowerUpFactory::cfg.colorSplit = {(unsigned char)s[0], (unsigned char)s[1], (unsigned char)s[2], 255};
        auto &l = data["graphics"]["powerups"]["life"]["color"];
        PowerUpFactory::cfg.colorLife = {(unsigned char)l[0], (unsigned char)l[1], (unsigned char)l[2], 255};

        PowerUpFactory::cfg.symbolGrow = data["graphics"]["powerups"]["grow"]["symbol"];
        PowerUpFactory::cfg.symbolSplit = data["graphics"]["powerups"]["split"]["symbol"];
        PowerUpFactory::cfg.symbolLife = data["graphics"]["powerups"]["life"]["symbol"];

        std::cout << "配置加载成功!" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "配置解析错误: " << e.what() << std::endl;
    }
    file.close();
    powerUpCfg = PowerUpFactory::cfg;
}

Game::Game(int width, int height)
    : screenWidth(width), screenHeight(height), running(true), bricksRemaining(0), ball(nullptr), paddle(nullptr), activeBuff(""), buffTimer(0)
{
    LoadConfig();

    screenWidth = config.screenWidth;
    screenHeight = config.screenHeight;
    lives = config.initialLives;

    state = MENU;

    btnStart = {(float)screenWidth / 2 - 100, 200, 200, 50};
    btnExit = {(float)screenWidth / 2 - 100, 300, 200, 50};
    btnResume = {(float)screenWidth / 2 - 100, 200, 200, 50};
    btnPause = {10, 10, 40, 40}; // 屏幕左上角的暂停按钮
    // 构造函数主要进行参数初始化
}

Game::~Game()
{
    delete ball;
    delete paddle;
    // vector 会自动释放
}

Rectangle paddleRect;

void Game::InitWindow()
{
    // 只在这里初始化窗口
    ::InitWindow(config.screenWidth, config.screenHeight, config.title.c_str());
    ::SetTargetFPS(config.targetFps);
}

void Game::ResetGame()
{
    // 清理旧对象
    for (auto b : balls)
        delete b;
    balls.clear();
    delete paddle;
    for (auto b : activePowerUps)
        delete b;
    activePowerUps.clear();

    // 创建新球
    ResetBalls();

    // 创建挡板
    float paddleX = (screenWidth - config.paddleWidth) / 2;
    paddle = new Paddle(paddleX, screenHeight - 50, config.paddleWidth, config.paddleHeight, config.paddleSpeed);

    // 3. 重置砖块
    bricks.clear();
    bricksRemaining = 0;

    float startX = (screenWidth - (config.brickCols * (config.brickWidth + config.brickSpacing))) / 2;
    float startY = 50;

    for (int row = 0; row < config.brickRows; row++)
    {
        for (int col = 0; col < config.brickCols; col++)
        {
            float x = startX + col * (config.brickWidth + config.brickSpacing);
            float y = startY + row * (config.brickHeight + config.brickSpacing);
            bricks.emplace_back(x, y, config.brickWidth, config.brickHeight);
        }
    }

    bricksRemaining = bricks.size();
    lives = config.initialLives; // 重置生命值
    activeBuff = "";
}

void Game::ResetBalls()
{
    Vector2 startPos = {(float)screenWidth / 2, (float)screenHeight / 2};
    Vector2 startSpeed = {config.ballStartSpeedX, config.ballStartSpeedY};
    balls.push_back(new Ball(startPos, startSpeed, config.ballRadius));
}

void Game::ProcessInput()
{

    switch (state)
    {
    case MENU:
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnStart))
            {
                ResetGame(); // <--- 改这里：只重置数据，不碰窗口
                state = PLAYING;
            }
            else if (CheckCollisionPointRec(mouse, btnExit))
            {
                running = false;
            }
        }
        break;

    case PAUSED:
        // 暂停菜单输入
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnResume))
            {
                state = PLAYING;
            }
            else if (CheckCollisionPointRec(mouse, btnExit))
            {
                state = MENU;
            }
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            state = PLAYING;
        }
        break;
    case PLAYING:
        if (IsKeyPressed(KEY_SPACE))
        {
            lives += 100;
        }
        // 游戏中的移动逻辑 (原来的 ProcessInput 内容)
        if (IsKeyDown(KEY_LEFT))
            paddle->MoveLeft();
        if (IsKeyDown(KEY_RIGHT))
            paddle->MoveRight();
        if (IsKeyPressed(KEY_ESCAPE))
        {
            state = PAUSED;
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnPause))
            { // 检测是否点击了 btnPause
                state = PAUSED;
            }
        }
        break;
    case GAMEOVER:
        // 游戏结束输入
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btnStart))
            {
                ResetGame();
                state = PLAYING;
            }
            else if (CheckCollisionPointRec(mouse, btnExit))
            {
                state = MENU;
            }
        }
        break;
    }
}

void Game::CheckCollisions()
{
    // ... (处理球与球的碰撞，如果需要的话)

    // 处理每个球与世界的碰撞
    for (auto &ball : balls)
    {
        Vector2 ballPos = ball->GetPosition();
        float ballRadius = ball->GetRadius();

        // 1. 球与挡板碰撞 (处理挡板加长逻辑)
        paddleRect = paddle->GetRect();

        // 动态计算当前挡板宽度 (实现平滑缩放)
        float currentPaddleWidth = paddleRect.width;
        if (activeBuff == "grow")
        {
            // 插值计算宽度
            float t = (PowerUpFactory::cfg.growDuration - buffTimer); // 0 到 1
            if (t < 1)
            {
                // 正在变大过程
                currentPaddleWidth = Lerp(paddleRect.width, paddleRect.width * PowerUpFactory::cfg.growFactor, t);
            }
            else if (buffTimer < PowerUpFactory::cfg.growAnimationTime)
            {
                // 效果结束，正在恢复原状
                float t = (PowerUpFactory::cfg.growAnimationTime - buffTimer);
                currentPaddleWidth = Lerp(paddleRect.width * PowerUpFactory::cfg.growFactor, paddleRect.width, t);
            }
            else
            {
                // 持续期间保持最大
                currentPaddleWidth = paddleRect.width * PowerUpFactory::cfg.growFactor;
            }
        }

        // 更新挡板矩形用于碰撞 (中心不变)
        float paddleX = paddleRect.x;
        float paddleCenter = paddleX + paddleRect.width / 2;
        paddleRect = {paddleCenter - currentPaddleWidth / 2, paddleRect.y, currentPaddleWidth, paddleRect.height};

        if (CheckCollisionCircleRec(ballPos, ballRadius, paddleRect))
        {
            // 简单反弹
            if (ballPos.y + ballRadius >= paddleRect.y)
            {
                ball->ReverseYSpeed();
            }
        }

        // 2. 球与砖块碰撞
        for (auto &brick : bricks)
        {
            if (!brick.IsActive())
                continue;
            if (CheckCollisionCircleRec(ballPos, ballRadius, brick.GetRect()))
            {

                // --- 道具生成逻辑 ---
                if (GetRandomValue(1, 100) <= powerUpCfg.spawnChance)
                {
                    std::vector<std::string> types = {"grow", "split", "life"};
                    std::string type = types[GetRandomValue(0, 2)];

                    Vector2 brickCenter = {
                        brick.GetRect().x + brick.GetRect().width / 2,
                        brick.GetRect().y + brick.GetRect().height / 2};
                    PowerUp *powerUp = PowerUpFactory::CreatePowerUp(type, brickCenter);
                    if (powerUp)
                    {
                        activePowerUps.push_back(powerUp);
                    }
                }
                brick.SetActive(false);
                bricksRemaining--;

                ball->ReverseYSpeed(); // 简单处理
            }
        }
    }
}

void Game::CheckPowerUpCollision()
{
    Vector2 paddlePos = {paddle->GetRect().x + paddle->GetRect().width / 2, paddle->GetRect().y};
    float paddleRadius = paddle->GetRect().width / 2; // 简单用圆形检测挡板

    for (auto &powerUp : activePowerUps)
    {
        Vector2 pos = powerUp->GetPosition();
        // 简单的圆形-圆形检测
        if (CheckCollisionCircles(pos, powerUp->GetRadius(), paddlePos, paddleRadius))
        {
            powerUp->ApplyEffect(this);
            powerUp->SetActive(false);
        }
    }
}

void Game::UpdateGame()
{
    if (state == PLAYING)
    {
        // --- 处理增益 Buff 时间 ---
        if (!activeBuff.empty())
        {
            buffTimer -= GetFrameTime();
            if (buffTimer <= 0)
            {
                activeBuff = "";
                // 如果是 Grow 效果结束，不需要立即缩小，由动画处理
            }
        }

        if (bricksRemaining <= 0)
        {
            state = GAMEOVER;
        }

        // --- 更新所有球 ---
        // --- 更新所有球 (新逻辑) ---
        bool anyBallDropped = false; // 标记本轮是否有球掉落

        for (auto it = balls.begin(); it != balls.end();)
        {
            (*it)->Move();
            (*it)->BounceEdge(screenWidth, screenHeight);

            // 检查球是否掉落 (仅做标记和删除，不扣血)
            if ((*it)->GetPosition().y > screenHeight)
            {
                delete *it;
                it = balls.erase(it);
                anyBallDropped = true; // 只要有球掉，就标记
            }
            else
            {
                ++it;
            }
        }

        // --- 结算逻辑 ---
        // 如果有球掉落 且 此时球容器为空 (意味着刚掉的那个球是最后一个)
        if (anyBallDropped && balls.empty())
        {
            lives--; // 扣血
            if (lives > 0)
            {
                // 重生：重新生成一个球
                ResetBalls(); // 复用初始化逻辑
            }
            else
            {
                state = GAMEOVER;
            }
        }
        // 注意：原来的 UpdatePowerUps, CheckCollisions 等调用保持不变，不要放在这里面

        // --- 更新道具 ---
        UpdatePowerUps(GetFrameTime());

        // --- 检查碰撞 ---
        CheckCollisions();
        CheckPowerUpCollision();
    }
}

void Game::UpdatePowerUps(float deltaTime)
{
    for (auto it = activePowerUps.begin(); it != activePowerUps.end();)
    {
        (*it)->Update(deltaTime);
        if (!(*it)->IsActive())
        {
            delete *it;
            it = activePowerUps.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Game::Render()
{
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // 根据状态绘制画面
    switch (state)
    {
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

void Game::DrawMenu()
{
    // 绘制标题
    DrawText("BRICK BREAKER", 250, 100, 40, DARKBLUE);

    // 绘制按钮
    DrawRectangleRec(btnStart, LIGHTGRAY);
    DrawText("START GAME", 320, 215, 20, DARKBLUE);

    DrawRectangleRec(btnExit, PINK);
    DrawText("EXIT", 370, 315, 20, DARKBLUE);
}

void Game::DrawPlaying()
{
    // 绘制游戏元素
    for (auto &ball : balls)
    {
        ball->Draw();
    }
    paddle->Draw(paddleRect);
    for (auto &brick : bricks)
    {
        if (brick.IsActive())
            brick.Draw();
    }
    for (auto &powerUp : activePowerUps)
    {
        powerUp->Draw();
    }
    for (auto &brick : bricks)
    {
        if (brick.IsActive())
            brick.Draw();
    }

    // 绘制UI
    DrawText(TextFormat("HP: %i", lives), 60, 10, 20, RED);
    DrawText(TextFormat("Bricks: %i", bricksRemaining), 600, 10, 20, GREEN);

    // --- 新增：绘制屏幕左上角的暂停按钮 ---
    DrawRectangleRec(btnPause, GRAY);
    DrawText("| |", 20, 18, 30, WHITE);
}

void Game::DrawPaused()
{
    // 半透明遮罩
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));

    // 恢复按钮
    DrawRectangleRec(btnResume, YELLOW);
    DrawText("RESUME", 330, 215, 20, DARKBLUE);

    // 退出到菜单按钮
    DrawRectangleRec(btnExit, ORANGE);
    DrawText("MAIN MENU", 310, 315, 20, DARKBLUE);
}

void Game::DrawGameOver()
{
    // 半透明遮罩
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.8f));

    if (lives <= 0)
    {
        DrawText("GAME OVER", 250, 150, 50, RED);
    }
    else
    {
        DrawText("YOU WIN!", 300, 150, 50, GOLD);
    }

    // 重新开始
    DrawRectangleRec(btnStart, GREEN);
    DrawText("RESTART", 340, 265, 20, DARKBLUE);

    // 返回菜单
    DrawRectangleRec(btnExit, BLUE);
    DrawText("MENU", 370, 365, 20, DARKBLUE);
}

// File: game.cpp

// 定义工厂类的静态成员

// --- PowerUp 基类实现 ---
PowerUp::PowerUp(Vector2 pos, float radius, Color col, std::string type, std::string symbol)
    : position(pos), color(col), type(type), active(true), radius(radius), symbol(symbol)
{
    velocity = {0, PowerUpFactory::cfg.fallSpeed}; // 初始速度
}

PowerUp::~PowerUp() {}

void PowerUp::Update(float deltaTime)
{
    position.y += velocity.y * deltaTime;
    // 检查是否出界
    if (position.y > GetScreenHeight() + radius)
    {
        active = false;
    }
}

void PowerUp::Draw()
{
    if (active)
    {
        DrawCircleV(position, radius, color);
        // 绘制符号 (简单居中)
        DrawText(symbol.c_str(), position.x - MeasureText(symbol.c_str(), 10) / 2, position.y - 5, 10, WHITE);
    }
}

// --- GrowPaddlePowerUp 实现 ---
GrowPaddlePowerUp::GrowPaddlePowerUp(Vector2 pos, float radius, Color col, std::string symbol, float duration)
    : PowerUp(pos, radius, col, "grow", symbol)
{
    this->duration = duration;
    this->timer = 0;
    this->isGrowing = false; // 刚生成时不是生长状态，ApplyEffect 时才开始
    this->originalWidth = 0;
    this->targetWidth = 0;
}

void GrowPaddlePowerUp::SetOriginalSize(float width)
{
    this->originalWidth = width;
    this->targetWidth = width * PowerUpFactory::cfg.growFactor;
}

void GrowPaddlePowerUp::ApplyEffect(Game *game)
{
    // 标记 Game 类需要处理变大逻辑
    // 这里我们只做标记，具体的宽度改变在 Game::Update 中处理
    if (game->activeBuff == "grow")
    {
        game->buffTimer = duration - PowerUpFactory::cfg.growAnimationTime;
    }
    else
    {
        game->activeBuff = "grow";
        game->buffTimer = duration;
        // 立即改变宽度 (视觉上由 Game 处理插值)
        // 注意：这里只是触发，实际的 Paddle 宽度改变在 Game::UpdateGame 中
    }
}

void GrowPaddlePowerUp::Update(float deltaTime)
{
    PowerUp::Update(deltaTime); // 继承下落
    // 这个类的 Update 主要用于处理动画，但为了简单，动画逻辑放在 Game 里处理
}

// --- SplitBallPowerUp 实现 ---
SplitBallPowerUp::SplitBallPowerUp(Vector2 pos, float radius, Color col, std::string symbol)
    : PowerUp(pos, radius, col, "split", symbol) {}

void SplitBallPowerUp::ApplyEffect(Game *game)
{
    // 在球的当前位置生成新球
    Vector2 spawnPos = game->balls[0]->GetPosition(); // 假设 balls[0] 是主球
    for (int i = 0; i < PowerUpFactory::cfg.splitCount; i++)
    {
        // 随机 X 速度
        float randomX = (GetRandomValue(0, 1) == 0) ? -1 : 1;
        Vector2 newSpeed = {randomX * game->config.ballStartSpeedX, -game->config.ballStartSpeedY};
        game->balls.push_back(new Ball(spawnPos, newSpeed, game->config.ballRadius));
    }
}

// --- ExtraLifePowerUp 实现 ---
ExtraLifePowerUp::ExtraLifePowerUp(Vector2 pos, float radius, Color col, std::string symbol)
    : PowerUp(pos, radius, col, "life", symbol) {}

void ExtraLifePowerUp::ApplyEffect(Game *game)
{
    game->lives += PowerUpFactory::cfg.extraLives;
}

// --- 工厂 Create 方法 ---
PowerUp *PowerUpFactory::CreatePowerUp(std::string type, Vector2 pos)
{
    float radius = cfg.size / 2;
    if (type == "grow")
    {
        return new GrowPaddlePowerUp(pos, radius, cfg.colorGrow, cfg.symbolGrow, cfg.growDuration);
    }
    else if (type == "split")
    {
        return new SplitBallPowerUp(pos, radius, cfg.colorSplit, cfg.symbolSplit);
    }
    else if (type == "life")
    {
        return new ExtraLifePowerUp(pos, radius, cfg.colorLife, cfg.symbolLife);
    }
    return nullptr;
}