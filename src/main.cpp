#include <iostream>
#include <algorithm> // For std::fill
#include <GL/glut.h>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <vector>


////////// PARAMETERS /////////////////////////
GLint cell_size = 50;
GLint FPS = 10;
///////////////////////////////////////////////

GLint ww, wh;
GLint tickRate = 1'000 / FPS, lastTick = 0;
bool paused = false;
bool prev_paused_state = false;
bool lmb_down = false, mmb_down = false;
GLint x_offset = 0, y_offset = 0;
GLint prev_x = 0, prev_y = 0;

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
        return std::hash<int>()(coords.x) ^ (std::hash<int>()(coords.y) << 1);
    }
};

std::unordered_set<Coords, CoordsHash> living_cells;

std::vector<Coords> get_neighbors(const Coords cell) {
    // dead + 3 live = live
    // live + 2/3 live = live
    // else dead
    std::vector<Coords> neighbors;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            if (x == 0 && y == 0)
                continue;

            neighbors.push_back({cell.x + x, cell.y + y});
        }
    }

    return neighbors;
}

void next_generation() {
    std::unordered_set<Coords, CoordsHash> next_gen;
    std::unordered_map<Coords, int, CoordsHash> deadToAliveNeighbors;

    for (auto &cell: living_cells) {
        auto neighbors = get_neighbors(cell);

        std::vector<Coords> deadNeighbors;
        std::ranges::copy_if(neighbors, std::back_inserter(deadNeighbors), [](const Coords &coord) {
            return !living_cells.contains(coord);
        });

        for (auto &dead: deadNeighbors) {
            deadToAliveNeighbors.try_emplace(dead, 0);
            auto &aliveNeighborCount = deadToAliveNeighbors[dead];
            ++aliveNeighborCount;
        }

        int aliveNeighborCount = neighbors.size() - deadNeighbors.size();
        if (aliveNeighborCount == 2 || aliveNeighborCount == 3) {
            next_gen.insert(cell);
        }
    }

    for (const auto &[deadCell, aliveNeighborCount]: deadToAliveNeighbors) {
        if (aliveNeighborCount == 3) {
            next_gen.insert(deadCell);
        }
    }

    living_cells.swap(next_gen);
}

void drawSquare(float x, float y, float size) {
    glBegin(GL_QUADS); // Start drawing a quadrilateral
    glVertex2f(x, y); // Bottom-left corner
    glVertex2f(x + size, y); // Bottom-right corner
    glVertex2f(x + size, y + size); // Top-right corner
    glVertex2f(x, y + size); // Top-left corner
    glEnd();
}


void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    const int rows = wh / cell_size + 1, cols = ww / cell_size + 1;

    glColor3fv(COLOR_BLACK);
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            GLint drawX = col * cell_size, drawY = row * cell_size;

            if (!living_cells.contains({drawX, drawY}))
                drawSquare(drawX + x_offset, drawY + y_offset, cell_size);
        }
    }

    if (!living_cells.empty()) {
        glColor3fv(COLOR_BLUE);
        for (const auto &[x, y]: living_cells) {
            GLint drawX = y * cell_size, drawY = x * cell_size;
            drawSquare(drawX + x_offset, drawY + y_offset, cell_size);
        }
    }

    glutSwapBuffers();
}

void init() {
    glClearColor(0, 0, 0, 1);

    glMatrixMode(GL_PROJECTION);
    // glLoadIdentity();
    glOrtho(0, ww, 0, wh, -1, 1);
    // glMatrixMode(GL_MODELVIEW);
}


void idle() {
    const int currentTime = glutGet(GLUT_ELAPSED_TIME);
    if (paused) {
        glutSetWindowTitle("Conway's Game of Life (paused)");
    } else {
        glutSetWindowTitle("Conway's Game of Life");
    }
    if (const int deltaTime = currentTime - lastTick; deltaTime >= tickRate) {
        if (!paused) {
            next_generation();
        }
        glutPostRedisplay();

        lastTick = glutGet(GLUT_ELAPSED_TIME);
    }
}

std::optional<Coords> getClickCell(int x, int y) {
    int row = y / cell_size, col = x / cell_size;
    return {{row, col}};
}

void restartGame() {
    // x_offset = y_offset = 0;
    // cell_size = 50;
    living_cells.clear();
    living_cells.insert({5, 5});
    living_cells.insert({4, 5});
    living_cells.insert({4, 6});
    living_cells.insert({4, 7});
    living_cells.insert({4, 8});
}

void keybinds(unsigned char key, int _x, int _y) {
    if (key == 'p' || key == 'P') {
        paused = !paused;
    }

    if (key == 'r' || key == 'R') {
        restartGame();
    }
}

void placeLiveCell(int x, int y) {
    x -= x_offset, y -= y_offset;
    auto cell = getClickCell(x, y);
    if (cell.has_value()) {
        std::cout << "Placing at (" << cell.value().x << "," << cell.value().y << ")" << std::endl;
        living_cells.insert({cell.value().x, cell.value().y});
    }
}

void mouseClick(int button, int state, int x, int y) {
    constexpr int WHEEL_UP = 3, WHEEL_DOWN = 4;
    y = wh - y;

    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_UP) {
            placeLiveCell(x, y);
        }
        lmb_down = state == GLUT_DOWN;
    } else if (button == GLUT_MIDDLE_BUTTON) {
        if (state == GLUT_DOWN) {
            mmb_down = true;
            prev_x = x, prev_y = y;
            prev_paused_state = paused;
            paused = true;
        }
        if (state == GLUT_UP) {
            mmb_down = false;
            paused = prev_paused_state;
        }
    } else if (button == WHEEL_UP) {
        ++cell_size;
    } else if (button == WHEEL_DOWN) {
        cell_size = std::max(1, cell_size - 1);
    }
}


void mouseHover(int x, int y) {
    y = wh - y;

    if (lmb_down) {
        placeLiveCell(x, y);
    } else if (mmb_down) {
        x_offset += x - prev_x;
        y_offset += y - prev_y;

        prev_x = x, prev_y = y;
    }
}

int main(int argc, char **argv) {
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInit(&argc, argv);
    ww = 100 * glutGet(GLUT_SCREEN_WIDTH) / 141;
    wh = 100 * glutGet(GLUT_SCREEN_HEIGHT) / 141;

    glutInitWindowSize(ww, wh);
    glutInitWindowPosition((glutGet(GLUT_SCREEN_WIDTH) - ww) / 2, (glutGet(GLUT_SCREEN_HEIGHT) - wh) / 2);
    glutCreateWindow("Conway's Game of Life");
    restartGame();

    init();
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutMouseFunc(mouseClick);
    glutMotionFunc(mouseHover);
    glutKeyboardFunc(keybinds);
    glutMainLoop();
}
