#include <cstdlib>
#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>
#include <stack>

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <array>

#include <GL/glut.h>
#include <GL/gl.h>

#include <thread>
#include <atomic>
#include "conwayutils.h"
#include "GL/freeglut_ext.h"

GLint cellSize = 50, targetFps = 10;
GLint ww, wh;

std::atomic_bool needsRedraw{true}, generationPaused{false};
std::mutex liveCellsLock;
bool workerShutdown = false;

struct TickCountState {
    GLint maxFrameRate = 1'000 / targetFps, maxTickRate = 1'000 / targetFps;
    GLint fps = 0, tps = 0;
    GLdouble lastFrameTime = 0, lastTickTime = 0;
    GLint frameCount = 0, tickCount = 0;
    GLdouble frameTimeAccumulator = 0.0, tickTimeAccumulator = 0.0;
} ticks;

struct MouseState {
    bool lmbDown{}, mmbDown{};
} mouseState;

struct DrawState {
    GLint prevDrawX{}, prevDrawY{};
} drawState;

struct GridMoveState {
    GLint xOffset{}, yOffset{};
    GLint prevX{}, prevY{};
} gridMoveState;

struct Coords {
    const int x;
    const int y;

    bool operator==(const Coords &other) const {
        return x == other.x && y == other.y;
    }
};

struct CoordsHash {
    std::size_t operator()(const Coords &coords) const {
        int h = coords.x ^ (coords.y * 0x9e3779b9);
        return h ^ (h >> 16);
    }
};

std::unordered_set<Coords, CoordsHash> livingCells;

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

void nextGeneration() {
    std::unordered_set<Coords, CoordsHash> nextGen;
    std::unordered_map<Coords, int, CoordsHash> deadToAliveNeighbors(livingCells.size());

    for (auto &cell: livingCells) {
        auto neighbors = getNeighbors(cell);

        int deadNeighborCount = 0;
        for (auto &neighbor: neighbors) {
            if (!livingCells.contains(neighbor)) {
                ++deadNeighborCount;
                auto &aliveNeighborCount = deadToAliveNeighbors[neighbor];
                ++aliveNeighborCount;
            }
        }

        int aliveNeighborCount = neighbors.size() - deadNeighborCount;
        if (aliveNeighborCount == 2 || aliveNeighborCount == 3) {
            nextGen.insert(cell);
        }
    }

    for (const auto &[deadCell, aliveNeighborCount]: deadToAliveNeighbors) {
        if (aliveNeighborCount == 3) {
            nextGen.insert(deadCell);
        }
    }

    while (!liveCellsLock.try_lock());
    livingCells.swap(nextGen);
    needsRedraw.store(true);
    liveCellsLock.unlock();
}

void drawSquare(float x, float y, float size) {
    glBegin(GL_QUADS); // Start drawing a quadrilateral
    glVertex2f(x, y); // Bottom-left corner
    glVertex2f(x + size, y); // Bottom-right corner
    glVertex2f(x + size, y + size); // Top-right corner
    glVertex2f(x, y + size); // Top-left corner
    glEnd();
}

void updateTitle() {
    auto base = std::string("Conway's Game of Life");
    if (generationPaused.load()) base += " | PAUSED | ";
    base += " FPS: " + std::to_string(ticks.fps);
    base += " TPS: " + std::to_string(ticks.tps);
    glutSetWindowTitle(base.c_str());
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    while (!liveCellsLock.try_lock());
    if (!livingCells.empty()) {
        glColor3fv(COLOR_BLUE);
        for (const auto &[x, y]: livingCells) {
            drawSquare(x * cellSize + gridMoveState.xOffset, y * cellSize + gridMoveState.yOffset, cellSize);
        }
    }
    std::cout << livingCells.size() << std::endl;
    liveCellsLock.unlock();

    glutSwapBuffers();
    ++ticks.frameCount;
}

void idle() {
    if (needsRedraw.load()) {
        needsRedraw.store(false);
        glutPostRedisplay();
    }

    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    if (const int deltaTime = currentTime - ticks.lastFrameTime; deltaTime >= ticks.maxFrameRate) {
        ticks.frameTimeAccumulator += deltaTime;
        if (ticks.frameTimeAccumulator >= 1000) {
            ticks.fps = ticks.frameCount;

            ticks.frameTimeAccumulator = 0.0;
            ticks.frameCount = 0;
        }
        ticks.lastFrameTime = currentTime;
    }
}

std::optional<Coords> getClickCell(int x, int y) {
    return {{x / cellSize, y / cellSize}};
}

void placeLiveCell(int x, int y) {
    x -= gridMoveState.xOffset, y -= gridMoveState.yOffset;
    auto cell = getClickCell(x, y);
    if (cell.has_value()) {
        while (!liveCellsLock.try_lock());
        livingCells.insert({cell.value().x, cell.value().y});
        needsRedraw.store(true);
        liveCellsLock.unlock();
    }
}

void restartGame() {
    while (liveCellsLock.try_lock());
    gridMoveState = {};
    livingCells.clear();
    livingCells.insert({ww / cellSize / 2 + 0, wh / cellSize / 2 + 0});
    livingCells.insert({ww / cellSize / 2 + -1, wh / cellSize / 2 + 0});
    livingCells.insert({ww / cellSize / 2 + -1, wh / cellSize / 2 + 1});
    livingCells.insert({ww / cellSize / 2 + -1, wh / cellSize / 2 + 2});
    livingCells.insert({ww / cellSize / 2 + -1, wh / cellSize / 2 + 3});
    needsRedraw.store(true);
    liveCellsLock.unlock();
}

