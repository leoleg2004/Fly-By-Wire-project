#include "FlightDisplay.hpp"
#include "Telemetry.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

// ============================================================
// HUD palette — F-16 green phosphor style
// ============================================================
#define HUD_GREEN  CLITERAL(Color){0,   255, 120, 220}
#define HUD_WHITE  CLITERAL(Color){255, 255, 255, 220}
#define HUD_RED    CLITERAL(Color){255,  60,  60, 255}
#define HUD_AMBER  CLITERAL(Color){255, 200,  50, 255}
#define HUD_DIM    CLITERAL(Color){0,   160,  80, 100}
#define HUD_BG     CLITERAL(Color){0,    15,   8, 210}

static int cameraMode = 0; // 0=chase, 1=side, 2=front

// ============================================================
// Constructor
// ============================================================
FlightDisplay::FlightDisplay(int width, int height, const std::string &title) {
  InitWindow(width, height, title.c_str());
  InitAudioDevice();

  sndEngineStart = LoadSound("engine_start.wav");
  sndEngineLoop  = LoadSound("engine_loop.wav");
  sndEngineDown  = LoadSound("engine_down.wav");
  sndGear        = LoadSound("gear.wav");
  sndLanding     = LoadSound("landing.wav");
  sndWarning     = LoadSound("warning.wav");
  sndPullUp      = LoadSound("pull_up.wav");
  sndCaution     = LoadSound("caution.wav");
  sndAir         = LoadSound("air.wav");

  SetTargetFPS(60);

  // Sky dome
  skyTexture = LoadTexture(
      "sky/textures/lambert1_emissive.png");
  if (skyTexture.id > 0) {
    skyLoaded  = true;
    Mesh sphere = GenMeshSphere(90000.0f, 64, 64);
    for (int i = 0; i < sphere.vertexCount; i++)
      sphere.texcoords[i * 2 + 1] = 1.0f - sphere.texcoords[i * 2 + 1];
    skyModel = LoadModelFromMesh(sphere);
    skyModel.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = skyTexture;
    skyModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color   = WHITE;
  } else {
    TraceLog(LOG_WARNING, "ATTENZIONE: Impossibile caricare Scene_-_Root_diffuse.png");
  }

  // Aircraft model
  modelF16   = LoadModel("f35.glb");
  modelAnims = LoadModelAnimations("f35.glb", &animsCount);

  for (int i = 0; i < modelF16.materialCount; i++)
    if (modelF16.materials[i].maps[MATERIAL_MAP_ALBEDO].color.a == 0)
      modelF16.materials[i].maps[MATERIAL_MAP_ALBEDO].color.a = 255;

  gearOpen  = true;
  gearFrame = (animsCount > 0) ? (float)(modelAnims[0].keyframeCount - 1) : 0.0f;
  modelF16.transform = MatrixIdentity();
  modelLoaded = (modelF16.meshCount > 0);
  if (!modelLoaded)
    TraceLog(LOG_WARNING, "ATTENZIONE: Impossibile caricare f35.glb");

  // Aerodrome
  mapModel = LoadModel("aerodrome.glb");
  mapLoaded = (mapModel.meshCount > 0);

  // Camera default
  camera.position   = (Vector3){0.0f, 15.0f, -35.0f};
  camera.target     = (Vector3){0.0f, 0.0f, 0.0f};
  camera.up         = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy       = 70.0f;
  camera.projection = CAMERA_PERSPECTIVE;
  cameraPositionLag = camera.position;
}

// ============================================================
// Destructor
// ============================================================
FlightDisplay::~FlightDisplay() {
  if (mapLoaded)   UnloadModel(mapModel);
  if (skyLoaded) { UnloadModel(skyModel); UnloadTexture(skyTexture); }
  if (modelLoaded) {
    if (modelAnims) UnloadModelAnimations(modelAnims, animsCount);
    UnloadModel(modelF16);
  }
  UnloadSound(sndEngineStart); UnloadSound(sndEngineLoop);
  UnloadSound(sndEngineDown);  UnloadSound(sndGear);
  UnloadSound(sndLanding);     UnloadSound(sndWarning);
  UnloadSound(sndPullUp);      UnloadSound(sndCaution);
  UnloadSound(sndAir);
  CloseAudioDevice();
  CloseWindow();
}

bool FlightDisplay::IsActive() { return !WindowShouldClose(); }

