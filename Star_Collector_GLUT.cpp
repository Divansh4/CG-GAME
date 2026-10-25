#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

// Star Collector: C++ / OpenGL / FreeGLUT version
// Controls: W/S move, A/D turn, R restart, ESC quit.

const int WIDTH = 1000;
const int HEIGHT = 650;
const float ARENA = 20.0f;
const int GOAL = 10;
const int START_LIVES = 3;
const float PI = 3.14159265358979323846f;

struct Gem {
    float x, z, phase;
};

struct Mine {
    float x, z, vx, vz, spin;
};

float playerX = 0.0f, playerZ = 0.0f;
float heading = 0.0f, cameraHeading = 0.0f;
float clockTime = 0.0f, playTime = 0.0f, invulnerable = 0.0f;
int score = 0, lives = START_LIVES;
enum GameState { PLAYING, WON, LOST };
GameState state = PLAYING;

Gem gems[GOAL];
int gemCount = 3;
Mine mines[5];
bool keys[256] = {};
GLuint floorTexture = 0;
int previousTime = 0;

float randomFloat(float lo, float hi) {
    return lo + (hi - lo) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
}

float distance2D(float x1, float z1, float x2, float z2) {
    const float dx = x1 - x2, dz = z1 - z2;
    return std::sqrt(dx * dx + dz * dz);
}

void drawCube(float sx = 1, float sy = 1, float sz = 1) {
    glPushMatrix();
    glScalef(sx, sy, sz);
    const float h = 0.5f;
    glBegin(GL_QUADS);
    // Front (+Z)
    glNormal3f(0, 0, 1);
    glVertex3f(-h,-h,h); glVertex3f(h,-h,h); glVertex3f(h,h,h); glVertex3f(-h,h,h);
    // Back (-Z)
    glNormal3f(0, 0, -1);
    glVertex3f(h,-h,-h); glVertex3f(-h,-h,-h); glVertex3f(-h,h,-h); glVertex3f(h,h,-h);
    // Right (+X)
    glNormal3f(1, 0, 0);
    glVertex3f(h,-h,h); glVertex3f(h,-h,-h); glVertex3f(h,h,-h); glVertex3f(h,h,h);
    // Left (-X)
    glNormal3f(-1, 0, 0);
    glVertex3f(-h,-h,-h); glVertex3f(-h,-h,h); glVertex3f(-h,h,h); glVertex3f(-h,h,-h);
    // Top (+Y)
    glNormal3f(0, 1, 0);
    glVertex3f(-h,h,h); glVertex3f(h,h,h); glVertex3f(h,h,-h); glVertex3f(-h,h,-h);
    // Bottom (-Y)
    glNormal3f(0, -1, 0);
    glVertex3f(-h,-h,-h); glVertex3f(h,-h,-h); glVertex3f(h,-h,h); glVertex3f(-h,-h,h);
    glEnd();
    glPopMatrix();
}

void drawGemShape() {
    const float k = 1.0f / std::sqrt(3.0f);
    glBegin(GL_TRIANGLES);
    for (int sx : {-1, 1}) {
        for (int sy : {-1, 1}) {
            for (int sz : {-1, 1}) {
                glNormal3f(sx*k, sy*k, sz*k);
                glVertex3f(static_cast<float>(sx), 0, 0);
                glVertex3f(0, static_cast<float>(sy), 0);
                glVertex3f(0, 0, static_cast<float>(sz));
            }
        }
    }
    glEnd();
}

void drawText(float x, float y, const std::string& text, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (unsigned char c : text) glutBitmapCharacter(font, c);
}

void makeFloorTexture() {
    const int size = 64;
    unsigned char data[size * size * 3];
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool light = (((x / 8) + (y / 8)) % 2) == 0;
            const int i = (y * size + x) * 3;
            data[i] = light ? 90 : 40;
            data[i + 1] = light ? 120 : 60;
            data[i + 2] = light ? 190 : 110;
        }
    }
    glGenTextures(1, &floorTexture);
    glBindTexture(GL_TEXTURE_2D, floorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
}

