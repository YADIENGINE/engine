#include "Editor/Internals/Git/GitClient.h"
#include "Editor/Internals/Platform/Windows/CapturedProcess.h"

std::map<std::string, std::string> GitEnvironment(GitAccess access) {
    // English errors, because they are classified by their text. Nothing may wait for an answer: there is no terminal,
    // and "Unlink of file failed. Try again?" fails instead of waiting.
    std::map<std::string, std::string> environment{
        {"LC_ALL", "C"},
        {"LANGUAGE", "C"},
        {"GIT_TERMINAL_PROMPT", "0"},
        {"GIT_ASK_YESNO", "false"},
        {"GIT_EDITOR", "true"},
        {"GIT_SEQUENCE_EDITOR", "true"},
        {"GIT_MERGE_AUTOEDIT", "no"},
        {"GIT_PAGER", "cat"},
    };
    if (access == GitAccess::Background) {
        environment["GIT_OPTIONAL_LOCKS"] = "0";
        environment["GCM_INTERACTIVE"] = "never";
        environment["GIT_ASKPASS"] = "";
        environment["SSH_ASKPASS_REQUIRE"] = "never";
    }
    return environment;
}

GitResult GitClient::Run(const std::vector<std::string>& arguments, GitAccess access, const std::atomic<bool>* cancel,
                         std::chrono::milliseconds timeout) const {
    ProcessCommand command;
    command.executable = _executable;
    command.directory = _directory;
    command.environment = GitEnvironment(access);
    command.arguments = {"--no-pager", "--literal-pathspecs", "-c", "core.quotepath=off", "-c", "color.ui=false"};
    command.arguments.insert(command.arguments.end(), arguments.begin(), arguments.end());
    const auto captured = RunCapturedProcess(command, {cancel, timeout});
    GitResult result;
    result.started = captured.started;
    result.exitCode = captured.exitCode;
    result.ok = captured.started && captured.exitCode == 0 && !captured.canceled && !captured.timedOut;
    result.canceled = captured.canceled;
    result.timedOut = captured.timedOut;
    result.output = captured.output;
    result.errors = captured.started ? captured.errors : captured.startError;
    return result;
}