// ============================================================
// HandleInput — keyboard → PilotInput
// ============================================================
void FlightDisplay::HandleInput(PlaneData &data, PilotInput &pilot_out) {
  float dt = GetFrameTime();

  static double engineStartTime = 0.0;
  static bool   engineReady     = false;

  // ── Aggiornamento flag di volo ────────────────────────────────────────
  // airborne: true quando siamo in aria (alt > 150m), reset a terra
  if (data.altitude > 150.0f) data.airborne = true;
  if (data.altitude <  10.0f) { data.airborne = false; data.above_3km = false; }

  // above_3km: si arma quando superiamo 3500m per la PRIMA VOLTA dopo il decollo.
  // Questo fa sì che l'allarme bassa quota NON suoni durante la salita iniziale
  // (che passa necessariamente attraverso 0→3000m), ma SOLO quando scendiamo
  // sotto 3000m durante la crociera o l'avvicinamento.
  if (data.airborne && data.altitude > 3500.0f) data.above_3km = true;

  // Auto-shutdown al touch-down: motori off quando airborne + alt < 2m + fermo
  if (data.airborne && data.altitude < 2.0f && data.speed <= 1.0f &&
      data.system_active) {
    data.system_active = false;
    engineReady        = false;
    StopSound(sndEngineStart); StopSound(sndEngineLoop);
    SetSoundPitch(sndEngineDown, 0.65f);
    SetSoundVolume(sndEngineDown, 0.3f);
    PlaySound(sndEngineDown);
    StopSound(sndLanding); StopSound(sndAir);
    StopSound(sndWarning); StopSound(sndPullUp);
    data.airborne  = false;
    data.above_3km = false;
  }

  // Landing mode toggle
  if (IsKeyPressed(KEY_L)) {
    data.landing_mode = !data.landing_mode;
    if (data.landing_mode) {
      SetSoundVolume(sndCaution, 1.0f);
      SetSoundPitch(sndCaution, 1.0f);
      PlaySound(sndCaution);
    }
  }

  // Gear toggle
  if (IsKeyPressed(KEY_G)) {
    gearOpen = !gearOpen;
    StopSound(sndGear);
    SetSoundPitch(sndGear, gearOpen ? 0.85f : 1.15f);
    SetSoundVolume(sndGear, 1.0f);
    PlaySound(sndGear);
  }

  // Camera mode cycle
  if (IsKeyPressed(KEY_C)) cameraMode = (cameraMode + 1) % 3;

  // Engine ignition
  if (IsKeyPressed(KEY_E)) {
    data.system_active = !data.system_active;
    if (data.system_active) {
      engineStartTime = GetTime();
      engineReady     = false;
      StopSound(sndEngineLoop); StopSound(sndEngineDown); StopSound(sndLanding);
    } else {
      engineReady = false;
      StopSound(sndEngineStart); StopSound(sndEngineLoop);
      SetSoundPitch(sndEngineDown, 0.65f);
      SetSoundVolume(sndEngineDown, 0.3f);
      PlaySound(sndEngineDown);
      StopSound(sndLanding); StopSound(sndAir);
      StopSound(sndWarning); StopSound(sndPullUp);
    }
  }

  if (data.system_active && !engineReady)
    if (GetTime() - engineStartTime >= 20.0) engineReady = true;

  // ── FBW Pilot Inputs ──────────────────────────────────────
  pilot_out.stick_pitch = 0.0f;
  pilot_out.stick_roll  = 0.0f;
  pilot_out.rudder      = 0.0f;

  if (IsKeyDown(KEY_UP))    pilot_out.stick_pitch = +1.0f;
  if (IsKeyDown(KEY_DOWN))  pilot_out.stick_pitch = -1.0f;
  if (IsKeyDown(KEY_LEFT))  pilot_out.stick_roll  = -1.0f;
  if (IsKeyDown(KEY_RIGHT)) pilot_out.stick_roll  = +1.0f;
  if (IsKeyDown(KEY_Z))     pilot_out.rudder      = -1.0f;
  if (IsKeyDown(KEY_X))     pilot_out.rudder      = +1.0f;

  // Throttle — 0.8/s normale, 2.0/s con SHIFT per risposta rapida
  static float current_throttle = 0.0f;
  if (data.system_active && engineReady) {
    float thr_rate = IsKeyDown(KEY_LEFT_SHIFT) ? 2.0f : 0.8f;
    if (IsKeyDown(KEY_W))       current_throttle += thr_rate * dt;
    else if (IsKeyDown(KEY_S))  current_throttle -= thr_rate * dt;
    current_throttle = std::clamp(current_throttle, 0.0f, 1.0f);
  } else {
    current_throttle = 0.0f;
  }
  pilot_out.throttle_input = current_throttle;
  pilot_out.engines_on     = data.system_active;
  pilot_out.engine_ready   = engineReady;
  pilot_out.landing_mode   = data.landing_mode;
  pilot_out.gear_deploy    = gearOpen;

  // ── Voice Warning System ──────────────────────────────────────────────
  //
  // LOGICA CORRETTA ALLARMI (come simulatore di volo reale):
  //
  // Bassa quota: suona SOLO se above_3km=true (siamo già saliti sopra i 3500m
  //   e ora scendiamo sotto 3000m). NON suona durante la salita iniziale.
  //
  // Alta quota: suona quando superiamo 12000m in crociera.
  //   NON silenziato in landing_mode (scendere da alta quota richiede avviso).
  //
  // Bank angle: avviso oltre 60° quando in volo (airborne + !landing_mode).
  //
  // Landing approach: suono ILS quando alt < 600m in landing_mode.
  //
  if (data.system_active) {
    bool inFlight = data.airborne && !data.landing_mode;

    // Bassa quota: armato solo dopo aver superato 3500m (above_3km)
    bool dangerLowAlt  = inFlight && data.above_3km && data.altitude < 3000.0f;
    // Alta quota: sempre attivo se in volo e > 12000m
    bool dangerHighAlt = inFlight && data.altitude > 12000.0f;
    // Bank angle 60° — solo in volo non-landing
    bool dangerBank    = inFlight && fabsf(data.roll) > 1.047f;

    bool dangerPullUp = dangerLowAlt || dangerHighAlt;

    if (dangerPullUp) {
      StopSound(sndWarning);
      if (!IsSoundPlaying(sndPullUp)) PlaySound(sndPullUp);
    } else if (dangerBank) {
      StopSound(sndPullUp);
      if (!IsSoundPlaying(sndWarning)) PlaySound(sndWarning);
    } else {
      if (IsSoundPlaying(sndPullUp))  StopSound(sndPullUp);
      if (IsSoundPlaying(sndWarning)) StopSound(sndWarning);
    }

    // ILS approach — suono quando in avvicinamento finale (< 600m, landing_mode on)
    if (data.landing_mode && data.airborne &&
        data.altitude < 600.0f && data.altitude > 10.0f) {
      if (!IsSoundPlaying(sndLanding)) PlaySound(sndLanding);
    } else {
      if (IsSoundPlaying(sndLanding)) StopSound(sndLanding);
    }
  }

  // ── Spatial Audio Mix ───────────────────────────────────
  if (data.system_active) {
    float speedRatio = data.speed / 200.0f;

    if (!IsSoundPlaying(sndAir)) PlaySound(sndAir);
    SetSoundPitch(sndAir, 0.8f + speedRatio * 0.5f);
    float airVol = std::pow(speedRatio, 2.0f) * 0.3f;

    float inputLoad = 0.0f;
    if (engineReady) {
      if (IsKeyDown(KEY_W))                       inputLoad =  0.2f;
      else if (IsKeyDown(KEY_S))                  inputLoad = -0.2f;
    }
    float engineVol   = std::clamp(0.2f + speedRatio * 0.2f + inputLoad, 0.01f, 1.0f);
    float enginePitch = std::clamp(0.8f + speedRatio * 0.4f + inputLoad * 0.2f, 0.5f, 1.5f);

    bool alarm = IsSoundPlaying(sndPullUp) || IsSoundPlaying(sndWarning) ||
                 IsSoundPlaying(sndCaution) || IsSoundPlaying(sndLanding) ||
                 IsSoundPlaying(sndGear);
    if (alarm) { engineVol *= 0.2f; airVol *= 0.2f; }

    Sound active   = (data.altitude < 1000.0f) ? sndEngineStart : sndEngineLoop;
    Sound inactive = (data.altitude < 1000.0f) ? sndEngineLoop  : sndEngineStart;
    if (IsSoundPlaying(inactive)) StopSound(inactive);
    if (!IsSoundPlaying(active))  PlaySound(active);
    SetSoundVolume(active, engineVol);
    SetSoundPitch(active, enginePitch);
    SetSoundVolume(sndAir, std::clamp(airVol, 0.0f, 1.0f));
  } else {
    if (IsSoundPlaying(sndAir)) StopSound(sndAir);
  }

  UpdateAnimations();
}