void drawFloor() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, floorTexture);
    glColor3f(1, 1, 1);
    glNormal3f(0, 1, 0);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);   glVertex3f(-ARENA, 0, -ARENA);
    glTexCoord2f(0, 10);  glVertex3f(-ARENA, 0, ARENA);
    glTexCoord2f(10, 10);  glVertex3f(ARENA, 0, ARENA);
    glTexCoord2f(10, 0);  glVertex3f(ARENA, 0, -ARENA);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void placeGem(Gem& g, float avoidX, float avoidZ) {
    int tries = 0;
    do {
        g.x = randomFloat(-ARENA + 2, ARENA - 2);
        g.z = randomFloat(-ARENA + 2, ARENA - 2);
        ++tries;
    } while (distance2D(g.x, g.z, avoidX, avoidZ) <= 6.0f && tries < 100);
    g.phase = randomFloat(0, 2 * PI);
}

void placeMine(Mine& m) {
    int tries = 0;
    do {
        m.x = randomFloat(-ARENA + 2, ARENA - 2);
        m.z = randomFloat(-ARENA + 2, ARENA - 2);
        ++tries;
    } while (distance2D(m.x, m.z, 0, 0) <= 7.0f && tries < 100);
    float a = randomFloat(0, 2 * PI);
    float speed = randomFloat(3.0f, 5.0f);
    m.vx = std::cos(a) * speed;
    m.vz = std::sin(a) * speed;
    m.spin = randomFloat(0, 360);
}

void resetGame() {
    playerX = playerZ = heading = cameraHeading = 0;
    score = 0;
    lives = START_LIVES;
    invulnerable = 0;
    clockTime = playTime = 0;
    state = PLAYING;
    gemCount = 3;
    for (int i = 0; i < gemCount; ++i) placeGem(gems[i], 0, 0);
    for (Mine& m : mines) placeMine(m);
}

void update(float dt) {
    clockTime += dt;
    for (Mine& m : mines) {
        if (state == PLAYING) {
            m.x += m.vx * dt;
            m.z += m.vz * dt;
            const float limit = ARENA - 1.0f;
            if (std::fabs(m.x) > limit) { m.x = (m.x > 0 ? limit : -limit); m.vx = -m.vx; }
            if (std::fabs(m.z) > limit) { m.z = (m.z > 0 ? limit : -limit); m.vz = -m.vz; }
            m.spin += 120.0f * dt;
        }
    }

    cameraHeading += (heading - cameraHeading) * std::min(1.0f, 5.0f * dt);
    if (state != PLAYING) return;

    playTime += dt;
    invulnerable = std::max(0.0f, invulnerable - dt);

    if (keys['a'] || keys['A']) heading += 120.0f * dt;
    if (keys['d'] || keys['D']) heading -= 120.0f * dt;

    const float rad = heading * PI / 180.0f;
    const float fx = std::sin(rad), fz = std::cos(rad);
    float move = 0;
    if (keys['w'] || keys['W']) move += 8.0f;
    if (keys['s'] || keys['S']) move -= 5.0f;

    const float limit = ARENA - 1.0f;
    playerX = std::max(-limit, std::min(limit, playerX + fx * move * dt));
    playerZ = std::max(-limit, std::min(limit, playerZ + fz * move * dt));

    for (int i = 0; i < gemCount; ) {
        if (distance2D(gems[i].x, gems[i].z, playerX, playerZ) < 1.4f) {
            ++score;
            for (int j = i; j < gemCount - 1; ++j) gems[j] = gems[j + 1];
            --gemCount;
            if (score + gemCount < GOAL && gemCount < GOAL) {
                placeGem(gems[gemCount], playerX, playerZ);
                ++gemCount;
            }
        } else {
            ++i;
        }
    }
    if (score >= GOAL) { state = WON; return; }

    if (invulnerable <= 0) {
        for (const Mine& m : mines) {
            if (distance2D(m.x, m.z, playerX, playerZ) < 1.7f) {
                --lives;
                invulnerable = 2.0f;
                if (lives <= 0) state = LOST;
                break;
            }
        }
    }
}

void setupCamera() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, static_cast<double>(WIDTH) / HEIGHT, 0.1, 120.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const float rad = cameraHeading * PI / 180.0f;
    const float fx = std::sin(rad), fz = std::cos(rad);
    gluLookAt(playerX - fx * 9.0f, 5.5, playerZ - fz * 9.0f,
              playerX + fx * 3.0f, 1.0, playerZ + fz * 3.0f,
              0, 1, 0);
}

