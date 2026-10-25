cmake_minimum_required(VERSION 3.21)

project(HyperThemes VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)

qt_standard_project_setup()

file(GLOB_RECURSE HT_QML_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS qml/*.qml)
file(GLOB HT_TEMPLATES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS templates/*)
file(GLOB_RECURSE HT_CATALOG RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS catalog/*.json)
file(GLOB HT_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS data/icons/*.svg)

qt_add_executable(hyperthemes
    src/main.cpp
    src/applier.cpp
    src/applier.h
    src/catalog.cpp
    src/catalog.h
    src/cli.cpp
    src/cli.h
    src/layoutapplier.cpp
    src/layoutapplier.h
    src/localsource.cpp
    src/localsource.h
    src/palette.cpp
    src/palette.h
    src/palettegenerator.cpp
    src/palettegenerator.h
    src/paths.cpp
    src/paths.h
    src/remoteinstaller.cpp
    src/remoteinstaller.h
    src/shellbuilder.cpp
    src/shellbuilder.h
    src/templaterenderer.cpp
    src/templaterenderer.h
    src/theme.cpp
    src/theme.h
    src/thememodel.cpp
    src/thememodel.h
)

target_link_libraries(hyperthemes PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

qt_add_resources(hyperthemes "hyperthemes-resources"
    PREFIX "/"
    FILES
        ${HT_QML_FILES}
        ${HT_TEMPLATES}
        ${HT_CATALOG}
        ${HT_ICONS}
        data/hyperthemes.svg
)

install(TARGETS hyperthemes RUNTIME DESTINATION bin)
install(FILES data/hyperthemes.desktop DESTINATION share/applications)
install(FILES data/hyperthemes.svg DESTINATION share/icons/hicolor/scalable/apps)

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    qt_add_executable(test_core
        tests/test_core.cpp
        src/applier.cpp
        src/applier.h
        src/catalog.cpp
        src/catalog.h
        src/cli.cpp
        src/cli.h
        src/layoutapplier.cpp
        src/layoutapplier.h
        src/palette.cpp
        src/palette.h
        src/palettegenerator.cpp
        src/palettegenerator.h
        src/paths.cpp
        src/paths.h
        src/remoteinstaller.cpp
        src/remoteinstaller.h
        src/shellbuilder.cpp
        src/shellbuilder.h
        src/templaterenderer.cpp
        src/templaterenderer.h
        src/theme.cpp
        src/theme.h
    )

    target_include_directories(test_core PRIVATE src)
    target_link_libraries(test_core PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Test
    )

    qt_add_resources(test_core "test-resources"
        PREFIX "/"
        FILES
            ${HT_TEMPLATES}
            ${HT_CATALOG}
    )

    add_test(NAME core COMMAND test_core)
    set_tests_properties(core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(test_qml
        tests/test_qml.cpp
        src/applier.cpp
        src/applier.h
        src/catalog.cpp
        src/catalog.h
        src/layoutapplier.cpp
        src/layoutapplier.h
        src/localsource.cpp
        src/localsource.h
        src/palette.cpp
        src/palette.h
        src/palettegenerator.cpp
        src/palettegenerator.h
        src/paths.cpp
        src/paths.h
        src/remoteinstaller.cpp
        src/remoteinstaller.h
        src/shellbuilder.cpp
        src/shellbuilder.h
        src/templaterenderer.cpp
        src/templaterenderer.h
        src/theme.cpp
        src/theme.h
    )

    target_include_directories(test_qml PRIVATE src)
    target_link_libraries(test_qml PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Test
    )

    qt_add_resources(test_qml "qml-test-resources"
        PREFIX "/"
        FILES
            ${HT_QML_FILES}
            ${HT_TEMPLATES}
            ${HT_CATALOG}
            ${HT_ICONS}
            data/hyperthemes.svg
    )

    add_test(NAME qml COMMAND test_qml)
    set_tests_properties(qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
