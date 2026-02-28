#ifndef FLIGHT_DISPLAY_HPP
#define FLIGHT_DISPLAY_HPP
#include "raylib.h"
#include <string>
#include <vector>

// Struttura Dati Aereo con cui passo alla grafic ai dati
struct PlaneData {
    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    float altitude = 5000.0f;
   //movimento nello spazio nelle tre dimensioni
    float x = 0.0f;
    float z = 0.0f;
float speed=0.0f;
char status_msg[64];//aggiunto per gestire nel monitor i messaggi di condizione di volo
    bool system_active = true;
    bool landing_mode = false;//per diabilitare il fly-by-wire
};


class FlightDisplay {

private:

	    ModelAnimation *modelAnims;
	    int animsCount=0;
	    float gearFrame=0.0f;    // DEVE essere float
	    bool gearOpen=false;
//devo implementare su blender
    // Variabili per i FLAP/ALETTONI
    int flapFrame = 0;
    bool flapOpen = false;

public:
    FlightDisplay(int width, int height, const std::string& title);
    ~FlightDisplay(); // Distruttore (Importante per scaricare il modello)

    bool IsActive();
    void HandleInput(PlaneData& data);
    void Draw(const PlaneData& data);

private:
    Camera3D camera;
    Vector3 cameraPositionLag;
    Model skyModel;
        bool skyLoaded;
    Model mapModel;       // Il modello del terreno
        bool mapLoaded;       // controlla che ci sia una mappa caricata
    Model modelF35;//carico modello dell'aereo
    Texture2D textureF35;
    bool modelLoaded;     //condizione che ci sia un modello per l'aereo

    // Funzioni interne al flightDisplay.cpp
    void UpdateChaseCamera(const PlaneData& data);
        void DrawUltimateF35(const PlaneData& data);
        void DrawMapWorld(const PlaneData& data); // gli pass plane data perche deve sapere dove si trova
        void DrawHUD(const PlaneData& data);
        void UpdateAnimations();
        void DrawSky(Vector3 cameraPosition);

        void DrawGround(const PlaneData& data);
};

#endif
