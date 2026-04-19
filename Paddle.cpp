#include "Paddle.h"

Paddle::Paddle(float x, float y, float w, float h, float s) {
    rect = { x, y, w, h };
    speed = s; // 初始化速度
}

void Paddle::Draw(Rectangle paddleRect) {
    DrawRectangleRec(paddleRect, BLUE);
}

void Paddle::MoveLeft() {
    rect.x -= speed;
    if (rect.x < 0) rect.x = 0;
}

void Paddle::MoveRight() {
    rect.x += speed;
    if (rect.x + rect.width > GetScreenWidth())
        rect.x = GetScreenWidth() - rect.width;
}