// ============================================================
// UpdateAnimations — gear keyframe
// ============================================================
void FlightDisplay::UpdateAnimations() {
  if (animsCount <= 0 || modelAnims == nullptr) return;
  int maxFrames = modelAnims[0].keyframeCount;
  if (maxFrames <= 0) return;

  float speed = 60.0f;
  if (gearOpen) {
    gearFrame = std::min(gearFrame + GetFrameTime() * speed, (float)(maxFrames - 2));
  } else {
    gearFrame = std::max(gearFrame - GetFrameTime() * speed, 1.0f);
  }
  UpdateModelAnimation(modelF16, modelAnims[0], (int)gearFrame);
}

// ============================================================
// UpdateChaseCamera — position + quaternion-derived up vector
// ============================================================
void FlightDisplay::UpdateChaseCamera(const PlaneData &data) {
  float renderY    = data.altitude * 5.0f;
  float speedRatio = std::min(data.speed / 200.0f, 1.0f);
  float hoverH     = (1.0f - speedRatio) * 120.0f;

  Vector3 idealPos, targetLook;

  switch (cameraMode) {
  case 0: { // Chase (behind)
    float distH   = 300.0f;
    float camH    = 90.0f + data.pitch * 50.0f + hoverH;
    idealPos  = {data.x - sinf(data.yaw) * distH,
                 renderY + camH,
                 data.z - cosf(data.yaw) * distH};
    targetLook = {data.x + sinf(data.yaw) * 60.0f,
                  renderY + 15.0f + data.pitch * 50.0f,
                  data.z + cosf(data.yaw) * 60.0f};
    // Partial roll effect on camera up vector
    float rollEff = data.roll * 0.3f;
    camera.up = {sinf(rollEff), cosf(rollEff), 0.0f};
    camera.fovy = 60.0f;
  } break;

  case 1: { // Side
    float sideAng = data.yaw + PI * 0.5f;
    idealPos  = {data.x + sinf(sideAng) * 300.0f,
                 renderY + 20.0f + hoverH,
                 data.z + cosf(sideAng) * 300.0f};
    targetLook = {data.x, renderY + 10.0f, data.z};
    camera.up  = {0.0f, 1.0f, 0.0f};
    camera.fovy = 65.0f;
  } break;

  case 2: { // Front
    idealPos  = {data.x + sinf(data.yaw) * 350.0f,
                 renderY + 25.0f + hoverH,
                 data.z + cosf(data.yaw) * 350.0f};
    targetLook = {data.x, renderY + 10.0f, data.z};
    camera.up  = {0.0f, 1.0f, 0.0f};
    camera.fovy = 60.0f;
  } break;
  }

  // No lag — 1:1 lock eliminates jitter
  cameraPositionLag = idealPos;
  camera.position   = idealPos;
  camera.target     = targetLook;
}

