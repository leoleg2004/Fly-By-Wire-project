#include "FlightDisplay.hpp"
#include "Telemetry.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

// Colori di sistema
#define COL_BODY    CLITERAL(Color){ 45, 50, 55, 255 }
#define COL_EDGE    CLITERAL(Color){ 70, 75, 80, 255 }
#define COL_GLASS   CLITERAL(Color){ 200, 160, 50, 230 }
#define COL_NOZZLE  CLITERAL(Color){ 20, 20, 20, 255 }
#define COL_FIRE    CLITERAL(Color){ 255, 100, 50, 200 }

// Variabili globali per la sessione grafica
static int cameraMode = 0; // 0 = Chase, 1 = Side, 2 = Front Cinematic

FlightDisplay::FlightDisplay(int width, int height, const std::string& title) {
    InitWindow(width, height, title.c_str());
    SetTargetFPS(60);

    skyModel = LoadModel("sky.glb");
    if (skyModel.meshCount > 0) skyLoaded = true;
    else { skyLoaded = false; TraceLog(LOG_WARNING, "ATTENZIONE: Impossibile caricare sky.glb"); }

    modelF35 = LoadModel("f35.glb");
    modelAnims = LoadModelAnimations("f35.glb", &animsCount);
    gearFrame = 0.0f;
    gearOpen = true;
    modelF35.transform = MatrixIdentity();

    if (modelF35.meshCount > 0) modelLoaded = true;
    else { modelLoaded = false; TraceLog(LOG_WARNING, "ATTENZIONE: Impossibile caricare f35.glb"); }

    if (animsCount > 0) gearFrame = modelAnims[0].frameCount - 1;
    flapFrame = 0;
    flapOpen = false;

    mapLoaded = false;

    camera.position = (Vector3){ 0.0f, 15.0f, -35.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    cameraPositionLag = camera.position;
}

FlightDisplay::~FlightDisplay() {
    if (skyLoaded) UnloadModel(skyModel);
    if (modelLoaded) {
        if (modelAnims) UnloadModelAnimations(modelAnims, animsCount);
        UnloadModel(modelF35);
    }
    CloseWindow();
}

bool FlightDisplay::IsActive() { return !WindowShouldClose(); }

// ========================================================
// LETTURA COMANDI (Aggiunti Tasti L e C)
// ========================================================
void FlightDisplay::HandleInput(PlaneData& data) {

    bool isPitching = false;
    bool isRolling = false;

    // --- TOGGLE CARRELLO E ATTERRAGGIO (TASTO L) ---
    if (IsKeyPressed(KEY_L)) {
        data.landing_mode = !data.landing_mode;
        gearOpen = data.landing_mode; // Apriamo/Chiudiamo anche i carrelli visivi!
    }

    // --- TOGGLE TELECAMERE (TASTO C) ---
    if (IsKeyPressed(KEY_C)) {
        cameraMode = (cameraMode + 1) % 3; // Cicla tra 0, 1 e 2
    }

    // 1. LETTURA CLOCHE
    if (IsKeyDown(KEY_UP)) {
        data.pitch += 0.03f;
        isPitching = true;
    }
    if (IsKeyDown(KEY_DOWN)) {
        data.pitch -= 0.03f;
        isPitching = true;
    }
    if (IsKeyDown(KEY_LEFT)) {
        data.roll -= 0.025f;
        isRolling = true;
    }
    if (IsKeyDown(KEY_RIGHT)) {
        data.roll += 0.025f;
        isRolling = true;
    }

    // 2. LETTURA TIMONE
    if (IsKeyDown(KEY_Q)) data.yaw += 0.01f;
    if (IsKeyDown(KEY_E)) data.yaw -= 0.01f;

    // Raddrizzamento naturale morbido
    if (!isRolling) {
        data.roll = Lerp(data.roll, 0.0f, 0.015f);
    }
    if (!isPitching) {
        data.pitch = Lerp(data.pitch, 0.0f, 0.005f);
    }

    // 3. GESTIONE MOTORE
    if (IsKeyDown(KEY_SPACE)) {
        data.speed = 0.0f;
    } else {
        if (IsKeyDown(KEY_W)) {
            data.speed += 0.8f;
        } else if (IsKeyDown(KEY_S)) {
            data.speed -= 1.2f;
        } else {
            if (data.speed > 0) data.speed -= 0.15f;
        }
    }

    // Limiti velocità (Se in atterraggio, limite raccomandato visivamente ma meccanica max a 200)
    if (data.speed > 200.0f) data.speed = 200.0f;
    if (data.speed < 0.0f) data.speed = 0.0f;

    UpdateAnimations();
}

// ========================================================
// SISTEMA TELECAMERE MULTIPLE
// ========================================================
void FlightDisplay::UpdateChaseCamera(const PlaneData& data) {
    float renderAlt = data.altitude / 1.5f;
    float speedRatio = std::min(data.speed / 200.0f, 1.0f);

    Vector3 idealPos;
    Vector3 targetLook;
    float trackingSpeed = 0.25f + (speedRatio * 0.15f);

    switch (cameraMode) {
        case 0: // BATTLEFIELD CHASE (Dietro all'aereo)
            {
                float distH = 26.0f + (speedRatio * 6.0f);
                float camHeight = 6.0f + (data.pitch * 6.0f);

                idealPos.x = data.x - (std::sin(data.yaw) * distH);
                idealPos.z = data.z - (std::cos(data.yaw) * distH);
                idealPos.y = renderAlt + camHeight;

                targetLook.x = data.x + (std::sin(data.yaw) * 5.0f);
                targetLook.y = renderAlt + 2.0f;
                targetLook.z = data.z + (std::cos(data.yaw) * 5.0f);

                camera.up = (Vector3){ std::sin(data.roll * 0.8f), std::cos(data.roll * 0.8f), 0.0f };
                camera.fovy = 65.0f + (speedRatio * 20.0f);
            }
            break;

        case 1: // CINEMATIC SIDE VIEW (Vera vista videogioco laterale)
            {
                // La camera si posiziona a +90 gradi rispetto a dove punta il muso
                float sideAngle = data.yaw + PI / 2.0f;
                float distSide = 40.0f; // Distanza laterale

                idealPos.x = data.x + (std::sin(sideAngle) * distSide);
                idealPos.z = data.z + (std::cos(sideAngle) * distSide);
                idealPos.y = renderAlt + 5.0f; // Leggermente sopra

                targetLook.x = data.x;
                targetLook.y = renderAlt;
                targetLook.z = data.z;

                camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; // Orizzonte fisso per ammirare il rollio dell'aereo
                camera.fovy = 60.0f;
            }
            break;

        case 2: // FRONT FLY-BY (Davanti all'aereo, guarda il muso)
            {
                float distFront = 50.0f;

                idealPos.x = data.x + (std::sin(data.yaw) * distFront);
                idealPos.z = data.z + (std::cos(data.yaw) * distFront);
                idealPos.y = renderAlt + 5.0f;

                targetLook.x = data.x;
                targetLook.y = renderAlt;
                targetLook.z = data.z;

                camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
                camera.fovy = 55.0f;
            }
            break;
    }

    // Interpolo dolcemente la telecamera verso la posizione ideale
    cameraPositionLag.x = Lerp(cameraPositionLag.x, idealPos.x, trackingSpeed);
    cameraPositionLag.y = Lerp(cameraPositionLag.y, idealPos.y, trackingSpeed * 0.8f);
    cameraPositionLag.z = Lerp(cameraPositionLag.z, idealPos.z, trackingSpeed);

    camera.position = cameraPositionLag;
    camera.target = targetLook;
}

void FlightDisplay::UpdateAnimations() {
    if (animsCount <= 0 || modelAnims == nullptr) return;
    int maxFrames = modelAnims[0].frameCount - 1;

    float speed = 60.0f;
    if (gearOpen) {
        gearFrame += GetFrameTime() * speed;
        if (gearFrame >= maxFrames) gearFrame = (float)maxFrames;
    } else {
        gearFrame -= GetFrameTime() * speed;
        if (gearFrame <= 0.0f) gearFrame = 0.0f;
    }
    UpdateModelAnimation(modelF35, modelAnims[0], (int)gearFrame);
}

void FlightDisplay::DrawMapWorld(const PlaneData& data) { }

void FlightDisplay::DrawSky(Vector3 camPos) {
    Color zenithColor  = { 20, 50, 110, 255 };
    Color sunCore      = { 255, 250, 240, 255 };
    Color sunGlow      = { 255, 200, 120, 255 };
    ClearBackground(zenithColor);

    if (skyLoaded) {
        rlDisableDepthMask();
        rlDisableDepthTest();
        rlDisableBackfaceCulling();

        rlPushMatrix();
            rlTranslatef(camPos.x * 0.99f, camPos.y, camPos.z * 0.99f);
            rlRotatef(GetTime() * 0.02f, 0, 1, 0);
            DrawModel(skyModel, (Vector3){0, 0, 0}, 20.0f, WHITE);
        rlPopMatrix();

        rlEnableBackfaceCulling();
        rlEnableDepthTest();
        rlEnableDepthMask();
    }

    rlDisableDepthMask();
    Vector3 sunPos = { camPos.x + 25000.0f, camPos.y + 15000.0f, camPos.z - 25000.0f };

    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 6; i > 0; i--) {
        float size = 800.0f + (i * 1200.0f);
        unsigned char alpha = (unsigned char)(255 / (i * i + 1));
        Color layerColor = { sunGlow.r, sunGlow.g, sunGlow.b, alpha };
        if (i == 1) layerColor = sunCore;
        DrawSphere(sunPos, size, layerColor);
    }
    EndBlendMode();
    rlEnableDepthMask();
}

