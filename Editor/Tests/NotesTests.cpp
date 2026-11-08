#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include "Editor/Internals/Notes/NoteDocument.h"
#include "Editor/Internals/Notes/NoteFiles.h"
#include "Tests/Support/TempDirectory.h"
#ifdef _WIN32
#include <Windows.h>
#endif

TEST_CASE("Editor.Notes.AutosaveWaitsForLastEditAndFlushes", "[engine-editor][notes]") {
    NoteDocument note;
    note.Open("original");
    note.Edit("first", 10);
    CHECK_FALSE(note.WriteDue(10.9));
    note.Edit("last", 10.9);
    CHECK_FALSE(note.WriteDue(11));
    REQUIRE(note.WriteDue(11.9));
    CHECK(*note.WriteDue(11.9) == "last");
    // Ctrl+S, focus loss, switching and shutdown all use the same explicit flush.
    CHECK(note.WriteDue(10.9, true) == "last");
    note.Written("last");
    CHECK_FALSE(note.Dirty());
    note.Observe("last");
    CHECK_FALSE(note.Conflict());
    CHECK_FALSE(note.WriteDue(20));
}
TEST_CASE("Editor.Notes.CleanReloadAndContentComparison", "[engine-editor][notes]") {
    NoteDocument note;
    note.Open("old");
    note.Observe("new");
    CHECK(note.Text() == "new");
    CHECK_FALSE(note.Dirty());
    note.Edit("mine", 0);
    note.Observe("new");
    CHECK_FALSE(note.Conflict()); // Timestamp changes alone cannot cause a conflict.
    CHECK(note.WriteDue(2) == "mine");
}
TEST_CASE("Editor.Notes.DirtyConflictChoicesStopAutosave", "[engine-editor][notes]") {
    NoteDocument note;
    note.Open("old");
    note.Edit("mine", 0);
    note.Observe("agent");
    REQUIRE(note.Conflict());
    CHECK(note.Text() == "mine");
    CHECK_FALSE(note.WriteDue(10));
    CHECK_FALSE(note.WriteDue(10, true));
    note.Observe("old"); // A conflict stays paused until an explicit choice, even if disk changes back.
    CHECK(note.Conflict());
    CHECK_FALSE(note.WriteDue(20));
    note.Observe("agent");
    SECTION("keep mine") {
        note.KeepMine();
        CHECK_FALSE(note.Conflict());
        CHECK(note.WriteDue(10, true) == "mine");
        note.Written("mine");
        note.Observe("mine");
        CHECK_FALSE(note.Conflict());
        CHECK_FALSE(note.Dirty());
    }
    SECTION("use the file") {
        note.UseFile();
        CHECK(note.Text() == "agent");
        CHECK_FALSE(note.Conflict());
        CHECK_FALSE(note.Dirty());
    }
}
TEST_CASE("Editor.Notes.DeletedFileNeedsExplicitRecreate", "[engine-editor][notes]") {
    NoteDocument note;
    note.Open("kept");
    note.Observe(std::nullopt);
    CHECK(note.Deleted());
    CHECK(note.Text() == "kept");
    CHECK_FALSE(note.WriteDue(20, true));
    note.Edit("kept edits", 0);
    CHECK_FALSE(note.WriteDue(20));
    note.Recreate();
    CHECK(note.WriteDue(20, true) == "kept edits");
    note.Written("kept edits");
    CHECK_FALSE(note.Deleted());
}
TEST_CASE("Editor.Notes.UTF8BomAndLineEndingsSurviveEditing", "[engine-editor][notes]") {
    NoteDocument note;
    note.Open("\xEF\xBB\xBF# Caf\xC3\xA9\r\nPlan\r\n");
    CHECK(note.Text() == "# Caf\xC3\xA9\nPlan\n");
    note.Edit("# Caf\xC3\xA9\nReport\nNew\n", 0);
    CHECK(note.WriteDue(1) == "\xEF\xBB\xBF# Caf\xC3\xA9\r\nReport\r\nNew\r\n");
    note.Open("one\r\ntwo\nthree\r");
    note.Edit("one\ntwo changed\nthree\n", 0);
    CHECK(note.WriteDue(1) == "one\r\ntwo changed\nthree\r");
}
TEST_CASE("Editor.Notes.FailedWriteWaitsBeforeRetry", "[engine-editor][notes]") {
    NoteDocument note;
    note.Open("old");
    note.Edit("mine", 0);
    REQUIRE(note.WriteDue(2));
    note.WriteFailed(2);
    CHECK_FALSE(note.WriteDue(3));
    CHECK(note.WriteDue(3, true) == "mine"); // Ctrl+S and leaving the panel still try at once.
    CHECK(note.WriteDue(2 + NoteDocument::RetrySeconds) == "mine");
}
TEST_CASE("Editor.Notes.UnsavedBytesExistOnlyWhileEditsAreUnwritten", "[engine-editor][notes]") {
    NoteDocument note;
    note.Open("old\r\n");
    CHECK_FALSE(note.UnsavedBytes());
    note.Edit("mine\n", 0);
    note.Observe("agent"); // A conflict blocks the write but not the recovery.
    REQUIRE(note.Conflict());
    CHECK(note.UnsavedBytes() == "mine\r\n");
    note.UseFile();
    CHECK_FALSE(note.UnsavedBytes());
}
TEST_CASE("Editor.Notes.LargeFilesAreReadOnly", "[engine-editor][notes]") {
    NoteDocument note;
    const std::string large(NoteDocument::MaximumEditableBytes + 1, 'x');
    note.Open(large);
    CHECK(note.ReadOnly());
    note.Edit("edited", 0);
    CHECK(note.Text() == large);
    CHECK_FALSE(note.WriteDue(20, true));
    note.Observe(std::nullopt);
    CHECK_FALSE(note.WriteDue(20, true));
    note.Recreate();
    CHECK(note.WriteDue(20, true) == large); // Recreating unchanged bytes is an explicit action, not editing.
}
TEST_CASE("Editor.Notes.ListingIsRestrictedToMarkdownBelowNotes", "[engine-editor][notes]") {
    TempDirectory temp;
    NoteFiles files(temp.Path());
    auto root = files.Scan();
    CHECK(root.name == "Notes");
    CHECK(root.children.empty());
    CHECK_FALSE(std::filesystem::exists(files.Root()));
    std::ofstream(temp.Path() / "Root.md") << "outside";
    std::filesystem::create_directories(temp.Path() / "Game/Assets");
    std::ofstream(temp.Path() / "Game/Assets/Other.md") << "outside";
    const auto folder = files.Create(files.Root(), "Reports", true);
    const auto note = files.Create(folder, "Plan", false);
    std::ofstream(folder / "Other.txt") << "ignored";
    root = files.Scan();
    REQUIRE(root.children.size() == 1);
    REQUIRE(root.children[0].children.size() == 1);
    CHECK(root.children[0].children[0].path == note);
    CHECK(root.children[0].children[0].name == "Plan.md");
    const auto library = ScanAssetLibrary(temp.Path() / "Engine", temp.Path() / "Game/Assets");
    REQUIRE(library.size() == 2);
    REQUIRE(library[1].children.size() == 1);
    CHECK(library[1].children[0].name == "Graphics"); // No Markdown in Game, Engine or the project root.
    CHECK_FALSE(files.Contains(temp.Path() / "Root.md"));
    CHECK_FALSE(files.Contains(files.Root() / "../Escape.md"));
}
TEST_CASE("Editor.Notes.NamesRejectEscapesAndNeverOverwrite", "[engine-editor][notes]") {
    TempDirectory temp;
    NoteFiles files(temp.Path());
    for (const auto* name :
         {"../escape", "a/b", "a\\b", "..", ".", "C:escape", "a?b", "a|b", "CON", "NUL.md", "COM1", ""}) {
        CHECK_FALSE(files.NameProblem(files.Root(), name, false).empty());
        CHECK_THROWS(files.Create(files.Root(), name, false));
    }
    const auto note = files.Create(files.Root(), "Plan.md", false);
    files.Write(note, "original");
    CHECK_FALSE(files.NameProblem(files.Root(), "Plan", false).empty());
    CHECK_FALSE(files.NameProblem(files.Root(), "plan.md", false).empty());
    CHECK_THROWS(files.Create(files.Root(), "Plan", false));
    const auto other = files.Create(files.Root(), "Other", false);
    CHECK_THROWS(files.Rename(other, "Plan"));
    CHECK(files.Read(note) == "original");
    const auto renamed = files.Rename(note, "Handoff");
    CHECK(files.Read(renamed) == "original");
    CHECK_FALSE(files.Read(note));
    CHECK_THROWS(files.Write(temp.Path() / "Escape.md", "bad"));
}
TEST_CASE("Editor.Notes.RenameCanChangeOnlyCaseAndUpperCaseExtensionsCount", "[engine-editor][notes]") {
    TempDirectory temp;
    NoteFiles files(temp.Path());
    const auto note = files.Create(files.Root(), "plan", false);
    CHECK(files.NameProblem(files.Root(), "Plan", false, note).empty());
    const auto renamed = files.Rename(note, "Plan");
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(files.Root()))
        names.push_back(entry.path().filename().string());
    CHECK(names == std::vector<std::string>{"Plan.md"});
    std::ofstream(files.Root() / "UPPER.MD") << "listed";
    const auto root = files.Scan();
    REQUIRE(root.children.size() == 2);
    CHECK(root.children[0].name == "Plan.md");
    CHECK(root.children[1].name == "UPPER.MD");
    CHECK(files.Read(files.Root() / "UPPER.MD") == "listed");
    CHECK(files.Rename(files.Root() / "UPPER.MD", "Lower.MD").filename() == "Lower.MD");
    CHECK(files.Read(renamed) == "");
}
TEST_CASE("Editor.Notes.InRootComparesPathsOnly", "[engine-editor][notes]") {
    TempDirectory temp;
    NoteFiles files(temp.Path());
    CHECK(files.InRoot(files.Root() / "Reports/Plan.md"));
    CHECK(files.InRoot(files.Root(), true));
    CHECK_FALSE(files.InRoot(files.Root()));
    CHECK_FALSE(files.InRoot(temp.Path() / "Root.md"));
    CHECK_FALSE(files.InRoot(files.Root() / "../Escape.md"));
}
TEST_CASE("Editor.Notes.RecoveryNeverOverwrites", "[engine-editor][notes]") {
    TempDirectory temp;
    NoteFiles files(temp.Path());
    const auto note = files.Create(files.Root(), "Plan", false);
    files.Write(note, "disk");
    const auto first = files.SaveRecovery(note, "one");
    const auto second = files.SaveRecovery(note, "two");
    CHECK(first.filename() == "Plan (recovered).md");
    CHECK(second.filename() == "Plan (recovered 2).md");
    CHECK(files.Read(note) == "disk");
    CHECK(files.Read(first) == "one");
    CHECK(files.Read(second) == "two");
    // A note whose folder was deleted still has somewhere to go.
    const auto gone = files.Root() / "Gone/Report.md";
    CHECK(files.SaveRecovery(gone, "kept").filename() == "Report (recovered).md");
}
#ifdef _WIN32
// Hidden: every run leaves its note in the real Recycle Bin. Run it by tag after changing RecycleNote.
TEST_CASE("Editor.Notes.RecycleMovesTheNoteAway", "[.][notes-recycle]") {
    TempDirectory temp;
    NoteFiles files(temp.Path());
    const auto note = files.Create(files.Root(), "RecycleProbe", false);
    files.Write(note, "probe");
    files.Recycle(note);
    CHECK_FALSE(std::filesystem::exists(note));
    CHECK_THROWS(files.Recycle(temp.Path() / "Outside.md"));
}
#endif
TEST_CASE("Editor.Notes.AtomicFailureRetainsOriginal", "[engine-editor][notes]") {
    TempDirectory temp;
    NoteFiles files(temp.Path());
    const auto path = files.Create(files.Root(), "Plan", false);
    files.Write(path, "original");
    CHECK_THROWS(files.Write(path, "overwrite", false));
    CHECK(files.Read(path) == "original");
#ifdef _WIN32
    // Deny delete-sharing so replacement fails after writing/flushing the temporary file.
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    REQUIRE(handle != INVALID_HANDLE_VALUE);
    CHECK_THROWS(files.Write(path, "replacement"));
    CloseHandle(handle);
    CHECK(files.Read(path) == "original");
#endif
    for (const auto& entry : std::filesystem::directory_iterator(files.Root()))
        CHECK(entry.path() == path);
    files.Write(path, "replacement");
    CHECK(files.Read(path) == "replacement");
}
TEST_CASE("Editor.Notes.TemplateContainsPlainReadme", "[engine-editor][notes]") {
    const auto source = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    CHECK(std::filesystem::is_regular_file(source / "Tools/ProjectTemplate/Notes/README.md"));
}
