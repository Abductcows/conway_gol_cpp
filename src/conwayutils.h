#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <GL/glut.h>

template <typename T> 
std::vector<T> generateLine(int x1, int y1, int x2, int y2) {
    std::vector<T> res;

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

GLfloat COLOR_BLUE[] = {0, 0, 1};
GLfloat COLOR_GREEN[] = {0, 1, 0};
GLfloat COLOR_RED[] = {1, 0, 0};
GLfloat COLOR_WHITE[] = {1, 1, 1};
GLfloat COLOR_BLACK[] = {0, 0, 0};
GLfloat COLOR_GREY[] = {0.2, 0.2, 0.2};

#endif //UTILS_H
