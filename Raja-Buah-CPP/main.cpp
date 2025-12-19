#include "raylib.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>

enum GameState {
    MENU,
    PLAYING,
    PAUSED,
    GAMEOVER,
    VICTORY
};

struct FruitDef {
    float radius;
    Color color;
    int scoreValue;
    Texture2D texture; 
};

struct Fruit {
    Vector2 position;
    Vector2 velocity;
    int tier;
    bool isActive;

    Fruit(float x, float y, int fruitTier) {
        position = { x, y };
        velocity = { 0, 0 };
        tier = fruitTier;
        isActive = true;
    }
};

struct Particle {
    Vector2 position;
    Vector2 velocity;
    float life;
    Color color;
    float size;
};

class RajaBuahGame {
private:
    const int screenWidth = 600;
    const int screenHeight = 800;
    const int overflowY = 150;
    const float wallThickness = 20.0f;
    const float SPAWN_COOLDOWN = 0.5f;
    const int PHYSICS_STEPS = 8;

    Texture2D backgroundTex;
    Sound fxDrop;
    Sound fxMerge;

    FruitDef fruitDefs[10] = {
        { 15.0f, BEIGE, 10, {0} },      // tier 0: mata kucing
        { 25.0f, RED, 20, {0} },        // tier 1: rambutan
        { 35.0f, YELLOW, 40, {0} },     // tier 2: langsat
        { 45.0f, PURPLE, 80, {0} },     // tier 3: manggis
        { 60.0f, GREEN, 160, {0} },     // tier 4: belimbing
        { 75.0f, LIME, 320, {0} },      // tier 5: jambu batu
        { 95.0f, ORANGE, 640, {0} },    // tier 6: betik
        { 115.0f, GOLD, 1280, {0} },    // tier 7: nanas
        { 135.0f, BROWN, 2560, {0} },   // tier 8: nangka
        { 160.0f, DARKGREEN, 5000, {0} }// tier 9: durian
    };

    GameState currentState;
    std::vector<Fruit> fruits;
    std::vector<Particle> particles;

    float dropperX;
    int currentTier;
    int nextTier;
    int score;
    int highScore;

    float overflowTimer;
    float spawnTimer;

public:
    RajaBuahGame() {
        Init();
    }

    ~RajaBuahGame() {
        Cleanup();
    }

    void Run() {
        while (!WindowShouldClose()) {
            Update();
            Draw();
        }
    }

private:
    void Init() {
        InitWindow(screenWidth, screenHeight, "Raja Buah");
        InitAudioDevice();
        SetTargetFPS(60);

        // icon load
        if (FileExists("fruit_9.png")) {
            Image icon = LoadImage("fruit_9.png");
            SetWindowIcon(icon);
            UnloadImage(icon);
        }

        // background load
        backgroundTex = { 0 };
        if (FileExists("background.png")) {
            backgroundTex = LoadTexture("background.png");
        }

        // fruits assets load
        for (int i = 0; i < 10; i++) {
            std::string fileName = "fruit_" + std::to_string(i) + ".png";
            fruitDefs[i].texture = LoadTextureOrFallback(fileName.c_str(), fruitDefs[i].radius, fruitDefs[i].color);
            SetTextureFilter(fruitDefs[i].texture, TEXTURE_FILTER_POINT);
        }

        fxDrop = LoadSound("drop.wav");
        fxMerge = LoadSound("merge.wav");
        highScore = LoadHighScore();

        ResetGame(true);
        currentState = MENU;
    }

    void Cleanup() {
        // assets unload
        for (int i = 0; i < 10; i++) {
            UnloadTexture(fruitDefs[i].texture);
        }
        if (backgroundTex.id != 0) UnloadTexture(backgroundTex);

        UnloadSound(fxDrop);
        UnloadSound(fxMerge);

        CloseAudioDevice();
        CloseWindow();
    }

    void ResetGame(bool fullReset) {
        if (fullReset) {
            score = 0;
            currentState = MENU;
        }
        fruits.clear();
        particles.clear();
        overflowTimer = 0.0f;
        currentTier = 0;
        nextTier = GetRandomValue(0, 3);
        spawnTimer = 0.0f;
        dropperX = screenWidth / 2.0f;
    }

