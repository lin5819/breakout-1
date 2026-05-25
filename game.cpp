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

float Ball::maxspeedx;
float Ball::randspeedx;
float Ball::addspeedx;
const int StatePacket::MAX_BRICKS;
const int StatePacket::MAX_BALLS;
const int StatePacket::MAX_POWERUPS;
const int Game::MAX_PARTICLES;

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

        if (data.contains("network"))
        {
            config.port = data["network"]["port"];
            config.hostIp = data["network"]["host_ip"];
            config.gameMode = data["network"]["game_mode"];
        }

        // 读取 Window 配置
        config.screenWidth = data["window"]["width"];
        config.screenHeight = data["window"]["height"];
        config.title = data["window"]["title"];
        config.targetFps = data["window"]["fps"];

        // --- 加载关卡数据 ---
        LoadLevelsFromJson(data);

        // 如果关卡加载成功，设置总关卡数
        config.totalLevels = data["levelsnum"];
        config.currentLevelIndex = 0; // 默认从 0 开始

        // 读取 Ball 配置
        config.ballRadius = data["ball"]["radius"];
        config.ballStartSpeedX = data["ball"]["start_speed_x"];
        //        config.ballStartSpeedY = data["ball"]["start_speed_y"];

        Ball::addspeedx = data["ball"]["addspeedx"];
        Ball::maxspeedx = data["ball"]["maxspeedx"];
        Ball::randspeedx = data["ball"]["randspeedx"];

        // 读取 Paddle 配置
        config.paddleWidth = data["paddle"]["width"];
        config.paddleHeight = data["paddle"]["height"];
        config.paddleSpeed = data["paddle"]["speed"];

        PowerUpFactory::cfg.fallSpeed = data["powerup"]["fall_speed"];
        PowerUpFactory::cfg.growDuration = data["powerup"]["duration"]["grow_paddle"];
        PowerUpFactory::cfg.growAnimationTime = data["powerup"]["animation"]["grow_time"];
        PowerUpFactory::cfg.growFactor = data["powerup"]["values"]["grow_factor"];
        PowerUpFactory::cfg.extraLives = data["powerup"]["values"]["extra_lives"];
        PowerUpFactory::cfg.splitCount = data["powerup"]["values"]["split_count"];
        PowerUpFactory::cfg.size = data["powerup"]["graphics"]["size"];

        // 加载颜色 (JSON数组转Color)
        auto &g = data["powerup"]["graphics"]["grow"]["color"];
        PowerUpFactory::cfg.colorGrow = {(unsigned char)g[0], (unsigned char)g[1], (unsigned char)g[2], 255};
        auto &s = data["powerup"]["graphics"]["split"]["color"];
        PowerUpFactory::cfg.colorSplit = {(unsigned char)s[0], (unsigned char)s[1], (unsigned char)s[2], 255};
        auto &l = data["powerup"]["graphics"]["life"]["color"];
        PowerUpFactory::cfg.colorLife = {(unsigned char)l[0], (unsigned char)l[1], (unsigned char)l[2], 255};

        PowerUpFactory::cfg.symbolGrow = data["powerup"]["graphics"]["grow"]["symbol"];
        PowerUpFactory::cfg.symbolSplit = data["powerup"]["graphics"]["split"]["symbol"];
        PowerUpFactory::cfg.symbolLife = data["powerup"]["graphics"]["life"]["symbol"];

        std::cout << "配置加载成功!" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "配置解析错误: " << e.what() << std::endl;
    }
    file.close();
    powerUpCfg = PowerUpFactory::cfg;
}

void Game::LoadLevelsFromJson(const json &data)
{
    if (!data.contains("levels"))
        return;

    for (auto &levelJson : data["levels"])
    {
        LevelData level;

        // 加载砖块布局
        if (levelJson.contains("bricks"))
        {
            auto &b = levelJson["bricks"];
            level.brickRows = b.value("rows", 5);
            level.brickCols = b.value("cols", 7);
            level.brickWidth = b.value("width", 90.0f);
            level.brickHeight = b.value("height", 30.0f);
            level.brickSpacing = b.value("spacing", 20.0f);

            // 加载自定义布局字符串
            if (b.contains("layout"))
            {
                level.customLayout.clear();
                for (auto &row : b["layout"])
                {
                    level.customLayout.push_back(row.get<std::string>());
                }
            }
        }

        // 加载游戏特定配置

        level.initialLives = levelJson["game"]["initial_lives"];

        // 加载球速配置

        level.ballStartSpeedY = levelJson["ball"]["start_speed_y"];

        // 加载道具掉落率

        level.spawnChance = levelJson["powerup"]["spawn_chance"];

        // 加载关卡标题
        level.title = levelJson["title"];

        config.levels.push_back(level);
    }
}

Game::Game(int width, int height)
    : screenWidth(width), screenHeight(height), running(true), bricksRemaining(0), ball(nullptr), activeBuff(""), buffTimer(0), netHost(nullptr), netPeer(nullptr),
      isNetworkMode(false), isConnected(false),
      paddle1(nullptr), paddle2(nullptr),
      isLoading(false),
      loadComplete(false),
      displayFps(0),          // 初始显示为0
      fpsAccumulator(0),      // 时间累计器为0
      frameCount(0),          // 帧数累计器为0
      currentMenuState(MAIN), // 初始化为主菜单
      btnLevel1{0}, btnLevel2{0}, btnLevel3{0}, btnNextLevel{0},
      showNextLevelButton(0)
{
    LoadConfig();

    screenWidth = config.screenWidth;
    screenHeight = config.screenHeight;
    lives = config.initialLives;

    state = MENU;

    btnStart = {(float)screenWidth / 2 - 100, 320, 200, 50};
    btnExit = {(float)screenWidth / 2 - 100, 500, 200, 50};
    btnResume = {(float)screenWidth / 2 - 100, 400, 200, 50};
    btnPause = {10, 10, 40, 40}; // 屏幕左上角的暂停按钮
    // 构造函数主要进行参数初始化
    int btnWidth = 200;
    int btnHeight = 50;
    int centerX = screenWidth / 2 - btnWidth / 2;

    // 垂直排列，间距 20
    btnSingle = (Rectangle){(float)centerX, 200, (float)btnWidth, (float)btnHeight};
    btnHost = (Rectangle){(float)centerX, 270, (float)btnWidth, (float)btnHeight};
    btnClient = (Rectangle){(float)centerX, 340, (float)btnWidth, (float)btnHeight};
    btnLevel1 = {(float)centerX, 200, (float)btnWidth, (float)btnHeight};
    btnLevel2 = {(float)centerX, 270, (float)btnWidth, (float)btnHeight};
    btnLevel3 = {(float)centerX, 340, (float)btnWidth, (float)btnHeight};
    btnNextLevel = {(float)centerX, 410, (float)btnWidth, (float)btnHeight};
}

