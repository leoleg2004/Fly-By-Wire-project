#include "FlightDisplay.hpp"
#include "Telemetry.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

// Colori di sistema
#define COL_BODY CLITERAL(Color){45, 50, 55, 255}
#define COL_EDGE CLITERAL(Color){70, 75, 80, 255}
#define COL_GLASS CLITERAL(Color){200, 160, 50, 230}
#define COL_NOZZLE CLITERAL(Color){20, 20, 20, 255}
#define COL_FIRE CLITERAL(Color){255, 100, 50, 200}

// Variabili globali per la sessione grafica
static int cameraMode = 0; // 0 = Chase, 1 = Side, 2 = Front Cinematic

FlightDisplay::FlightDisplay(int width, int height, const std::string &title) {
  InitWindow(width, height, title.c_str());
  InitAudioDevice();

  sndEngineStart = LoadSound("engine_start.wav");
  sndEngineLoop = LoadSound("engine_loop.wav");
  sndEngineDown = LoadSound("engine_down.wav");
  sndGear = LoadSound("gear.wav");
  sndLanding = LoadSound("landing.wav");
  sndWarning = LoadSound("warning.wav");
  sndPullUp = LoadSound("pull_up.wav");
  sndCaution = LoadSound("caution.wav");
  sndAir = LoadSound("air.wav");

  isEngineStarting = false;
  SetTargetFPS(60);

  skyModel = LoadModel("sky.glb");
  if (skyModel.meshCount > 0)
    skyLoaded = true;
  else {
    skyLoaded = false;
    TraceLog(LOG_WARNING, "ATTENZIONE: Impossibile caricare sky.glb");
  }

  modelF35 = LoadModel("f35.glb");
  modelAnims = LoadModelAnimations("f35.glb", &animsCount);
  modelF35 = LoadModel("f35.glb");
  modelAnims = LoadModelAnimations("f35.glb", &animsCount);

  // Fix per i materiali .glb corrotti o salvati con opacità zero
  for (int i = 0; i < modelF35.materialCount; i++) {
    if (modelF35.materials[i].maps[MATERIAL_MAP_ALBEDO].color.a == 0) {
      modelF35.materials[i].maps[MATERIAL_MAP_ALBEDO].color.a = 255;
    }
  }

  gearFrame = 0.0f;
  gearOpen = true;
  modelF35.transform = MatrixIdentity();

  if (modelF35.meshCount > 0)
    modelLoaded = true;
  else {
    modelLoaded = false;
    TraceLog(LOG_WARNING, "ATTENZIONE: Impossibile caricare f35.glb");
  }

  if (animsCount > 0)
    gearFrame = modelAnims[0].keyframeCount - 1;
  flapFrame = 0;
  flapOpen = false;

  mapModel = LoadModel("aerodrome.glb");
  if (mapModel.meshCount > 0) {
    mapLoaded = true;
  } else {
    mapLoaded = false;
    TraceLog(LOG_WARNING, "ATTENZIONE: Impossibile caricare aerodrome.glb");
  }

  camera.position = (Vector3){0.0f, 15.0f, -35.0f};
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 70.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  cameraPositionLag = camera.position;
}

FlightDisplay::~FlightDisplay() {
  if (mapLoaded)
    UnloadModel(mapModel);
  if (skyLoaded)
    UnloadModel(skyModel);
  if (modelLoaded) {
    if (modelAnims)
      UnloadModelAnimations(modelAnims, animsCount);
    UnloadModel(modelF35);
    UnloadSound(sndEngineStart);
    UnloadSound(sndEngineLoop);
    UnloadSound(sndEngineDown);
    UnloadSound(sndGear);
    UnloadSound(sndLanding);
    UnloadSound(sndWarning);
    UnloadSound(sndPullUp);
    UnloadSound(sndCaution);
    UnloadSound(sndAir);
  }
  CloseWindow();
}

