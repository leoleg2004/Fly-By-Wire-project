#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// Assicurati che PlaneData sia accessibile
#include "FlightDisplay.hpp"

class MonitorDisplay {
public:
    MonitorDisplay(int width, int height, const std::string& title);
    ~MonitorDisplay();

    bool IsActive();

    // Torniamo a 1 solo argomento come richiesto
    void Draw(const PlaneData& data);

private:
    int m_width;
    int m_height;

    // Palette "Stealth Command"
    const Color colBack      = { 2, 6, 12, 255 };
    const Color colHUD       = { 0, 225, 255, 255 };
    const Color colWarning   = { 255, 40, 0, 255 };
    const Color colGreen     = { 0, 255, 120, 255 };
    const Color colPanel     = { 15, 25, 35, 240 };

    // Moduli di Rendering
    void DrawTechFrame(int x, int y, int w, int h, const char* title);
    void DrawTacticalRadar(int x, int y, int size, const PlaneData& data);
    void DrawArtificialHorizon(int x, int y, int w, int h, float pitch, float roll);
    void DrawVerticalTape(int x, int y, int w, int h, float value, float step, Color color, bool rightAlign);
    void DrawHeadingTape(int x, int y, int w, float yaw);
    void DrawAttitudeIndicator(int x, int y, float value, const char* label);
    void DrawVFX();
};