Game::~Game()
{
    //    ShutdownNetwork();
    delete ball;
    delete paddle1;
    delete paddle2;
    // vector 会自动释放
    ShutdownNetwork();
}

void Game::ShutdownNetwork()
{
    if (netHost != nullptr)
    {
        enet_host_destroy(netHost);
        netHost = nullptr;
    }
    enet_deinitialize();
}

void Game::InitNetwork()
{
    // 初始化 ENet
    if (enet_initialize() < 0)
    {
        printf("ENet 初始化失败!\n");
        return;
    }

    ENetAddress address;

    if (IsHost())
    {
        // 主机：监听端口
        address.host = ENET_HOST_ANY;
        address.port = config.port;
        netHost = enet_host_create(&address, 32, 2, 0, 0); // 支持32个连接，2个通道
        if (netHost == nullptr)
        {
            printf("无法创建 ENet Host!\n");
            return;
        }
        printf("主机启动，等待连接...\n");
        state = CONNECTING; // 引入 CONNECTING 状态或在 PLAYING 中轮询
    }
    else if (IsClient())
    {
        // 客户端：连接主机
        netHost = enet_host_create(NULL, 1, 2, 0, 0);
        enet_address_set_host(&address, config.hostIp.c_str());
        address.port = config.port;

        netPeer = enet_host_connect(netHost, &address, 2, 0);
        if (netPeer == nullptr)
        {
            printf("无法连接到主机!\n");
            state = MENU;
        }
    }
    // SINGLEPLAYER 不做任何事
}

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
    delete paddle1;
    delete paddle2; // 新增
    paddle1 = nullptr;
    paddle2 = nullptr;
    for (auto b : activePowerUps)
        delete b;
    activePowerUps.clear();

    // 创建新球
    ResetBalls();

    // 创建挡板
    float paddleX = (screenWidth - config.paddleWidth) / 2;
    paddle1 = new Paddle(paddleX, screenHeight - 50, config.paddleWidth, config.paddleHeight, config.paddleSpeed);

    // P2 挡板 (顶部) - 仅在双人模式下创建
    if (!IsSinglePlayer())
    {
        paddle2 = new Paddle(paddleX, screenHeight - 50, config.paddleWidth, config.paddleHeight, config.paddleSpeed);
    }

    // 3. 重置砖块
    bricks.clear();
    bricksRemaining = 0;

    float startX = (screenWidth - (config.brickCols * (config.brickWidth + config.brickSpacing))) / 2;
    float startY = 50;

    bool useCustomLayout = !config.customLayout.empty();

    // 如果使用自定义布局，行数以 Layout 为准（防止 JSON 中 rows 设置错误）
    int actualRows = useCustomLayout ? config.customLayout.size() : config.brickRows;
    int actualCols = config.brickCols; // 列数通常由砖块宽度决定

    for (int row = 0; row < actualRows; row++)
    {
        // 如果是自定义布局，需要检查当前行字符串长度是否足够
        int currentRowLength = actualCols;
        if (useCustomLayout && row < config.customLayout.size())
        {
            currentRowLength = config.customLayout[row].length();
        }

        for (int col = 0; col < currentRowLength && col < actualCols; col++)
        {
            bool shouldCreateBrick = false;

            if (useCustomLayout)
            {
                // 从自定义布局字符串中读取
                // 注意：需要确保 row 和 col 在字符串范围内
                if (row < config.customLayout.size() && col < config.customLayout[row].size())
                {
                    char cell = config.customLayout[row][col];
                    shouldCreateBrick = (cell == '1'); // '1' 表示有砖块
                }
                // 如果 col 超出字符串长度，默认为 '0' (无砖块)
            }
            else
            {
                // 默认整齐排列：全部生成
                shouldCreateBrick = true;
            }

            if (shouldCreateBrick)
            {
                float x = startX + col * (config.brickWidth + config.brickSpacing);
                float y = startY + row * (config.brickHeight + config.brickSpacing);
                bricks.emplace_back(x, y, config.brickWidth, config.brickHeight);
            }
        }
    }
    bricksRemaining = bricks.size();

    activeBuff = "";
}

void Game::ResetBalls()
{
    Vector2 startPos = {(float)screenWidth / 2, (float)screenHeight / 2};
    Vector2 startSpeed = {config.ballStartSpeedX, config.ballStartSpeedY};
    balls.push_back(new Ball(startPos, startSpeed, config.ballRadius));
    for (auto &ball : balls)
    {
        ball->Randspeedx();
    }
}

