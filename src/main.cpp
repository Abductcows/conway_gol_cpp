#include <cstdlib>
#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>
#include <stack>
#include <chrono>

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

int cellSize = 50, targetFps = 10;
int ww, wh;

std::atomic_bool generationPaused{false};
std::mutex liveCellsLock, userInputLock;

int prevMouseX, prevMouseY;

bool workerShutdown = false;
bool needsRedraw = false;

struct TickCountState {
    int fps = 0, tps = 0;
    int frameCount = 0, tickCount = 0;
    int64_t maxFrameRate = 1000.0f / targetFps, maxTickRate = 1000.0f / targetFps;
    int64_t lastFrameTime = 0, lastTickTime = 0;
    int64_t frameTimeAccumulator = 0.0, tickTimeAccumulator = 0.0;
} ticks;

struct MouseState {
    bool lmbDown{}, mmbDown{};
} mouseState;

struct CursorDrawState {
    int prevDrawX{}, prevDrawY{};
} drawState;

struct GridMoveState {
    // game board panning
    int xOffset{}, yOffset{};
    int prevX{}, prevY{};
} gridMoveState;

std::unordered_set<Coords, CoordsHash> livingCells;
std::vector<Coords> userAddedCells;

void updateTitle() {
    auto base = std::string("Conway's Game of Life");
    if (generationPaused.load()) base = "[PAUSED] " + base;
    base += " FPS: " + std::to_string(ticks.fps);
    base += " TPS: " + std::to_string(ticks.tps);
    glutSetWindowTitle(base.c_str());
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // cursor shadow
    if (!mouseState.mmbDown) {
        glColor3fv(COLOR_GREY);
        auto [shadowX, shadowY] = mouseToWorldCoords(prevMouseX, prevMouseY,
                                                     gridMoveState.xOffset, gridMoveState.yOffset);
        auto [shadowCellX, shadowCellY] = worldCoordsToCell(shadowX, shadowY, cellSize);
        drawSquare(shadowCellX * cellSize + gridMoveState.xOffset,
                   shadowCellY * cellSize + gridMoveState.yOffset,
                   cellSize);
    }

    glColor3fv(COLOR_BLUE);
    for (const auto &[x, y]: livingCells) {
        drawSquare(x * cellSize + gridMoveState.xOffset,
                   y * cellSize + gridMoveState.yOffset,
                   cellSize);
    }

    glutSwapBuffers();
}

void idle() {
    int64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    bool frameDrawn = false;

    // urgent redraw for responsiveness (ignores fps cap)
    while (!liveCellsLock.try_lock());
    if (needsRedraw) {
        display();
        needsRedraw = false;
        frameDrawn = true;
        ++ticks.frameCount;
    }
    liveCellsLock.unlock();

    if (auto deltaTime = currentTime - ticks.lastFrameTime; deltaTime >= ticks.maxFrameRate) {
        ticks.frameTimeAccumulator += deltaTime;
        if (ticks.frameTimeAccumulator >= 1000) {
            ticks.fps = ticks.frameCount;

            ticks.frameTimeAccumulator = 0.0;
            ticks.frameCount = 0;
        }
        ticks.lastFrameTime = currentTime;
        while (!liveCellsLock.try_lock());
        if (!generationPaused.load() && !frameDrawn) {
            display();
            ++ticks.frameCount;
        }
        liveCellsLock.unlock();
    }
}

void bufferUserInput(int x_mouse, int y_mouse) {
    auto [x, y] = mouseToWorldCoords(x_mouse, y_mouse,
                                     gridMoveState.xOffset, gridMoveState.yOffset);
    userAddedCells.push_back(worldCoordsToCell(x, y, cellSize));
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
    liveCellsLock.unlock();
    needsRedraw = true;
}

void togglePause() {
    generationPaused.store(!generationPaused.load());
}

