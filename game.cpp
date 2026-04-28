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

        // 读取 Game 配置
        config.initialLives = data["game"]["initial_lives"];

        // 读取 Ball 配置
        config.ballRadius = data["ball"]["radius"];
        config.ballStartSpeedX = data["ball"]["start_speed_x"];
        config.ballStartSpeedY = data["ball"]["start_speed_y"];

        Ball::addspeedx = data["ball"]["addspeedx"];
        Ball::maxspeedx = data["ball"]["maxspeedx"];
        Ball::randspeedx = data["ball"]["randspeedx"];

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
    : screenWidth(width), screenHeight(height), running(true), bricksRemaining(0), ball(nullptr), activeBuff(""), buffTimer(0), netHost(nullptr), netPeer(nullptr),
      isNetworkMode(false), isConnected(false),
      paddle1(nullptr), paddle2(nullptr)
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
    int btnWidth = 200;
    int btnHeight = 50;
    int centerX = screenWidth / 2 - btnWidth / 2;

    // 垂直排列，间距 20
    btnSingle = (Rectangle){(float)centerX, 400, (float)btnWidth, (float)btnHeight};
    btnHost = (Rectangle){(float)centerX, 470, (float)btnWidth, (float)btnHeight};
    btnClient = (Rectangle){(float)centerX, 540, (float)btnWidth, (float)btnHeight};
}

Game::~Game()
{
    //    ShutdownNetwork();
    delete ball;
    delete paddle1;
    delete paddle2;
    // vector 会自动释放
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
    for (auto &ball : balls)
    {
        ball->Randspeedx();
    }
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
                Vector2 vel = {cosf(angle) * speed, (float)GetRandomValue(-50, 50)};
                // 使用球的颜色 (RED)
                particles.emplace_back(spawnPos, vel, 0.3f, RED, 1.5f);
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
                Vector2 vel = {(float)GetRandomValue(-50, 50), speed};
                particles.emplace_back(spawnPos, vel, 0.3f, RED, 1.5f);
            }
            bounced = true;
        }
        for (auto &brick : bricks)
        {
            if (!brick.IsActive())
                continue;
            if (CheckCollisionCircleRec(ballPos, ballRadius, brick.GetRect()))
            {
                Vector2 brickCenter = {
                    brick.GetRect().x + brick.GetRect().width / 2,
                    brick.GetRect().y + brick.GetRect().height / 2};

                int count = GetRandomValue(5, 10);
                for (int i = 0; i < count; i++)
                {
                    float angle = GetRandomValue(0, 360) * 3.14159f / 180.0f;
                    float speed = GetRandomValue(50, 150);
                    Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
                    // 使用砖块的绿色 (GREEN)
                    particles.emplace_back(brickCenter, vel, 0.5f, GREEN, 2.0f);
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
                            if (GetRandomValue(1, 100) <= powerUpCfg.spawnChance)
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

                            ball->ReverseYSpeed(); // 简单处理
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

    if (state == GAMEOVER)
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
            state = GAMEOVER;
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
                }
            }
        }
        // 注意：原来的 UpdatePowerUps, CheckCollisions 等调用保持不变，不要放在这里面

        for (auto it = particles.begin(); it != particles.end();)
        {
            it->Update(GetFrameTime());
            it->CheckWallBounce(screenWidth, screenHeight); // 关键：让粒子碰壁反弹
            if (!it->IsAlive())
            {
                it = particles.erase(it);
            }
            else
            {
                ++it;
            }
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
        packet.ballX[i] = -1; // 用 -1 标记无效位置，防止客户端渲染乱码
        packet.ballY[i] = -1;
    }

    // 获取当前球数量
    packet.ballCount = std::min((int)balls.size(), StatePacket::MAX_BALLS);

    // 遍历游戏中的球列表，填充到数据包中
    for (int i = 0; i < packet.ballCount; i++)
    {
        Vector2 pos = balls[i]->GetPosition();
        packet.ballX[i] = pos.x;
        packet.ballY[i] = pos.y;
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

    // --- 新增：打包砖块状态 ---
    packet.brickCount = std::min((int)bricks.size(), StatePacket::MAX_BRICKS);
    for (int i = 0; i < packet.brickCount; i++)
    {
        packet.brickStates[i] = bricks[i].IsActive(); // true=存在, false=已碎
    }
    // 如果砖块少于 MAX_BRICKS，后面的值无所谓；如果多于，会被截断（需确保配置不会超过）

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
                        // 直接重置位置，不改变速度（或者也可以重置速度，取决于网络抖动情况）
                        // 这里选择只重置位置，让客户端物理继续运行，减少卡顿感
                        if (state->ballX[i] >= 0 && state->ballY[i] >= 0)
                        { // 有效性检查
                            balls[i]->SetPosition(state->ballX[i], state->ballY[i]);
                        }
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
                    for (int i = 0; i < bricks.size() && i < state->brickCount; i++)
                    {
                        // 只有当状态不一致时才更新，防止频繁触发粒子或音效
                        if (bricks[i].IsActive() != state->brickStates[i])
                        {
                            bricks[i].SetActive(state->brickStates[i]);
                        }
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
    ClearBackground(DARKGRAY);

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
    }

    EndDrawing();
}

void Game::UpdateMenu()
{
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 mousePoint = GetMousePosition();

        if (CheckCollisionPointRec(mousePoint, btnSingle))
        {
            config.gameMode = "SINGLEPLAYER";
            state = PLAYING;
            ResetGame(); // 重置游戏状态
            printf("模式切换：单人模式\n");
        }
        // 2. 检测是否点击“创建主机”
        else if (CheckCollisionPointRec(mousePoint, btnHost))
        {
            config.gameMode = "MULTIPLAYER_HOST";
            state = CONNECTING; // 进入连接中状态
            InitNetwork();      // 初始化 ENet 主机
            printf("模式切换：创建主机 (等待连接...)\n");
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

    for (auto &p : particles)
    {
        // 使用 DrawCircleGradient 模拟光晕：中心亮，边缘透明
        // 这里的颜色是基础色，alpha 控制透明度
        Color colorWithAlpha = p.color;
        colorWithAlpha.a = (unsigned char)(255 * p.alpha);

        // 绘制多层圆圈来模拟光晕
        // 外层光晕 (大半径，低透明度)
        DrawCircleV(p.position, p.radius * 3, Fade(colorWithAlpha, 0.1f));
        DrawCircleV(p.position, p.radius * 2, Fade(colorWithAlpha, 0.3f));
        // 内核 (小半径，高亮度)
        DrawCircleV(p.position, p.radius, colorWithAlpha);
    }

    // 绘制UI
    DrawText(TextFormat("HP: %i", lives), 60, 10, 20, RED);
    DrawText(TextFormat("Bricks: %i", bricksRemaining), 600, 10, 20, GREEN);

    // --- 新增：绘制屏幕左上角的暂停按钮 ---
    DrawRectangleRec(btnPause, GRAY);
    DrawText("| |", 20, 18, 30, WHITE);

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