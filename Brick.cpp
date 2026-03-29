#include "Brick.h"

Brick::Brick(float x, float y, float w, float h) {
    rect = { x, y, w, h };
    active = true;
}

void Brick::Draw() {
    if (active) {
        DrawRectangleRec(rect, GREEN);
    }
}
