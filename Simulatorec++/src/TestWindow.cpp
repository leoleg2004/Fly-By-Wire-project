#include "raylib.h"
//fatta per vedere se funzionava raylib
int main() {

    InitWindow(800, 450, "TEST RAYLIB - LEONARDO");


    SetTargetFPS(60);


    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(RAYWHITE); // Sfondo Bianco
            DrawText("SE LEGGI QUESTO, RAYLIB FUNZIONA!", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
    }

    // Chiudi tutto
    CloseWindow();
    return 0;
}