void Game::ProcessInput()
{
    if (IsKeyPressed(KEY_L) && !isLoading)
    {
        StartLoadingAsync();
    }

    switch (state)
    {
    case MENU:
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 mouse = GetMousePosition();

            if (CheckCollisionPointRec(mouse, btnExit))
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
                currentMenuState = MAIN;
                if (!IsSinglePlayer())
                {
                    config.gameMode = "SINGLEPLAYER";
                    ShutdownNetwork();
                    isConnected = false;
                }
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
        } // 作弊：生命值+100
        if (IsKeyPressed(KEY_E))
        {
            for (auto &brick : bricks)
                brick.SetActive(false);
            bricksRemaining = 0;
        } // 作弊：清空砖块

        // 游戏中的移动逻辑 (原来的 ProcessInput 内容)
        if (IsSinglePlayer())
        {
            // 单人模式：仅控制 P1
            if (IsKeyDown(KEY_LEFT))
                paddle1->MoveLeft();
            if (IsKeyDown(KEY_RIGHT))
                paddle1->MoveRight();
        }
        else if (IsHost())
        {
            // 主机模式：本地控制 P1
            if (IsKeyDown(KEY_LEFT))
                paddle1->MoveLeft();
            if (IsKeyDown(KEY_RIGHT))
                paddle1->MoveRight();

            // 主机不处理 P2 输入，P2 输入由客户端发送过来
        }
        else if (IsClient())
        {
            // 客户端模式：本地控制 P2 (发送输入)
            // 客户端不直接移动本地 P2，而是发送请求给主机
            SendInputPacket();

            // 如果需要本地预测 (Local Prediction)，可以在这里临时移动 P2，但最终以主机为准
        }
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
                currentMenuState = MAIN;
                if (!IsSinglePlayer())
                {
                    config.gameMode = "SINGLEPLAYER";
                    ShutdownNetwork();
                    isConnected = false;
                }
            }
        }
        break;
    case LEVEL_COMPLETE:
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
                currentMenuState = MAIN;
                if (!IsSinglePlayer())
                {
                    config.gameMode = "SINGLEPLAYER";
                    ShutdownNetwork();
                    isConnected = false;
                }
            }
            else if (CheckCollisionPointRec(mouse, btnNextLevel))
            {
                StartLevel(config.currentLevelIndex + 1);
            }
        }
        break;
    }
}

void Game::CheckCollisions()
{
    for (auto &ball : balls)
    {
        Vector2 ballPos = ball->GetPosition();
        float ballRadius = ball->GetRadius();

        bool bounced = false;

        if ((ballPos.x - ball->GetRadius() <= 0) ||
            (ballPos.x + ball->GetRadius() >= screenWidth))
        {
            // --- 侧壁碰撞粒子 ---
            Vector2 spawnPos = ballPos;
            // 限制生成位置在屏幕内
            spawnPos.x = std::clamp(spawnPos.x, 0.0f, (float)screenWidth);
            int count = GetRandomValue(5, 10);
            for (int i = 0; i < count; i++)
            {
                // 沿着碰撞法线方向飞溅 (左右碰撞，粒子主要沿X轴飞)
                float angle = (ballPos.x < screenWidth / 2) ? GetRandomValue(-45, 45) : GetRandomValue(135, 225);
                angle *= 3.14159f / 180.0f;
                float speed = GetRandomValue(80, 120);

                // 使用球的颜色 (RED)
                for (int i = 0; i < MAX_PARTICLES; i++)
                {
                    if (!particlePool[i].active)
                    { // 找到空闲槽
                        Vector2 vel = {cosf(angle) * speed, (float)GetRandomValue(-50, 50)};
                        // 直接重置数据，不申请内存
                        particlePool[i].Reset(spawnPos, vel, 0.3f, RED, 1.5f);
                        break; // 只发射一个
                    }
                }
            }
            bounced = true;
        }

        if ((ballPos.y - ball->GetRadius() <= 0))
        {
            // --- 顶部碰撞粒子 ---
            Vector2 spawnPos = ballPos;
            spawnPos.y = 0; // 强制在顶部
            int count = GetRandomValue(5, 10);
            for (int i = 0; i < count; i++)
            {
                float angle = GetRandomValue(45, 135) * 3.14159f / 180.0f; // 向下飞溅
                float speed = GetRandomValue(80, 120);
                for (int i = 0; i < MAX_PARTICLES; i++)
                {
                    if (!particlePool[i].active)
                    { // 找到空闲槽
                        Vector2 vel = {cosf(angle) * speed, (float)GetRandomValue(-50, 50)};
                        // 直接重置数据，不申请内存
                        particlePool[i].Reset(spawnPos, vel, 0.3f, RED, 1.5f);
                        break; // 只发射一个
                    }
                }
            }
            bounced = true;
        }
        for (auto &brick : bricks)
        {
            if (!brick.IsActive())
                continue;
            if (CheckCollisionCircleRec(ballPos, ballRadius + 3, brick.GetRect()))
            {
                Vector2 brickCenter = {
                    brick.GetRect().x + brick.GetRect().width / 2,
                    brick.GetRect().y + brick.GetRect().height / 2};

                int count = GetRandomValue(5, 10);
                for (int i = 0; i < count; i++)
                {
                    float angle = GetRandomValue(0, 360) * 3.14159f / 180.0f;
                    float speed = GetRandomValue(50, 150);
                    for (int i = 0; i < MAX_PARTICLES; i++)
                    {
                        if (!particlePool[i].active)
                        { // 找到空闲槽
                            Vector2 vel = {cosf(angle) * speed, (float)GetRandomValue(-50, 50)};
                            // 直接重置数据，不申请内存
                            particlePool[i].Reset(brickCenter, vel, 0.5f, GREEN, 2.0f);
                            break; // 只发射一个
                        }
                    }
                }
            }
        }
    }

    if (IsSinglePlayer() || IsHost())
    {
        std::vector<Paddle *> p = {paddle1, paddle2};
        for (auto &paddle : p)
        {
            if (paddle != nullptr)
            {
                // 处理每个球与世界的碰撞
                for (auto &ball : balls)
                {
                    Vector2 ballPos = ball->GetPosition();
                    float ballRadius = ball->GetRadius();

                    // 1. 球与挡板碰撞 (处理挡板加长逻辑)
                    paddle->paddleRect = paddle->GetRect();

                    // 动态计算当前挡板宽度 (实现平滑缩放)
                    float currentPaddleWidth = paddle->paddleRect.width;
                    if (activeBuff == "grow")
                    {
                        // 插值计算宽度
                        float t = (PowerUpFactory::cfg.growDuration - buffTimer); // 0 到 1
                        if (t < 1)
                        {
                            // 正在变大过程
                            currentPaddleWidth = Lerp(paddle->paddleRect.width, paddle->paddleRect.width * PowerUpFactory::cfg.growFactor, t);
                        }
                        else if (buffTimer < PowerUpFactory::cfg.growAnimationTime)
                        {
                            // 效果结束，正在恢复原状
                            float t = (PowerUpFactory::cfg.growAnimationTime - buffTimer);
                            currentPaddleWidth = Lerp(paddle->paddleRect.width * PowerUpFactory::cfg.growFactor, paddle->paddleRect.width, t);
                        }
                        else
                        {
                            // 持续期间保持最大
                            currentPaddleWidth = paddle->paddleRect.width * PowerUpFactory::cfg.growFactor;
                        }
                    }

                    // 更新挡板矩形用于碰撞 (中心不变)
                    float paddleX = paddle->paddleRect.x;
                    float paddleCenter = paddleX + paddle->paddleRect.width / 2;
                    paddle->paddleRect = {paddleCenter - currentPaddleWidth / 2, paddle->paddleRect.y, currentPaddleWidth, paddle->paddleRect.height};

                    if (CheckCollisionCircleRec(ballPos, ballRadius, paddle->paddleRect))
                    {
                        // 简单反弹
                        if (ballPos.y + ballRadius >= paddle->paddleRect.y && ball->GetSpeed().y > 0)
                        {
                            ball->ReverseYSpeed();
                            ball->Addspeedx();
                            ball->Randspeedx();
                        }
                    }

                    // 2. 球与砖块碰撞
                    for (auto &brick : bricks)
                    {
                        if (!brick.IsActive())
                            continue;
                        if (CheckCollisionCircleRec(ballPos, ballRadius, brick.GetRect()))
                        {
                            Vector2 brickCenter = {
                                brick.GetRect().x + brick.GetRect().width / 2,
                                brick.GetRect().y + brick.GetRect().height / 2};

                            // --- 道具生成逻辑 ---
                            if (GetRandomValue(1, 100) <= PowerUpFactory::cfg.spawnChance)
                            {
                                std::vector<std::string> types = {"grow", "split", "life"};
                                std::string type = types[GetRandomValue(0, 2)];

                                PowerUp *powerUp = PowerUpFactory::CreatePowerUp(type, brickCenter);
                                if (powerUp)
                                {
                                    activePowerUps.push_back(powerUp);
                                }
                            }
                            brick.SetActive(false);
                            bricksRemaining--;
                            if (ball->GetPosition().x >= brickCenter.x - brick.GetRect().width / 2 && ball->GetPosition().x <= brickCenter.x + brick.GetRect().width / 2)
                                ball->ReverseYSpeed();
                            else if (ball->GetPosition().y >= brickCenter.y - brick.GetRect().height / 2 && ball->GetPosition().y <= brickCenter.y + brick.GetRect().height / 2)
                                ball->ReverseXSpeed();
                            else
                            {
                                if ((ball->GetSpeed().y > 0 && ball->GetPosition().y < brickCenter.y) || (ball->GetSpeed().y < 0 && ball->GetPosition().y > brickCenter.y))
                                    ball->ReverseYSpeed();
                                if ((ball->GetSpeed().x > 0 && ball->GetPosition().x < brickCenter.x) || (ball->GetSpeed().x < 0 && ball->GetPosition().x > brickCenter.x))
                                    ball->ReverseXSpeed();
                            }
                        }
                    }
                }
            }
        }
    }
}

