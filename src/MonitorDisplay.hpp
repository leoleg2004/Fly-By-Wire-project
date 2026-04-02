#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// Dipendenza per definizioni telemetriche strutturate
#include "FlightDisplay.hpp"

class MonitorDisplay {
public:
    MonitorDisplay(int width, int height, const std::string& title);
    ~MonitorDisplay();

    bool IsActive();

    // Routine principale di aggiornamento canvas
    void Draw(const PlaneData& data);

private:
    int m_width;
    int m_height;

    // Costanti cromatiche MFD
    const Color colBack      = { 2, 6, 12, 255 };
    const Color colHUD       = { 0, 225, 255, 255 };
    const Color colWarning   = { 255, 40, 0, 255 };
    const Color colGreen     = { 0, 255, 120, 255 };
    const Color colPanel     = { 15, 25, 35, 240 };

    // Funzioni di rendering specializzate per quadrante
    void DrawTechFrame(int x, int y, int w, int h, const char* title);
    void DrawTacticalRadar(int x, int y, int size, const PlaneData& data);
    void DrawArtificialHorizon(int x, int y, int w, int h, float pitch, float roll);
    void DrawVerticalTape(int x, int y, int w, int h, float value, float step, Color color, bool rightAlign);
    void DrawHeadingTape(int x, int y, int w, float yaw);
    void DrawAttitudeIndicator(int x, int y, float value, const char* label);
    void DrawVFX();
};
