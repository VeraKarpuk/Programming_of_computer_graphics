cmake_minimum_required(VERSION 3.19)
project(color_systems_lab1 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(color_systems_lab1
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    colormodel.h colormodel.cpp
    colorcontroller.h colorcontroller.cpp
    README.md

)

target_link_libraries(color_systems_lab1
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS color_systems_lab1
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET color_systems_lab1
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
