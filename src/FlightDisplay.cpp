#include "FlightDisplay.hpp"
#include "Telemetry.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
//colori presi e utilizzati nel codice
#define COL_BODY    CLITERAL(Color){ 45, 50, 55, 255 }
#define COL_EDGE    CLITERAL(Color){ 70, 75, 80, 255 }
#define COL_GLASS   CLITERAL(Color){ 200, 160, 50, 230 }
#define COL_NOZZLE  CLITERAL(Color){ 20, 20, 20, 255 }
#define COL_FIRE    CLITERAL(Color){ 255, 100, 50, 200 }

// colori dell'ambiente
#define COL_SKY_TOP    CLITERAL(Color){ 30, 60, 100, 255 }  // Blu scuro in alto
#define COL_SKY_HOR    CLITERAL(Color){ 120, 150, 180, 255 }// Azzurro/Grigio all'orizzonte
#define COL_GRASS      CLITERAL(Color){ 30, 50, 30, 255 }   // Erba scura
#define COL_ROAD       CLITERAL(Color){ 40, 40, 40, 255 }   // Asfalto
#define COL_ROAD_LINE  CLITERAL(Color){ 180, 180, 180, 255 }// Strisce bianche










FlightDisplay::FlightDisplay(int width, int height, const std::string& title) {
    InitWindow(width, height, title.c_str());
    SetTargetFPS(60);

    modelF35 = LoadModel("f35.glb");
        modelAnims = LoadModelAnimations("f35.glb", &animsCount);

        gearFrame = 0.0f;
        gearOpen = true; // Partiamo con le ruote fuori

        // IMPORTANTE: Resetta la trasformazione del modello appena caricato
        modelF35.transform = MatrixIdentity();
    // Verifica se il modello è caricato correttamente
    if (modelF35.meshCount > 0) {
        modelLoaded = true;
    } else {
        modelLoaded = false;
        TraceLog(LOG_WARNING, "ATTENZIONE: Impossibile caricare f35.glb");
    }


    animsCount = 0;
    modelAnims = LoadModelAnimations("f35.glb", &animsCount);
    TraceLog(LOG_INFO, "Animazioni trovate nel file: %d", animsCount);


    gearFrame = 0;
    gearOpen = true; // Diciamo che parte con il carrello aperto
    flapFrame = 0;
    flapOpen = false;

    // Se abbiamo trovato animazioni, impostiamo il frame iniziale
    if (animsCount > 0) {

        gearFrame = modelAnims[0].frameCount - 1;
    }


    mapModel = LoadModel("map.glb");
    if (mapModel.meshCount > 0) {
        mapLoaded = true;
    } else {
        mapLoaded = false;
    }


    camera.position = (Vector3){ 0.0f, 15.0f, -35.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 50.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Inizializza la posizione ritardata uguale a quella di partenza
    cameraPositionLag = camera.position;
}

FlightDisplay::~FlightDisplay() {
    // 1. Scarica la mappa
    if (mapLoaded) {
        UnloadModel(mapModel);
    }

    // 2. Scarica Aereo e Animazioni
    if (modelLoaded) {

        if (modelAnims) {
            UnloadModelAnimations(modelAnims, animsCount);
        }
        UnloadModel(modelF35);
    }


    CloseWindow();
}
bool FlightDisplay::IsActive() {
    return !WindowShouldClose();
}

void FlightDisplay::HandleInput(PlaneData& data) {

    if (IsKeyDown(KEY_UP))    data.pitch += 0.01f;
    if (IsKeyDown(KEY_DOWN))  data.pitch -= 0.01f;
    if (IsKeyDown(KEY_LEFT))  data.roll -= 0.015f;
    if (IsKeyDown(KEY_RIGHT)) data.roll += 0.015f;
    if (IsKeyDown(KEY_Q))     data.yaw += 0.0045f;
    if (IsKeyDown(KEY_E))     data.yaw -= 0.0045f;
    float potenza = 0.5f;

    if (IsKeyDown(KEY_SPACE)) {
        data.speed = 0.0f;
        data.altitude -= 25.0f;
    }
    else {
        if (IsKeyDown(KEY_W) && !IsKeyDown(KEY_ZERO)) {
            data.speed += 0.5f;
            if (data.speed > 200) data.speed = 200;
        }
        else if (IsKeyDown(KEY_S) && !IsKeyDown(KEY_ZERO)) {
            data.speed -= 0.8f;
        }
        else {
            if (data.speed > 0) {
                data.speed -= 0.05f;
            }
        }

        data.speed += data.pitch * 0.05f;

        if (data.speed < 0) data.speed = 0;

        if (data.altitude > 0) {
            if(data.speed == 0 && data.pitch < -20){
                data.speed+=3.0f;
            } else if (data.speed < 50.0f) {
                float fattore_caduta = (50.0f - data.speed) / 10.0f;
                data.altitude -= fattore_caduta * 2;
            }
        }
    }

    if (data.altitude < 0) data.altitude = 0;
        UpdateAnimations();

}


//mi aggionra la camera con l'andamento dell'aereo
void FlightDisplay::UpdateChaseCamera(const PlaneData& data) {

    float visualAltScale = 20.0f + (data.altitude / 500.0f);

    Vector3 idealPos;
    idealPos.x = sin(data.yaw) * -20.0f;
    idealPos.y = (data.altitude / 10.0f) + 10.0f + (data.pitch * 20.0f);
    idealPos.z = -35.0f - (std::abs(data.pitch) * 10.0f);

    Vector3 targetLook;
    targetLook.x = 0.0f;
    targetLook.y = (data.altitude / 10.0f); // Guarda l'aereo
    targetLook.z = 100.0f;


    cameraPositionLag.x = Lerp(cameraPositionLag.x, idealPos.x, 0.1f);
    cameraPositionLag.y = Lerp(cameraPositionLag.y, idealPos.y, 0.1f);
    cameraPositionLag.z = Lerp(cameraPositionLag.z, idealPos.z, 0.1f);

    camera.position = cameraPositionLag;
    camera.target = targetLook;
    camera.up.x = sin(data.roll * 0.5f);
    camera.up.y = cos(data.roll * 0.5f);
}
void FlightDisplay::UpdateAnimations() {

    if (animsCount <= 0 || modelAnims == nullptr) return;

    int maxFrames = modelAnims[0].frameCount - 1;


    if (IsKeyPressed(KEY_G)) {
        gearOpen = !gearOpen;
    }


    float speed = 60.0f;
    if (gearOpen) {

        // Aumenta dolcemente
        gearFrame += GetFrameTime() * speed;


        if (gearFrame >= maxFrames) {
            gearFrame = (float)maxFrames;
        }

    } else {
        gearFrame -= GetFrameTime() * speed;

        //  Se va sotto zero, bloccalo a zero.
        if (gearFrame <= 0.0f) {
            gearFrame = 0.0f;
        }
    }


    UpdateModelAnimation(modelF35, modelAnims[0], (int)gearFrame);
}




//stampo la mappa partendo da un chank centrandolo sulla aereo passando la struct alla funzionee e rigenrandola all'infinito
void FlightDisplay::DrawMapWorld(const PlaneData& data) {
    if (!mapLoaded) return;


    float mapScale = 1.4f;
    float overlapFactor = 0.85f;


    float mountainStretch = 0.9f;


    BoundingBox box = GetModelBoundingBox(mapModel);

    float rawWidth  = (box.max.x - box.min.x) * mapScale;
    float rawLength = (box.max.z - box.min.z) * mapScale;
    float mapWidth  = rawWidth * overlapFactor;
    float mapLength = rawLength * overlapFactor;

    float offsetX = (box.min.x + box.max.x) / 2.0f * mapScale;
    float offsetZ = (box.min.z + box.max.z) / 2.0f * mapScale;


    float lowestPoint = box.min.y * mapScale * mountainStretch;

    float finalY = -15.0f*lowestPoint;

//li calcolo in base alla posizione dell'aereo
    int currentChunkX = (int)round(data.x / mapWidth);
    int currentChunkZ = (int)round(data.z / mapLength);
    int renderDistance = 5;

    rlDisableBackfaceCulling();

    for (int x = -renderDistance; x <= renderDistance; x++) {
        for (int z = -renderDistance; z <= renderDistance; z++) {

            float chunkX = (currentChunkX + x) * mapWidth;
            float chunkZ = (currentChunkZ + z) * mapLength;

            rlPushMatrix();
                // A. Posiziona il pezzo di mappa
                rlTranslatef(chunkX - offsetX, finalY, chunkZ - offsetZ);

                // B. (NUOVO) Stira le montagne verso l'alto
                // Scaliamo solo l'asse Y (il secondo parametro)
                rlScalef(1.0f, mountainStretch, 1.0f);

                DrawModel(mapModel, (Vector3){0, 0, 0}, mapScale, WHITE);
            rlPopMatrix();
        }
    }
    rlEnableBackfaceCulling();
}







//funzione fatta per disegnare il cielo
void FlightDisplay::DrawSky(Vector3 camPos) {
    // 1. Colore del cielo allo Zenit (sopra la testa) - Blu scuro
    Color topColor = (Color){ 60, 120, 200, 255 };
    Color horizonColor = (Color){ 200, 220, 255, 255 }; // Bianco-Azzurrino all'orizzonte

    ClearBackground(topColor);


    rlDisableDepthMask();
    rlDisableBackfaceCulling();


    float sunDistance = 25000.0f;


    Vector3 sunPos = {
        camPos.x + sunDistance,
        camPos.y + (sunDistance * 0.4f),
    };


    DrawSphere(sunPos, 800.0f, (Color){ 255, 255, 220, 255 });
    DrawSphere(sunPos, 1500.0f, (Color){ 255, 255, 200, 80 });


    rlPushMatrix();
    rlTranslatef(camPos.x, camPos.y - 2000.0f, camPos.z); // Ci segue e parte dal basso

    float raggioCielo = 10000.0f; // Cilindro gigante
    float altezzaCielo = 12000.0f;


    rlBegin(RL_QUADS);

        // FACCIA 1
        rlColor4ub(horizonColor.r, horizonColor.g, horizonColor.b, horizonColor.a);
        rlVertex3f(-raggioCielo, 0, -raggioCielo);
        rlVertex3f(raggioCielo, 0, -raggioCielo);

        rlColor4ub(topColor.r, topColor.g, topColor.b, 0); // Trasparente in alto
        rlVertex3f(raggioCielo, altezzaCielo, -raggioCielo);
        rlVertex3f(-raggioCielo, altezzaCielo, -raggioCielo);

        // FACCIA 2
        rlColor4ub(horizonColor.r, horizonColor.g, horizonColor.b, horizonColor.a);
        rlVertex3f(raggioCielo, 0, -raggioCielo);
        rlVertex3f(raggioCielo, 0, raggioCielo);

        rlColor4ub(topColor.r, topColor.g, topColor.b, 0);
        rlVertex3f(raggioCielo, altezzaCielo, raggioCielo);
        rlVertex3f(raggioCielo, altezzaCielo, -raggioCielo);

        // FACCIA 3
        rlColor4ub(horizonColor.r, horizonColor.g, horizonColor.b, horizonColor.a);
        rlVertex3f(raggioCielo, 0, raggioCielo);
        rlVertex3f(-raggioCielo, 0, raggioCielo);

        rlColor4ub(topColor.r, topColor.g, topColor.b, 0);
        rlVertex3f(-raggioCielo, altezzaCielo, raggioCielo);
        rlVertex3f(raggioCielo, altezzaCielo, raggioCielo);

        // FACCIA 4
        rlColor4ub(horizonColor.r, horizonColor.g, horizonColor.b, horizonColor.a);
        rlVertex3f(-raggioCielo, 0, raggioCielo);
        rlVertex3f(-raggioCielo, 0, -raggioCielo);

        rlColor4ub(topColor.r, topColor.g, topColor.b, 0);
        rlVertex3f(-raggioCielo, altezzaCielo, -raggioCielo);
        rlVertex3f(-raggioCielo, altezzaCielo, raggioCielo);
    rlEnd();

    rlPopMatrix();

    // Ripristina le impostazioni normali per disegnare l'aereo dopo
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}







void FlightDisplay::DrawUltimateF35(const PlaneData& data) {
    if (!modelLoaded) return;


    float modelScaleAereo = 0.00015f;


    float modelScaleFuoco = 1.1f;


    float fuocoZ = -8.0f;
    float fuocoY = 3.5f;
    float fuocoX = 0.0f;



    modelF35.transform = MatrixIdentity();
    modelF35.transform = MatrixMultiply(modelF35.transform, MatrixRotateY(-90.0f * DEG2RAD));
    modelF35.transform = MatrixMultiply(modelF35.transform, MatrixRotateZ(0.0f * DEG2RAD));

    rlEnableDepthTest();
    rlDisableBackfaceCulling();

    DrawModel(modelF35, (Vector3){0, 0, 0}, modelScaleAereo, WHITE);

    // come implemento il fuoco
    if (data.system_active) {
        rlPushMatrix();
            BeginBlendMode(BLEND_ADDITIVE);
            rlDisableDepthMask();

            float lunghezzaFiamma = 5.0f * modelScaleFuoco;

            Vector3 startPos = { fuocoX, fuocoY, fuocoZ };
            Vector3 endPos   = { fuocoX, fuocoY, fuocoZ - lunghezzaFiamma };

            // 1. Nucleo centrale (Più sottile)
            DrawCylinderEx(startPos, endPos, 0.3f * modelScaleFuoco, 0.1f, 10, (Color){200, 220, 255, 255});

            // 2. Fiamma principale
            Vector3 endBlue = { fuocoX, fuocoY, fuocoZ - (lunghezzaFiamma * 0.8f) };
            DrawCylinderEx(startPos, endBlue, 0.5f * modelScaleFuoco, 0.1f * modelScaleFuoco, 10, (Color){50, 150, 255, 200});

            // 3. Alone esterno
            Vector3 startOrange = { fuocoX, fuocoY, fuocoZ - 1.0f };
            Vector3 endOrange   = { fuocoX, fuocoY, fuocoZ - (lunghezzaFiamma * 0.6f) };
            DrawCylinderEx(startOrange, endOrange, 0.8f * modelScaleFuoco, 0.2f * modelScaleFuoco, 12, (Color){255, 50, 0, 100});

            // 4. Diamanti di shock (Più piccoli)
            float step = lunghezzaFiamma / 4.0f;
            for(int i=1; i<=3; i++) {
                DrawSphere((Vector3){fuocoX, fuocoY, fuocoZ - (step*i)}, 0.35f * modelScaleFuoco, (Color){255, 255, 200, 150});
            }

            // Bagliore
            DrawSphere(startPos, 0.8f * modelScaleFuoco, (Color){255, 100, 50, 150});

            rlEnableDepthMask();
            EndBlendMode();
        rlPopMatrix();
    }
    rlEnableBackfaceCulling();
}




//qua disegno la legenda e anche i messaggi a schermo in base all'altitudine
void FlightDisplay::DrawHUD(const PlaneData& data) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int cx = sw/2; int cy = sh/2;


    DrawRectangle(20, 20, 320, 170, Fade(BLACK, 0.7f));
    DrawRectangleLines(20, 20, 320, 170, GREEN);
    DrawText("F-35 Fly-By-Wire Leo Version", 30, 30, 10, BLACK);
    DrawText(TextFormat("ALTITUDE : %05.0f FT", data.altitude,"metri"), 30, 25, 25, WHITE);

    float DriftVelocity = data.pitch * 2000; // Stima drift velocity
    Color DriftCol = (DriftVelocity>100) ? WHITE: (DriftVelocity < -100 ? RED : WHITE);
    DrawText(TextFormat("PITCH(RAD): %+04.0f", data.pitch*RAD2DEG), 30, 55, 20, GREEN);
    DrawText(TextFormat("ROLL(RAD): %+04.0f", data.roll*RAD2DEG), 30, 85, 20, GREEN);
    DrawText(TextFormat("YAW (RAD): %+04.0f", data.yaw*RAD2DEG), 30, 115, 20, GREEN);
    DrawText(TextFormat("SPEED(Km/h): %+04.0f", data.speed), 30, 145, 20, GREEN);


    if (data.altitude < 2000) {
        DrawText("TERRAIN! PULL UP!", cx - 150, sh - 180, 40, RED);
        DrawRectangleLines(0,0,sw,sh,RED);

    }else if(data.altitude >12500){

        DrawText("TOO HIGH! PULL DOWN!", cx - 150, sh - 180, 40, RED);
                DrawRectangleLines(0,0,sw,sh,RED);
    }


    if (std::abs(data.roll) > 1.2f) {

            // Scritta lampeggiante
    		//il getime serve per far comparire e sparire la scritta
            if ((int)(GetTime() * 4) % 2 == 0) {
                DrawText("BANK ANGLE", sw/2 - 120, sh - 250, 40, ORANGE);
            }

            // Frecce direzionali
            if (data.roll > 0) {
                // Se sei inclinato a Destra, freccia verso Sinistra
                DrawText("<<<", sw/2 + 100, sh/2, 50, RED);
            } else {
                // Se sei inclinato a Sinistra, freccia verso Destra
                DrawText(">>>", sw/2 - 180, sh/2, 50, RED);
            }
        }
}


