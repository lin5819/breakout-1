#include "game.h"

// File: main.cpp
int main() {
    Game game(800, 600);
    game.InitGame(); // 注意：现在的 InitGame 可能只初始化窗口，或者由构造函数处理
    
    while (!WindowShouldClose() && game.running) {
        game.ProcessInput(); // 状态机在此处处理输入
        game.UpdateGame();  // 状态机在此处处理更新
        game.Render();      // 状态机在此处处理渲染
    }
    
    CloseWindow();
    return 0;
}