// ============================================================
// DrawSky — equirectangular dome or procedural fallback
// ============================================================
void FlightDisplay::DrawSky(Vector3 camPos) {
  rlDisableDepthTest();
  rlDisableDepthMask();
  rlDisableBackfaceCulling();
  rlPushMatrix();
  rlTranslatef(camPos.x, camPos.y, camPos.z);

  if (skyLoaded) {
    skyModel.transform = MatrixRotateY(GetTime() * 0.002f);
    DrawModelEx(skyModel, {0, -5000.0f, 0}, {0, 0, 1}, 180.0f,
                {1.0f, 1.0f, 1.0f}, WHITE);
  } else {
    DrawSphereEx({0, 0, 0}, 20000.0f, 16, 16, {100, 150, 230, 255});
    BeginBlendMode(BLEND_ALPHA);
    for (int i = 0; i < 8; i++)
      DrawSphereEx({0, -(5000.0f - i * 700.0f), 0}, 18000.0f, 16, 16,
                   Fade({130, 180, 255, 255}, 0.08f + i * 0.045f));
    DrawSphereEx({0, -2000.0f, 0}, 15000.0f, 16, 16,
                 Fade({200, 230, 255, 255}, 0.7f));
    EndBlendMode();
  }

  rlPopMatrix();
  rlEnableBackfaceCulling();
  rlEnableDepthMask();
  rlEnableDepthTest();
}

// ============================================================
// DrawMapWorld — terrain tiles + aerodrome mesh
// ============================================================
void FlightDisplay::DrawMapWorld(const PlaneData &data) {
  rlEnableDepthTest();
  rlEnableBackfaceCulling();
  rlSetClipPlanes(150.0f, 150000.0f);

  float asphaltY = -8.5f;
  DrawPlane({camera.position.x, asphaltY - 0.5f, camera.position.z},
            {1000000.0f, 1000000.0f}, {150, 95, 65, 255});

  float tileSize = 2000.0f;
  int   viewDist = 12;
  float snapX    = floorf(camera.position.x / tileSize);
  float snapZ    = floorf(camera.position.z / tileSize);

  for (int i = -viewDist; i <= viewDist; i++) {
    for (int j = -viewDist; j <= viewDist; j++) {
      float wx = (snapX + i) * tileSize;
      float wz = (snapZ + j) * tileSize;
      if (fabsf(wx) < 3500.0f && fabsf(wz) < 3500.0f) continue;
      float noise = sinf(wx * 0.001f) * cosf(wz * 0.001f);
      Color col = (noise > 0.0f) ? Color{160, 100, 70, 255} : Color{140, 85, 55, 255};
      DrawCubeV({wx, asphaltY, wz}, {tileSize * 0.98f, 1.0f, tileSize * 0.98f}, col);
    }
  }

  if (!mapLoaded) return;

  rlPushMatrix();
  DrawModel(mapModel, {0.0f, asphaltY, 0.0f}, 7.0f, {255, 220, 180, 255});
  rlPopMatrix();

  if (data.landing_mode) {
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCylinderEx({0, asphaltY, 0}, {0, 60000.0f, 0},
                   300.0f, 300.0f, 16, Fade(GREEN, 0.4f));
    EndBlendMode();
    rlEnableDepthMask();
  }
}