//in questo metodo stampo tutto aereo e mappa e metto gli scalingDrawSky(camera.position);
void FlightDisplay::Draw(const PlaneData& data) {

    BeginDrawing();


    float distH = 50.0f;
    float distV = 20.0f;
//controlla effettivamente il movimento dell'aereo
    Vector3 camPos;
    camPos.x = data.x - (std::sin(data.yaw) * distH);
    camPos.z = data.z - (std::cos(data.yaw) * distH);
    camPos.y = (data.altitude/1.5f) + distV;

    camera.position = camPos;
    camera.target = (Vector3){ data.x, data.altitude/1.5f, data.z };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;


    rlSetClipPlanes(1.0f, 100000.0f);

    BeginMode3D(camera);


        DrawSky(camera.position);


        DrawMapWorld(data);

        rlPushMatrix();

            rlTranslatef(data.x, data.altitude/1.5f, data.z);

            // Ruotiamo tutto (Aereo + Fuoco insieme)
            rlRotatef(data.yaw * RAD2DEG, 0, 1, 0);
            rlRotatef(data.pitch * RAD2DEG, -1, 0, 0);
            rlRotatef(data.roll * RAD2DEG, 0, 0, 1);

            // Funzione che disegna il mio aereo
            DrawUltimateF35(data);

        rlPopMatrix();

    EndMode3D();

    DrawHUD(data);
    EndDrawing();
}
