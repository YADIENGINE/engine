engine_tests(EnginePopupTests SOURCES PopupTests.cpp LINKS EnginePopup)
target_compile_definitions(EnginePopupTests PRIVATE
    ENGINE_TEST_FONT="${ENGINE_THIRD_PARTY_DIR}/NotoSans/NotoSans-Regular.ttf")