void togglePause() {
    generationPaused.store(!generationPaused.load());
}

void mouseClick(int button, int state, int x, int y) {
    constexpr int WHEEL_UP = 3, WHEEL_DOWN = 4;
    y = wh - y;

    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_UP) {
            placeLiveCell(x, y);
        }

        mouseState.lmbDown = state == GLUT_DOWN;
        drawState.prevDrawX = x, drawState.prevDrawY = y;
    } else if (button == GLUT_MIDDLE_BUTTON) {
        if (state == GLUT_DOWN) {
            mouseState.mmbDown = true;
            gridMoveState.prevX = x, gridMoveState.prevY = y;
        }
        if (state == GLUT_UP) {
            mouseState.mmbDown = false;
        }
    } else if (button == WHEEL_UP || button == WHEEL_DOWN) {
        // Adjust offsets based on mouse position
        int mouseWorldX = (x - gridMoveState.xOffset) / cellSize;
        int mouseWorldY = (y - gridMoveState.yOffset) / cellSize;

        cellSize = button == WHEEL_UP ? cellSize + 2 : std::max(cellSize - 2, 2);

        gridMoveState.xOffset = x - mouseWorldX * cellSize;
        gridMoveState.yOffset = y - mouseWorldY * cellSize;

        needsRedraw.store(true);
    }
}

void mouseHover(int x, int y) {
    y = wh - y;

    if (mouseState.lmbDown) {
        for (auto &[x, y]: generateLine<Coords>(drawState.prevDrawX, drawState.prevDrawY, x, y)) {
            placeLiveCell(x, y);
        }
        drawState.prevDrawX = x, drawState.prevDrawY = y;
    } else if (mouseState.mmbDown) {
        gridMoveState.xOffset += x - gridMoveState.prevX;
        gridMoveState.yOffset += y - gridMoveState.prevY;

        gridMoveState.prevX = x, gridMoveState.prevY = y;
        needsRedraw.store(true);
    }
}

void fps_menu(int code) {
    switch (code) {
        case 0:
            targetFps = 1;
            break;
        case 1:
            targetFps = 4;
            break;
        case 2:
            targetFps = 10;
            break;
        case 3:
            targetFps = 24;
            break;
        case 4:
            targetFps = 60;
            break;
        case 5:
            targetFps = 144;
            break;
        default:
            std::cerr << "Unrecognized menu command" << std::endl;
    }

    ticks.maxTickRate = 1'000 / targetFps;
    ticks.maxFrameRate = 1'000 / targetFps;
}

void main_menu(int code) {
    if (code == 0) {
        togglePause();
        if (generationPaused.load()) {
            glutChangeToMenuEntry(1, "> Play", 0);
        } else {
            glutChangeToMenuEntry(1, "|| Pause", 0);
        }
    }
}

void createMenus() {
    int fpsMenuEntry = glutCreateMenu(fps_menu);
    glutAddMenuEntry("1 FPS", 0);
    glutAddMenuEntry("4 FPS", 1);
    glutAddMenuEntry("10 FPS", 2);
    glutAddMenuEntry("24 FPS", 3);
    glutAddMenuEntry("60 FPS", 4);
    glutAddMenuEntry("144 FPS", 5);
    glutCreateMenu(main_menu);
    glutAddMenuEntry("|| Pause", 0);
    glutAddSubMenu("Target FPS", fpsMenuEntry);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void keyUps(unsigned char key, int x, int y) {
    if (key == 'p' || key == 'P') {
        main_menu(0);
    } else if (key == 'r' || key == 'R') {
        restartGame();
    }
}

void init() {
    glClearColor(0, 0, 0, 1);

    glMatrixMode(GL_PROJECTION);
    glOrtho(0, ww, 0, wh, -1, 1);
}

void workerRun() {
    while (!workerShutdown) {
        int currentTime = glutGet(GLUT_ELAPSED_TIME);
        if (const int deltaTime = currentTime - ticks.lastTickTime; deltaTime >= ticks.maxTickRate) {
            if (!generationPaused.load()) nextGeneration();

            updateTitle();
            ++ticks.tickCount;
            ticks.tickTimeAccumulator += deltaTime;
            if (ticks.tickTimeAccumulator >= 1000) {
                ticks.tps = ticks.tickCount;

                ticks.tickTimeAccumulator = 0.0;
                ticks.tickCount = 0;
            }

            ticks.lastTickTime = glutGet(GLUT_ELAPSED_TIME);
        }
    }
}

void cleanup() {
    workerShutdown = true;
}

int main(int argc, char **argv) {
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInit(&argc, argv);
    ww = 100 * glutGet(GLUT_SCREEN_WIDTH) / 141;
    wh = 100 * glutGet(GLUT_SCREEN_HEIGHT) / 141;

    glutInitWindowSize(ww, wh);
    glutInitWindowPosition((glutGet(GLUT_SCREEN_WIDTH) - ww) / 2, (glutGet(GLUT_SCREEN_HEIGHT) - wh) / 2);
    glutCreateWindow("");
    restartGame();

    init();
    glutDisplayFunc(display);
    glutIdleFunc(idle);

    glutMouseFunc(mouseClick);
    glutMotionFunc(mouseHover);

    glutKeyboardUpFunc(keyUps);
    createMenus();

    glutCloseFunc(cleanup);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    std::thread nextGenerator(workerRun);
    glutMainLoop();
    nextGenerator.join();
}
