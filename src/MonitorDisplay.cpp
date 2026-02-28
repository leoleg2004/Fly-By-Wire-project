#include "MonitorDisplay.hpp"
#include "rlgl.h"
#include <cmath>
#include <algorithm>
#include "SharedMemory.hpp"
#include "TelemetryPubSubTypes.hpp"
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/rtps/attributes/PropertyPolicy.hpp>
#include <fastdds/statistics/dds/domain/DomainParticipant.hpp>
#include <fastdds/statistics/topic_names.hpp>
#include <fastdds/statistics/dds/publisher/qos/DataWriterQos.hpp>
#include <thread>
#include <iostream>
#include <iomanip>
#include <vector>
#include "raylib.h"
#include "raymath.h"
#include "FlightDisplay.hpp"

MonitorDisplay::MonitorDisplay(int width, int height, const std::string& title)
    : m_width(width), m_height(height) {
    InitWindow(width, height, title.c_str());
    SetTargetFPS(60);
}

MonitorDisplay::~MonitorDisplay() { CloseWindow(); }
bool MonitorDisplay::IsActive() { return !WindowShouldClose(); }


void DrawTacticalGrid(int x, int y, int w, int h, Color color) {
    int spacing = 20;
    for (int i = 0; i <= w; i += spacing) DrawLine(x + i, y, x + i, y + h, color);
    for (int i = 0; i <= h; i += spacing) DrawLine(x, y + i, x + w, y + i, color);
}

void MonitorDisplay::DrawTechFrame(int x, int y, int w, int h, const char* title) {
    // Sfondo e Griglia
    DrawRectangle(x, y, w, h, Fade(colPanel, 0.9f));
    DrawTacticalGrid(x, y, w, h, Fade(colHUD, 0.05f));

    // Cornice Principale
    DrawRectangleLinesEx({(float)x, (float)y, (float)w, (float)h}, 1.0f, Fade(colHUD, 0.5f));

    // Corner Brackets (Angoli militari)
    int cl = 15; // Corner length
    int ct = 3;  // Corner thickness
    DrawRectangle(x, y, cl, ct, colHUD); DrawRectangle(x, y, ct, cl, colHUD); // Top-Left
    DrawRectangle(x+w-cl, y, cl, ct, colHUD); DrawRectangle(x+w-ct, y, ct, cl, colHUD); // Top-Right
    DrawRectangle(x, y+h-ct, cl, ct, colHUD); DrawRectangle(x, y+h-cl, ct, cl, colHUD); // Bot-Left
    DrawRectangle(x+w-cl, y+h-ct, cl, ct, colHUD); DrawRectangle(x+w-ct, y+h-cl, ct, cl, colHUD); // Bot-Right

    // Header Tecnico
    DrawRectangle(x, y - 20, w, 20, Fade(colHUD, 0.15f));
    DrawRectangle(x, y - 20, 4, 20, colHUD);
    DrawText(TextFormat("[ %s ]", title), x + 10, y - 16, 10, colHUD);
}

void MonitorDisplay::DrawAttitudeIndicator(int x, int y, float value, const char* label) {
    int barW = 160;
    DrawText(label, x, y, 10, colHUD);

    // Background slot
    DrawRectangle(x, y + 15, barW, 10, Fade(BLACK, 0.6f));
    DrawRectangleLines(x, y + 15, barW, 10, Fade(colHUD, 0.3f));

    // Marker Centrale
    DrawLineEx({(float)x + barW/2, (float)y + 12}, {(float)x + barW/2, (float)y + 28}, 2.0f, WHITE);

    // Valore dinamicamente limitato tra -PI e PI
    float clampedVal = value;
    if (clampedVal > PI) clampedVal = PI;
    if (clampedVal < -PI) clampedVal = -PI;

    float norm = (clampedVal + PI) / (2 * PI);
    float indicatorX = x + (norm * barW);

    // Cursore mobile
    DrawTriangle({indicatorX - 4, (float)y + 25}, {indicatorX + 4, (float)y + 25}, {indicatorX, (float)y + 15}, colGreen);
    DrawText(TextFormat("%+05.2f", value), x + barW + 15, y + 14, 10, colGreen);
}

