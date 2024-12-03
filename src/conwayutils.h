#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <GL/glut.h>
#include <chrono>


GLfloat COLOR_BLUE[] = {0, 0, 1};
GLfloat COLOR_GREEN[] = {0, 1, 0};
GLfloat COLOR_RED[] = {1, 0, 0};
GLfloat COLOR_WHITE[] = {1, 1, 1};
GLfloat COLOR_BLACK[] = {0, 0, 0};
GLfloat COLOR_GREY[] = {0.2, 0.2, 0.2};


struct Coords {
    const int x;
    const int y;

    bool operator==(const Coords &other) const {
        return x == other.x && y == other.y;
    }
};

struct CoordsHash {
    std::size_t operator()(const Coords &coords) const {
        int h = coords.x ^ coords.y * 0x9e3779b9;
        return h ^ h >> 16;
    }
};

void drawSquare(float x, float y, float size) {
    glBegin(GL_QUADS);
    glVertex2f(x, y); // bottom-left
    glVertex2f(x + size, y);
    glVertex2f(x + size, y + size);
    glVertex2f(x, y + size);
    glEnd();
}

inline int floorDivToNegInfinity(int a, int b) {
    return a / b - (a < 0 != b < 0 && a % b);
}

inline Coords worldCoordsToCell(int x, int y, int cellSize) {
    return {
        floorDivToNegInfinity(x, cellSize),
        floorDivToNegInfinity(y, cellSize),
    };
}

inline Coords mouseToWorldCoords(int x, int y, int worldOffsetX, int worldOffsetY) {
    return {
        x - worldOffsetX,
        y - worldOffsetY
    };
}

inline std::array<Coords, 8> getNeighbors(const Coords &cell) {
    return {
        {
            {cell.x - 1, cell.y - 1},
            {cell.x - 1, cell.y},
            {cell.x - 1, cell.y + 1},
            {cell.x, cell.y - 1},
            {cell.x, cell.y + 1},
            {cell.x + 1, cell.y - 1},
            {cell.x + 1, cell.y},
            {cell.x + 1, cell.y + 1},
        }
    };
}

std::vector<Coords> generateLine(int x1, int y1, int x2, int y2) {
    std::vector<Coords> res;

    int dx = x2 - x1, dy = y2 - y1;

    int max_steps = std::max(std::abs(dx), std::abs(dy));
    float step_x = dx / static_cast<float>(max_steps), step_y = dy / static_cast<float>(max_steps);

    float x = x1, y = y1;

    for (int i = 0; i < max_steps; i++) {
        res.push_back({static_cast<int>(std::round(x)), static_cast<int>(std::round(y))});
        x += step_x, y += step_y;
    }

    res.push_back({x2, y2});
    return res;
}

#endif //UTILS_H
