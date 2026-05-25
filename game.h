// File: Game.h
#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include <vector>
#include <string>
#include <algorithm>
#include <future>
#include <mutex>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
extern "C"
{
#include <enet/enet.h>
}

enum GameState
{
    MENU,
    PLAYING,
    PAUSED,
    LEVEL_COMPLETE,
    GAMEOVER,
    CONNECTING
};

// --- 粒子系统定义 ---
struct Particle
{
    Vector2 position;
    Vector2 velocity;
    float alpha;
    float lifeTime;
    float maxLifeTime;
    Color color;
    float radius;
    bool active;

    Particle() : active(false) {}

    void Reset(Vector2 pos, Vector2 vel, float life, Color col, float r)
    {
        // 重置数据，而不是重新构造
        position = pos;
        velocity = vel;
        lifeTime = life;
        maxLifeTime = life;
        color = col;
        radius = r;
        alpha = 1.0f;
        active = true; // 激活
    }

    void Update(float deltaTime)
    {
        if (!active)
            return;
        position.x += velocity.x * deltaTime;
        position.y += velocity.y * deltaTime;
        velocity.y += 100.0f * deltaTime; // 模拟重力
        lifeTime -= deltaTime;
        alpha = lifeTime / maxLifeTime; // 随着生命减少而变透明
        if (lifeTime <= 0)
            active = false; // 死亡时不释放内存，仅标记
    }

    void CheckWallBounce(int screenWidth, int screenHeight)
    {
        // 左右边界反弹
        if ((position.x - radius <= 0 && velocity.x < 0) ||
            (position.x + radius >= screenWidth && velocity.x > 0))
        {
            velocity.x *= -0.8f; // 反弹并损失能量
            position.x = std::clamp(position.x, radius, screenWidth - radius);
        }
        // 上下边界反弹
        if ((position.y - radius <= 0 && velocity.y < 0) ||
            (position.y + radius >= screenHeight && velocity.y > 0))
        {
            velocity.y *= -0.8f;
            position.y = std::clamp(position.y, radius, screenHeight - radius);
        }
    }

    bool IsAlive() { return lifeTime > 0; }
};

// --- 关卡数据结构 ---
struct LevelData
{
    // 1. 砖块配置 (对应 JSON 中的 bricks)
    int brickRows;
    int brickCols;
    float brickWidth;
    float brickHeight;
    float brickSpacing;
    std::vector<std::string> customLayout; // 存储关卡布局字符串
    Color brickColor;                      // 新增：存储该关卡砖块的颜色

    // 2. 游戏基础配置 (对应 JSON 中的 game)
    int initialLives; // 每关开始时的初始生命值

    // 3. 球的配置 (对应 JSON 中的 ball)
    float ballStartSpeedY; // 仅 Y 轴速度随关卡变化 (X 轴通常为 0 或随机)

    // 4. 道具配置 (对应 JSON 中的 powerup)
    int spawnChance; // 道具掉落概率

    // 5. 关卡标题 (对应 JSON 中的 title)
    std::string title;

    // 构造函数，初始化默认值
    LevelData()
        : brickRows(0), brickCols(0), brickWidth(0), brickHeight(0),
          brickSpacing(0), initialLives(0), ballStartSpeedY(0),
          spawnChance(0), brickColor(WHITE) {}
};

struct GameConfig
{
    // Window
    int screenWidth = 800;
    int screenHeight = 600;
    std::string title = "Brick Breaker";
    int targetFps = 60;

    // Game
    int initialLives = 5;

    // Ball
    float ballRadius = 10.0f;
    float ballStartSpeedX = 3.0f;
    float ballStartSpeedY = 3.0f;

    // Paddle
    float paddleWidth = 100.0f;
    float paddleHeight = 20.0f;
    float paddleSpeed = 5.0f; // 注意：Paddle 类内部逻辑需要支持传入 speed

    // Bricks
    int brickRows = 5;
    int brickCols = 7;
    float brickWidth = 90.0f;
    float brickHeight = 30.0f;
    float brickSpacing = 20.0f;
    std::vector<std::string> customLayout;

    // Network
    int port = 12345;
    std::string hostIp = "127.0.0.1";
    std::string gameMode = "SINGLEPLAYER"; // "SINGLEPLAYER", "MULTIPLAYER_HOST", "MULTIPLAYER_CLIENT"

