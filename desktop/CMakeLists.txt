cmake_minimum_required(VERSION 3.28)

project(SmTool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 Sql Test)

qt_standard_project_setup(REQUIRES 6.5)

set(SMTOOL_SOURCES
    src/main.cpp
    src/app/appcontroller.cpp
    src/data/contentrepository.cpp
    src/data/dashboardrepository.cpp
    src/data/database.cpp
    src/data/lookupsrepository.cpp
    src/data/seriesrepository.cpp
    src/models/calendarentrymodel.cpp
    src/models/contentlistmodel.cpp
    src/models/dashboardmetricmodel.cpp
    src/models/lookuplistmodel.cpp
    src/models/serieslistmodel.cpp
)

set(SMTOOL_HEADERS
    src/app/appcontroller.h
    src/data/contentrepository.h
    src/data/dashboardrepository.h
    src/data/database.h
    src/data/lookupsrepository.h
    src/data/seriesrepository.h
    src/domain/constants.h
    src/domain/entities.h
    src/models/calendarentrymodel.h
    src/models/contentlistmodel.h
    src/models/dashboardmetricmodel.h
    src/models/lookuplistmodel.h
    src/models/serieslistmodel.h
)

qt_add_executable(SmToolApp
    ${SMTOOL_SOURCES}
    ${SMTOOL_HEADERS}
)

set_target_properties(SmToolApp PROPERTIES
    OUTPUT_NAME SmTool
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

qt_add_qml_module(SmToolApp
    URI SmTool
    VERSION 1.0
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/components/BoardColumn.qml
        src/ui/qml/components/SectionCard.qml
)

qt_add_resources(SmToolApp smtool_resources
    PREFIX /
    FILES
        resources/migrations/001_initial.sql
)

target_include_directories(SmToolApp PRIVATE src)

target_link_libraries(SmToolApp
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::Sql
)

enable_testing()

qt_add_executable(SmToolTests
    tests/test_database.cpp
    src/data/contentrepository.cpp
    src/data/dashboardrepository.cpp
    src/data/database.cpp
    src/data/lookupsrepository.cpp
    src/data/seriesrepository.cpp
    src/models/calendarentrymodel.cpp
    src/models/contentlistmodel.cpp
    src/models/dashboardmetricmodel.cpp
    src/models/lookuplistmodel.cpp
    src/models/serieslistmodel.cpp
)

target_include_directories(SmToolTests PRIVATE src)

qt_add_resources(SmToolTests smtool_test_resources
    PREFIX /
    FILES
        resources/migrations/001_initial.sql
)

target_link_libraries(SmToolTests
    PRIVATE
        Qt6::Core
        Qt6::Sql
        Qt6::Test
)

add_test(NAME SmToolTests COMMAND SmToolTests)
