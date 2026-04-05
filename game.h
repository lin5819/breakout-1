// File: Game.h
#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include <vector>
#include <string>


struct GameConfig {
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

enum GameState { MENU, PLAYING, PAUSED, GAMEOVER };

class Game {
private:
    GameState state;
    GameConfig config;

    Rectangle btnStart;
    Rectangle btnExit;
    Rectangle btnResume;
    Rectangle btnPause;

public:
    // --- 游戏状态 ---
    bool running;
    
    // --- 游戏对象 ---
    Ball* ball;
    Paddle* paddle;
    std::vector<Brick> bricks;
    
    // --- 游戏数据 ---
    int lives;
    int bricksRemaining;
    int screenWidth;
    int screenHeight;

    // --- 私有方法 (逻辑处理) ---
    void InitGame();          // 初始化游戏数据和对象
    void ProcessInput();      // 处理键盘输入
    void UpdateGame();        // 更新球的位置、碰撞检测等
    void CheckCollisions();   // 专门处理碰撞的子函数
    void Render();            // 负责所有绘制工作

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


    Game(int width = 800, int height = 600);
    ~Game(); // 记得释放 new 出来的 ball 和 paddle

};

#endif