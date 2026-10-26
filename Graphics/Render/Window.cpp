#include "Graphics/Render/Window.h"
#include <raylib.h>
#include "Utilities/StartupTimer.h"

Window::Window(const WindowSettings& settings)
    : _visible(settings.visible) {
    const StartupTimer timer("Window");
#ifndef __ANDROID__
    // Pencere ilk kare çizilene kadar gizli (bkz. Show): yükleme süresince boyanmamış pencere beyaz görünmez; boyut ve
    // ekran modu da gizliyken oturur.
    unsigned int flags = FLAG_WINDOW_HIDDEN;
    if (settings.resizable) flags |= FLAG_WINDOW_RESIZABLE;
    if (settings.alwaysRun) flags |= FLAG_WINDOW_ALWAYS_RUN;
    SetConfigFlags(flags);
#endif
    InitWindow(settings.width, settings.height, settings.title);
#ifndef __ANDROID__
    SetWindowMinSize(settings.minWidth, settings.minHeight);
    if (settings.maximized) MaximizeWindow();
#endif
    SetTargetFPS(settings.targetFps);
    SetExitKey(KEY_NULL); // ESC'yi Game yönetir; pencere sadece kapatma tuşuyla kapanır
}

Window::~Window() { CloseWindow(); }

void Window::Show() {
#ifndef __ANDROID__
    if (_visible && IsWindowState(FLAG_WINDOW_HIDDEN)) ClearWindowState(FLAG_WINDOW_HIDDEN);
#endif
}

bool Window::ShouldClose() const { return WindowShouldClose(); }

int Window::Width() const { return GetScreenWidth(); }

int Window::Height() const { return GetScreenHeight(); }

void Window::SetTargetFps(int fps) { SetTargetFPS(fps); }

void Window::Resize(Vec2 size) {
#ifndef __ANDROID__
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) return;
    if (size.x <= 0 || size.y <= 0) {
        if (!IsWindowMaximized()) MaximizeWindow();
        return;
    }
    if (IsWindowMaximized()) RestoreWindow();
    SetWindowSize(size.x, size.y);
    const int monitor = GetCurrentMonitor();
    const Vector2 origin = GetMonitorPosition(monitor);
    SetWindowPosition(static_cast<int>(origin.x) + (GetMonitorWidth(monitor) - size.x) / 2,
                      static_cast<int>(origin.y) + (GetMonitorHeight(monitor) - size.y) / 2);
#else
    (void)size;
#endif
}

Vec2 Window::MonitorSize() const {
    const int monitor = GetCurrentMonitor();
    return {GetMonitorWidth(monitor), GetMonitorHeight(monitor)};
}

void Window::SetBorderlessFullscreen(bool enabled) {
#ifndef __ANDROID__
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) == enabled) return;
    if (enabled) {
        _wasMaximized = IsWindowMaximized();
        if (_wasMaximized) RestoreWindow();
    }
    ToggleBorderlessWindowed();
    // raylib borderless pencereyi her zaman üstte yapar; alt-tab ile seçilen pencere arkasında kalmasın
    if (enabled && IsWindowState(FLAG_WINDOW_TOPMOST)) ClearWindowState(FLAG_WINDOW_TOPMOST);
    if (!enabled && _wasMaximized) MaximizeWindow();
#else
    (void)enabled;
#endif
}

void Window::SetVSync(bool enabled) {
#ifndef __ANDROID__
    if (IsWindowState(FLAG_VSYNC_HINT) == enabled) return;
    if (enabled)
        SetWindowState(FLAG_VSYNC_HINT);
    else
        ClearWindowState(FLAG_VSYNC_HINT);
#else
    (void)enabled;
#endif
}
