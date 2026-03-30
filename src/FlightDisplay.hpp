#ifndef FLIGHT_DISPLAY_HPP
#define FLIGHT_DISPLAY_HPP
#include "FBW_Types.hpp"
#include "raylib.h"
#include <string>

// Telemetry transfer struct — populated from FlightState + ActuatorOut each frame
struct PlaneData {
  // Attitude (rad)
  float roll  = 0.0f;
  float pitch = 0.0f;
  float yaw   = 0.0f;

  // Angular rates (rad/s)
  float roll_rate  = 0.0f;
  float pitch_rate = 0.0f;
  float yaw_rate   = 0.0f;

  // Body velocities (m/s)
  float u = 0.0f, v = 0.0f, w = 0.0f;

  // Aerodynamic angles (rad)
  float alpha = 0.0f; // AoA
  float beta  = 0.0f; // sideslip

  // Navigation
  float altitude = 0.0f; // m MSL
  float x = 0.0f;        // m East
  float z = 0.0f;        // m North

  // Derived kinematic quantities
  float speed     = 0.0f; // m/s total
  float speed_kts = 0.0f; // CAS knots
  float mach      = 0.0f; // Mach number
  float nz        = 1.0f; // Load factor (g)

  // Actuator deflections (deg)
  float act_ele = 0.0f;
  float act_ail = 0.0f;
  float act_rud = 0.0f;
  float act_lef = 0.0f;
  float act_thr = 0.0f;

  // System state
  char status_msg[64] = {};
  bool system_active  = false;
  bool landing_mode   = true;

  // Flag di volo — aggiornati in HandleInput, usati da DrawWarnings
  // airborne  : diventa true quando alt > 150m, reset a terra (< 10m)
  // above_3km : diventa true quando alt > 3500m → ARMA l'allarme bassa quota.
  //             Senza questo flag, l'allarme suonerebbe durante tutta la salita
  //             iniziale (0→3000m), che è corretto fisicamente ma molto fastidioso.
  bool airborne  = false;
  bool above_3km = false;  // arma allarme bassa quota solo dopo aver superato 3500m
};

class FlightDisplay {
public:
  FlightDisplay(int width, int height, const std::string &title);
  ~FlightDisplay();

  bool IsActive();
  // Reads keys, updates audio/animations, writes pilot commands to pilot_out
  void HandleInput(PlaneData &data, PilotInput &pilot_out);
  void Draw(const PlaneData &data);

private:
  Camera3D camera;
  Vector3  cameraPositionLag;

  // Sky
  Texture2D skyTexture;
  Model     skyModel;
  bool      skyLoaded = false;

  // Terrain / aerodrome
  Model mapModel;
  bool  mapLoaded = false;

  // Aircraft model
  Model          modelF16;
  ModelAnimation *modelAnims  = nullptr;
  int             animsCount  = 0;
  float           gearFrame   = 0.0f;
  bool            gearOpen    = false;
  bool            modelLoaded = false;

  // Audio
  Sound sndEngineStart, sndEngineLoop, sndEngineDown;
  Sound sndGear, sndLanding, sndWarning, sndPullUp, sndCaution, sndAir;

  // Private rendering helpers
  void UpdateChaseCamera(const PlaneData &data);
  void DrawAircraftModel(const PlaneData &data);
  void DrawMapWorld(const PlaneData &data);
  void DrawSky(Vector3 cameraPosition);
  void UpdateAnimations();

  // HUD elements
  void DrawHUD(const PlaneData &data);
  void DrawPitchLadder(const PlaneData &data);
  void DrawSpeedTape(const PlaneData &data);
  void DrawAltitudeTape(const PlaneData &data);
  void DrawHeadingTape(const PlaneData &data);
  void DrawAlphaGMeter(const PlaneData &data);
  void DrawWarnings(const PlaneData &data);
};

#endif
