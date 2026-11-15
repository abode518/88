// overlay.cpp — ImDrawList draw helper implementations

#include "overlay.h"
#include "imgui.h"

void Overlay::drawBox(ImDrawList* dl, Vec2 pos, float w, float h, uint32_t col, float thickness) {
    dl->AddRect({pos.x, pos.y}, {pos.x+w, pos.y+h},
                col, 0.f, 0, thickness);
}

void Overlay::drawBoxFilled(ImDrawList* dl, Vec2 pos, float w, float h, uint32_t col) {
    dl->AddRectFilled({pos.x, pos.y}, {pos.x+w, pos.y+h}, col);
}

void Overlay::drawLine(ImDrawList* dl, Vec2 a, Vec2 b, uint32_t col, float thickness) {
    dl->AddLine({a.x, a.y}, {b.x, b.y}, col, thickness);
}

void Overlay::drawText(ImDrawList* dl, Vec2 pos, uint32_t col, const std::string& text) {
    // shadow for readability
    dl->AddText({pos.x+1, pos.y+1}, 0xFF000000, text.c_str());
    dl->AddText({pos.x,   pos.y},   col,         text.c_str());
}

void Overlay::drawCircle(ImDrawList* dl, Vec2 center, float radius, uint32_t col, int segs) {
    dl->AddCircle({center.x, center.y}, radius, col, segs);
}

void Overlay::drawHealthBar(ImDrawList* dl, Vec2 boxPos, float boxH, float hp, float maxHp) {
    float ratio  = (maxHp > 0.f) ? (hp / maxHp) : 0.f;
    float barH   = boxH * ratio;
    float x      = boxPos.x - 6.f;
    float yFull  = boxPos.y;
    float yBar   = boxPos.y + boxH - barH;

    // bg
    dl->AddRectFilled({x, yFull}, {x+4, yFull+boxH}, 0xAA000000);
    // health color: green → yellow → red
    uint32_t col;
    if (ratio > 0.6f)       col = 0xFF44FF44;
    else if (ratio > 0.3f)  col = 0xFF44CCFF;
    else                     col = 0xFF4444FF;
    dl->AddRectFilled({x, yBar}, {x+4, yFull+boxH}, col);
}
