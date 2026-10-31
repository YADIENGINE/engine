engine_tests(EngineTerminalTests SOURCES TerminalTests.cpp LINKS EngineTerminal EngineVterm)
if(TARGET EngineTerminalUi)
    # ImGui frames only: no native window or shell startup.
    engine_tests(EngineTerminalUiTests SOURCES TerminalUiTests.cpp LINKS EngineTerminalUi)
endif()