bool FlightDisplay::IsActive() { return !WindowShouldClose(); }
void FlightDisplay::HandleInput(PlaneData &data, PilotInput &pilot_out) {
  bool isPitching = false;
  bool isRolling = false;
  float dt = GetFrameTime();

  // ==========================================
  // VARIABILI DI STATO (FSM)
  // ==========================================
  static double engineStartTime = 0.0;
  static bool hasTakenOff = false;
  static bool engineReady = false;

  if (data.altitude > 100.0f)
    hasTakenOff = true;

  // --- LOGICA DI AUTO-SHUTDOWN ---
  if (hasTakenOff && data.altitude < 2.0f && data.speed <= 1.0f &&
      data.system_active) {
    data.system_active = false;
    engineReady = false;

    StopSound(sndEngineStart);
    StopSound(sndEngineLoop);

    SetSoundPitch(sndEngineDown, 0.65f);
    SetSoundVolume(sndEngineDown, 0.3f);
    PlaySound(sndEngineDown);

    StopSound(sndLanding);
    StopSound(sndAir);
    StopSound(sndWarning);
    StopSound(sndPullUp);
    hasTakenOff = false;
  }

  // ==========================================
  // 1. SISTEMI CARRELLO E AUTOPILOTA
  // ==========================================
  if (IsKeyPressed(KEY_G)) {
    gearOpen = !gearOpen;
    StopSound(sndGear);
    if (gearOpen)
      SetSoundPitch(sndGear, 0.85f);
    else
      SetSoundPitch(sndGear, 1.15f);
    SetSoundVolume(sndGear, 1.0f);
    PlaySound(sndGear);
  }

  if (IsKeyPressed(KEY_L)) {
    data.landing_mode = !data.landing_mode;
    if (data.landing_mode) {
      StopSound(sndCaution);
      SetSoundVolume(sndCaution, 1.0f);
      SetSoundPitch(sndCaution, 1.0f);
      PlaySound(sndCaution);
    }
  }

  if (IsKeyPressed(KEY_C))
    cameraMode = (cameraMode + 1) % 3;

  // RACCOLTA INPUT FBW — comandi pilota normalizzati per il FCC
  // In Normal Law: stick_pitch/roll → rate demand (non angolo diretto)
  pilot_out.stick_pitch = 0.0f;
  pilot_out.stick_roll = 0.0f;
  if (IsKeyDown(KEY_UP)) {
    pilot_out.stick_pitch = +1.0f;
    isPitching = true;
  }
  if (IsKeyDown(KEY_DOWN)) {
    pilot_out.stick_pitch = -1.0f;
    isPitching = true;
  }
  if (IsKeyDown(KEY_LEFT)) {
    pilot_out.stick_roll = -1.0f;
    isRolling = true;
  }
  if (IsKeyDown(KEY_RIGHT)) {
    pilot_out.stick_roll = +1.0f;
    isRolling = true;
  }

  // --- Voice Warning System (VWS) ---
  bool dangerPullUp = false;
  bool dangerBank = false;

  if (data.system_active) {
    // VWS Trigger:
    // Audio pull-up (Terreno/Emergenza) < 2000m o > 12500m
    dangerPullUp = hasTakenOff && !data.landing_mode &&
                   (data.altitude < 2000.0f || data.altitude > 12500.0f);

    // Audio bank: suona poco prima dell'ingaggio autopilota (0.8 rad = 45°)
    dangerBank = (!data.landing_mode && std::abs(data.roll) > 0.8f);

    if (dangerPullUp) {
      StopSound(sndWarning);
      if (!IsSoundPlaying(sndPullUp)) {
        SetSoundVolume(sndPullUp, 1.0f);
        SetSoundPitch(sndPullUp, 1.0f);
        PlaySound(sndPullUp);
      }
    } else if (dangerBank) {
      if (!IsSoundPlaying(sndWarning)) {
        SetSoundVolume(sndWarning, 1.0f);
        SetSoundPitch(sndWarning, 1.0f);
        PlaySound(sndWarning);
      }
    } else {
      if (IsSoundPlaying(sndPullUp))
        StopSound(sndPullUp);
      if (IsSoundPlaying(sndWarning))
        StopSound(sndWarning);
    }

    if (data.landing_mode && data.altitude < 2000.0f && data.altitude > 15.0f &&
        hasTakenOff) {
      if (!IsSoundPlaying(sndLanding)) {
        SetSoundVolume(sndLanding, 1.0f);
        SetSoundPitch(sndLanding, 1.0f);
        PlaySound(sndLanding);
      }
    } else {
      if (IsSoundPlaying(sndLanding))
        StopSound(sndLanding);
    }
  }

  // --- Engine Ignition ---
  if (IsKeyPressed(KEY_E)) {
    data.system_active = !data.system_active;

    if (data.system_active) {
      engineStartTime = GetTime();
      engineReady = false;

      StopSound(sndEngineLoop);
      StopSound(sndEngineDown);
      StopSound(sndLanding);
    } else {
      engineReady = false;
      StopSound(sndEngineStart);
      StopSound(sndEngineLoop);

      SetSoundPitch(sndEngineDown, 0.65f);
      SetSoundVolume(sndEngineDown, 0.3f);
      PlaySound(sndEngineDown);

      StopSound(sndLanding);
      StopSound(sndAir);
      StopSound(sndWarning);
      StopSound(sndPullUp);
      data.speed = 0.0f;
    }
  }

  if (data.system_active && !engineReady) {
    if (GetTime() - engineStartTime >= 20.0) {
      engineReady = true;
    }
  }

  // ==========================================
  // 4. MIXER AUDIO SPAZIALE E VOLUMI
  // ==========================================
  if (data.system_active) {
    float speedRatio = data.speed / 200.0f;

    if (!IsSoundPlaying(sndAir))
      PlaySound(sndAir);
    float airVol = std::pow(speedRatio, 2.0f) * 0.3f;
    SetSoundPitch(sndAir, 0.8f + (speedRatio * 0.5f));

    float engineVol = 0.0f;
    float enginePitch = 1.0f;

    float inputLoad = 0.0f;
    if (engineReady) {
      if (IsKeyDown(KEY_W))
        inputLoad = 0.2f;
      else if (IsKeyDown(KEY_S) || IsKeyDown(KEY_SPACE))
        inputLoad = -0.2f;
    }

    engineVol = 0.2f + (speedRatio * 0.2f) + inputLoad;
    enginePitch = 0.8f + (speedRatio * 0.4f) + (inputLoad * 0.2f);

    bool isAlarmPlaying = IsSoundPlaying(sndPullUp) ||
                          IsSoundPlaying(sndWarning) ||
                          IsSoundPlaying(sndCaution) ||
                          IsSoundPlaying(sndLanding) || IsSoundPlaying(sndGear);
    if (isAlarmPlaying) {
      engineVol *= 0.2f;
      airVol *= 0.2f;
    }

    engineVol = std::max(0.01f, std::min(engineVol, 1.0f));
    enginePitch = std::max(0.5f, std::min(enginePitch, 1.5f));

    Sound activeEngineSound =
        (data.altitude < 1000.0f) ? sndEngineStart : sndEngineLoop;
    Sound inactiveEngineSound =
        (data.altitude < 1000.0f) ? sndEngineLoop : sndEngineStart;

    if (IsSoundPlaying(inactiveEngineSound))
      StopSound(inactiveEngineSound);
    if (!IsSoundPlaying(activeEngineSound))
      PlaySound(activeEngineSound);

    SetSoundVolume(activeEngineSound, engineVol);
    SetSoundPitch(activeEngineSound, enginePitch);
    SetSoundVolume(sndAir, std::max(0.0f, std::min(airVol, 1.0f)));
  } else {
    if (IsSoundPlaying(sndAir))
      StopSound(sndAir);
  }

  // SPEED FBW — delta velocità diretto per frame (identico all'originale)
  // KEY_W:     +0.75 KPH/frame  (accelerazione)
  // KEY_S:     -1.0  KPH/frame  (riduzione gas)
  // KEY_SPACE: -2.0  KPH/frame  (freno aerodinamico)
  pilot_out.speed_delta = 0.0f;
  if (data.system_active && engineReady) {
    if (IsKeyDown(KEY_W))
      pilot_out.speed_delta = +0.75f;
    else if (IsKeyDown(KEY_S))
      pilot_out.speed_delta = -1.0f;
    if (IsKeyDown(KEY_SPACE))
      pilot_out.speed_delta -= 2.0f;
  }

  // Propagazione flag FBW verso il FCC
  pilot_out.engines_on = data.system_active;
  pilot_out.engine_ready = engineReady;
  pilot_out.landing_mode = data.landing_mode;
  pilot_out.gear_deploy = gearOpen;
  pilot_out.rudder = 0.0f;

  UpdateAnimations();
}

