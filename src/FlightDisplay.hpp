#ifndef FLIGHT_DISPLAY_HPP
#define FLIGHT_DISPLAY_HPP
#include "FBW_Types.hpp"
#include "raylib.h"
#include <string>
#include <vector>

// Struttura dati per il trasferimento della telemetria al motore grafico
struct PlaneData {
  float roll = 0.0f;
  float pitch = 0.0f;
  float yaw = 0.0f;
  float altitude = 5000.0f;
  // Coordinate spaziali
  float x = 0.0f;
  float z = 0.0f;
  float speed = 0.0f;
  char status_msg[64]; // Buffer messaggi strumentali
  bool system_active = false;
  bool landing_mode = true; // Disabilita i controlli FCS
};

class FlightDisplay {

private:
  ModelAnimation *modelAnims;
  int animsCount = 0;
  float gearFrame = 0.0f; // Stato animazione carrello
  bool gearOpen = false;
  // TODO: Implementazione superfici in modello sorgente
  // Variabili d'assetto superfici (Flap/Alettoni)
  int flapFrame = 0;
  bool flapOpen = false;

public:
  FlightDisplay(int width, int height, const std::string &title);
  ~FlightDisplay(); // Distruttore per il clean-up dei modelli caricati

  bool IsActive();
  // Legge i tasti, aggiorna audio/animazioni, scrive i comandi pilota in
  // pilot_out
  void HandleInput(PlaneData &data, PilotInput &pilot_out);
  void Draw(const PlaneData &data);

private:
  Camera3D camera;
  Vector3 cameraPositionLag;
  Model skyModel;
  bool skyLoaded;
  Model mapModel; // Modello del suolo
  bool mapLoaded; // Flag di caricamento terreno
                  // Model terrainModel;
                  //    bool terrainLoaded;
  Model modelF35; // Modello 3D principale
  Texture2D textureF35;
  bool modelLoaded; // Flag validità mesh
  // --- SISTEMA AUDIO ---
  Sound sndEngineStart;
  Sound sndEngineLoop;
  bool isEngineStarting;
  Sound sndGear;
  Sound sndLanding;
  Sound sndWarning;
  Sound sndPullUp;
  Sound sndCaution;
  Sound sndAir;
  Sound sndEngineDown;

  // Metodi privati di rendering e aggiornamento
  void UpdateChaseCamera(const PlaneData &data);
  void DrawUltimateF35(const PlaneData &data);
  void DrawMapWorld(const PlaneData &data); // Richiede offset posizionale
  void DrawHUD(const PlaneData &data);
  void UpdateAnimations();
  void DrawSky(Vector3 cameraPosition);

  void DrawGround(const PlaneData &data);
};

#endif
