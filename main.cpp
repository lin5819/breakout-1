#include "game.h"

int main() {
    // 创建游戏实例并运行
    Game game(800, 600);
    
    game.InitGame(); // 调用初始化
    
    // 主循环
    while (!WindowShouldClose() && game.running) {
        game.UpdateGame();
        game.Render();
    }

    // 结局画面逻辑
    // 注意：这里为了保持逻辑完整，也可以移入 Render，但为了清晰暂时放在这里
    

    CloseWindow();

    return 0;
}