void FlightDisplay::UpdateChaseCamera(const PlaneData &data) {
  float renderAlt = (data.altitude * 5.0f);
  float speedRatio = std::min(data.speed / 200.0f, 1.0f);

  // Calcolo DeltaTime sicuro contro i micro-blocchi
  float dt = GetFrameTime();
  if (dt > 0.1f)
    dt = 0.1f;

  // Base per la formula esponenziale della telecamera
  float trackingSpeedBase = 4.0f + (speedRatio * 2.0f);
  float slowFactor = 1.0f - speedRatio;
  float hoverHeight = slowFactor * 120.0f;

  Vector3 idealPos;
  Vector3 targetLook;

  switch (cameraMode) {
  case 0: { // DIETRO
    // Distanza fissa per tenere l'aereo ben visibile senza zoom-out in velocità
    float distH = 300.0f;
    float camHeight = 90.0f + (data.pitch * 50.0f) + hoverHeight;

    idealPos.x = data.x - (std::sin(data.yaw) * distH);
    idealPos.z = data.z - (std::cos(data.yaw) * distH);
    idealPos.y = renderAlt + camHeight;

    targetLook.x = data.x + (std::sin(data.yaw) * 60.0f);
    targetLook.y = renderAlt + 15.0f + (data.pitch * 50.0f);
    targetLook.z = data.z + (std::cos(data.yaw) * 60.0f);

    camera.up = (Vector3){std::sin(data.roll * 0.25f),
                          std::cos(data.roll * 0.25f), 0.0f};
    camera.fovy = 60.0f; // FOV fisso, niente effetto zoom
  } break;

  case 1: { // LATO
    float sideAngle = data.yaw + PI / 2.0f;
    float distSide = 300.0f; // Distanza laterale fissa

    idealPos.x = data.x + (std::sin(sideAngle) * distSide);
    idealPos.z = data.z + (std::cos(sideAngle) * distSide);
    idealPos.y = renderAlt + 20.0f + hoverHeight;

    targetLook.x = data.x;
    targetLook.y = renderAlt + 10.0f;
    targetLook.z = data.z;

    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 65.0f;
    trackingSpeedBase = 12.0f; // Più reattiva di lato
  } break;

  case 2: {                   // FRONTE
    float distFront = 350.0f; // Distanza frontale fissa

    idealPos.x = data.x + (std::sin(data.yaw) * distFront);
    idealPos.z = data.z + (std::cos(data.yaw) * distFront);
    idealPos.y = renderAlt + 25.0f + hoverHeight;

    targetLook.x = data.x;
    targetLook.y = renderAlt + 10.0f;
    targetLook.z = data.z;

    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 60.0f;
    trackingSpeedBase = 12.0f;
  } break;
  }

  // ==============================================================
  // IL FIX ASSOLUTO PER IL TREMOLIO (JITTER DELLA TELECAMERA)
  // ==============================================================

  // NIENTE INTERPOLAZIONI O SMOOTHING - ANCORAGGIO TOTALE 1:1
  // Questo garantisce che l'aereo sia SEMPRE visivamente incollato
  // allo stesso identico pixel dello schermo, prescindendo dall'accelerazione.

  cameraPositionLag = idealPos;
  camera.position = idealPos;
  camera.target = targetLook;
  // ==============================================================
}

