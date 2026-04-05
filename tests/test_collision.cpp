#include <gtest/gtest.h>
#include "raylib.h"
#include "../game.h" // 假设我们将 CheckCollisions 中的逻辑提取或设为 public 供测试

// 为了测试，我们创建一个简单的测试夹具（Test Fixture）
// 在实际工程中，你可能需要将碰撞检测逻辑提取到一个独立的 Utils 类或函数中
class CollisionTest : public ::testing::Test {
protected:
    // 模拟游戏对象（使用指针或结构体模拟）
    struct MockBall {
        Vector2 position;
        Vector2 speed;
        float radius;
        
        MockBall(float x, float y, float r, float sx, float sy) 
            : position{x, y}, speed{sx, sy}, radius(r) {}
    };

    struct MockPaddle {
        Rectangle rect;
        MockPaddle(float x, float y, float w, float h) 
            : rect{x, y, w, h} {}
    };

    // 模拟碰撞检测逻辑（复制自 Game::CheckCollisions 的核心部分）
    // 这是一个简化版，仅用于演示测试逻辑
    void SimulatePaddleCollision(MockBall& ball, const MockPaddle& paddle) {
        // 核心逻辑复刻
        bool isAbovePaddle = (ball.position.y + ball.radius) <= paddle.rect.y;
        bool isMovingDown = ball.speed.y > 0;

        // 仅当球在挡板上方且向下移动时才触发反弹
        if (isAbovePaddle && isMovingDown) {
            ball.speed.y = -ball.speed.y; // 反转Y速度
        }
    }
};

// 测试 1: 球从上方击中挡板，应发生垂直反弹
TEST_F(CollisionTest, BallBouncesWhenHittingPaddleTop) {
    // Arrange: 设置测试场景
    // 挡板位于屏幕底部中间
    MockPaddle paddle(350, 550, 100, 20); 
    
    // 球位于挡板正上方，即将接触，且正在向下移动
    // 球心 Y = 540, 挡板 Y = 550, 球半径 = 10 -> 540 + 10 = 550 (刚好接触)
    MockBall ball(400, 540, 10, 0, 5); // X, Y, Radius, SpeedX, SpeedY

    // Act: 执行碰撞检测逻辑
    SimulatePaddleCollision(ball, paddle);

    // Assert: 验证结果
    // 1. 球的 Y 速度应该变为负数（向上反弹）
    // 2. X 速度应保持不变
    EXPECT_LT(ball.speed.y, 0); // 速度变为负，表示向上
    EXPECT_FLOAT_EQ(ball.speed.y, -5); // 数值应为 -5
    EXPECT_FLOAT_EQ(ball.speed.x, 0); // X 速度不变
}

// 测试 2: 球从侧面穿过挡板（未击中顶部），不应发生反弹
TEST_F(CollisionTest, BallPassesThroughIfNotHittingTop) {
    // Arrange: 设置测试场景
    MockPaddle paddle(100, 550, 100, 20);
    
    // 球位于挡板的侧面（Y轴位置在挡板下方），且向下移动
    // 这模拟球错过了挡板，从侧面飞过
    MockBall ball(400, 560, 10, 0, 5); // Y=560 > 挡板Y=550

    // 保存原始速度用于对比
    float originalSpeedY = ball.speed.y;

    // Act: 执行碰撞检测逻辑
    SimulatePaddleCollision(ball, paddle);

    // Assert: 验证结果
    // 球的速度不应改变，球应该继续下落
    EXPECT_GT(ball.speed.y, 0); // 速度仍为正，继续向下
    EXPECT_FLOAT_EQ(ball.speed.y, originalSpeedY); // 速度值未变
}