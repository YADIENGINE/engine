engine_tests(EnginePersistenceTests
    SOURCES EncryptionTests.cpp PersistenceTests.cpp RecoveryTests.cpp StorageTests.cpp
    LINKS EnginePersistence EngineStorage
)