void FlightDisplay::UpdateAnimations() {
  if (animsCount <= 0 || modelAnims == nullptr)
    return;
  int maxFrames =
      modelAnims[0].keyframeCount; // FIX: keykeykeyframeCount corretto
  if (maxFrames <= 0)
    return;

  float speed = 60.0f;

  // Blocco di sicurezza sui frame estremi per evitare mesh corrotte
  if (gearOpen) {
    gearFrame += GetFrameTime() * speed;
    if (gearFrame >= maxFrames - 2.0f)
      gearFrame = maxFrames - 2.0f;
  } else {
    gearFrame -= GetFrameTime() * speed;
    if (gearFrame <= 1.0f)
      gearFrame = 1.0f;
  }

  UpdateModelAnimation(modelF35, modelAnims[0], (int)gearFrame);
}

void FlightDisplay::DrawMapWorld(const PlaneData &data) {
  rlEnableDepthTest();
  rlEnableBackfaceCulling();

  // 1. CLIP PLANES
  rlSetClipPlanes(2.0f, 300000.0f);

  float asphaltOffset = -8.5f;
  Vector3 runwayPosition = {0.0f, asphaltOffset, 0.0f};

  // 3. PIANO DI BASE INFERIORE
  DrawPlane((Vector3){camera.position.x, -50.0f, camera.position.z},
            (Vector2){1000000.0f, 1000000.0f}, (Color){10, 15, 10, 255});

  // 4. GENERAZIONE TERRENO CITTADINO
  float groundY = -400.0f;

  // Ottimizzazione del terreno per liberare CPU e mantenere alti i frame
  float tileSize = 20000.0f;
  int viewDist = 6;
  float snapX = std::floor(camera.position.x / tileSize);
  float snapZ = std::floor(camera.position.z / tileSize);

  for (int i = -viewDist; i <= viewDist; i++) {
    for (int j = -viewDist; j <= viewDist; j++) {
      float worldX = (snapX + i) * tileSize;
      float worldZ = (snapZ + j) * tileSize;

      // BUCO PER L'AEROPORTO
      if (std::abs(worldX) < 15000.0f && std::abs(worldZ) < 15000.0f)
        continue;

      float noise = std::sin(worldX * 0.0001f) * std::cos(worldZ * 0.0001f);
      Color fieldColor =
          (noise > 0.0f) ? (Color){30, 45, 20, 255} : (Color){35, 50, 25, 255};

      DrawCubeV((Vector3){worldX, groundY - 0.5f, worldZ},
                (Vector3){tileSize * 0.98f, 1.0f, tileSize * 0.98f},
                fieldColor);
    }
  }

  if (!mapLoaded)
    return;

  // 5. RENDERING MESH AEROPORTO
  rlPushMatrix();
  DrawModel(mapModel, runwayPosition, 7.0f, WHITE);
  rlPopMatrix();

  // 6. GUIDA VISIVA ILS (INSTRUMENT LANDING SYSTEM)
  if (data.landing_mode) {
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    // Il faro parte dalla pista e sale in cielo
    DrawCylinderEx(runwayPosition,
                   (Vector3){runwayPosition.x, 60000.0f, runwayPosition.z},
                   300.0f, 300.0f, 16, Fade(GREEN, 0.4f));
    EndBlendMode();
    rlEnableDepthMask();
  }
}

