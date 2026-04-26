#include "MonitorDisplay.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <iostream>

// Costanti cromatiche HUD/MFD
#define colBack CLITERAL(Color){2, 8, 15, 255}
#define colHUD CLITERAL(Color){0, 180, 255, 255}
#define colGreen CLITERAL(Color){50, 255, 120, 255}
#define colWarning CLITERAL(Color){255, 170, 0, 255}
#define colDanger CLITERAL(Color){255, 40, 50, 255}
#define colPanel CLITERAL(Color){0, 20, 40, 180}

MonitorDisplay::MonitorDisplay(int width, int height, const std::string &title)
    : m_width(width), m_height(height) {
  InitWindow(width, height, title.c_str());
  SetTargetFPS(60);
}

MonitorDisplay::~MonitorDisplay() { CloseWindow(); }
bool MonitorDisplay::IsActive() { return !WindowShouldClose(); }

void DrawTacticalGrid(int x, int y, int w, int h, Color color) {
  int spacing = 25;
  for (int i = 0; i <= w; i += spacing)
    DrawLine(x + i, y, x + i, y + h, color);
  for (int i = 0; i <= h; i += spacing)
    DrawLine(x, y + i, x + w, y + i, color);
}

void MonitorDisplay::DrawTechFrame(int x, int y, int w, int h,
                                   const char *title) {
  DrawRectangleGradientV(x, y, w, h, colPanel, Fade(BLACK, 0.8f));
  DrawTacticalGrid(x, y, w, h, Fade(colHUD, 0.04f));
  DrawRectangleLinesEx({(float)x, (float)y, (float)w, (float)h}, 1.5f,
                       Fade(colHUD, 0.4f));

  int cl = 20, ct = 3;
  DrawRectangle(x, y, cl, ct, colHUD);
  DrawRectangle(x, y, ct, cl, colHUD);
  DrawRectangle(x + w - cl, y, cl, ct, colHUD);
  DrawRectangle(x + w - ct, y, ct, cl, colHUD);
  DrawRectangle(x, y + h - ct, cl, ct, colHUD);
  DrawRectangle(x, y + h - cl, ct, cl, colHUD);
  DrawRectangle(x + w - cl, y + h - ct, cl, ct, colHUD);
  DrawRectangle(x + w - ct, y + h - cl, ct, cl, colHUD);

  int titleWidth = MeasureText(title, 10) + 20;
  DrawRectangle(x + 15, y - 10, titleWidth, 20, colBack);
  DrawRectangleLines(x + 15, y - 10, titleWidth, 20, Fade(colHUD, 0.6f));
  DrawText(title, x + 25, y - 5, 10, colHUD);
}

void MonitorDisplay::DrawAttitudeIndicator(int x, int y, float value,
                                           const char *label) {
  int barW = 160;
  DrawText(label, x, y, 10, Fade(WHITE, 0.6f));
  DrawRectangleGradientH(x, y + 15, barW, 10, Fade(colHUD, 0.1f),
                         Fade(BLACK, 0.6f));
  DrawRectangleLines(x, y + 15, barW, 10, Fade(colHUD, 0.3f));
  DrawLineEx({(float)x + barW / 2, (float)y + 12},
             {(float)x + barW / 2, (float)y + 28}, 1.0f, Fade(WHITE, 0.5f));

  float clampedVal = std::max(-PI, std::min(value, (float)PI));
  float norm = (clampedVal + PI) / (2 * PI);
  float indicatorX = x + (norm * barW);

  DrawTriangle({indicatorX - 5, (float)y + 27}, {indicatorX + 5, (float)y + 27},
               {indicatorX, (float)y + 13}, colHUD);
  DrawText(TextFormat("%+05.2f", value), x + barW + 15, y + 14, 10, colHUD);
}