void setupLights() {
    const GLfloat sunPos[] = {0.4f, 1.0f, 0.3f, 0.0f};
    const GLfloat sunDiff[] = {0.7f, 0.7f, 0.8f, 1};
    const GLfloat sunSpec[] = {0.8f, 0.8f, 0.9f, 1};
    const GLfloat pointPos[] = {playerX, 4.0f, playerZ, 1};
    const GLfloat pointDiff[] = {1.0f, 0.8f, 0.4f, 1};
    glLightfv(GL_LIGHT0, GL_POSITION, sunPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, sunDiff);
    glLightfv(GL_LIGHT0, GL_SPECULAR, sunSpec);
    glLightfv(GL_LIGHT1, GL_POSITION, pointPos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, pointDiff);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.05f);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.01f);
}

void drawPillars() {
    for (int sx : {-1, 1}) for (int sz : {-1, 1}) {
        glPushMatrix();
        glTranslatef(sx * ARENA, 2.0f, sz * ARENA);
        glColor3f(0.6f, 0.6f, 0.7f);
        drawCube(1.5f, 4.0f, 1.5f);
        glTranslatef(0, 2.4f, 0);
        glRotatef(clockTime * 60.0f, 0, 1, 0);
        glColor3f(0.2f, 0.9f, 1.0f);
        drawCube(0.8f, 0.8f, 0.8f);
        glPopMatrix();
    }
}

void drawGems() {
    for (int i = 0; i < gemCount; ++i) {
        const float bob = 1.2f + 0.3f * std::sin(clockTime * 2.5f + gems[i].phase);
        glPushMatrix();
        glTranslatef(gems[i].x, bob, gems[i].z);
        glRotatef(clockTime * 90.0f + gems[i].phase * 180.0f / PI, 0, 1, 0);
        glScalef(0.6f, 0.9f, 0.6f);
        glColor3f(1.0f, 0.85f, 0.1f);
        drawGemShape();
        glPopMatrix();
    }
}

void drawMines() {
    for (const Mine& m : mines) {
        glPushMatrix();
        glTranslatef(m.x, 1.0f, m.z);
        glRotatef(m.spin, 0, 1, 0);
        glColor3f(0.85f, 0.1f, 0.1f);
        glutSolidSphere(0.8, 20, 16);
        glColor3f(0.25f, 0.25f, 0.28f);
        drawCube(1.5f, 0.18f, 0.18f);
        drawCube(0.18f, 1.5f, 0.18f);
        drawCube(0.18f, 0.18f, 1.5f);
        glPopMatrix();
    }
}

void drawPlayer() {
    glPushMatrix();
    glTranslatef(playerX, 0.9f + 0.08f * std::sin(clockTime * 4), playerZ);
    glRotatef(heading, 0, 1, 0);
    glColor3f(0.2f, 0.5f, 0.95f);
    drawCube(1.0f, 0.5f, 1.8f);

    glPushMatrix();
    glTranslatef(0, 0.35f, 0.2f);
    glColor3f(0.8f, 0.95f, 1.0f);
    glutSolidSphere(0.38, 20, 16);
    glPopMatrix();

    for (int side : {-1, 1}) {
        glPushMatrix();
        glTranslatef(side * 0.9f, -0.05f, -0.3f);
        glRotatef(side * -15.0f, 0, 0, 1);
        glColor3f(0.95f, 0.3f, 0.2f);
        drawCube(1.0f, 0.1f, 0.9f);
        glPopMatrix();
    }

    glPushMatrix();
    glTranslatef(0, 0.6f, -0.7f);
    glRotatef(clockTime * 300.0f, 0, 0, 1);
    glColor3f(1.0f, 0.85f, 0.2f);
    glRotatef(90, 1, 0, 0);
    glutSolidTorus(0.08, 0.32, 10, 24);
    glPopMatrix();
    glPopMatrix();
}

void drawShadows() {
    glDisable(GL_LIGHTING);
    glColor4f(0, 0, 0, 0.25f);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    auto disc = [](float x, float z, float r) {
        glPushMatrix();
        glTranslatef(x, 0.025f, z);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0, 0, 0);
        for (int i = 0; i <= 32; ++i) {
            float a = 2 * PI * i / 32;
            glVertex3f(std::cos(a) * r, 0, std::sin(a) * r);
        }
        glEnd();
        glPopMatrix();
    };
    disc(playerX, playerZ, 1.1f);
    for (int i = 0; i < gemCount; ++i) disc(gems[i].x, gems[i].z, 0.7f);
    for (const Mine& m : mines) disc(m.x, m.z, 0.9f);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void drawShield() {
    if (invulnerable > 0 && static_cast<int>(invulnerable * 8) % 2 == 0) {
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glColor4f(1.0f, 0.3f, 0.3f, 0.28f);
        glPushMatrix();
        glTranslatef(playerX, 1.0f, playerZ);
        glutWireSphere(1.6, 20, 16);
        glPopMatrix();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }
}