// ============================================================
// DrawAircraftModel — quaternion rotation + control surface hook
// ============================================================
void FlightDisplay::DrawAircraftModel(const PlaneData &data) {
  if (!modelLoaded) return;

  const float globalScale    = 13.0f;
  const float modelScaleBody = 0.00015f * globalScale;
  const float modelScaleFire = 0.7f    * globalScale;

  // Base orientation: rotate model so nose points forward (+Z world)
  modelF16.transform = MatrixMultiply(MatrixIdentity(),
                                      MatrixRotateY(-90.0f * DEG2RAD));

  // ── Quaternion body rotation (avoids gimbal lock) ──────────
  // Chain: Yaw(Y) → Pitch(-X) → Roll(Z) — same semantic as original Euler
  Quaternion qYaw   = QuaternionFromAxisAngle({0.0f, 1.0f, 0.0f},  data.yaw);
  Quaternion qPitch = QuaternionFromAxisAngle({1.0f, 0.0f, 0.0f}, -data.pitch);
  Quaternion qRoll  = QuaternionFromAxisAngle({0.0f, 0.0f, 1.0f},  data.roll);
  Quaternion q      = QuaternionMultiply(QuaternionMultiply(qYaw, qPitch), qRoll);

  // NOTE: surface animation via bone rotation requires known bone indices
  // in the GLB model (model-dependent, TODO when model bones are mapped).
  // act_ele, act_ail, act_rud, act_lef are available in data for that purpose.

  rlEnableDepthTest();
  rlDisableBackfaceCulling();

  rlPushMatrix();
  // Rotation first (body→world), then body-frame offset — matches original Euler order
  Matrix rotMat = QuaternionToMatrix(q);
  rlMultMatrixf(MatrixToFloat(rotMat));
  rlTranslatef(0.0f, 5.0f, 0.0f);

  rlPushMatrix();
  rlScalef(modelScaleBody, modelScaleBody, modelScaleBody);

  // Pass 1: opaque meshes
  for (int i = 0; i < modelF16.meshCount; i++) {
    Material mat = modelF16.materials[modelF16.meshMaterial[i]];
    if (mat.maps[MATERIAL_MAP_ALBEDO].color.a > 100)
      DrawMesh(modelF16.meshes[i], mat, modelF16.transform);
  }

  // Pass 2: transparent canopy
  rlDisableDepthMask();
  for (int i = 0; i < modelF16.meshCount; i++) {
    Material mat = modelF16.materials[modelF16.meshMaterial[i]];
    if (mat.maps[MATERIAL_MAP_ALBEDO].color.a <= 100)
      DrawMesh(modelF16.meshes[i], mat, modelF16.transform);
  }
  rlEnableDepthMask();
  rlPopMatrix(); // pop scale

  // ── Engine exhaust / afterburner ───────────────────────────
  if (data.system_active && data.speed > 5.0f) {
    const float fuocoZ = -9.0f * globalScale;
    const float fuocoY =  3.0f * globalScale;

    rlPushMatrix();
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();

    float thrust    = std::clamp(data.speed / 200.0f, 0.05f, 1.0f);
    float timePulse = GetTime() * 30.0f;
    float flameLen  = 9.0f * modelScaleFire * thrust * (0.95f + 0.05f * sinf(timePulse));
    float baseRad   = 0.5f * modelScaleFire * (0.8f + 0.3f * thrust);
    int   slices    = 10;
    float stepZ     = flameLen / slices;

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    for (int i = 0; i < slices; i++) {
      float t = (float)i / slices;
      float shock = (thrust > 0.5f)
                        ? (0.6f + 0.4f * fabsf(cosf(t * PI * 6.0f * thrust - GetTime() * 20.0f)))
                        : 1.0f;
      float r = 0, g = 0, b = 0;
      if (thrust > 0.6f) {
        if      (t < 0.2f) { float nt = t/0.2f;       r=lerp(255,50,nt);  g=lerp(255,200,nt); b=255; }
        else if (t < 0.6f) { float nt=(t-0.2f)/0.4f;  r=lerp(50,150,nt);  g=lerp(200,20,nt);  b=lerp(255,200,nt); }
        else               { float nt=(t-0.6f)/0.4f;  r=lerp(150,255,nt); g=lerp(20,80,nt);   b=lerp(200,10,nt); }
      } else {
        r=255; g=lerp(220,50,t); b=lerp(100,0,t);
      }
      float a = lerp(255, 0, powf(t, 0.8f)) * (0.8f + 0.2f * sinf(timePulse + i));
      Color sc = {(unsigned char)r,(unsigned char)g,(unsigned char)b,(unsigned char)a};

      float   rad      = baseRad * (1.0f - powf(t, 1.5f)) * shock;
      Vector3 slicePos = {0.0f, fuocoY + 5.0f, fuocoZ - stepZ * i};
      DrawSphere(slicePos, rad * 1.8f, Fade(sc, 0.15f));
      DrawSphere(slicePos, rad * 0.9f, Fade(sc, 0.60f));
      DrawSphere(slicePos, rad * 0.4f, sc);
    }
    DrawSphere({0.0f, fuocoY + 5.0f, fuocoZ}, baseRad * 1.2f, WHITE);

    rlEnableDepthMask();
    EndBlendMode();
    rlPopMatrix();
  }

  rlPopMatrix(); // pop translate+quat
  rlEnableBackfaceCulling();
}

// ============================================================
// HUD helpers — rotated coordinate helper
// ============================================================
static inline Vector2 RotatePt(float cx, float cy, float phi,
                                float ox, float oy) {
  float c = cosf(phi), s = sinf(phi);
  return {cx + ox * c + oy * s, cy + ox * s - oy * c};
}

// ============================================================
// DrawPitchLadder
// ============================================================
void FlightDisplay::DrawPitchLadder(const PlaneData &data) {
  float cx        = GetScreenWidth()  * 0.5f;
  float cy        = GetScreenHeight() * 0.5f;
  float pitchDeg  = data.pitch * RAD2DEG;
  float phi       = data.roll;
  const float PPD = 9.0f; // pixels per degree
  Color col       = HUD_GREEN;

  for (int p = -80; p <= 80; p += 5) {
    if (p == 0) continue;

    float pErr   = (p - pitchDeg) * PPD;
    // Skip lines that are off-screen (rough cull)
    if (fabsf(pErr) > cy * 1.5f) continue;

    bool  major  = (p % 10 == 0);
    float halfW  = major ? 55.0f : 35.0f;
    float tickH  = 8.0f * (p > 0 ? 1.0f : -1.0f);

    Vector2 left  = RotatePt(cx, cy, phi, -halfW, pErr);
    Vector2 right = RotatePt(cx, cy, phi,  halfW, pErr);
    Vector2 lTick = RotatePt(cx, cy, phi, -halfW, pErr + tickH);
    Vector2 rTick = RotatePt(cx, cy, phi,  halfW, pErr + tickH);

    if (p < 0) {
      // Dashed line (below horizon)
      Vector2 d = {(right.x - left.x) / 7.0f, (right.y - left.y) / 7.0f};
      for (int seg = 0; seg < 7; seg += 2)
        DrawLineEx({left.x + d.x * seg,       left.y + d.y * seg},
                   {left.x + d.x * (seg + 1), left.y + d.y * (seg + 1)},
                   1.5f, col);
    } else {
      DrawLineEx(left, right, 1.5f, col);
    }
    DrawLineEx(left,  lTick, 1.5f, col);
    DrawLineEx(right, rTick, 1.5f, col);

    if (major) {
      const char *lbl = TextFormat("%d", abs(p));
      int tw = MeasureText(lbl, 10);
      Vector2 lPos = RotatePt(cx, cy, phi, -halfW - tw - 6, pErr);
      Vector2 rPos = RotatePt(cx, cy, phi,  halfW + 4,      pErr);
      DrawText(lbl, (int)lPos.x, (int)lPos.y - 5, 10, col);
      DrawText(lbl, (int)rPos.x, (int)rPos.y - 5, 10, col);
    }
  }

  // Waterline (aircraft reference)
  DrawLineEx({cx - 50.0f, cy}, {cx - 10.0f, cy}, 2.0f, col);
  DrawLineEx({cx + 10.0f, cy}, {cx + 50.0f, cy}, 2.0f, col);
  DrawCircleLines((int)cx, (int)cy, 3.0f, col);
}