// Radar Tattico Top-Down
void MonitorDisplay::DrawTacticalRadar(int x, int y, int size,
                                       const PlaneData &data) {
  Vector2 ctr = {(float)x + size / 2, (float)y + size / 2};
  float r = size / 2.2f;
  float radarRange = 100000.0f;

  // Layer disco radar
  DrawCircleV(ctr, r, Fade(BLACK, 0.95f));
  DrawCircleLines(ctr.x, ctr.y, r, colHUD);

  for (int i = 1; i <= 3; i++) {
    float ringR = (r / 3) * i;
    DrawCircleLines(ctr.x, ctr.y, ringR,
                    Fade(colHUD, 0.6f)); // Anelli radiali di distanza
    DrawText(TextFormat("%dK", (int)(radarRange / 3000 * i)), ctr.x + 4,
             ctr.y - ringR + 4, 10, colHUD);
  }

  DrawLineV({ctr.x - r, ctr.y}, {ctr.x + r, ctr.y}, Fade(colHUD, 0.2f));
  DrawLineV({ctr.x, ctr.y - r}, {ctr.x, ctr.y + r}, Fade(colHUD, 0.2f));

  float time = GetTime();
  float sweepAngle = (float)fmod(time * 120.0f, 360.0f);
  DrawCircleSector(ctr, r, sweepAngle, sweepAngle + 45, 15,
                   Fade(colGreen, 0.1f));
  DrawCircleSector(ctr, r, sweepAngle, sweepAngle + 10, 5,
                   Fade(colGreen, 0.3f));

  float sectorSize = 60000.0f;
  float baseZ = std::floor(data.z / sectorSize) * sectorSize;

  for (int i = -1; i <= 1; i++) {
    float airportZ = baseZ + (i * sectorSize) + 30000.0f;
    float dx = 0.0f - data.x;
    float dz = airportZ - data.z;
    float distance = sqrtf(dx * dx + dz * dz);

    if (distance < radarRange) {
      float angle = data.yaw;
      float rotX = dx * cosf(angle) - dz * sinf(angle);
      float rotZ = dx * sinf(angle) + dz * cosf(angle);

      float screenX = ctr.x + (rotX / radarRange) * r;
      float screenY = ctr.y - (rotZ / radarRange) * r;

      DrawRectangleLines(screenX - 8, screenY - 8, 16, 16, colGreen);
      DrawRectangle(screenX - 2, screenY - 2, 4, 4, colWarning);
      DrawText("TGT", screenX + 12, screenY - 10, 10, colGreen);
      DrawText(TextFormat("D:%.0f", distance), screenX + 12, screenY, 10,
               colHUD);

      Vector2 dir = {ctr.x - screenX, ctr.y - screenY};
      float distToCenter = sqrtf(dir.x * dir.x + dir.y * dir.y);
      dir.x /= distToCenter;
      dir.y /= distToCenter;
      for (float step = 0; step < distToCenter - 15; step += 10) {
        DrawPixel(screenX + dir.x * step, screenY + dir.y * step, colGreen);
      }
    }
  }

  DrawTriangle({ctr.x, ctr.y - 10}, {ctr.x - 6, ctr.y + 6},
               {ctr.x + 6, ctr.y + 6}, WHITE);
  DrawCircle(ctr.x, ctr.y, 1.0f, colDanger);
}

void MonitorDisplay::DrawArtificialHorizon(int x, int y, int w, int h,
                                           float pitch, float roll) {
  Vector2 center = {(float)x + w / 2, (float)y + h / 2};
  BeginScissorMode(x, y, w, h);

  rlPushMatrix();
  rlTranslatef(center.x, center.y, 0);
  rlRotatef(roll * RAD2DEG, 0, 0, 1);

  float pOff = pitch * 200.0f;

  DrawRectangleGradientV(-w, -h - pOff, w * 2, h, Fade(BLUE, 0.6f),
                         Fade(colHUD, 0.3f));
  DrawRectangleGradientV(-w, -pOff, w * 2, h, Fade(DARKBROWN, 0.8f),
                         Fade(BLACK, 0.9f));
  DrawLineEx({(float)-w, -pOff}, {(float)w, -pOff}, 2.0f, WHITE);

  for (int i = -90; i <= 90; i += 10) {
    if (i == 0)
      continue;
    float ly = -pOff - (i * 3.5f);
    int lineW = (i % 30 == 0) ? 40 : 20;
    DrawLineEx({(float)-lineW, ly}, {(float)lineW, ly}, 2.0f, WHITE);
    DrawLineEx({(float)-lineW, ly}, {(float)-lineW, ly + (i > 0 ? 5 : -5)},
               2.0f, WHITE);
    DrawLineEx({(float)lineW, ly}, {(float)lineW, ly + (i > 0 ? 5 : -5)}, 2.0f,
               WHITE);
    DrawText(TextFormat("%d", std::abs(i)), lineW + 5, ly - 5, 10, WHITE);
  }
  rlPopMatrix();
  EndScissorMode();

  DrawLineEx({center.x - 40, center.y}, {center.x - 15, center.y}, 3.0f,
             colWarning);
  DrawLineEx({center.x + 15, center.y}, {center.x + 40, center.y}, 3.0f,
             colWarning);
  DrawLineEx({center.x - 15, center.y}, {center.x, center.y + 10}, 3.0f,
             colWarning);
  DrawLineEx({center.x, center.y + 10}, {center.x + 15, center.y}, 3.0f,
             colWarning);
  DrawCircle(center.x, center.y, 2.0f, colDanger);
}