void MonitorDisplay::DrawTacticalRadar(int x, int y, int size, const PlaneData& data) {
    Vector2 ctr = {(float)x + size/2, (float)y + size/2};
    float r = size/2.2f;

    // Crosshair del radar
    DrawLineV({ctr.x - r - 10, ctr.y}, {ctr.x + r + 10, ctr.y}, Fade(colHUD, 0.3f));
    DrawLineV({ctr.x, ctr.y - r - 10}, {ctr.x, ctr.y + r + 10}, Fade(colHUD, 0.3f));

    // Anelli di distanza con etichette
    for(int i=1; i<=3; i++) {
        DrawCircleLines(ctr.x, ctr.y, (r/3)*i, Fade(colHUD, 0.2f));
        DrawText(TextFormat("%d NM", i*5), ctr.x + 2, ctr.y - (r/3)*i - 10, 10, Fade(colHUD, 0.5f));
    }

    // Effetto sweep rotante
    float sweep = (float)fmod(GetTime() * 120.0f, 360.0f);
    DrawCircleSector(ctr, r, sweep, sweep + 45, 20, Fade(colHUD, 0.15f));
    DrawLineEx(ctr, {ctr.x + cosf(sweep*DEG2RAD)*r, ctr.y + sinf(sweep*DEG2RAD)*r}, 2.0f, colHUD);

    // Tracking del bersaglio (Aereo)
    float blipX = ctr.x + (fmod(data.x, 2500.0f)/2500.0f)*r;
    float blipZ = ctr.y + (fmod(data.z, 2500.0f)/2500.0f)*r;

    // Target Lock Box
    DrawRectangleLines(blipX - 6, blipZ - 6, 12, 12, colGreen);
    DrawPoly({blipX, blipZ}, 3, 5, data.yaw * RAD2DEG, colGreen);

    // Dati target a schermo
    DrawText(TextFormat("TGT X: %.0f", data.x), x + 5, y + size - 25, 10, colHUD);
    DrawText(TextFormat("TGT Z: %.0f", data.z), x + 5, y + size - 12, 10, colHUD);
}

void MonitorDisplay::DrawArtificialHorizon(int x, int y, int w, int h, float pitch, float roll) {
    Vector2 center = {(float)x + w/2, (float)y + h/2};
    BeginScissorMode(x, y, w, h);

    rlPushMatrix();
    rlTranslatef(center.x, center.y, 0);
    rlRotatef(roll * RAD2DEG, 0, 0, 1);

    float pOff = pitch * 200.0f; // Sensibilità pitch

    // Cielo e Terra con gradiente
    DrawRectangleGradientV(-w, -h-pOff, w*2, h, Fade(BLUE, 0.4f), Fade(SKYBLUE, 0.2f));
    DrawRectangleGradientV(-w, -pOff, w*2, h, Fade(BROWN, 0.4f), Fade(DARKBROWN, 0.8f));
    DrawLineEx({(float)-w, -pOff}, {(float)w, -pOff}, 2.0f, WHITE);

    // Pitch Ladder Avanzata (Gradi)
    for(int i=-90; i<=90; i+=10) {
        if (i == 0) continue;
        float ly = -pOff - (i * 3.5f);
        int lineW = (i % 30 == 0) ? 40 : 20; // Linee più larghe ogni 30 gradi
        DrawLineEx({(float)-lineW, ly}, {(float)lineW, ly}, 2.0f, WHITE);
        // Tick marks laterali
        DrawLineEx({(float)-lineW, ly}, {(float)-lineW, ly + (i>0?5:-5)}, 2.0f, WHITE);
        DrawLineEx({(float)lineW, ly}, {(float)lineW, ly + (i>0?5:-5)}, 2.0f, WHITE);

        DrawText(TextFormat("%d", std::abs(i)), lineW + 5, ly - 5, 10, WHITE);
        DrawText(TextFormat("%d", std::abs(i)), -lineW - 20, ly - 5, 10, WHITE);
    }
    rlPopMatrix();
    EndScissorMode();

    // Simbolo Aereo Fisso (Boresight)
    DrawLineEx({center.x-40, center.y}, {center.x-15, center.y}, 3.0f, YELLOW);
    DrawLineEx({center.x+15, center.y}, {center.x+40, center.y}, 3.0f, YELLOW);
    DrawLineEx({center.x-15, center.y}, {center.x, center.y+10}, 3.0f, YELLOW);
    DrawLineEx({center.x, center.y+10}, {center.x+15, center.y}, 3.0f, YELLOW);
    DrawCircle(center.x, center.y, 2.0f, RED);
}