// ============================================================
// DrawSpeedTape — left side, CAS in knots
// ============================================================
void FlightDisplay::DrawSpeedTape(const PlaneData &data) {
  int sh  = GetScreenHeight();
  int cy  = sh / 2;
  int tx  = 30, tw = 85, th = 220, ty = cy - th / 2;

  DrawRectangle(tx, ty, tw, th, HUD_BG);
  DrawRectangleLines(tx, ty, tw, th, HUD_DIM);

  float v = data.speed_kts;
  const float PPK = 3.0f; // pixels per knot

  BeginScissorMode(tx + 2, ty + 2, tw - 4, th - 4);
  for (int kts = 0; kts <= 800; kts += 10) {
    float dy   = (v - kts) * PPK;
    float lineY = cy + dy;
    if (lineY < ty || lineY > ty + th) continue;
    bool major = (kts % 50 == 0);
    int  ll    = major ? 14 : 7;
    DrawLineEx({(float)(tx + tw - ll), lineY}, {(float)(tx + tw), lineY},
               1.5f, HUD_GREEN);
    if (major) {
      const char *lbl = TextFormat("%d", kts);
      DrawText(lbl, tx + 4, (int)lineY - 5, 9, HUD_GREEN);
    }
  }
  EndScissorMode();

  // Current value box
  DrawRectangle(tx, cy - 13, tw, 26, Fade({0, 30, 15, 255}, 0.95f));
  DrawRectangleLines(tx, cy - 13, tw, 26, HUD_GREEN);
  DrawText(TextFormat("%03.0f", v), tx + 6, cy - 9, 18, HUD_WHITE);

  // Labels
  DrawText("KT",                   tx + tw - 18, ty - 14, 9, HUD_DIM);
  DrawText(TextFormat("M%.2f", data.mach), tx,       ty + th + 4, 9, HUD_GREEN);
}

// ============================================================
// DrawAltitudeTape — right side
// METRI come valore principale (soglie allarmi in metri!),
// piedi come secondario per riferimento.
// ============================================================
void FlightDisplay::DrawAltitudeTape(const PlaneData &data) {
  int sw  = GetScreenWidth();
  int sh  = GetScreenHeight();
  int cy  = sh / 2;
  int tw  = 105, th = 220;  // allargata per mostrare metri
  int tx  = sw - 30 - tw;
  int ty  = cy - th / 2;

  DrawRectangle(tx, ty, tw, th, HUD_BG);
  DrawRectangleLines(tx, ty, tw, th, HUD_DIM);

  // Tape graduata in METRI (100m minor, 500m major)
  float altM  = data.altitude;
  const float PPM = 0.04f; // pixel per metro (500m per 20px = 0.04)

  BeginScissorMode(tx + 2, ty + 2, tw - 4, th - 4);
  for (int m = 0; m <= 60000; m += 100) {
    float dy    = (altM - m) * PPM;
    float lineY = cy + dy;
    if (lineY < ty || lineY > ty + th) continue;
    bool major = (m % 500 == 0);
    int  ll    = major ? 14 : 7;
    DrawLineEx({(float)tx, lineY}, {(float)(tx + ll), lineY}, 1.5f, HUD_GREEN);
    if (major) {
      DrawText(TextFormat("%d", m), tx + 18, (int)lineY - 5, 9, HUD_GREEN);
    }
  }
  EndScissorMode();

  // Colore del box: ambra se in zona allarme quota
  bool lowWarn  = (data.altitude < 3000.0f && data.system_active && data.speed > 20.0f);
  bool highWarn = (data.altitude > 12000.0f && data.system_active);
  Color boxCol  = (lowWarn || highWarn) ? HUD_AMBER : HUD_GREEN;

  // Box principale: METRI (grande, leggibile, è il valore usato dagli allarmi)
  DrawRectangle(tx, cy - 16, tw, 32, Fade({0, 20, 10, 255}, 0.95f));
  DrawRectangleLines(tx, cy - 16, tw, 32, boxCol);
  DrawText(TextFormat("%5.0f m", altM), tx + 3, cy - 12, 18, HUD_WHITE);

  // Secondario: piedi in piccolo
  float altFt = altM * 3.28084f;
  DrawText("M", tx, ty - 14, 9, boxCol);
  DrawText(TextFormat("%.0fft", altFt), tx + 2, ty + th + 4, 9, HUD_DIM);
}