void MonitorDisplay::DrawVerticalTape(int x, int y, int w, int h, float value,
                                      float step, Color color,
                                      bool rightAlign) {
  DrawRectangleGradientH(x, y, w, h, Fade(BLACK, 0.8f), Fade(colPanel, 0.9f));
  DrawRectangleLines(x, y, w, h, Fade(colHUD, 0.3f));

  BeginScissorMode(x, y, w, h);
  float offset = fmod(value, step);
  for (int i = -4; i <= 4; i++) {
    float v = ((int)(value / step) * step) + (i * step);
    float py = (y + h / 2) - (i * (h / 5)) + (offset * (h / 5) / step);

    DrawLineEx({(float)(rightAlign ? x + w - 8 : x), py},
               {(float)(rightAlign ? x + w : x + 8), py}, 2.0f, color);
    DrawText(TextFormat("%d", (int)v), (rightAlign ? x + 5 : x + 15), py - 5,
             10, Fade(color, 0.8f));
  }
  EndScissorMode();

  int boxY = y + h / 2 - 12;
  DrawRectangle(x - 5, boxY, w + 10, 24, BLACK);
  DrawRectangleLines(x - 5, boxY, w + 10, 24, color);

  if (rightAlign)
    DrawTriangle({(float)x - 5, (float)boxY + 12},
                 {(float)x - 10, (float)boxY + 7},
                 {(float)x - 10, (float)boxY + 17}, color);
  else
    DrawTriangle({(float)x + w + 5, (float)boxY + 12},
                 {(float)x + w + 10, (float)boxY + 7},
                 {(float)x + w + 10, (float)boxY + 17}, color);

  DrawText(TextFormat("%03d", (int)value), x + 5, boxY + 7, 10, WHITE);
}

void MonitorDisplay::DrawHeadingTape(int x, int y, int w, float yaw) {
  DrawRectangleGradientV(x, y, w, 25, Fade(BLACK, 0.9f), Fade(colPanel, 0.8f));
  DrawRectangleLines(x, y, w, 25, Fade(colHUD, 0.4f));

  float head = yaw * RAD2DEG;
  BeginScissorMode(x, y, w, 25);

  int startTick = (((int)head - 90) / 10) * 10;
  int endTick = startTick + 180;

  for (int i = startTick; i <= endTick; i += 10) {
    float px = (x + w / 2) + (i - head) * 5;
    if (px > x && px < x + w) {
      DrawLineEx({px, (float)y + 15}, {px, (float)y + 25}, 2.0f, colHUD);
      if (i % 30 == 0) {
        int deg = i % 360;
        if (deg < 0)
          deg += 360;
        const char *lbl = (deg == 0)     ? "N"
                          : (deg == 90)  ? "E"
                          : (deg == 180) ? "S"
                          : (deg == 270) ? "W"
                                         : TextFormat("%03d", deg);
        DrawText(lbl, px - MeasureText(lbl, 10) / 2, y + 4, 10, WHITE);
      }
    }
  }
  EndScissorMode();
  DrawTriangle({(float)x + w / 2, (float)y + 25},
               {(float)x + w / 2 - 6, (float)y + 35},
               {(float)x + w / 2 + 6, (float)y + 35}, colWarning);
}