void drawHUD() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glColor3f(1.0f, 0.86f, 0.24f);
    drawText(20, HEIGHT - 30, "Gems: " + std::to_string(score) + "/" + std::to_string(GOAL));
    glColor3f(1.0f, 0.38f, 0.38f);
    drawText(20, HEIGHT - 60, "Lives: " + std::to_string(lives));
    glColor3f(0.7f, 0.86f, 1.0f);
    std::ostringstream timeText;
    timeText << "Time: " << static_cast<int>(playTime) << "s";
    drawText(20, HEIGHT - 90, timeText.str());
    glColor3f(0.85f, 0.85f, 0.88f);
    drawText(WIDTH - 340, HEIGHT - 30, "W/S move   A/D turn   R restart");

    if (state == WON || state == LOST) {
        glColor3f(state == WON ? 0.35f : 1.0f, state == WON ? 1.0f : 0.25f, state == WON ? 0.45f : 0.25f);
        std::string message = state == WON ? "YOU WIN!" : "GAME OVER";
        int width = static_cast<int>(message.size()) * 12;
        drawText((WIDTH - width) / 2.0f, HEIGHT / 2 + 20, message, GLUT_BITMAP_HELVETICA_18);
        glColor3f(1, 1, 1);
        drawText(WIDTH / 2.0f - 105, HEIGHT / 2 - 15,
                 state == WON ? "Press R to play again" : "Press R to try again");
    }

    glEnable(GL_FOG);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setupCamera();
    setupLights();

    glEnable(GL_LIGHTING);
    drawFloor();
    drawShadows();
    drawPillars();
    drawGems();
    drawMines();
    drawPlayer();
    drawShield();
    drawHUD();

    glutSwapBuffers();
}

void idle() {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - previousTime) / 1000.0f;
    previousTime = now;
    dt = std::min(dt, 0.05f); // avoid a large physics jump after a pause
    update(dt);
    glutPostRedisplay();
}

void keyDown(unsigned char key, int, int) {
    if (key == 27) std::exit(0);
    if (key == 'r' || key == 'R') resetGame();
    keys[key] = true;
}

void keyUp(unsigned char key, int, int) {
    keys[key] = false;
}

void specialDown(int key, int, int) {
    if (key == GLUT_KEY_UP) keys['w'] = true;
    if (key == GLUT_KEY_DOWN) keys['s'] = true;
    if (key == GLUT_KEY_LEFT) keys['a'] = true;
    if (key == GLUT_KEY_RIGHT) keys['d'] = true;
}

void specialUp(int key, int, int) {
    if (key == GLUT_KEY_UP) keys['w'] = false;
    if (key == GLUT_KEY_DOWN) keys['s'] = false;
    if (key == GLUT_KEY_LEFT) keys['a'] = false;
    if (key == GLUT_KEY_RIGHT) keys['d'] = false;
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    // HUD remains designed for the reference window; keep the aspect correct in 3D.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, static_cast<double>(std::max(1, w)) / std::max(1, h), 0.1, 120.0);
    glMatrixMode(GL_MODELVIEW);
}

void initOpenGL() {
    glClearColor(0.05f, 0.07f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    const GLfloat ambient[] = {0.25f, 0.25f, 0.3f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    const GLfloat spec[] = {0.9f, 0.9f, 0.9f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 50.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    const GLfloat fogColor[] = {0.05f, 0.07f, 0.15f, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_START, 25.0f);
    glFogf(GL_FOG_END, 70.0f);

    makeFloorTexture();
}

int main(int argc, char** argv) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Star Collector - C++ OpenGL / GLUT");
    initOpenGL();
    resetGame();

    previousTime = glutGet(GLUT_ELAPSED_TIME);
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutReshapeFunc(reshape);
    glutMainLoop();
    return 0;
}