    int currentLevelIndex;         // 当前关卡索引 (0-based)
    int totalLevels;               // 总关卡数
    std::vector<LevelData> levels; // 存储所有关卡的数据
};
// 前向声明 (Forward Declarations)，因为我们还没有包含具体的头文件
// 或者你可以选择在这里 include "Ball.h" "Paddle.h" "Brick.h"
class Ball;
class Paddle;
class Brick;

// 网络包类型
enum PacketType
{
    PACKET_INPUT, // 客户端发给主机：我的挡板位置
    PACKET_STATE  // 主机发给客户端：球和所有挡板的位置
};

// 输入数据包 (由客户端发送)
struct InputPacket
{
    float paddleX; // P2 挡板的期望位置
    bool button;   // 预留
};

// 状态数据包 (由主机广播)
struct StatePacket
{
    static const int MAX_BALLS = 20; // 假设一局游戏最多同时存在 20 个球
    float ballX_Current[MAX_BALLS];
    float ballY_Current[MAX_BALLS];
    float ballX_Previous[MAX_BALLS];
    float ballY_Previous[MAX_BALLS];
    // 或者直接传输数量，客户端根据数量渲染前 N 个
    int ballCount; // 新增：当前实际存在的球数量

    static const int MAX_POWERUPS = 20;
    Vector2 poweruppositions[MAX_POWERUPS];
    int poweruptypes[MAX_POWERUPS];
    int powerupCount;

    Rectangle paddle1X; // P1 挡板
    Rectangle paddle2X; // P2 挡板
    int lives;
    int bricksRemaining;
    bool gameActive;
    static const int MAX_BRICKS = 100;
    Rectangle brickPositions[MAX_BRICKS];
    int brickCount; // 实际砖块数量，防止越界
    GameState states;
};

struct PowerUpConfig
{
    int spawnChance;
    float fallSpeed;
    float growDuration;
    float growAnimationTime;
    float growFactor;
    int extraLives;
    int splitCount;
    int size;
    // 颜色存储为 Vector4 或直接处理
    Color colorGrow;
    Color colorSplit;
    Color colorLife;
    std::string symbolGrow;
    std::string symbolSplit;
    std::string symbolLife;
};

// --- 前向声明 ---
class PowerUp;
class Game;
// --- 道具基类 ---
class PowerUp
{
protected:
    Vector2 position;
    Vector2 velocity;
    Color color;
    std::string type;
    bool active;
    float radius;
    std::string symbol;

public:
    PowerUp(Vector2 pos, float radius, Color col, std::string type, std::string symbol);
    virtual ~PowerUp();

    virtual void Update(float deltaTime);
    virtual void Draw();
    virtual void ApplyEffect(Game *game) = 0; // 纯虚函数

    bool IsActive() const { return active; }
    void SetActive(bool a) { active = a; }
    Rectangle GetBounds() const { return {position.x - radius, position.y - radius, radius * 2, radius * 2}; }
    std::string GetType() const { return type; }
    int GetTypeNum()
    {
        if (type == "grow")
        {
            return 0;
        }
        else if (type == "split")
        {
            return 1;
        }
        else if (type == "life")
        {
            return 2;
        }
        else
        {
            return -1;
        }
    }

    Vector2 GetPosition() { return position; }
    float GetRadius() { return radius; }
    Color GetColor() { return color; }
    void SetPosition(Vector2 a) { position = a; }
};

// --- 具体道具类声明 ---
class GrowPaddlePowerUp : public PowerUp
{
private:
    float originalWidth;
    float targetWidth;
    float duration;
    float timer;
    bool isGrowing; // 标记是正在生效还是正在恢复

public:
    GrowPaddlePowerUp(Vector2 pos, float radius, Color col, std::string symbol, float duration);
    void ApplyEffect(Game *game) override;
    void Update(float deltaTime) override;
    void SetOriginalSize(float width);
};

class SplitBallPowerUp : public PowerUp
{
public:
    SplitBallPowerUp(Vector2 pos, float radius, Color col, std::string symbol);
    void ApplyEffect(Game *game) override;
};

class ExtraLifePowerUp : public PowerUp
{
public:
    ExtraLifePowerUp(Vector2 pos, float radius, Color col, std::string symbol);
    void ApplyEffect(Game *game) override;
};

// --- 工厂类声明 ---
class PowerUpFactory
{
private:
public:
    static PowerUpConfig cfg;
    static PowerUp *CreatePowerUp(std::string type, Vector2 pos);
};