void Game::CheckPowerUpCollision()
{
    std::vector<Paddle *> p = {paddle1, paddle2};
    for (auto &paddle : p)
    {
        if (paddle != nullptr)
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
    }
}

void Game::UpdateGame()
{
    if (isLoading)
    {
        // 检查 future 是否有效且是否完成
        // wait_for 的 timeout 为 0，表示非阻塞检查
        if (loadingFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            // 任务完成，获取结果（这里只是等待完成）
            loadingFuture.get(); // 调用 get() 来释放 future 的资源

            // 2. 任务完成后，通过锁安全地读取共享数据
            // 这里我们读取 loadComplete 状态
            bool success = false;
            {
                std::lock_guard<std::mutex> lock(gameMutex);
                success = loadComplete;
            }

            if (success)
            {
                // 3. 执行主线程操作：改变背景颜色
                // 注意：改变颜色通常是在 Render 阶段，所以我们设置一个标志
                // 或者直接修改配置。为了演示，我们修改一个背景颜色变量
                // 假设我们有一个 bgColor 成员变量
                // bgColor = PURPLE;

                // 如果没有 bgColor 变量，我们可以直接在这里设置一个标志
                // 然后在 Render 函数中判断

                // 这里为了简单，直接打印日志或设置状态
                printf("异步加载完成！背景已改为紫色。\n");
            }

            isLoading = false;
        }
    }

    if (state == MENU)
    {
        UpdateMenu();
        return;
    }

    if (state == CONNECTING)
    {
        HandleNetworkPackets(); // 持续轮询网络事件

        // 如果是客户端，连接成功后进入游戏
        if (IsClient() && isConnected)
        {
            state = PLAYING;
            ResetGame(); // 客户端也需要初始化对象以便接收状态
            printf("已连接到主机，进入游戏！\n");
        }
        // 如果是主机，一旦有连接（在 HandleNetworkPackets 中设置 isConnected = true），也可以视为开始
        // 这里简单处理：主机点击后即视为开始，或者你可以等待一个客户端
        if (IsHost() && isConnected)
        {
            state = PLAYING;
            ResetGame();
            printf("客户端已连接，游戏开始！\n");
        }
        return; // 在连接阶段不更新物理逻辑
    }

    if (state == GAMEOVER || state == LEVEL_COMPLETE)
    {
        if (!IsSinglePlayer())
        {
            HandleNetworkPackets();
        }
        if (IsHost())
        {
            BroadcastStatePacket();
        }
    }

    if (state == PLAYING)
    {
        if (!IsSinglePlayer())
        {
            HandleNetworkPackets();
        }
        if (IsHost())
        {
            BroadcastStatePacket();
        }

        // --- 新增：FPS 计算逻辑 ---
        float deltaTime = GetFrameTime(); // 获取上一帧耗时
        fpsAccumulator += deltaTime;      // 累计时间
        frameCount++;                     // 累计帧数

        // 如果累计时间超过 1 秒
        if (fpsAccumulator >= 1.0f)
        {
            displayFps = (float)frameCount / fpsAccumulator; // 计算平均帧率
            // 重置计数器，开始计算下一秒
            fpsAccumulator = 0;
            frameCount = 0;
        }
        // --- 结束新增 ---

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
        CheckCollisions();
        if (bricksRemaining <= 0)
        {
            // 检查是否还有下一关
            if (config.currentLevelIndex + 1 < config.totalLevels)
            {
                state = LEVEL_COMPLETE;     // 假设新增了一个状态，或者复用 GAMEOVER 但用标志区分
                showNextLevelButton = true; // 添加一个成员变量来控制 UI
            }
            else
            {
                state = GAMEOVER; // 全部通关
                showNextLevelButton = false;
            }
        }

        // --- 更新所有球 ---
        // --- 更新所有球 (新逻辑) ---
        bool anyBallDropped = false; // 标记本轮是否有球掉落
        if (IsSinglePlayer() || IsHost())
        {
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
                    showNextLevelButton = false;
                }
            }
        }
        // 注意：原来的 UpdatePowerUps, CheckCollisions 等调用保持不变，不要放在这里面

        for (int i = 0; i < MAX_PARTICLES; i++)
        {
            particlePool[i].Update(GetFrameTime()); // Update 内部会检查 active
        }
        // --- 更新道具 ---
        UpdatePowerUps(GetFrameTime());

        // --- 检查碰撞 ---

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