void MonitorDisplay::DrawVerticalTape(int x, int y, int w, int h, float value, float step, Color color, bool rightAlign) {
    DrawRectangle(x, y, w, h, Fade(BLACK, 0.7f));
    DrawRectangleLines(x, y, w, h, Fade(colHUD, 0.2f));

    BeginScissorMode(x, y, w, h);
    float offset = fmod(value, step);
    for (int i = -4; i <= 4; i++) {
        float v = ((int)(value/step)*step) + (i*step);
        float py = (y+h/2) - (i*(h/5)) + (offset*(h/5)/step);

        // Tick marks
        DrawLineEx({(float)(rightAlign ? x+w-8 : x), py}, {(float)(rightAlign ? x+w : x+8), py}, 2.0f, color);
        DrawText(TextFormat("%d", (int)v), (rightAlign ? x+5 : x+15), py-5, 10, Fade(color, 0.7f));
    }
    EndScissorMode();

    // Central Value Box (Target Box)
    int boxY = y + h/2 - 10;
    DrawRectangle(x - 5, boxY, w + 10, 20, BLACK);
    DrawRectangleLines(x - 5, boxY, w + 10, 20, color);

    // Indicatore triangolare
    if (rightAlign) DrawTriangle({(float)x-5, (float)boxY+10}, {(float)x-10, (float)boxY+5}, {(float)x-10, (float)boxY+15}, color);
    else DrawTriangle({(float)x+w+5, (float)boxY+10}, {(float)x+w+10, (float)boxY+5}, {(float)x+w+10, (float)boxY+15}, color);

    DrawText(TextFormat("%03d", (int)value), x + 5, boxY + 5, 10, WHITE);
}

void MonitorDisplay::DrawHeadingTape(int x, int y, int w, float yaw) {
    DrawRectangle(x, y, w, 25, Fade(BLACK, 0.7f));
    DrawRectangleLines(x, y, w, 25, Fade(colHUD, 0.4f));

    float head = yaw * RAD2DEG;
    BeginScissorMode(x, y, w, 25);
    for(int i=-180; i<=540; i+=10) {
        float px = (x+w/2) + (i-head)*5;
        if(px > x && px < x+w) {
            DrawLineEx({px, (float)y+15}, {px, (float)y+25}, 2.0f, colHUD);
            if(i%30 == 0) {
                const char* lbl = (i%360==0)?"N":(i%360==90)?"E":(i%360==180)?"S":(i%360==270)?"W":TextFormat("%d", i%360);
                DrawText(lbl, px-5, y+2, 10, WHITE);
            }
        }
    }
    EndScissorMode();

    // Puntatore centrale bussola
    DrawTriangle({(float)x+w/2, (float)y+25}, {(float)x+w/2-5, (float)y+35}, {(float)x+w/2+5, (float)y+35}, colWarning);
}

