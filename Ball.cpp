#include "Ball.h"

Ball::Ball(Vector2 pos, Vector2 sp, float r)
{
    position = pos;
    speed = sp;
    radius = r;
}

void Ball::Move()
{
    position.x += speed.x;
    position.y += speed.y;
}

void Ball::Draw()
{
    DrawCircleV(position, radius, RED);
}

// void Ball::BounceEdge(int screenWidth, int screenHeight) {
//     // 左右边界
//     if (position.x - radius <= 0 || position.x + radius >= screenWidth) {
//         speed.x *= -1;
//     }
//     // 上边界
//     if (position.y - radius <= 0) {
//         speed.y *= -1;
//     }
//     // 下边界暂时不处理
// }

// 修改后的 Ball::BounceEdge 函数
void Ball::BounceEdge(int screenWidth, int screenHeight)
{
    // 左右边界
    if (position.x - radius <= 0 || position.x + radius >= screenWidth)
    {
        speed.x *= -1;
    }
    // 上边界
    if (position.y - radius <= 0)
    {
        speed.y *= -1;
    }
}
// 下边界逻辑已删除，球会掉出屏幕
void Ball::Randspeedx()
{
    speed.x += GetRandomValue(-randspeedx, randspeedx);
    if (speed.x >= maxspeedx)
    {
        speed.x = maxspeedx;
    }
    if (speed.x <= -maxspeedx)
    {
        speed.x = -maxspeedx;
    }
}

void Ball::Addspeedx()
{
    if (IsKeyDown(KEY_LEFT))
        speed.x -= addspeedx;
    if (IsKeyDown(KEY_RIGHT))
        speed.x += addspeedx;
}

void Ball::SetPosition(float x, float y)
{
    position={x,y};
}