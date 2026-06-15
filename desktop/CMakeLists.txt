cmake_minimum_required(VERSION 3.28)

project(SmTool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(SMTOOL_LOGFAULT_SOURCE_DIR "/home/jgaa/src/logfault" CACHE PATH "Use a local source dir for logfault")
if(SMTOOL_LOGFAULT_SOURCE_DIR AND EXISTS "${SMTOOL_LOGFAULT_SOURCE_DIR}/CMakeLists.txt")
    message(STATUS "SmTool: using local logfault from ${SMTOOL_LOGFAULT_SOURCE_DIR}")
    FetchContent_Declare(logfault SOURCE_DIR "${SMTOOL_LOGFAULT_SOURCE_DIR}")
else()
    FetchContent_Declare(logfault
        GIT_REPOSITORY https://github.com/jgaa/logfault.git
        GIT_TAG v0.8.5
    )
endif()
FetchContent_MakeAvailable(logfault)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Quick Qml QuickControls2 Sql Test Widgets)

qt_standard_project_setup(REQUIRES 6.5)

set(SMTOOL_SOURCES
    src/main.cpp
    src/app/appcontroller.cpp
    src/app/appinfo.cpp
    src/app/appsettings.cpp
    src/app/dashboardservice.cpp
    src/app/loggingcontroller.cpp
    src/data/calendarrepository.cpp
    src/data/contentrepository.cpp
    src/data/database.cpp
    src/data/goalsrepository.cpp
    src/data/lookupsrepository.cpp
    src/data/mediarepository.cpp
    src/data/publicationrepository.cpp
    src/data/seriesrepository.cpp
    src/models/calendarentrymodel.cpp
    src/models/contentlistmodel.cpp
    src/models/contentstatuslistmodel.cpp
    src/models/dashboardrowmodel.cpp
    src/models/goalslistmodel.cpp
    src/models/lookuplistmodel.cpp
    src/models/serieslistmodel.cpp
)

set(SMTOOL_HEADERS
    src/app/appcontroller.h
    src/app/appinfo.h
    src/app/appsettings.h
    src/app/dashboardservice.h
    src/app/loggingcontroller.h
    src/data/calendarrepository.h
    src/data/contentrepository.h
    src/data/database.h
    src/data/goalsrepository.h
    src/data/lookupsrepository.h
    src/data/mediarepository.h
    src/data/publicationrepository.h
    src/data/seriesrepository.h
    src/domain/constants.h
    src/domain/entities.h
    src/models/calendarentrymodel.h
    src/models/contentlistmodel.h
    src/models/contentstatuslistmodel.h
    src/models/dashboardrowmodel.h
    src/models/goalslistmodel.h
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
        src/ui/qml/AboutDialog.qml
        src/ui/qml/DashboardView.qml
        src/ui/qml/Main.qml
        src/ui/qml/LogSettings.qml
        src/ui/qml/SettingsDialog.qml
        src/ui/qml/SystemSettings.qml
        src/ui/qml/components/BoardColumn.qml
        src/ui/qml/components/ContentSummaryCard.qml
        src/ui/qml/components/DateSelector.qml
        src/ui/qml/components/MediaAttachmentsEditor.qml
        src/ui/qml/components/MediaItemDialog.qml
        src/ui/qml/components/SectionCard.qml
)

qt_add_resources(SmToolApp smtool_resources
    PREFIX /
    FILES
        resources/migrations/001_initial.sql
        resources/migrations/002_goals.sql
        resources/migrations/003_series_positions.sql
)

target_include_directories(SmToolApp PRIVATE src)
target_compile_definitions(SmToolApp PRIVATE SMTOOL_VERSION="${PROJECT_VERSION}")

target_link_libraries(SmToolApp
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::Sql
        Qt6::Widgets
        logfault::logfault
)

enable_testing()

qt_add_executable(SmToolTests
    tests/test_database.cpp
    src/app/appcontroller.cpp
    src/app/appinfo.cpp
    src/app/dashboardservice.cpp
    src/data/contentrepository.cpp
    src/data/calendarrepository.cpp
    src/data/database.cpp
    src/data/goalsrepository.cpp
    src/data/lookupsrepository.cpp
    src/data/mediarepository.cpp
    src/data/publicationrepository.cpp
    src/data/seriesrepository.cpp
    src/models/calendarentrymodel.cpp
    src/models/contentlistmodel.cpp
    src/models/contentstatuslistmodel.cpp
    src/models/dashboardrowmodel.cpp
    src/models/goalslistmodel.cpp
    src/models/lookuplistmodel.cpp
    src/models/serieslistmodel.cpp
    src/app/appsettings.cpp
    src/app/loggingcontroller.cpp
)

target_include_directories(SmToolTests PRIVATE src)
target_compile_definitions(SmToolTests PRIVATE SMTOOL_VERSION="${PROJECT_VERSION}")

qt_add_resources(SmToolTests smtool_test_resources
    PREFIX /
    FILES
        resources/migrations/001_initial.sql
        resources/migrations/002_goals.sql
        resources/migrations/003_series_positions.sql
)

target_link_libraries(SmToolTests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Sql
        Qt6::Test
        Qt6::Widgets
        logfault::logfault
)

add_test(NAME SmToolTests COMMAND SmToolTests)