// ============================================================
// DrawHeadingTape — top center
// ============================================================
void FlightDisplay::DrawHeadingTape(const PlaneData &data) {
  int sw  = GetScreenWidth();
  int cx  = sw / 2;
  int tx  = cx - 180, ty = 25, tw = 360, th = 34;

  DrawRectangle(tx, ty, tw, th, HUD_BG);
  DrawRectangleLines(tx, ty, tw, th, HUD_DIM);

  float head = data.yaw * RAD2DEG;
  if (head < 0.0f) head += 360.0f;

  BeginScissorMode(tx + 3, ty + 3, tw - 6, th - 6);
  for (int i = -180; i <= 540; i += 10) {
    float px = (tx + tw / 2.0f) + (i - head) * 4.5f;
    if (px < tx || px > tx + tw) continue;
    bool major = (i % 30 == 0);
    DrawLineEx({px, (float)(ty + (major ? 8 : 18))}, {px, (float)(ty + th - 4)},
               1.5f, HUD_GREEN);
    if (major) {
      int   hdg = ((i % 360) + 360) % 360;
      const char *lbl = (hdg == 0)   ? "N" : (hdg == 90)  ? "E" :
                        (hdg == 180) ? "S" : (hdg == 270) ? "W" :
                                             TextFormat("%03d", hdg);
      DrawText(lbl, (int)(px - MeasureText(lbl, 9) * 0.5f), ty + 2, 9, HUD_WHITE);
    }
  }
  EndScissorMode();

  // Pointer
  DrawTriangle({(float)cx,     (float)(ty + th)},
               {(float)(cx-5), (float)(ty + th + 7)},
               {(float)(cx+5), (float)(ty + th + 7)}, HUD_RED);
}

// ============================================================
// DrawAlphaGMeter — AoA box (bottom-left) + G-meter (bottom-right)
// ============================================================
void FlightDisplay::DrawAlphaGMeter(const PlaneData &data) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();

  // Alpha box
  {
    int ax = 30, ay = sh - 110, aw = 115, ah = 58;
    DrawRectangle(ax, ay, aw, ah, HUD_BG);
    DrawRectangleLines(ax, ay, aw, ah, HUD_DIM);
    DrawText("AOA",                         ax + 6, ay + 4,  9,  HUD_DIM);
    DrawText(TextFormat("%+5.1f\xc2\xb0",   // °
                        data.alpha * RAD2DEG),
             ax + 6, ay + 18, 22, HUD_GREEN);
    DrawText("deg",                          ax + aw - 28, ay + ah - 14, 9, HUD_DIM);
  }

  // G-meter box
  {
    int gx = sw - 145, gy = sh - 110, gw = 115, gh = 58;
    DrawRectangle(gx, gy, gw, gh, HUD_BG);
    DrawRectangleLines(gx, gy, gw, gh, HUD_DIM);
    DrawText("G LOAD",                      gx + 6, gy + 4,  9,  HUD_DIM);
    Color gc = (data.nz > 7.5f || data.nz < -0.5f) ? HUD_RED :
               (data.nz > 6.0f)                     ? HUD_AMBER : HUD_GREEN;
    DrawText(TextFormat("%+5.1fG", data.nz), gx + 6, gy + 18, 22, gc);
  }
}