void Game::SendInputPacket()
{
    if (netHost == nullptr || !isConnected)
        return;

    InputPacket packet;
    // 客户端控制 P2，发送 P2 的位置意图
    // 注意：这里发送的是输入状态，而不是直接位置（防止作弊/不同步）
    packet.paddleX = GetMouseX(); // 或者基于键盘计算的期望位置
    // 实际上，更严谨的是发送按键状态，但为了简单发送位置

    ENetPacket *enetPacket = enet_packet_create(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(netPeer, 0, enetPacket);
}

void Game::BroadcastStatePacket()
{
    StatePacket packet;
    for (int i = 0; i < StatePacket::MAX_BALLS; i++)
    {
        if (i < lastFrameBallPositions.size())
        {
            packet.ballX_Previous[i] = lastFrameBallPositions[i].x;
            packet.ballY_Previous[i] = lastFrameBallPositions[i].y;
        }
        else
        {
            // 如果缓存不够，用 -1 填充或用 Current 值填充
            packet.ballX_Previous[i] = -1;
            packet.ballY_Previous[i] = -1;
        }
    }

    // --- 2. 填充 Current 位置 (当前帧) ---
    packet.ballCount = std::min((int)balls.size(), StatePacket::MAX_BALLS);

    // 清空临时容器，准备更新缓存
    lastFrameBallPositions.clear();

    for (int i = 0; i < packet.ballCount; i++)
    {
        Vector2 pos = balls[i]->GetPosition();

        // 1. 填入当前数据包的 Current
        packet.ballX_Current[i] = pos.x;
        packet.ballY_Current[i] = pos.y;

        // 2. 存入缓存，供下一帧作为 Previous 使用
        lastFrameBallPositions.push_back(pos);
    }

    for (int i = 0; i < StatePacket::MAX_POWERUPS; i++)
    {
        packet.poweruppositions[i] = {-1, -1};
    }

    // 获取当前数量
    packet.powerupCount = std::min((int)activePowerUps.size(), StatePacket::MAX_POWERUPS);

    // 遍历游戏中的列表，填充到数据包中
    for (int i = 0; i < packet.powerupCount; i++)
    {
        packet.poweruppositions[i] = activePowerUps[i]->GetPosition();
        packet.poweruptypes[i] = activePowerUps[i]->GetTypeNum();
    }

    packet.paddle1X = paddle1->paddleRect;
    if (paddle2)
        packet.paddle2X = paddle2->paddleRect;
    packet.lives = lives;
    packet.states = this->state;
    packet.bricksRemaining = bricksRemaining;
    packet.gameActive = (state == PLAYING);

    packet.brickCount = 0;
    // 遍历所有砖块
    for (int i = 0; i < bricks.size() && packet.brickCount < StatePacket::MAX_BRICKS; i++)
    {
        if (bricks[i].IsActive())
        {
            // 只有存活的砖块才同步
            packet.brickPositions[packet.brickCount] = bricks[i].GetRect();

            packet.brickCount++; // 增加存活计数
        }
    }

    // 广播给所有连接的客户端
    ENetPacket *packetToSend = enet_packet_create(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
    if (!packetToSend)
    {
        printf("Failed to create packet!\n");
        return;
    }
    enet_host_broadcast(netHost, 1, packetToSend);
}

void Game::HandleNetworkPackets()
{
    ENetEvent event;
    while (enet_host_service(netHost, &event, 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            printf("玩家连接!\n");
            isConnected = true;
            event.peer->timeoutLimit = 4096;
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            if (event.channelID == 0)
            { // 输入通道 (客户端 -> 主机)
                if (IsHost() && event.packet->dataLength == sizeof(InputPacket))
                {
                    InputPacket *input = (InputPacket *)event.packet->data;
                    // 主机接收到 P2 的输入，移动本地的 P2 挡板
                    if (paddle2)
                    {
                        // 简单同步，实际游戏中可能需要平滑插值
                        float targetX = input->paddleX;
                        // 简单限制
                        if (targetX < 0)
                            targetX = 0;
                        if (targetX + paddle2->GetRect().width > screenWidth)
                            targetX = screenWidth - paddle2->GetRect().width;
                        // 直接设置位置 (主机权威)
                        Rectangle tempRect = paddle2->GetRect();
                        tempRect.x = targetX;
                        paddle2->SetRect(tempRect);
                    }
                }
            }
            if (event.channelID == 1)
            { // 主机->客户端
                if (IsClient() && event.packet->dataLength == sizeof(StatePacket))
                {
                    StatePacket *state = (StatePacket *)event.packet->data;
                    // 客户端接收到主机状态，覆盖本地数据
                    while (balls.size() < state->ballCount)
                    {
                        // 获取第一个球的半径作为参考（假设所有球半径一致）
                        float radius = config.ballRadius;
                        if (!balls.empty())
                        {
                            radius = balls[0]->GetRadius(); // 参考主球
                        }
                        // 创建新球，初始速度设为 0，位置会在下面的循环中更新
                        // 注意：这里没有传入速度，因为我们只同步位置（状态同步）
                        balls.push_back(new Ball({0, 0}, {0, 0}, radius));
                    }

                    // 2. 如果接收到的球比本地少，移除多余的球
                    while (balls.size() > state->ballCount)
                    {
                        delete balls.back();
                        balls.pop_back();
                    }

                    // 3. 更新所有现存球的位置
                    for (int i = 0; i < state->ballCount && i < balls.size(); i++)
                    {
                        Vector2 targetPos = {state->ballX_Current[i], state->ballY_Current[i]};
                        Vector2 prevPos = {state->ballX_Previous[i], state->ballY_Previous[i]};

                        // 如果 Previous 是无效值 (-1)，则直接瞬移
                        if (prevPos.x < 0 || prevPos.y < 0)
                        {
                            balls[i]->SetPosition(targetPos.x, targetPos.y);
                            continue;
                        }

                        // --- 计算平滑移动 ---
                        // 方案：计算从 prevPos 到 targetPos 的向量，作为球的速度
                        // 这样球会沿着正确的轨迹移动

                        Vector2 desiredVelocity = {targetPos.x - prevPos.x, targetPos.y - prevPos.y};
                        // desiredVelocity 是这一帧应该移动的距离

                        // 获取 Ball 内部的速度变量 (需要通过指针操作)
                        // 我们限制一下最大速度
                        float maxInterpSpeed = 500.0f; // 限制插值速度，防止瞬间飞过屏幕
                        float len = sqrtf(desiredVelocity.x * desiredVelocity.x + desiredVelocity.y * desiredVelocity.y);

                        // 关键：设置球的位置为上一帧位置 (Previous)
                        // 设置球的速度为计算出的速度
                        // 在下一帧，如果没收到包，球会继续按这个速度飞，直到收到新包
                        balls[i]->SetPosition(prevPos.x, prevPos.y);
                        balls[i]->SetSpeed(desiredVelocity.x, desiredVelocity.y);
                    }

                    activePowerUps.clear();
                    for (int i = 0; i < state->powerupCount; i++)
                    {

                        // 注意：这里没有传入速度，因为我们只同步位置（状态同步）
                        PowerUp *pp;
                        //               std::cout<<state->poweruptypes[i]<<std::endl;
                        if (state->poweruptypes[i] == 0)
                            pp = PowerUpFactory::CreatePowerUp("grow", state->poweruppositions[i]);
                        if (state->poweruptypes[i] == 2)
                            pp = PowerUpFactory::CreatePowerUp("life", state->poweruppositions[i]);
                        if (state->poweruptypes[i] == 1)
                            pp = PowerUpFactory::CreatePowerUp("split", state->poweruppositions[i]);
                        if (pp == nullptr)
                            printf("pp=0\n");
                        else
                            activePowerUps.push_back(pp);
                    }

                    // 但是需要更新 P1 的位置
                    if (paddle1)
                    {
                        paddle1->paddleRect = state->paddle1X;
                    }
                    if (paddle2)
                    {
                        paddle2->paddleRect = state->paddle2X;
                    }
                    if (this->state != state->states)
                    {
                        this->state = state->states;
                        // 如果状态变为 PLAYING，可能需要重置某些UI元素，但不要重置整个游戏对象（由主机管理）
                    }
                    lives = state->lives;
                    bricksRemaining = state->bricksRemaining;
                    for (auto &brick : bricks)
                    {
                        brick.SetActive(false);
                    }
                    for (int i = 0; i < bricks.size() && i < state->brickCount; i++)
                    {
                        bricks[i].SetActive(true);
                        // 更新
                        bricks[i].SetPos(state->brickPositions[i]);
                    }
                }
            }
            enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            isConnected = false;
            break;
        }
    }
}

void Game::Render()
{
    BeginDrawing();

    if (isLoading)
    {
        // 加载过程中显示深灰色
        ClearBackground(DARKGRAY);
    }
    else
    {
        // 加载完成后或正常游戏，如果 loadComplete 为 true 则显示紫色
        // 注意：这里需要检查 loadComplete，但为了线程安全，我们用锁
        bool showPurple = false;
        {
            std::lock_guard<std::mutex> lock(gameMutex);
            showPurple = loadComplete;
        }

        if (showPurple)
        {
            ClearBackground(PURPLE); // 加载后变紫
        }
        else
        {
            ClearBackground(DARKGRAY); // 正常游戏背景
        }
    }

    // 根据状态绘制画面
    switch (state)
    {
    case MENU:
        DrawMenu();
        break;
    case CONNECTING:
        ClearBackground(DARKGRAY);
        DrawText("Connecting to server...", screenWidth / 2 - 100, screenHeight / 2, 20, YELLOW);
        DrawText("Please wait...", screenWidth / 2 - 60, screenHeight / 2 + 30, 20, LIGHTGRAY);
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
    case LEVEL_COMPLETE:
        DrawPlaying(); // 如果你想在结束画面看到最终状态，否则可以 DrawGameOver()
        DrawGameOver();
        break;
    }

    if (isLoading)
    {
        DrawText("Loading...", screenWidth / 2 - 50, screenHeight / 2, 40, YELLOW);
    }

    EndDrawing();
}

void Game::UpdateMenu()
{
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 mousePoint = GetMousePosition();

        if (currentMenuState == MAIN)
        {
            // 主菜单逻辑
            if (CheckCollisionPointRec(mousePoint, btnSingle))
            {
                config.gameMode = "SINGLEPLAYER";
                currentMenuState = SELECT_LEVEL;
                printf("模式切换：单人模式\n");
            }
            // 2. 检测是否点击“创建主机”
            else if (CheckCollisionPointRec(mousePoint, btnHost))
            {
                config.gameMode = "MULTIPLAYER_HOST";
                currentMenuState = SELECT_LEVEL;
            }
            // 3. 检测是否点击“加入游戏”
            else if (CheckCollisionPointRec(mousePoint, btnClient))
            {
                config.gameMode = "MULTIPLAYER_CLIENT";
                state = CONNECTING; // 进入连接中状态
                InitNetwork();      // 初始化 ENet 客户端
                printf("模式切换：尝试连接主机...\n");
            }
        }
        else if (currentMenuState == SELECT_LEVEL)
        {
            // 关卡选择逻辑
            if (CheckCollisionPointRec(mousePoint, btnLevel1) && config.levels.size() > 0)
            {
                if (config.gameMode == "MULTIPLAYER_HOST")
                    SetLevel(0);
                else
                    StartLevel(0);
            }
            else if (CheckCollisionPointRec(mousePoint, btnLevel2) && config.levels.size() > 1)
            {
                if (config.gameMode == "MULTIPLAYER_HOST")
                    SetLevel(1);
                else
                    StartLevel(1);
            }
            else if (CheckCollisionPointRec(mousePoint, btnLevel3) && config.levels.size() > 2)
            {
                if (config.gameMode == "MULTIPLAYER_HOST")
                    SetLevel(2);
                else
                    StartLevel(2);
            }
        }
    }
}

void Game::DrawMenu()
{
    if (currentMenuState == MAIN)
    {
        // 绘制标题
        DrawText("BRICK BREAKER", 250, 100, 40, DARKBLUE);

        DrawRectangleRec(btnExit, RED);
        DrawRectangleLinesEx(btnExit, 2, RAYWHITE);
        DrawText("EXIT", btnExit.x + btnExit.width / 2 - MeasureText("EXIT", 20) / 2, btnExit.y + 15, 20, RAYWHITE);

        // 单人模式 (绿色)
        DrawRectangleRec(btnSingle, GREEN);
        DrawRectangleLinesEx(btnSingle, 2, RAYWHITE);

        // 创建主机 (蓝色)
        DrawRectangleRec(btnHost, BLUE);
        DrawRectangleLinesEx(btnHost, 2, RAYWHITE);

        // 加入游戏 (橙色)
        DrawRectangleRec(btnClient, ORANGE);
        DrawRectangleLinesEx(btnClient, 2, RAYWHITE);

        // 3. 绘制按钮文字 (居中计算)
        // 单人
        const char *textSingle = "SINGLE PLAYER";
        DrawText(textSingle, btnSingle.x + btnSingle.width / 2 - MeasureText(textSingle, 20) / 2, btnSingle.y + 15, 20, RAYWHITE);

        // 主机
        const char *textHost = "HOST GAME";
        DrawText(textHost, btnHost.x + btnHost.width / 2 - MeasureText(textHost, 20) / 2, btnHost.y + 15, 20, RAYWHITE);

        // 客户端
        const char *textClient = "JOIN GAME";
        DrawText(textClient, btnClient.x + btnClient.width / 2 - MeasureText(textClient, 20) / 2, btnClient.y + 15, 20, RAYWHITE);

        // 4. 底部提示
        DrawText("Config loaded from config.json", 10, screenHeight - 20, 10, GRAY);
    }
    else
    {
        // 绘制关卡选择界面
        DrawText("SELECT LEVEL", 300, 100, 40, DARKBLUE);

        // 绘制关卡按钮 (根据实际关卡数量启用/禁用)
        DrawRectangleRec(btnLevel1, config.levels.size() > 0 ? GREEN : GRAY);
        DrawRectangleLinesEx(btnLevel1, 2, RAYWHITE);
        DrawText(config.levels[0].title.c_str(),
                 btnLevel1.x + 10, btnLevel1.y + 15, 20, RAYWHITE);

        DrawRectangleRec(btnLevel2, config.levels.size() > 1 ? GREEN : GRAY);
        DrawRectangleLinesEx(btnLevel2, 2, RAYWHITE);
        DrawText(config.levels[1].title.c_str(),
                 btnLevel2.x + 10, btnLevel2.y + 15, 20, RAYWHITE);

        DrawRectangleRec(btnLevel3, config.levels.size() > 2 ? GREEN : GRAY);
        DrawRectangleLinesEx(btnLevel3, 2, RAYWHITE);
        DrawText(config.levels[2].title.c_str(),
                 btnLevel3.x + 10, btnLevel3.y + 15, 20, RAYWHITE);
    }
}

void Game::StartLevel(int index)
{
    SetLevel(index);
    ResetGame();
    state = PLAYING;
}
void Game::SetLevel(int index)
{
    if (index < 0 || index >= config.levels.size())
        return;

    if ((config.gameMode == "MULTIPLAYER_HOST") && (!isConnected))
    {
        state = CONNECTING; // 进入连接中状态
        InitNetwork();      // 初始化 ENet 主机
        printf("模式切换：创建主机 (等待连接...)\n");
    }

    config.currentLevelIndex = index;

    // 应用当前关卡的配置
    auto &level = config.levels[index];

    // 重置球速
    config.ballStartSpeedY = level.ballStartSpeedY;
    // 重置生命值
    lives = level.initialLives;

    // 重置道具掉落率 (需要在 PowerUpFactory 中设置)
    PowerUpFactory::cfg.spawnChance = level.spawnChance;

    // 重置砖块布局
    config.brickRows = level.brickRows;
    config.brickCols = level.brickCols;
    config.brickWidth = level.brickWidth;
    config.brickHeight = level.brickHeight;
    config.brickSpacing = level.brickSpacing;
    config.customLayout = level.customLayout;
}

void Game::DrawPlaying()
{
    // 绘制游戏元素
    for (auto &ball : balls)
    {
        ball->Draw();
    }

    if (paddle1)
    {
        DrawRectangleRec(paddle1->paddleRect, BLUE);
    }
    if (paddle2 && !IsSinglePlayer())
    {
        DrawRectangleRec(paddle2->paddleRect, MAROON); // 用不同颜色区分
    }

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

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (particlePool[i].active)
        {
            // 使用 DrawCircleGradient 模拟光晕：中心亮，边缘透明
            // 这里的颜色是基础色，alpha 控制透明度
            Color colorWithAlpha = particlePool[i].color;
            colorWithAlpha.a = (unsigned char)(255 * particlePool[i].alpha);

            // 绘制多层圆圈来模拟光晕
            // 外层光晕 (大半径，低透明度)
            DrawCircleV(particlePool[i].position, particlePool[i].radius * 3, Fade(colorWithAlpha, 0.1f));
            DrawCircleV(particlePool[i].position, particlePool[i].radius * 2, Fade(colorWithAlpha, 0.3f));
            // 内核 (小半径，高亮度)
            DrawCircleV(particlePool[i].position, particlePool[i].radius, colorWithAlpha);
        }
    }

    // 绘制UI
    DrawText(TextFormat("HP: %i", lives), 60, 10, 20, RED);
    DrawText(TextFormat("Bricks: %i", bricksRemaining), 600, 10, 20, GREEN);

    DrawText(TextFormat("FPS: %2.0f", displayFps), 10, 570, 20, WHITE);

    // --- 新增：绘制屏幕左上角的暂停按钮 ---
    if (IsSinglePlayer() || IsHost())
    {
        DrawRectangleRec(btnPause, GRAY);
        DrawText("| |", 20, 18, 30, WHITE);
    }

    // 网络状态提示
    if (!IsSinglePlayer())
    {
        DrawText(TextFormat("Network: %s", isConnected ? "ONLINE" : "OFFLINE"), 10, 30, 10, isConnected ? GREEN : RED);
    }
}

