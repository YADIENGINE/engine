#pragma once

class Painter;
struct LoadingScreenFrame;

// One function per choice of ProjectSettings::loadingScreen; each lives in its own .cpp beside this header.
void DrawDefaultLoadingScreen(Painter& paint, const LoadingScreenFrame& frame);
void DrawHorrorLoadingScreen(Painter& paint, const LoadingScreenFrame& frame);
void DrawRpgLoadingScreen(Painter& paint, const LoadingScreenFrame& frame);
void DrawSciFiLoadingScreen(Painter& paint, const LoadingScreenFrame& frame);
void DrawArcadeLoadingScreen(Painter& paint, const LoadingScreenFrame& frame);
void DrawCozyLoadingScreen(Painter& paint, const LoadingScreenFrame& frame);