    // helpers
    Texture2D LoadTextureOrFallback(const char* fileName, float radius, Color color) {
        if (FileExists(fileName)) {
            return LoadTexture(fileName);
        }
        else {
            Image img = GenImageColor((int)radius * 2, (int)radius * 2, BLANK);
            ImageDrawCircle(&img, (int)radius, (int)radius, (int)radius, color);
            ImageDrawCircle(&img, (int)radius + 5, (int)radius - 5, (int)(radius * 0.2f), Fade(WHITE, 0.5f));
            Texture2D tex = LoadTextureFromImage(img);
            UnloadImage(img);
            return tex;
        }
    }

    int LoadHighScore() {
        int s = 0;
        std::ifstream file("highscore.txt");
        if (file.is_open()) {
            file >> s;
            file.close();
        }
        return s;
    }

    void SaveHighScore(int s) {
        std::ofstream file("highscore.txt");
        if (file.is_open()) {
            file << s;
            file.close();
        }
    }

    float GetFruitRadius(int tier) const { return fruitDefs[tier].radius; }
    float GetFruitMass(int tier) const { return fruitDefs[tier].radius; } 

    // core logic
    void Update() {
        switch (currentState) {
        case MENU:
            if (IsKeyPressed(KEY_ENTER)) {
                ResetGame(true);
                currentState = PLAYING;
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                CloseWindow();
            }
            break;

        case PAUSED:
            if (IsKeyPressed(KEY_P)) currentState = PLAYING;
            if (IsKeyPressed(KEY_ESCAPE)) currentState = MENU;
            break;

        case PLAYING:
            UpdatePlaying();
            break;

        case GAMEOVER:
        case VICTORY:
            if (IsKeyPressed(KEY_R)) {
                ResetGame(true); 
                currentState = MENU;
            }
            break;
        }
    }

    void UpdatePlaying() {
        spawnTimer += GetFrameTime();

        if (IsKeyPressed(KEY_P)) {
            currentState = PAUSED;
            return;
        }

        HandleInput();
        UpdatePhysics();
        UpdateParticles();
        CheckGameOver();
    }

    void HandleInput() {
        float moveSpeed = 300.0f * GetFrameTime();

        // movement
        if (IsKeyDown(KEY_LEFT)) dropperX -= moveSpeed;
        if (IsKeyDown(KEY_RIGHT)) dropperX += moveSpeed;
        if (GetMouseDelta().x != 0) dropperX = GetMousePosition().x;

        // dropper clamp
        float currentRadius = fruitDefs[currentTier].radius;
        if (dropperX < wallThickness + currentRadius) dropperX = wallThickness + currentRadius;
        if (dropperX > screenWidth - wallThickness - currentRadius) dropperX = screenWidth - wallThickness - currentRadius;

        // fruit spawner
        if ((IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) && spawnTimer >= SPAWN_COOLDOWN) {
            Fruit newFruit(dropperX, 50.0f, currentTier);
            fruits.push_back(newFruit);
            currentTier = nextTier;
            nextTier = GetRandomValue(0, 3);
            PlaySound(fxDrop);
            spawnTimer = 0.0f;
        }
    }

