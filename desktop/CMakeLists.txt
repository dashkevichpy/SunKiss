cmake_minimum_required(VERSION 3.20)
project(SunKissDesktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort Core Gui)

qt_add_executable(SunKissDesktop
    src/main.cpp
    src/MainWindow.cpp
src/StatusFrame.cpp
    src/MainWindow.h
    src/DeviceTab.cpp
    src/DeviceTab.h
    src/ProcessView.cpp
    src/ProcessView.h
    src/ServiceView.cpp
    src/ServiceView.h
    src/SerialManager.cpp
    src/SerialManager.h
    src/StatusFrame.h
    src/Settings.cpp
    src/Settings.h
    src/Logger.cpp
    src/Logger.h
    src/CommandQueue.h
    src/StatusParser.cpp
    src/StatusParser.h
    src/RecipeProfile.h
    src/Utils.cpp
    src/Utils.h
)

target_link_libraries(SunKissDesktop PRIVATE Qt6::Widgets Qt6::SerialPort Qt6::Core Qt6::Gui)

install(TARGETS SunKissDesktop RUNTIME DESTINATION bin)