// --- Volumetric Sky & Procedural Atmosphere ---
void FlightDisplay::DrawSky(Vector3 camPos) {
  rlDisableDepthTest();
  rlDisableDepthMask();
  rlDisableBackfaceCulling();

  rlPushMatrix();
  rlTranslatef(camPos.x, camPos.y, camPos.z);

  // --- Rayleigh Scattering ---
  // Background gradient
  DrawSphereEx((Vector3){0, 0, 0}, 20000.0f, 16, 16,
               (Color){100, 150, 230, 255});

  BeginBlendMode(BLEND_ALPHA);
  for (int i = 0; i < 8; i++) {
    float offset = 5000.0f - (i * 700.0f);
    float alpha = 0.08f + (i * 0.045f);
    DrawSphereEx((Vector3){0, -offset, 0}, 18000.0f, 16, 16,
                 Fade((Color){130, 180, 255, 255}, alpha));
  }
  // Horizon haze
  DrawSphereEx((Vector3){0, -2000.0f, 0}, 15000.0f, 16, 16,
               Fade((Color){200, 230, 255, 255}, 0.7f));
  EndBlendMode();

  // --- Sun & Bloom ---
  // Spostiamo il sole un po' più in alto e lontano
  Vector3 sunPos = {8000.0f, 3500.0f, 9000.0f};

  // A) La dispersione di luce nell'aria (Additive)
  BeginBlendMode(BLEND_ADDITIVE);
  DrawSphereEx(
      sunPos, 4000.0f, 16, 16,
      Fade((Color){255, 180, 80, 255}, 0.08f)); // Vastissimo alone caldo
  DrawSphereEx(sunPos, 1800.0f, 16, 16,
               Fade((Color){255, 210, 120, 255}, 0.2f)); // Corona esterna
  DrawSphereEx(sunPos, 600.0f, 16, 16,
               Fade((Color){255, 240, 200, 255}, 0.5f)); // Corona interna

  // Lens Flare (Raggio orizzontale della lente della telecamera)
  rlPushMatrix();
  rlTranslatef(sunPos.x, sunPos.y, sunPos.z);
  rlScalef(35.0f, 0.05f, 1.0f);
  DrawSphereEx((Vector3){0, 0, 0}, 200.0f, 12, 12,
               Fade((Color){255, 210, 160, 255}, 0.2f));
  rlPopMatrix();
  EndBlendMode();

  // B) Il modello del Sole fisico: un disco nettissimo al centro del bagliore
  BeginBlendMode(BLEND_ALPHA);
  DrawSphereEx(sunPos, 130.0f, 24, 24, (Color){255, 250, 245, 255});
  EndBlendMode();

  // --- Procedural Clouds ---
  BeginBlendMode(BLEND_ALPHA);
  srand(8888); // Nuovo seme per una composizione più aperta
  float timeOffset = GetTime();

  // --- LAYER 1: CIRRI STRATOSFERICI ---
  rlPushMatrix();
  rlRotatef(timeOffset * 0.1f, 0, 1, 0);
  // Ridotte da 35 a 15 (Molto diradate)
  for (int i = 0; i < 15; i++) {
    float angle = (rand() % 360) * DEG2RAD;
    float dist = 3000.0f + (rand() % 6000);
    // Quota alzata in modo estremo (8000+)
    float alt = 8000.0f + (rand() % 2000);
    float size = 2000.0f + (rand() % 3000);

    Vector3 pos = {std::cos(angle) * dist, alt, std::sin(angle) * dist};

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef((rand() % 360), 0, 1, 0);
    rlScalef(1.0f, 0.02f, 0.15f); // Velo sottilissimo
    DrawSphereEx((Vector3){0, 0, 0}, size, 8, 8, Fade(WHITE, 0.08f));
    rlPopMatrix();
  }
  rlPopMatrix();

  // --- LAYER 2: CUMULI VOLUMETRICI SPARSI ---
  rlPushMatrix();
  rlRotatef(timeOffset * 0.03f, 0, 1, 0); // Vento lentissimo

  // Ridotte da 50 a 20 formazioni enormi isolate
  for (int i = 0; i < 20; i++) {
    float angle = (rand() % 360) * DEG2RAD;
    // Distanza allargata per fare spazio tra una nuvola e l'altra
    float dist = 2500.0f + (rand() % 8000);
    // Base alzata da 600m a oltre 3000m
    float alt = 3000.0f + (rand() % 2500);
    float baseSize = 1000.0f + (rand() % 1500);

    Vector3 clusterPos = {std::cos(angle) * dist, alt, std::sin(angle) * dist};

    rlPushMatrix();
    rlTranslatef(clusterPos.x, clusterPos.y, clusterPos.z);
    rlRotatef((rand() % 360), 0, 1, 0);

    // A) OMBRA BASE (Resa più morbida e meno aggressiva)
    rlPushMatrix();
    rlTranslatef(0, -baseSize * 0.15f, 0);
    rlScalef(1.0f, 0.2f, 0.9f);
    DrawSphereEx((Vector3){0, 0, 0}, baseSize * 1.1f, 8, 8,
                 Fade((Color){110, 125, 145, 255}, 0.4f));
    rlPopMatrix();

    // B) CORPO CENTRALE
    rlPushMatrix();
    rlTranslatef(0, 0, 0);
    rlScalef(1.0f, 0.3f, 0.8f);
    DrawSphereEx((Vector3){0, 0, 0}, baseSize, 8, 8,
                 Fade((Color){210, 220, 230, 255}, 0.6f));
    rlPopMatrix();

    // C) CIMA ILLUMINATA (Rimbalzo della luce stellare)
    rlPushMatrix();
    rlTranslatef(baseSize * 0.15f, baseSize * 0.2f, baseSize * 0.15f);
    rlScalef(0.85f, 0.4f, 0.7f);
    DrawSphereEx((Vector3){0, 0, 0}, baseSize * 0.85f, 8, 8, Fade(WHITE, 0.8f));
    rlPopMatrix();

    rlPopMatrix();
  }
  rlPopMatrix();

  EndBlendMode();

  rlPopMatrix();

  rlEnableBackfaceCulling();
  rlEnableDepthMask();
  rlEnableDepthTest();
}
void FlightDisplay::DrawUltimateF35(const PlaneData &data) {
  if (!modelLoaded)
    return;

  float globalScale = 13.0f;
  float modelScaleAereo = 0.00015f * globalScale;
  float modelScaleFuoco = 0.7f * globalScale;

  float fuocoZ = -9.0f * globalScale;
  float fuocoY = 3.0f * globalScale;
  float fuocoX = 0.0f;

  modelF35.transform = MatrixIdentity();
  modelF35.transform =
      MatrixMultiply(modelF35.transform, MatrixRotateY(-90.0f * DEG2RAD));
  modelF35.transform =
      MatrixMultiply(modelF35.transform, MatrixRotateZ(0.0f * DEG2RAD));

  rlEnableDepthTest();
  rlDisableBackfaceCulling();

  rlPushMatrix();
  rlTranslatef(0.0f, 5.0f, 0.0f);

  // =========================================================================
  // RENDERING MODELLO AEREOMOBILE
  // =========================================================================
  rlPushMatrix(); // Isola la trasformazione della scala dal resto della scena
  // Applichiamo la scala manualmente per il DrawMesh
  rlScalef(modelScaleAereo, modelScaleAereo, modelScaleAereo);

  // PASSAGGIO 1: Disegna la Fusoliera, il Pilota e i Monitor (Tutto ciò che è
  // OPaco)
  for (int i = 0; i < modelF35.meshCount; i++) {
    Material mat = modelF35.materials[modelF35.meshMaterial[i]];
    // Se il pezzo è solido (opacità quasi al massimo)
    if (mat.maps[MATERIAL_MAP_ALBEDO].color.a > 100) {
      DrawMesh(modelF35.meshes[i], mat, modelF35.transform);
    }
  }

  // PASSAGGIO 2: Disegna i Vetri (Tutto ciò che è Trasparente)
  rlDisableDepthMask(); // Diciamo alla GPU che i vetri NON devono cancellare
                        // l'interno!
  for (int i = 0; i < modelF35.meshCount; i++) {
    Material mat = modelF35.materials[modelF35.meshMaterial[i]];
    // Se il pezzo è semi-trasparente (come il canopy del vetro)
    if (mat.maps[MATERIAL_MAP_ALBEDO].color.a <= 100) {
      DrawMesh(modelF35.meshes[i], mat, modelF35.transform);
    }
  }
  rlEnableDepthMask(); // Riattiviamo la profondità
  rlPopMatrix(); // Ripristino della scala matrice per i successivi draw-call
  // =========================================================================
  // FINE MODELLO AEREOMOBILE
  // =========================================================================

  // =========================================================================
  // EFFETTI MOTORE (SHOCK DIAMONDS E AFTERBURNER)
  // =========================================================================
  if (data.system_active && data.speed > 5.0f) {
    rlPushMatrix();
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();

    // 1. Calcolo Spinta (da 0.0 a 1.0)
    float thrust = std::max(0.05f, std::min(data.speed / 200.0f, 1.0f));
    float timePulse = GetTime() * 30.0f;

    // Lunghezza della fiamma tremolante in base alla velocità
    float flameLength =
        9.0f * modelScaleFuoco * thrust * (0.95f + 0.05f * std::sin(timePulse));
    float baseRadius = 0.5f * modelScaleFuoco * (0.8f + 0.3f * thrust);

    // Ottimizzato per salvare risorse
    int slices = 10;
    float stepZ = flameLength / slices;

    // Funzione lambda per calcolare i colori fluidamente (Interpolazione
    // Lineare)
    auto lerpColor = [](float start, float end, float t) {
      return start + (end - start) * t;
    };

    for (int i = 0; i < slices; i++) {
      float t =
          (float)i / slices; // Variabile da 0.0 (motore) a 1.0 (fine scia)

      // 2. FORMA DELLA FIAMMA E SHOCK DIAMONDS
      // Restringimento progressivo (profilo a goccia lunga)
      float shapeTaper = 1.0f - std::pow(t, 1.5f);

      // Onde di pressione (Shock Diamonds) attive solo ad alta velocità
      float shockWave = 1.0f;
      if (thrust > 0.5f) {
        float numDiamonds = 6.0f * thrust; // Più vai veloce, più diamanti
        // Valore che pulsa tra 0.6 e 1.0 creando i "nodi" luminosi
        shockWave =
            0.6f +
            0.4f * std::abs(std::cos(t * PI * numDiamonds - GetTime() * 20.0f));
      }

      float currentRadius = baseRadius * shapeTaper * shockWave;
      Vector3 slicePos = {fuocoX, fuocoY + 5.0f, fuocoZ - (stepZ * i)};

      // 3. TUTTE LE SFUMATURE DI COLORE
      float r = 0, g = 0, b = 0, a = 0;

      if (thrust > 0.6f) {
        // --- MODALITÀ AFTERBURNER (Supersonico) ---
        if (t < 0.2f) {
          // Dal Bianco puro (motore) al Ciano accecante
          float nt = t / 0.2f;
          r = lerpColor(255, 50, nt);
          g = lerpColor(255, 200, nt);
          b = 255;
        } else if (t < 0.6f) {
          // Dal Ciano al Viola profondo / Magenta
          float nt = (t - 0.2f) / 0.4f;
          r = lerpColor(50, 150, nt);
          g = lerpColor(200, 20, nt);
          b = lerpColor(255, 200, nt);
        } else {
          // Dal Viola all'Arancione scuro fiammeggiante che si spegne
          float nt = (t - 0.6f) / 0.4f;
          r = lerpColor(150, 255, nt);
          g = lerpColor(20, 80, nt);
          b = lerpColor(200, 10, nt);
        }
      } else {
        // --- MODALITÀ CROCIERA (Subsonico) ---
        // Dal Giallo all'Arancione al Rosso
        r = 255;
        g = lerpColor(220, 50, t);
        b = lerpColor(100, 0, t);
      }

      // Opacità: svanisce dolcemente verso la coda e trema leggermente
      a = lerpColor(255, 0, std::pow(t, 0.8f)) *
          (0.8f + 0.2f * std::sin(timePulse + i));

      Color sliceColor = {(unsigned char)r, (unsigned char)g, (unsigned char)b,
                          (unsigned char)a};

      // 4. DISEGNO DEI LIVELLI VOLUMETRICI
      // Un alone gigantesco, morbido e molto trasparente (il calore irradiato)
      DrawSphere(slicePos, currentRadius * 1.8f, Fade(sliceColor, 0.15f));

      // Il raggio di fuoco principale
      DrawSphere(slicePos, currentRadius * 0.9f, Fade(sliceColor, 0.6f));

      // Il nucleo incandescente (quasi bianco, strettissimo)
      DrawSphere(slicePos, currentRadius * 0.4f, sliceColor);
    }

    // Una scintilla purissima e accecante proprio all'uscita della turbina
    DrawSphere((Vector3){fuocoX, fuocoY + 5.0f, fuocoZ}, baseRadius * 1.2f,
               (Color){255, 255, 255, 255});

    rlEnableDepthMask();
    EndBlendMode();
    rlPopMatrix();
  }
  rlPopMatrix();
  rlEnableBackfaceCulling();
}
void FlightDisplay::DrawHUD(const PlaneData &data) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  int cx = sw / 2;
  int cy = sh / 2;

  // --- NUOVA PALETTE COLORI: BLU INTENSO ---
  Color hudMain = {0, 150, 255, 255};   // Blu Ciano Intenso (Testi e bordi)
  Color hudGreen = {50, 255, 180, 255}; // Verde Acqua (Altitudine e Landing)
  Color hudRed = {255, 50, 50, 255};    // Rosso Allarme
  Color hudDim = Fade(hudMain, 0.4f);   // Blu sfumato per griglie
  Color hudBg = Fade({0, 20, 50, 255}, 0.85f); // Sfondo Blu Notte molto scuro
  // -----------------------------------------

  auto DrawTechBox = [&](int x, int y, int w, int h, const char *title,
                         bool alignRight = false) {
    DrawRectangle(x, y, w, h, hudBg);
    DrawRectangleLines(x, y, w, h, hudDim);

    // Angoli rinforzati
    int cl = 12;
    int ct = 2;
    DrawRectangle(x, y, cl, ct, hudMain);
    DrawRectangle(x, y, ct, cl, hudMain);
    DrawRectangle(x + w - cl, y, cl, ct, hudMain);
    DrawRectangle(x + w - ct, y, ct, cl, hudMain);
    DrawRectangle(x, y + h - ct, cl, ct, hudMain);
    DrawRectangle(x, y + h - cl, ct, cl, hudMain);
    DrawRectangle(x + w - cl, y + h - ct, cl, ct, hudMain);
    DrawRectangle(x + w - ct, y + h - cl, ct, cl, hudMain);

    int titleW = MeasureText(title, 10) + 20;
    int titleX = alignRight ? (x + w - titleW) : x;
    DrawRectangle(titleX, y - 18, titleW, 18, hudDim);
    DrawText(title, titleX + 10, y - 14, 10, hudMain);
  };

  // BOX SINISTRO: SENSORI CINETICI
  int lx = 30, ly = cy - 80, lw = 220, lh = 150;
  DrawTechBox(lx, ly, lw, lh, "KINETIC SENSORS");
  DrawText("AIRSPEED (CAS)", lx + 15, ly + 20, 10, Fade(WHITE, 0.8f));
  DrawText(TextFormat("%03.0f", data.speed), lx + 15, ly + 40, 40, hudMain);
  DrawText("KPH", lx + 110, ly + 62, 12, hudMain);

  DrawText("ENGINE THRUST", lx + 15, ly + 100, 10, Fade(WHITE, 0.8f));
  float pwr = std::min(data.speed / 200.0f, 1.0f);
  for (int i = 0; i < 20; i++) {
    // La barra della potenza ora sfuma dal blu al rosso
    Color c = (i < pwr * 20) ? ((i >= 17) ? hudRed : hudMain) : hudDim;
    DrawRectangle(lx + 15 + (i * 9), ly + 115, 6, 15, c);
  }

  // BOX DESTRO: DATI INERZIALI
  int rx = sw - 250, ry = cy - 80, rw = 220, rh = 160;
  DrawTechBox(rx, ry, rw, rh, "INERTIAL DATA", true);
  DrawText("ALTITUDE (MSL)", rx + 15, ry + 20, 10, Fade(WHITE, 0.8f));
  DrawText(TextFormat("%05.0f", data.altitude), rx + 15, ry + 40, 40, hudGreen);
  DrawText("M", rx + 150, ry + 62, 12, hudGreen);

  DrawRectangle(rx + 15, ry + 95, rw - 30, 1, hudDim);

  auto DrawDataRow = [&](int yOff, const char *lbl, const char *val) {
    DrawText(lbl, rx + 15, ry + yOff, 10, Fade(WHITE, 0.7f));
    DrawText(val, rx + rw - 15 - MeasureText(val, 10), ry + yOff, 10, hudMain);
  };
  DrawDataRow(105, "PITCH (DEG)", TextFormat("%+06.2f", data.pitch * RAD2DEG));
  DrawDataRow(120, "ROLL  (DEG)", TextFormat("%+06.2f", data.roll * RAD2DEG));
  DrawDataRow(135, "YAW   (DEG)", TextFormat("%+06.2f", data.yaw * RAD2DEG));

  // BUSSOLA (HEADING)
  int tx = cx - 180, ty = 25, tw = 360, th = 35;
  DrawTechBox(tx, ty, tw, th, "HEADING (AZIMUTH)");
  BeginScissorMode(tx + 5, ty + 5, tw - 10, th - 10);
  float head = data.yaw * RAD2DEG;
  for (int i = -180; i <= 540; i += 10) {
    float px = (tx + tw / 2) + (i - head) * 5;
    if (px > tx && px < tx + tw) {
      DrawLineEx({px, (float)ty + 15}, {px, (float)ty + 30}, 2.0f, hudMain);
      if (i % 30 == 0) {
        const char *lbl = (i % 360 == 0)     ? "N"
                          : (i % 360 == 90)  ? "E"
                          : (i % 360 == 180) ? "S"
                          : (i % 360 == 270) ? "W"
                                             : TextFormat("%03d", i % 360);
        DrawText(lbl, px - MeasureText(lbl, 10) / 2, ty + 4, 10, WHITE);
      }
    }
  }
  EndScissorMode();
  DrawTriangle({(float)cx, (float)ty + 35}, {(float)cx - 6, (float)ty + 42},
               {(float)cx + 6, (float)ty + 42}, hudRed);

  // SISTEMA DI ALLARMI E LANDING
  bool blink = ((int)(GetTime() * 8) % 2 == 0);
  bool hasAlarm = false;
  const char *warnMsg = "";

  if (data.landing_mode) {
    DrawRectangleLinesEx({0, 0, (float)sw, (float)sh}, 6.0f, hudGreen);
    int wx = cx - 220, wy = cy + 120, ww = 440, wh = 50;
    DrawRectangle(wx, wy, ww, wh, Fade({0, 40, 30, 255}, 0.9f));
    DrawRectangleLinesEx({(float)wx, (float)wy, (float)ww, (float)wh}, 2.0f,
                         hudGreen);
    DrawText("LANDING MODE ENGAGED - FOLLOW BEACON",
             cx - MeasureText("LANDING MODE ENGAGED - FOLLOW BEACON", 20) / 2,
             wy + 15, 20, hudGreen);
  } else {
    if (data.altitude < 2000) {
      hasAlarm = true;
      warnMsg = "TERRAIN PULL UP";
    } else if (data.altitude > 12500) {
      hasAlarm = true;
      warnMsg = "OVERSHOOT PULL DOWN";
    } else if (std::abs(data.roll) > 1.6f) {
      hasAlarm = true;
      warnMsg = "RECOVERY SYSTEM ACTIVE";
    }

    if (hasAlarm) {
      DrawRectangleLinesEx({0, 0, (float)sw, (float)sh}, 6.0f,
                           blink ? hudRed : Fade(hudRed, 0.4f));
      int wx = cx - 220, wy = cy + 120, ww = 440, wh = 50;
      DrawRectangle(wx, wy, ww, wh,
                    blink ? Fade(hudRed, 0.5f) : Fade(BLACK, 0.9f));
      DrawRectangleLinesEx({(float)wx, (float)wy, (float)ww, (float)wh}, 2.0f,
                           hudRed);
      const char *fullMsg = TextFormat("! ! !  %s  ! ! !", warnMsg);
      DrawText(fullMsg, cx - MeasureText(fullMsg, 20) / 2, wy + 15, 20,
               blink ? WHITE : hudRed);
    }
  }

  // INFO DI SISTEMA IN BASSO
  DrawText("SYS: F-35 ADVANCED AVIONICS v31.5 // BLUE_SCAN_ACTIVE", 30, sh - 30,
           10, hudDim);

  // Effetto Scanline (Linee orizzontali sottili tipiche dei CRT/HUD)
  for (int i = 0; i < sh; i += 3)
    DrawLine(0, i, sw, i, Fade(BLACK, 0.1f));
}

void FlightDisplay::Draw(const PlaneData &data) {
  BeginDrawing();

  // Mettiamo NERO, così se il cielo finisce vediamo lo spazio e non un colore
  // finto
  ClearBackground(BLACK);

  UpdateChaseCamera(data);

  // Manteniamo la tua distanza di rendering
  rlSetClipPlanes(15.0f, 300000.0f);

  BeginMode3D(camera);

  DrawSky(camera.position);

  DrawMapWorld(data);

  rlPushMatrix();

  rlTranslatef(data.x, (data.altitude * 5.0f), data.z);
  rlRotatef(data.yaw * RAD2DEG, 0, 1, 0);
  rlRotatef(data.pitch * RAD2DEG, -1, 0, 0);
  rlRotatef(data.roll * RAD2DEG, 0, 0, 1);
  DrawUltimateF35(data);

  rlPopMatrix();

  EndMode3D();

  DrawHUD(data);
  EndDrawing();
}