    void UpdatePhysics() {
        const float dt = GetFrameTime() / (float)PHYSICS_STEPS;

        for (int step = 0; step < PHYSICS_STEPS; step++) {
            std::vector<Fruit> spawnedFruits;

            for (size_t i = 0; i < fruits.size(); i++) {
                if (!fruits[i].isActive) continue;

                // gravity
                fruits[i].velocity.y += 1000.0f * dt;
                // damp
                fruits[i].velocity.x *= 0.995f;
                fruits[i].velocity.y *= 0.995f;
                // integration
                fruits[i].position.x += fruits[i].velocity.x * dt;
                fruits[i].position.y += fruits[i].velocity.y * dt;

                // wall collisions
                float r = GetFruitRadius(fruits[i].tier);

                // floor
                if (fruits[i].position.y > screenHeight - r) {
                    fruits[i].position.y = screenHeight - r;
                    fruits[i].velocity.y *= -0.2f;
                    fruits[i].velocity.x *= 0.8f;
                }
                // left wall
                if (fruits[i].position.x < wallThickness + r) {
                    fruits[i].position.x = wallThickness + r;
                    fruits[i].velocity.x *= -0.3f;
                }
                // right wall
                if (fruits[i].position.x > screenWidth - wallThickness - r) {
                    fruits[i].position.x = screenWidth - wallThickness - r;
                    fruits[i].velocity.x *= -0.3f;
                }

                // fruits collision
                for (size_t j = i + 1; j < fruits.size(); j++) {
                    if (!fruits[j].isActive) continue;
                    SolveCollision(i, j, spawnedFruits);
                }
            }

            // merged fruits
            for (const auto& f : spawnedFruits) fruits.push_back(f);

            // inactive fruits
            fruits.erase(std::remove_if(fruits.begin(), fruits.end(),
                [](const Fruit& f) { return !f.isActive; }), fruits.end());
        }
    }

    void SolveCollision(size_t i, size_t j, std::vector<Fruit>& spawnedFruits) {
        float dx = fruits[j].position.x - fruits[i].position.x;
        float dy = fruits[j].position.y - fruits[i].position.y;
        float distSq = dx * dx + dy * dy;
        float radSum = GetFruitRadius(fruits[i].tier) + GetFruitRadius(fruits[j].tier);

        if (distSq < radSum * radSum) {
            // merge checking
            if (fruits[i].tier == fruits[j].tier) {
                // merge logic
                fruits[i].isActive = false;
                fruits[j].isActive = false;

                float midX = (fruits[i].position.x + fruits[j].position.x) / 2.0f;
                float midY = (fruits[i].position.y + fruits[j].position.y) / 2.0f;

                SpawnParticles(midX, midY, fruits[i].tier);

                // check if tier 9 is durian
                if (fruits[i].tier == 9) {
                    currentState = VICTORY;
                    if (score > highScore) {
                        highScore = score;
                        SaveHighScore(highScore);
                    }
                    PlaySound(fxMerge);
                }
                else {
                    // next fruit tier creation
                    int newTier = fruits[i].tier + 1;
                    Fruit mergedFruit(midX, midY, newTier);
                    mergedFruit.velocity.y = -50.0f; 
                    spawnedFruits.push_back(mergedFruit);

                    score += fruitDefs[fruits[i].tier].scoreValue * 2;

                    SetSoundPitch(fxMerge, 0.8f + (newTier * 0.1f));
                    PlaySound(fxMerge);
                }
            }
            else {
                // bounce physics
                float dist = sqrtf(distSq);
                if (dist < 0.0001f) { dx = 0.01f; dist = 0.01f; }

                float overlap = radSum - dist;
                float nx = dx / dist;
                float ny = dy / dist;
                float correction = overlap * 0.5f;

                fruits[i].position.x -= nx * correction;
                fruits[i].position.y -= ny * correction;
                fruits[j].position.x += nx * correction;
                fruits[j].position.y += ny * correction;

                float rvx = fruits[j].velocity.x - fruits[i].velocity.x;
                float rvy = fruits[j].velocity.y - fruits[i].velocity.y;
                float velAlongNormal = rvx * nx + rvy * ny;

                if (velAlongNormal < 0) {
                    float restitution = 0.2f;
                    float massA = GetFruitMass(fruits[i].tier);
                    float massB = GetFruitMass(fruits[j].tier);

                    float jVal = -(1.0f + restitution) * velAlongNormal;
                    jVal /= (1.0f / massA + 1.0f / massB);

                    float impulseX = jVal * nx;
                    float impulseY = jVal * ny;

                    fruits[i].velocity.x -= impulseX * (1.0f / massA);
                    fruits[i].velocity.y -= impulseY * (1.0f / massA);
                    fruits[j].velocity.x += impulseX * (1.0f / massB);
                    fruits[j].velocity.y += impulseY * (1.0f / massB);
                }
            }
        }
    }