class Game
{
private:
    std::future<void> loadingFuture;
    mutable std::mutex gameMutex;
    bool isLoading;
    bool loadComplete;
    bool showNextLevelButton = 0;

    enum MenuState
    {
        MAIN,
        SELECT_LEVEL
    }; // 菜单的子状态
    MenuState currentMenuState;

    GameState state;

    Rectangle btnStart;
    Rectangle btnSingle; // 单人模式按钮
    Rectangle btnHost;   // 创建主机按钮
    Rectangle btnClient; // 加入游戏按钮
    Rectangle btnExit;
    Rectangle btnResume;
    Rectangle btnPause;
    Rectangle btnLevel1;
    Rectangle btnLevel2;
    Rectangle btnLevel3;
    Rectangle btnNextLevel; // 通关后的下一关按钮

    float displayFps;     // 这个是真正显示在屏幕上的数值（每秒更新一次）
    float fpsAccumulator; // 累计时间（秒）
    int frameCount;       // 累计帧数

    PowerUpConfig powerUpCfg;

    std::vector<Vector2> lastFrameBallPositions;

    // 新增道具管理

    // 修改球的管理，以支持多球
    // 将单个 Ball* 改为 vector，方便管理主球和分裂球
    // 如果不想改结构，可以用一个主球加一个额外球vector，这里为了逻辑清晰改用vector

    // 在 Game 类的 private 区域添加
    static const int MAX_PARTICLES = 1000; // 限制最大数量，防止无限申请
    Particle particlePool[MAX_PARTICLES];  // 预分配的内存池
    int activeParticleCount;               // 当前活跃粒子数（用于优化遍历）

public:
    GameConfig config;
    std::vector<PowerUp *> activePowerUps;
    std::string activeBuff; // 记录当前生效的增益类型，如 "grow"
    float buffTimer;        // 增益计时器
    std::vector<Ball *> balls;
    // --- 游戏状态 ---
    bool running;

    // --- 游戏对象 ---
    Ball *ball;
    std::vector<Brick> bricks;

    // --- 游戏数据 ---
    int lives;
    int bricksRemaining;
    int screenWidth;
    int screenHeight;

    // 网络核心组件
    ENetHost *netHost;
    ENetPeer *netPeer; // 用于客户端或主机连接特定对等体

    // 游戏状态
    bool isNetworkMode;
    bool isConnected;

    // 双人游戏对象 (单人模式下 P2 不可见或不生效)
    Paddle *paddle1;
    Paddle *paddle2;

    // 网络逻辑函数
    void InitNetwork();
    void ShutdownNetwork();
    void SendInputPacket();
    void BroadcastStatePacket();
    void HandleNetworkPackets();

    bool IsSinglePlayer() { return config.gameMode == "SINGLEPLAYER"; }
    bool IsHost() { return config.gameMode == "MULTIPLAYER_HOST"; }
    bool IsClient() { return config.gameMode == "MULTIPLAYER_CLIENT"; }

    // --- 私有方法 (逻辑处理) ---
    void InitGame();        // 初始化游戏数据和对象
    void ProcessInput();    // 处理键盘输入
    void UpdateGame();      // 更新球的位置、碰撞检测等
    void CheckCollisions(); // 专门处理碰撞的子函数
    void Render();          // 负责所有绘制工作

    void UpdateMenu();
    void DrawMenu();
    void UpdatePlaying();
    void DrawPlaying();
    void UpdatePaused();
    void DrawPaused();
    void UpdateGameOver();
    void DrawGameOver();

    void InitWindow(); // <--- 新增：专门负责窗口初始化
    void ResetGame();  // <--- 新增：专门负责重置球、拍、砖块等数据

    void LoadConfig();

    void LoadPowerUpConfig(); // 声明
    void UpdatePowerUps(float deltaTime);
    void CheckPowerUpCollision();
    void ResetBalls();

    void StartLoadingAsync();

    void UpdateLevelSelect();                  // 更新关卡选择界面
    void DrawLevelSelect();                    // 绘制关卡选择界面
    void LoadLevelsFromJson(const json &data); // 专门加载关卡数据的函数
    void StartLevel(int index);                // 开始指定关卡
    void SetLevel(int index);

    Game(int width = 800, int height = 600);
    ~Game(); // 记得释放 new 出来的 ball 和 paddle
};

#endif