void mouseClick(int button, int state, int x, int y) {
    y = wh - y;

    constexpr int WHEEL_UP = 3, WHEEL_DOWN = 4;
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_UP) {
            mouseState.lmbDown = false;
            while (!userInputLock.try_lock());
            bufferUserInput(x, y);
            userInputLock.unlock();
            needsRedraw = true;
        } else {
            mouseState.lmbDown = true;
            drawState.prevDrawX = x, drawState.prevDrawY = y;
        }
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
        needsRedraw = true;
    }
}

void mouseHoverButtonPressed(int x, int y) {
    y = wh - y;
    prevMouseX = x, prevMouseY = y;

    if (mouseState.lmbDown) {
        auto line = generateLine(drawState.prevDrawX, drawState.prevDrawY, x, y);

        while (!userInputLock.try_lock());
        for (auto &[x, y]: line) bufferUserInput(x, y);
        userInputLock.unlock();
        needsRedraw = true;
        drawState.prevDrawX = x, drawState.prevDrawY = y;
    } else if (mouseState.mmbDown) {
        gridMoveState.xOffset += x - gridMoveState.prevX;
        gridMoveState.yOffset += y - gridMoveState.prevY;

        gridMoveState.prevX = x, gridMoveState.prevY = y;
        needsRedraw = true;
    }
}

void mouseHoverNoButton(int x, int y) {
    y = wh - y;
    prevMouseX = x, prevMouseY = y;
    needsRedraw = true;
}

void fpsMenu(int code) {
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

    ticks.maxTickRate = 1000.0f / targetFps;
    ticks.maxFrameRate = 1000.0f / targetFps;
}

void mainMenu(int code) {
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
    int fpsMenuEntry = glutCreateMenu(fpsMenu);
    glutAddMenuEntry("1 FPS", 0);
    glutAddMenuEntry("4 FPS", 1);
    glutAddMenuEntry("10 FPS", 2);
    glutAddMenuEntry("24 FPS", 3);
    glutAddMenuEntry("60 FPS", 4);
    glutAddMenuEntry("144 FPS", 5);
    glutCreateMenu(mainMenu);
    glutAddMenuEntry("|| Pause", 0);
    glutAddSubMenu("Target FPS", fpsMenuEntry);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void keyUps(unsigned char key, int x, int y) {
    if (key == 'p' || key == 'P') {
        mainMenu(0);
    } else if (key == 'r' || key == 'R') {
        restartGame();
    }
}

void nextGeneration() {
    std::unordered_set<Coords, CoordsHash> nextGen;
    std::unordered_map<Coords, int, CoordsHash> deadToAliveNeighbors;

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
    liveCellsLock.unlock();
}

void workerRun() {
    while (!workerShutdown) {
        while (!userInputLock.try_lock());
        livingCells.insert(userAddedCells.begin(), userAddedCells.end());
        userAddedCells.clear();
        userInputLock.unlock();

        int64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        if (const auto deltaTime = currentTime - ticks.lastTickTime; deltaTime >= ticks.maxTickRate) {
            if (workerShutdown) return;
            if (!generationPaused.load()) nextGeneration();

            if (workerShutdown) return;
            updateTitle();

            ++ticks.tickCount;
            ticks.tickTimeAccumulator += deltaTime;
            if (deltaTime >= 1000 || ticks.tickTimeAccumulator >= 1000) {
                ticks.tps = ticks.tickCount;

                ticks.tickTimeAccumulator = 0.0;
                ticks.tickCount = 0;
            }

            ticks.lastTickTime = currentTime;
        }
    }
}

void init() {
    glClearColor(0, 0, 0, 1);

    glMatrixMode(GL_PROJECTION);
    glOrtho(0, ww, 0, wh, -1, 1);
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
    glutMotionFunc(mouseHoverButtonPressed);
    glutPassiveMotionFunc(mouseHoverNoButton);

    glutKeyboardUpFunc(keyUps);
    createMenus();

    glutCloseFunc(cleanup);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    std::thread nextGenerator(workerRun);
    glColor3fv(COLOR_BLUE);
    glutMainLoop();
    nextGenerator.join();
    std::cout << "Bye!" << std::endl;
}