    void SpawnParticles(float x, float y, int tier) {
        for (int p = 0; p < 12; p++) {
            Particle part;
            part.position = { x, y };
            part.velocity = { (float)GetRandomValue(-150, 150), (float)GetRandomValue(-150, 150) };
            part.life = 1.0f;
            part.color = fruitDefs[tier].color;
            part.size = (float)GetRandomValue(4, 9);
            particles.push_back(part);
        }
    }

    void UpdateParticles() {
        for (auto& p : particles) {
            p.position.x += p.velocity.x * GetFrameTime();
            p.position.y += p.velocity.y * GetFrameTime();
            p.life -= 2.0f * GetFrameTime();
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return p.life <= 0; }), particles.end());
    }

    void CheckGameOver() {
        bool isOverflowing = false;
        for (const auto& f : fruits) {
            float speed = sqrt(f.velocity.x * f.velocity.x + f.velocity.y * f.velocity.y);
            if ((f.position.y - GetFruitRadius(f.tier) < overflowY) && (speed < 100.0f)) {
                isOverflowing = true;
                break;
            }
        }

        if (isOverflowing) {
            overflowTimer += GetFrameTime();
            if (overflowTimer > 2.0f) {
                currentState = GAMEOVER;
                if (score > highScore) {
                    highScore = score;
                    SaveHighScore(highScore);
                }
            }
        }
        else {
            overflowTimer -= GetFrameTime();
            if (overflowTimer < 0.0f) overflowTimer = 0.0f;
        }
    }

    // render
    void Draw() {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (backgroundTex.id != 0) {
            DrawTexture(backgroundTex, 0, 0, WHITE);
        }

        // walls
        DrawRectangle(0, 0, (int)wallThickness, screenHeight, GRAY);
        DrawRectangle(screenWidth - (int)wallThickness, 0, (int)wallThickness, screenHeight, GRAY);

        switch (currentState) {
        case MENU: DrawMenu(); break;
        case PLAYING: DrawPlaying(); break;
        case PAUSED: DrawPlaying(); DrawPaused(); break; 
        case GAMEOVER: DrawPlaying(); DrawGameOver(); break;
        case VICTORY: DrawPlaying(); DrawVictory(); break;
        }

        EndDrawing();
    }

    void DrawMenu() {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(SKYBLUE, 0.3f));

        const char* titleText = "RAJA BUAH";
        int titleW = MeasureText(titleText, 40);
        DrawText(titleText, (screenWidth - titleW) / 2, 200, 40, DARKGREEN);

        const char* startText = "Press ENTER to Start";
        int startW = MeasureText(startText, 30);
        DrawText(startText, (screenWidth - startW) / 2, 400, 30, DARKGRAY);

        const char* quitText = "Press ESC to Quit";
        int quitW = MeasureText(quitText, 20);
        DrawText(quitText, (screenWidth - quitW) / 2, 450, 20, DARKGRAY);

        Texture2D durianTex = fruitDefs[9].texture;
        DrawTexturePro(durianTex, { 0,0,(float)durianTex.width,(float)durianTex.height }, { screenWidth / 2.0f - 50, 300, 100, 100 }, { 0,0 }, 0.0f, WHITE);

        const char* scoreText = TextFormat("High Score: %i", highScore);
        int scoreW = MeasureText(scoreText, 20);
        DrawText(scoreText, (screenWidth - scoreW) / 2, 500, 20, MAROON);
    }

    void DrawPlaying() {
        // game over line
        Color lineColor = RED;
        if (overflowTimer > 0.0f && ((int)(GetTime() * 10) % 2 == 0)) lineColor = MAROON;
        DrawLine(0, overflowY, screenWidth, overflowY, lineColor);

        if (currentState == PLAYING) {
            DrawLine((int)dropperX, 0, (int)dropperX, screenHeight, Fade(LIGHTGRAY, 0.5f));
            Texture2D tex = fruitDefs[currentTier].texture;
            float r = fruitDefs[currentTier].radius;
            DrawTexturePro(tex, { 0,0,(float)tex.width,(float)tex.height }, { dropperX, 50.0f, r * 2, r * 2 }, { r,r }, 0.0f, WHITE);
        }

        for (const auto& fruit : fruits) {
            float r = GetFruitRadius(fruit.tier);
            Texture2D tex = fruitDefs[fruit.tier].texture;
            float rotation = fruit.position.x * 2.0f;
            DrawTexturePro(tex, { 0,0,(float)tex.width,(float)tex.height }, { fruit.position.x, fruit.position.y, r * 2, r * 2 }, { r,r }, rotation, WHITE);
        }

        for (const auto& p : particles) {
            DrawCircleV(p.position, p.size, Fade(p.color, p.life));
        }

        DrawUI();
    }

    void DrawUI() {
        // top left score
        DrawRectangleRounded({ 20, 20, 220, 80 }, 0.3f, 6, Fade(SKYBLUE, 0.7f));
        DrawText(TextFormat("Score: %i", score), 40, 40, 30, DARKBLUE);
        DrawText(TextFormat("High: %i", highScore), 40, 70, 20, MAROON);

        // top right, next fruit
        float boxX = (float)screenWidth - 140;
        float boxY = 20;
        float boxWidth = 120;
        float boxHeight = 120;

        DrawRectangleRounded({ boxX, boxY, boxWidth, boxHeight }, 0.3f, 6, Fade(LIGHTGRAY, 0.7f));
        DrawText("NEXT:", (int)boxX + 10, (int)boxY + 5, 20, DARKGRAY);

        float displayDiameter = 80.0f;
        float fruitCenterX = boxX + boxWidth / 2.0f;
        float fruitCenterY = boxY + 25 + (boxHeight - 25) / 2.0f;

        Texture2D nextTex = fruitDefs[nextTier].texture;
        DrawTexturePro(nextTex, { 0,0,(float)nextTex.width,(float)nextTex.height }, { fruitCenterX, fruitCenterY, displayDiameter, displayDiameter }, { displayDiameter / 2.0f, displayDiameter / 2.0f }, 0.0f, WHITE);
    }

    void DrawPaused() {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.4f));
        DrawText("PAUSED", screenWidth / 2 - 60, screenHeight / 2 - 20, 40, WHITE);
        DrawText("Press P to Resume", screenWidth / 2 - 80, screenHeight / 2 + 30, 20, LIGHTGRAY);
        DrawText("Press ESC to Menu", screenWidth / 2 - 80, screenHeight / 2 + 60, 20, LIGHTGRAY);
    }

    void DrawGameOver() {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.8f));

        const char* goText = "GAME OVER";
        int goW = MeasureText(goText, 50);
        DrawText(goText, (screenWidth - goW) / 2, 300, 50, RED);

        const char* finalScoreText = TextFormat("Final Score: %i", score);
        int fsW = MeasureText(finalScoreText, 30);
        DrawText(finalScoreText, (screenWidth - fsW) / 2, 380, 30, WHITE);

        if (score >= highScore && score > 0) {
            const char* newHighText = "NEW HIGH SCORE!";
            int nhW = MeasureText(newHighText, 30);
            DrawText(newHighText, (screenWidth - nhW) / 2, 420, 30, GOLD);
        }

        const char* menuText = "Press R to Menu";
        int menuW = MeasureText(menuText, 20);
        DrawText(menuText, (screenWidth - menuW) / 2, 500, 20, LIGHTGRAY);
    }

    void DrawVictory() {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(GOLD, 0.9f));
        DrawText("KING OF FRUITS!", 80, 300, 50, MAROON);
        DrawText("You made the Durian!", 140, 360, 30, BLACK);
        DrawText(TextFormat("Final Score: %i", score), 180, 420, 30, DARKBLUE);
        DrawText("Press R to Menu", 190, 500, 20, BLACK);
    }
};

int main() {
    RajaBuahGame game;
    game.Run();
    return 0;
}