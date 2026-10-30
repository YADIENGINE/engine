#include "Graphics/Render/Internals/Window.h"
#include <raylib.h>
#include "Runtime/Internals/StartupTimer.h"

Window::Window(const WindowSettings& settings)
    : _visible(settings.visible) {
    const StartupTimer timer("Window");
    // Pencere ilk kare çizilene kadar gizli (bkz. Show): yükleme süresince boyanmamış pencere beyaz görünmez; boyut ve
    // ekran modu da gizliyken oturur.
    unsigned int flags = FLAG_WINDOW_HIDDEN;
    if (settings.resizable) flags |= FLAG_WINDOW_RESIZABLE;
    if (settings.alwaysRun) flags |= FLAG_WINDOW_ALWAYS_RUN;
    SetConfigFlags(flags);
    InitWindow(settings.width, settings.height, settings.title);
    SetWindowMinSize(settings.minWidth, settings.minHeight);
    if (settings.maximized) MaximizeWindow();
    SetTargetFPS(settings.targetFps);
    SetExitKey(KEY_NULL); // ESC'yi Game yönetir; pencere sadece kapatma tuşuyla kapanır
}

Window::~Window() { CloseWindow(); }

void Window::Show() {
    if (_visible && IsWindowState(FLAG_WINDOW_HIDDEN)) ClearWindowState(FLAG_WINDOW_HIDDEN);
}

bool Window::ShouldClose() const { return WindowShouldClose(); }

int Window::Width() const { return GetScreenWidth(); }

int Window::Height() const { return GetScreenHeight(); }

void Window::SetTargetFps(int fps) { SetTargetFPS(fps); }

void Window::Resize(Vec2 size) {
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
}

Vec2 Window::MonitorSize() const {
    const int monitor = GetCurrentMonitor();
    return {GetMonitorWidth(monitor), GetMonitorHeight(monitor)};
}

void Window::SetBorderlessFullscreen(bool enabled) {
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) == enabled) return;
    if (enabled) {
        _wasMaximized = IsWindowMaximized();
        if (_wasMaximized) RestoreWindow();
    }
    ToggleBorderlessWindowed();
    // raylib borderless pencereyi her zaman üstte yapar; alt-tab ile seçilen pencere arkasında kalmasın
    if (enabled && IsWindowState(FLAG_WINDOW_TOPMOST)) ClearWindowState(FLAG_WINDOW_TOPMOST);
    if (!enabled && _wasMaximized) MaximizeWindow();
}

void Window::SetVSync(bool enabled) {
    if (IsWindowState(FLAG_VSYNC_HINT) == enabled) return;
    if (enabled)
        SetWindowState(FLAG_VSYNC_HINT);
    else
        ClearWindowState(FLAG_VSYNC_HINT);
}