void MonitorDisplay::Draw(const PlaneData& data) {
    BeginDrawing();
    ClearBackground(colBack);

    int m = 25;
    int pW = (m_width - m*4)/3;
    int pH = m_height - m*2;

    // --- SINISTRA: Radar Tattico ---
    DrawTechFrame(m, m, pW, pH, "TACTICAL AIRSPACE MONITOR");
    DrawTacticalRadar(m + 15, m + 20, pW - 30, data);

    // Barra velocità inferiore
    float sRatio = std::min(data.speed / 200.0f, 1.0f);
    DrawRectangle(m+20, m+pH-50, pW-40, 12, Fade(BLACK, 0.6f));
    DrawRectangleLines(m+20, m+pH-50, pW-40, 12, Fade(colHUD, 0.3f));
    DrawRectangle(m+20, m+pH-50, (pW-40)*sRatio, 12, (sRatio > 0.9f ? colWarning : colHUD));
    DrawText(TextFormat("AIRSPEED: %03.0f / 200 KPH", data.speed), m+20, m+pH-30, 10, colHUD);

    // --- CENTRO: PFD (Primary Flight Display) ---
    int px2 = m*2 + pW;
    DrawTechFrame(px2, m, pW, pH, "PRIMARY FLIGHT DISPLAY");
    DrawHeadingTape(px2 + 20, m + 20, pW - 40, data.yaw);
    DrawArtificialHorizon(px2 + 45, m + 60, pW - 90, pH - 90, data.pitch, data.roll);
    DrawVerticalTape(px2 + 5, m + 60, 35, pH - 90, data.speed, 20, colGreen, false);
    DrawVerticalTape(px2 + pW - 40, m + 60, 35, pH - 90, data.altitude, 500, colHUD, true);

    // --- DESTRA: Cinematica & Allarmi ---
    int px3 = m*3 + pW*2;
    DrawTechFrame(px3, m, pW, pH, "FLIGHT KINEMATICS");
    int startY = m + 40;
    DrawAttitudeIndicator(px3 + 20, startY, data.roll, "ROLL AXIS (RAD)");
    DrawAttitudeIndicator(px3 + 20, startY + 60, data.pitch, "PITCH AXIS (RAD)");
    DrawAttitudeIndicator(px3 + 20, startY + 120, data.yaw, "YAW AXIS (RAD)");

    DrawRectangle(px3 + 20, startY + 180, pW - 40, 1, Fade(colHUD, 0.3f)); // Separatore

        std::string currentStatus(data.status_msg);
        Color statusColor = colGreen;
        bool isWarning = false;

        // Se il messaggio contiene "NORMAL FLIGHT" (o "NOMINAL"), è verde. Altrimenti è rosso.
        if (currentStatus.find("NORMAL FLIGHT") != std::string::npos ||
            currentStatus.find("NOMINAL") != std::string::npos) {
            statusColor = colGreen;
        } else {
            statusColor = RED;
            isWarning = true;
        }


        int labelY = startY + 160;
        int msgY = labelY + 18; // Spostiamo il messaggio sotto l'etichetta

        // 1. Etichetta descrittiva (piccola, opaca)
        DrawText("SYSTEM INTEGRITY STATUS:", px3 + 20, labelY, 10, Fade(WHITE, 0.5f));

        // 2. Box di emergenza (compare solo se non è NORMAL FLIGHT)
        if (isWarning) {
            // Bordo rosso fisso per l'emergenza
            DrawRectangleLines(px3 + 20, msgY - 2, pW - 40, 26, RED);

            // Sfondo lampeggiante (Rosso/Trasparente) per catturare l'attenzione del pilota
            if ((int)(GetTime() * 8) % 2 == 0) {
                DrawRectangle(px3 + 20, msgY - 2, pW - 40, 26, Fade(RED, 0.4f));
                statusColor = WHITE; // Il testo diventa bianco lampeggiante per massimo contrasto
            }
        }

        // 3. Testo dello stato (Ingrandito a 16 per massima leggibilità)
        // Se è tutto ok sarà una bella scritta verde, se c'è un allarme sarà dentro il box rosso.
        DrawText(data.status_msg, px3 + 28, msgY + 3, 16, statusColor);
    // Visual Effect: Scanlines
    for(int i=0; i<m_height; i+=3) DrawLine(0, i, m_width, i, Fade(BLACK, 0.2f));

    EndDrawing();
}
