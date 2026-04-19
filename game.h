// File: Game.h
#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include <vector>
#include <string>
#include <algorithm>

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

    Particle(Vector2 pos, Vector2 vel, float life, Color col, float r)
        : position(pos), velocity(vel), lifeTime(life), maxLifeTime(life), color(col), radius(r), alpha(1.0f) {}

    void Update(float deltaTime)
    {
        position.x += velocity.x * deltaTime;
        position.y += velocity.y * deltaTime;
        velocity.y += 100.0f * deltaTime; // 模拟重力
        lifeTime -= deltaTime;
        alpha = lifeTime / maxLifeTime; // 随着生命减少而变透明
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
};
// 前向声明 (Forward Declarations)，因为我们还没有包含具体的头文件
// 或者你可以选择在这里 include "Ball.h" "Paddle.h" "Brick.h"
class Ball;
class Paddle;
class Brick;

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

    Vector2 GetPosition() { return position; }
    float GetRadius() { return radius; }
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

enum GameState
{
    MENU,
    PLAYING,
    PAUSED,
    GAMEOVER
};

class Game
{
private:
    GameState state;

    Rectangle btnStart;
    Rectangle btnExit;
    Rectangle btnResume;
    Rectangle btnPause;

    PowerUpConfig powerUpCfg;

    // 新增道具管理

    // 修改球的管理，以支持多球
    // 将单个 Ball* 改为 vector，方便管理主球和分裂球
    // 如果不想改结构，可以用一个主球加一个额外球vector，这里为了逻辑清晰改用vector

    // 在 Game 类的 private 区域添加
    std::vector<Particle> particles;

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
    Paddle *paddle;
    std::vector<Brick> bricks;

    // --- 游戏数据 ---
    int lives;
    int bricksRemaining;
    int screenWidth;
    int screenHeight;

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
    void ResetBalls(); // 需要重构 ResetGame 中的球初始化逻辑

    Game(int width = 800, int height = 600);
    ~Game(); // 记得释放 new 出来的 ball 和 paddle
};

#endif