void MonitorDisplay::Draw(const PlaneData &data) {
  BeginDrawing();
  ClearBackground(colBack);

  int m = 25;
  int pW = (m_width - m * 4) / 3;
  int pH = m_height - m * 2;

  DrawTechFrame(m, m, pW, pH, "TACTICAL AIRSPACE MONITOR");
  DrawTacticalRadar(m + 15, m + 30, pW - 30, data);

  float sRatio = std::min(std::max(data.speed / 300.0f, 0.0f), 1.0f);
  DrawRectangle(m + 20, m + pH - 50, pW - 40, 14, Fade(BLACK, 0.8f));
  DrawRectangleLines(m + 20, m + pH - 50, pW - 40, 14, Fade(colHUD, 0.4f));
  DrawRectangleGradientH(m + 20, m + pH - 50, (pW - 40) * sRatio, 14,
                         Fade(colHUD, 0.8f),
                         (sRatio > 0.85f ? colDanger : colHUD));
  DrawText(TextFormat("THRUST: %03.0f / 300 KPH", data.speed), m + 20,
           m + pH - 30, 10, Fade(WHITE, 0.7f));
  if (sRatio > 0.85f)
    DrawText("A/B", m + pW - 40, m + pH - 30, 10, colDanger);

  int px2 = m * 2 + pW;
  DrawTechFrame(px2, m, pW, pH, "PRIMARY FLIGHT DISPLAY");
  DrawHeadingTape(px2 + 20, m + 30, pW - 40, data.yaw);
  DrawArtificialHorizon(px2 + 45, m + 70, pW - 90, pH - 100, data.pitch,
                        data.roll);
  DrawVerticalTape(px2 + 5, m + 70, 35, pH - 100, data.speed, 20, colGreen,
                   false);
  DrawVerticalTape(px2 + pW - 40, m + 70, 35, pH - 100, data.altitude, 500,
                   colHUD, true);

  int px3 = m * 3 + pW * 2;
  DrawTechFrame(px3, m, pW, pH, "FLIGHT KINEMATICS");
  int startY = m + 40;
  DrawAttitudeIndicator(px3 + 20, startY, data.roll, "ROLL AXIS (RAD)");
  DrawAttitudeIndicator(px3 + 20, startY + 60, data.pitch,
                        "PITCH AXIS WITH RAD (RAD)");

  DrawRectangle(px3 + 20, startY + 140, pW - 40, 1, Fade(colHUD, 0.3f));

  // =========================================================================
  // EICAS SPLIT: PROPULSIONE & WARNINGS
  // =========================================================================
  int eicasY = startY + 120;

  // ----- SEZIONE 1: PROPULSION SYSTEM STATUS -----
  DrawText("[ PROPULSION SYSTEM STATUS ]", px3 + 20, eicasY, 10, colHUD);

  int propBoxY = eicasY + 15;
  DrawRectangle(px3 + 20, propBoxY, pW - 40, 30, Fade(BLACK, 0.8f));

  std::string engineStatus = "ENGINES RUNNING NORMAL";
  Color engineColor = colGreen;

  if (!data.system_active) {
    engineStatus = "ENGINES SHUT DOWN";
    engineColor = colDanger;
  } else if (data.speed < 30.0f) {
    engineStatus = "ENGINES SPOOLING UP...";
    engineColor = colWarning;
  }

  DrawRectangleLines(px3 + 20, propBoxY, pW - 40, 30, Fade(engineColor, 0.3f));
  int eTxtW = MeasureText(engineStatus.c_str(), 16);
  DrawText(engineStatus.c_str(), px3 + 20 + ((pW - 40) / 2) - (eTxtW / 2),
           propBoxY + 7, 16, engineColor);

  // ----- SEZIONE 2: WARNINGS & ALARMS -----
  int warnStartY = propBoxY + 45;
  DrawText("[ WARNINGS & ALARMS ]", px3 + 20, warnStartY, 10, colHUD);

  int warnBoxY = warnStartY + 15;
  DrawRectangle(px3 + 20, warnBoxY, pW - 40, 30, Fade(BLACK, 0.8f));

  std::string warnMsg = "NO ACTIVE WARNINGS";
  Color warnColor = Fade(colGreen, 0.5f);
  bool isWarningAlert = false;
  std::string rawStatus(data.status_msg);

  // Escludi i messaggi di engine shutting giù dalla sezione warnings (già
  // coperti sopra)
  if (rawStatus.find("SHUT DOWN") == std::string::npos &&
      rawStatus != "NORMAL FLIGHT") {
    warnMsg = rawStatus;
    isWarningAlert = true;
    if (rawStatus.find("DANGER") != std::string::npos ||
        rawStatus.find("PULL UP") != std::string::npos ||
        rawStatus.find("STALL") != std::string::npos) {
      warnColor = colDanger;
    } else {
      warnColor = colWarning; // Landing, gear, bank
    }
  }

  if (isWarningAlert) {
    float pulse = (std::sin(GetTime() * 15.0f) + 1.0f) * 0.5f;
    DrawRectangleGradientV(px3 + 20, warnBoxY, pW - 40, 30,
                           Fade(warnColor, 0.2f + pulse * 0.4f),
                           Fade(BLACK, 0.9f));
    DrawRectangleLinesEx(
        {(float)px3 + 20, (float)warnBoxY, (float)pW - 40, 30.0f}, 2.0f,
        Fade(warnColor, 0.6f + pulse * 0.4f));
  } else {
    DrawRectangleLines(px3 + 20, warnBoxY, pW - 40, 30, Fade(warnColor, 0.3f));
  }

  int wTxtW = MeasureText(warnMsg.c_str(), 14);
  DrawText(warnMsg.c_str(), px3 + 20 + ((pW - 40) / 2) - (wTxtW / 2),
           warnBoxY + 8, 14, isWarningAlert ? WHITE : warnColor);

  // Maschera lente e scanlines per effetto CRT
  DrawCircleGradient({(float)m_width / 2, (float)m_height / 2}, m_width * 0.7f,
                     BLANK, Fade(BLACK, 0.6f));
  for (int i = 0; i < m_height; i += 3)
    DrawLine(0, i, m_width, i, Fade(BLACK, 0.25f));

  EndDrawing();
}