void FlightDisplay::DrawUltimateF35(const PlaneData& data) {
    if (!modelLoaded) return;

    float globalScale = 4.0f;
    float modelScaleAereo = 0.00015f * globalScale;
    float modelScaleFuoco = 1.1f * globalScale;

    float fuocoZ = -8.0f * globalScale;
    float fuocoY = 3.5f * globalScale;
    float fuocoX = 0.0f;

    modelF35.transform = MatrixIdentity();
    modelF35.transform = MatrixMultiply(modelF35.transform, MatrixRotateY(-90.0f * DEG2RAD));
    modelF35.transform = MatrixMultiply(modelF35.transform, MatrixRotateZ(0.0f * DEG2RAD));

    rlEnableDepthTest();
    rlDisableBackfaceCulling();

    DrawModel(modelF35, (Vector3){0, 0, 0}, modelScaleAereo, WHITE);

    if (data.system_active && data.speed > 5.0f) {
        rlPushMatrix();
            BeginBlendMode(BLEND_ADDITIVE);
            rlDisableDepthMask();

            float lunghezzaFiamma = 5.0f * modelScaleFuoco;
            Vector3 startPos = { fuocoX, fuocoY, fuocoZ };
            Vector3 endPos   = { fuocoX, fuocoY, fuocoZ - lunghezzaFiamma };

            DrawCylinderEx(startPos, endPos, 0.3f * modelScaleFuoco, 0.1f, 10, (Color){200, 220, 255, 255});
            Vector3 endBlue = { fuocoX, fuocoY, fuocoZ - (lunghezzaFiamma * 0.8f) };
            DrawCylinderEx(startPos, endBlue, 0.5f * modelScaleFuoco, 0.1f * modelScaleFuoco, 10, (Color){50, 150, 255, 200});
            Vector3 startOrange = { fuocoX, fuocoY, fuocoZ - 1.0f };
            Vector3 endOrange   = { fuocoX, fuocoY, fuocoZ - (lunghezzaFiamma * 0.6f) };
            DrawCylinderEx(startOrange, endOrange, 0.8f * modelScaleFuoco, 0.2f * modelScaleFuoco, 12, (Color){255, 50, 0, 100});

            float step = lunghezzaFiamma / 4.0f;
            for(int i=1; i<=3; i++) {
                DrawSphere((Vector3){fuocoX, fuocoY, fuocoZ - (step*i)}, 0.35f * modelScaleFuoco, (Color){255, 255, 200, 150});
            }
            DrawSphere(startPos, 0.8f * modelScaleFuoco, (Color){255, 100, 50, 150});

            rlEnableDepthMask();
            EndBlendMode();
        rlPopMatrix();
    }
    rlEnableBackfaceCulling();
}