// ============================================================
// DrawWarnings — overlay allarmi + barra status quota sempre visibile
//
// SOGLIE IN METRI (leggibili dal box altimetrico sulla destra):
//   Bassa quota:  ALLARME 1000m–3000m | AP PITCH-UP  sotto 1000m
//   Alta quota:   ALLARME 12000m–15000m | AP PITCH-DOWN sopra 15000m
//   Bank angle:   WARNING >60° | LIMIT >75°
//
// LANDING MODE ON → green border, nessun allarme, AP quota off.
// ============================================================
void FlightDisplay::DrawWarnings(const PlaneData &data) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  int cx = sw / 2;

  bool blink = ((int)(GetTime() * 8) % 2 == 0);

  // ── Barra status quota — sempre visibile, mostra la zona corrente ──
  // Logica identica al voice warning: allarme bassa quota solo se above_3km
  {
    const char *zoneLbl = nullptr;
    Color zoneCol = HUD_DIM;
    bool inFlt = data.system_active && data.airborne && !data.landing_mode;

    if (!data.airborne) {
      zoneLbl = "A TERRA — decollo: W + freccia SU";
      zoneCol = HUD_DIM;
    } else if (inFlt && data.above_3km && data.altitude < 1000.0f) {
      zoneLbl = "AP QUOTA: PITCH UP ATTIVO (<1000m)";
      zoneCol = HUD_AMBER;
    } else if (inFlt && data.above_3km && data.altitude < 3000.0f) {
      zoneLbl = "!! ALLARME BASSA QUOTA (<3000m) !!";
      zoneCol = HUD_RED;
    } else if (inFlt && data.altitude > 15000.0f) {
      zoneLbl = "AP QUOTA: PITCH DOWN ATTIVO (>15000m)";
      zoneCol = HUD_AMBER;
    } else if (inFlt && data.altitude > 12000.0f) {
      zoneLbl = "!! ALLARME ALTA QUOTA (>12000m) !!";
      zoneCol = HUD_RED;
    } else if (!data.above_3km && data.airborne) {
      zoneLbl = "SALITA — allarme bassa quota non armato";
      zoneCol = HUD_DIM;
    } else if (data.system_active) {
      zoneLbl = "QUOTA OK";
      zoneCol = HUD_DIM;
    }
    if (zoneLbl) {
      int lw = MeasureText(zoneLbl, 10);
      DrawText(zoneLbl, cx - lw / 2, sh - 56, 10, zoneCol);
    }
  }

  // ── Landing Mode ──────────────────────────────────────────────────
  if (data.landing_mode) {
    DrawRectangleLinesEx({0, 0, (float)sw, (float)sh}, 5.0f, HUD_GREEN);
    const char *msg = "LANDING MODE  [L = esci]  —  AP QUOTA OFF";
    int msgW = MeasureText(msg, 15);
    DrawRectangle(cx - msgW / 2 - 8, sh / 2 + 120, msgW + 16, 28, HUD_BG);
    DrawRectangleLines(cx - msgW / 2 - 8, sh / 2 + 120, msgW + 16, 28, HUD_GREEN);
    DrawText(msg, cx - msgW / 2, sh / 2 + 126, 15, HUD_GREEN);
    return;
  }

  // ── Allarmi in volo — stessa logica della parte audio ─────────────
  // inFlight: airborne + system_active + !landing_mode
  bool inFlight = data.system_active && data.airborne && !data.landing_mode;

  const char *warn = nullptr;
  bool        isAP = false;

  if (inFlight) {
    // Bassa quota: solo se above_3km (armato dopo aver superato 3500m)
    if (data.above_3km && data.altitude < 1000.0f) {
      warn = TextFormat("AP PITCH UP  alt=%.0fm (< 1000m)", data.altitude);
      isAP = true;
    } else if (data.above_3km && data.altitude < 3000.0f) {
      warn = TextFormat("!! PULL UP !!  alt=%.0fm  soglia 3000m", data.altitude);
    }
    // Alta quota (sempre armato in crociera)
    else if (data.altitude > 15000.0f) {
      warn = TextFormat("AP PITCH DOWN  alt=%.0fm (> 15000m)", data.altitude);
      isAP = true;
    } else if (data.altitude > 12000.0f) {
      warn = TextFormat("!! PUSH DOWN !!  alt=%.0fm  soglia 12000m", data.altitude);
    }
    // Bank angle
    else if (fabsf(data.roll) > 1.309f) {
      warn = TextFormat("BANK LIMIT 75deg  phi=%.0fdeg", fabsf(data.roll) * RAD2DEG);
    } else if (fabsf(data.roll) > 1.047f) {
      warn = TextFormat("BANK ANGLE 60deg  phi=%.0fdeg", fabsf(data.roll) * RAD2DEG);
    } else if (data.nz > 8.5f) {
      warn = TextFormat("G LIMIT  nz=%.1fG", data.nz);
    }
  }

  if (warn) {
    Color borderCol = isAP ? HUD_AMBER : HUD_RED;
    DrawRectangleLinesEx({0, 0, (float)sw, (float)sh}, 5.0f,
                         blink ? borderCol : Fade(borderCol, 0.3f));
    int fw = MeasureText(warn, 17);
    DrawRectangle(cx - fw / 2 - 12, sh / 2 + 118, fw + 24, 34,
                  blink ? Fade(borderCol, 0.35f) : HUD_BG);
    DrawRectangleLines(cx - fw / 2 - 12, sh / 2 + 118, fw + 24, 34, borderCol);
    DrawText(warn, cx - fw / 2, sh / 2 + 126, 17, blink ? WHITE : borderCol);
  }
}

// ============================================================
// DrawHUD — master compositor
// ============================================================
void FlightDisplay::DrawHUD(const PlaneData &data) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();

  // CRT scanline overlay
  for (int i = 0; i < sh; i += 3)
    DrawLine(0, i, sw, i, Fade(BLACK, 0.08f));

  DrawPitchLadder(data);
  DrawHeadingTape(data);
  DrawSpeedTape(data);
  DrawAltitudeTape(data);
  DrawAlphaGMeter(data);
  DrawWarnings(data);

  // FBW mode badge (bottom centre)
  const char *modeLbl = (data.system_active) ? "FBW NORMAL LAW" : "ENGINES OFF";
  Color modeCol = data.system_active ? HUD_GREEN : HUD_AMBER;
  DrawText(modeLbl,
           sw / 2 - MeasureText(modeLbl, 10) / 2, sh - 22, 10, modeCol);

  // Status message
  if (data.status_msg[0])
    DrawText(data.status_msg, 30, sh - 40, 9, HUD_DIM);
}

// ============================================================
// Draw — main render call
// ============================================================
void FlightDisplay::Draw(const PlaneData &data) {
  BeginDrawing();
  ClearBackground(BLACK);

  UpdateChaseCamera(data);
  rlSetClipPlanes(150.0f, 150000.0f);

  BeginMode3D(camera);
  DrawSky(camera.position);
  DrawMapWorld(data);

  // Position aircraft in world space
  rlPushMatrix();
  rlTranslatef(data.x, data.altitude * 5.0f, data.z);
  DrawAircraftModel(data);  // rotation is applied internally via quaternion
  rlPopMatrix();

  EndMode3D();

  DrawHUD(data);
  EndDrawing();
}
