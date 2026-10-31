#pragma once
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

struct PseudoConsoleOptions {
    std::wstring commandLine;
    std::filesystem::path directory;
    std::vector<std::pair<std::wstring, std::wstring>> environment; // editörün ortamına eklenir / üstüne yazar
    int rows = 24;
    int columns = 80;
};

// Windows sahte konsolunda (ConPTY) çalışan bir süreç. Çıktıyı okuyucu thread kuyruğa alır, TakeOutput UI
// thread'inden boşaltır. Süreç ve başlattığı her şey job object'tedir: nesne yıkılınca hepsi kapanır.
class PseudoConsole {
    struct Token {}; // kurucuyu sadece Start çağırabilir

public:
    // Başarısızsa nesne kurulmaz; hata metni error'a yazılır.
    [[nodiscard]] static std::unique_ptr<PseudoConsole> Start(const PseudoConsoleOptions& options, std::string& error);
    ~PseudoConsole();
    explicit PseudoConsole(Token) {}

    void Write(std::string_view bytes);
    void Resize(int rows, int columns);
    [[nodiscard]] std::string TakeOutput();
    // Süreç çıktıysa çıkış kodu.
    [[nodiscard]] std::optional<int> ExitCode() const;

private:
    void ReadLoop();

    struct HandleCloser {
        void operator()(void* handle) const;
    };
    struct ConsoleCloser {
        void operator()(void* console) const;
    };
    using Handle = std::unique_ptr<void, HandleCloser>;

    // Kapanış sırası ~PseudoConsole'da açıkça yönetilir: job sonlanır → input kapanır → ConPTY kapanır (okuyucu
    // boşaltmaya devam eder) → okuyucu join edilir → kalan handle'lar.
    Handle _job;
    Handle _process;
    Handle _output; // programın çıktısını okuyan uç
    Handle _input;  // programın girişine yazan uç
    std::unique_ptr<void, ConsoleCloser> _console;
    std::thread _reader;
    mutable std::mutex _mutex;
    std::string _pending;

public:
    PseudoConsole(const PseudoConsole&) = delete;
    PseudoConsole& operator=(const PseudoConsole&) = delete;
};