void FlightDisplay::DrawHUD(const PlaneData& data) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int cx = sw / 2;
    int cy = sh / 2;

    Color hudMain  = { 255, 170, 0, 255 };
    Color hudGreen = { 50, 255, 50, 255 };
    Color hudRed   = { 255, 30, 30, 255 };
    Color hudDim   = Fade(hudMain, 0.4f);
    Color hudBg    = Fade({ 10, 10, 15, 255 }, 0.85f);

    auto DrawTechBox = [&](int x, int y, int w, int h, const char* title, bool alignRight = false) {
        DrawRectangle(x, y, w, h, hudBg);
        DrawRectangleLines(x, y, w, h, hudDim);
        int cl = 12; int ct = 2;
        DrawRectangle(x, y, cl, ct, hudMain); DrawRectangle(x, y, ct, cl, hudMain);
        DrawRectangle(x+w-cl, y, cl, ct, hudMain); DrawRectangle(x+w-ct, y, ct, cl, hudMain);
        DrawRectangle(x, y+h-ct, cl, ct, hudMain); DrawRectangle(x, y+h-cl, ct, cl, hudMain);
        DrawRectangle(x+w-cl, y+h-ct, cl, ct, hudMain); DrawRectangle(x+w-ct, y+h-cl, ct, cl, hudMain);
        int titleW = MeasureText(title, 10) + 20;
        int titleX = alignRight ? (x + w - titleW) : x;
        DrawRectangle(titleX, y - 18, titleW, 18, hudDim);
        DrawText(title, titleX + 10, y - 14, 10, hudMain);
    };


    int lx = 30, ly = cy - 80, lw = 220, lh = 150;
    DrawTechBox(lx, ly, lw, lh, "KINETIC SENSORS");
    DrawText("AIRSPEED (CAS)", lx + 15, ly + 20, 10, Fade(WHITE, 0.8f));
    DrawText(TextFormat("%03.0f", data.speed), lx + 15, ly + 40, 40, hudMain);
    DrawText("KPH", lx + 110, ly + 62, 12, hudMain);

    DrawText("ENGINE THRUST", lx + 15, ly + 100, 10, Fade(WHITE, 0.8f));
    float pwr = std::min(data.speed / 200.0f, 1.0f);
    for(int i = 0; i < 20; i++) {
        Color c = (i < pwr * 20) ? ((i >= 17) ? hudRed : hudMain) : hudDim;
        DrawRectangle(lx + 15 + (i * 9), ly + 115, 6, 15, c);
    }

    int rx = sw - 250, ry = cy - 80, rw = 220, rh = 160;
    DrawTechBox(rx, ry, rw, rh, "INERTIAL DATA", true);
    DrawText("ALTITUDE (MSL)", rx + 15, ry + 20, 10, Fade(WHITE, 0.8f));
    DrawText(TextFormat("%05.0f", data.altitude), rx + 15, ry + 40, 40, hudGreen);
    DrawText("M", rx + 150, ry + 62, 12, hudGreen);

    DrawRectangle(rx + 15, ry + 95, rw - 30, 1, hudDim);

    auto DrawDataRow = [&](int yOff, const char* lbl, const char* val) {
        DrawText(lbl, rx + 15, ry + yOff, 10, Fade(WHITE, 0.7f));
        DrawText(val, rx + rw - 15 - MeasureText(val, 10), ry + yOff, 10, hudMain);
    };
    DrawDataRow(105, "PITCH (RAD)", TextFormat("%+06.2f", data.pitch * RAD2DEG));
    DrawDataRow(120, "ROLL  (RAD)", TextFormat("%+06.2f", data.roll * RAD2DEG));
    DrawDataRow(135, "YAW   (RAD)", TextFormat("%+06.2f", data.yaw * RAD2DEG));

    int tx = cx - 180, ty = 25, tw = 360, th = 35;
    DrawTechBox(tx, ty, tw, th, "HEADING (AZIMUTH)");
    BeginScissorMode(tx + 5, ty + 5, tw - 10, th - 10);
    float head = data.yaw * RAD2DEG;
    for (int i = -180; i <= 540; i += 10) {
        float px = (tx + tw/2) + (i - head) * 5;
        if (px > tx && px < tx + tw) {
            DrawLineEx({px, (float)ty + 15}, {px, (float)ty + 30}, 2.0f, hudMain);
            if (i % 30 == 0) {
                const char* lbl = (i%360==0)?"N":(i%360==90)?"E":(i%360==180)?"S":(i%360==270)?"W":TextFormat("%03d", i%360);
                DrawText(lbl, px - MeasureText(lbl, 10)/2, ty + 4, 10, WHITE);
            }
        }
    }
    EndScissorMode();
    DrawTriangle({(float)cx, (float)ty + 35}, {(float)cx - 6, (float)ty + 42}, {(float)cx + 6, (float)ty + 42}, hudRed);


    // =========================================================
    // HUD: ALLARMI E MODALITÀ ATTERRAGGIO
    // =========================================================
    bool blink = ((int)(GetTime() * 8) % 2 == 0);
    bool hasAlarm = false;
    const char* warnMsg = "";

    // SE IL LANDING MODE È ATTIVO:
    // Spegne gli allarmi del terreno per permetterti di atterrare e mostra uno stato verde
    if (data.landing_mode) {
        DrawRectangleLinesEx({0, 0, (float)sw, (float)sh}, 6.0f, hudGreen);
        int wx = cx - 220, wy = cy + 120, ww = 440, wh = 50;
        DrawRectangle(wx, wy, ww, wh, Fade({10, 50, 10, 255}, 0.9f));
        DrawRectangleLinesEx({(float)wx, (float)wy, (float)ww, (float)wh}, 2.0f, hudGreen);
        DrawText("LANDING MODE ENGAGED - FBW DISABLED", cx - MeasureText("LANDING MODE ENGAGED - FBW DISABLED", 20)/2, wy + 15, 20, hudGreen);
    }
    // SE IL LANDING MODE È SPENTO: Normali allarmi
    else {
        if (data.altitude < 2000)            { hasAlarm = true; warnMsg = "TERRAIN PULL UP"; }
        else if (data.altitude > 12500)      { hasAlarm = true; warnMsg = "OVERSHOOT PULL DOWN"; }
        else if (std::abs(data.roll) > 1.6f) { hasAlarm = true; warnMsg = "AUTOPILOT RECOVERY ENGAGED"; }
        else if (std::abs(data.roll) > 1.0f) { hasAlarm = true; warnMsg = "CRITICAL BANK ANGLE"; }

        if (hasAlarm) {
            DrawRectangleLinesEx({0, 0, (float)sw, (float)sh}, 6.0f, blink ? hudRed : Fade(hudRed, 0.4f));
            int wx = cx - 220, wy = cy + 120, ww = 440, wh = 50;
            DrawRectangle(wx, wy, ww, wh, blink ? Fade(hudRed, 0.5f) : Fade(BLACK, 0.9f));
            DrawRectangleLinesEx({(float)wx, (float)wy, (float)ww, (float)wh}, 2.0f, hudRed);
            const char* fullMsg = TextFormat("! ! !  %s  ! ! !", warnMsg);
            DrawText(fullMsg, cx - MeasureText(fullMsg, 20)/2, wy + 15, 20, blink ? WHITE : hudRed);
        }
    }

    DrawText("SYS: F-35 LEO-FLIGHT-OS v29.0 // CAMERA [C] - LANDING [L]", 30, sh - 30, 10, hudDim);
    for(int i = 0; i < sh; i += 3) DrawLine(0, i, sw, i, Fade(BLACK, 0.15f));
}

void FlightDisplay::Draw(const PlaneData& data) {
    BeginDrawing();
    ClearBackground(BLACK);

    UpdateChaseCamera(data);

    rlSetClipPlanes(1.0f, 100000.0f);

    BeginMode3D(camera);

        DrawSky(camera.position);

        rlPushMatrix();
            rlTranslatef(data.x, data.altitude/1.5f, data.z);
            rlRotatef(data.yaw * RAD2DEG, 0, 1, 0);
            rlRotatef(data.pitch * RAD2DEG, -1, 0, 0);
            rlRotatef(data.roll * RAD2DEG, 0, 0, 1);
            DrawUltimateF35(data);
        rlPopMatrix();

    EndMode3D();

    DrawHUD(data);
    EndDrawing();
}