void Game::DrawPaused()
{
    // 半透明遮罩
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));

    // 恢复按钮
    if (IsSinglePlayer() || IsHost())
    {
        DrawRectangleRec(btnResume, ORANGE);
        DrawRectangleLinesEx(btnResume, 2, RAYWHITE);
        DrawText("RESUME", btnResume.x + btnResume.width / 2 - MeasureText("RESUME", 20) / 2, btnResume.y + 15, 20, RAYWHITE);
    }
    // 退出到菜单按钮
    DrawRectangleRec(btnExit, ORANGE);
    DrawRectangleLinesEx(btnExit, 2, RAYWHITE);
    DrawText("MAIN MENU", btnExit.x + btnExit.width / 2 - MeasureText("MAIN MENU", 20) / 2, btnExit.y + 15, 20, RAYWHITE);
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
    // 返回菜单
    DrawRectangleRec(btnExit, BLUE);
    DrawRectangleLinesEx(btnExit, 2, RAYWHITE);
    DrawText("MAIN MENU", btnExit.x + btnExit.width / 2 - MeasureText("MAIN MENU", 20) / 2, btnExit.y + 15, 20, RAYWHITE);

    if (IsSinglePlayer() || IsHost())
    {
        // 重新开始
        DrawRectangleRec(btnStart, GREEN);
        DrawRectangleLinesEx(btnStart, 2, RAYWHITE);
        DrawText("RESTART", btnStart.x + btnStart.width / 2 - MeasureText("RESTART", 20) / 2, btnStart.y + 15, 20, RAYWHITE);

        if (showNextLevelButton)
        {
            DrawRectangleRec(btnNextLevel, ORANGE);
            DrawRectangleLinesEx(btnNextLevel, 2, RAYWHITE);
            DrawText("NEXT LEVEL", btnNextLevel.x + btnNextLevel.width / 2 - MeasureText("NEXT LEVEL", 20) / 2, btnNextLevel.y + 15, 20, RAYWHITE);
        }
    }
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
        DrawCircleV(position, radius * 4, Fade(WHITE, 0.1f * sin(GetTime() * 10))); // 脉动辉光
        DrawCircleV(position, radius * 2, Fade(YELLOW, 0.3f));

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

void Game::StartLoadingAsync()
{
    // 1. 创建一个 packaged_task，包装耗时的加载函数
    // 这里使用 lambda 表达式模拟耗时操作
    std::packaged_task<void()> task([this]()
                                    {
        // 模拟耗时加载（5秒）
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // 模拟加载完成后需要传递的数据
        // 这里我们直接修改 loadComplete，为了线程安全，使用锁
        std::lock_guard<std::mutex> lock(gameMutex);
        loadComplete = true; });

    // 2. 通过 async 启动异步任务
    // 将 packaged_task 的 future 保存下来以便查询状态
    loadingFuture = std::async(std::launch::async, std::move(task));

    // 3. 更新本地状态
    isLoading = true;
    loadComplete = false; // 重置状态
}