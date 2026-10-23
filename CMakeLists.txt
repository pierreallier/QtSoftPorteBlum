cmake_minimum_required(VERSION 3.21)
project(SoftPorteBlum VERSION 0.2.0 LANGUAGES CXX)
add_compile_definitions(BLUM_VERSION="${PROJECT_VERSION}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Widgets SerialPort)
include(cmake/GenerateProtocol.cmake)
add_library(blum_core STATIC src/serial/Commands.h src/serial/Commands.cpp src/core/AppSettings.h src/core/AppSettings.cpp src/core/Event.h src/core/Event.cpp src/core/DebugStyles.h src/core/DebugStyles.cpp ${CMAKE_CURRENT_BINARY_DIR}/generated/serial/Protocol.h src/core/SystemState.h src/core/EventQueue.h src/serial/SerialDecoder.h src/serial/SerialDecoder.cpp)
target_include_directories(blum_core PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated" src)
target_link_libraries(blum_core PUBLIC Qt6::Core Qt6::Gui)
qt_add_executable(SoftPorteBlum src/main.cpp src/Application.h src/Application.cpp src/gui/MainWindow.h src/gui/MainWindow.cpp
 src/gui/StartupSplash.h src/gui/StartupSplash.cpp
 src/gui/widgets/StatusWidget.h src/gui/widgets/StatusWidget.cpp
 src/gui/components/StatusIndicator.h
 src/gui/components/ColorCheckBox.h src/gui/components/ColorCheckBox.cpp
 src/gui/components/CalibrationButton.h src/gui/components/CalibrationButton.cpp
 src/gui/components/TwoStateButton.h src/gui/components/TwoStateButton.cpp
 src/gui/widgets/Page.h
 src/gui/widgets/GraphWidget.h src/gui/widgets/GraphWidget.cpp
 src/gui/widgets/VisualisationPage.h src/gui/widgets/VisualisationPage.cpp
 src/serial/SerialController.h src/serial/SerialController.cpp src/serial/SerialWorker.h src/serial/SerialWorker.cpp
 src/gui/widgets/MeasurePage.h src/gui/widgets/MeasurePage.cpp
 src/gui/widgets/StateGraphWidget.h src/gui/widgets/StateGraphWidget.cpp
 src/gui/widgets/PilotagePage.h src/gui/widgets/PilotagePage.cpp
 src/gui/widgets/ConsigneWidget.h src/gui/widgets/ConsigneWidget.cpp
 src/gui/widgets/PIDWidget.h src/gui/widgets/PIDWidget.cpp
 src/gui/widgets/DebugPage.h src/gui/widgets/DebugPage.cpp
 src/gui/widgets/SerialWidget.h src/gui/widgets/SerialWidget.cpp src/gui/components/StyleHelpers.h src/core/MeasurementStore.h resources/resources.qrc)
target_link_libraries(SoftPorteBlum PRIVATE blum_core Qt6::Widgets Qt6::SerialPort)
if(WIN32)
 enable_language(RC)
 target_sources(SoftPorteBlum PRIVATE resources/application.rc)
 target_include_directories(SoftPorteBlum PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
endif()
if(MSVC)
 target_compile_options(blum_core PRIVATE /utf-8 /W4)
 target_compile_options(SoftPorteBlum PRIVATE /utf-8 /W4)
else()
 target_compile_options(blum_core PRIVATE -Wall -Wextra -Wpedantic)
 target_compile_options(SoftPorteBlum PRIVATE -Wall -Wextra -Wpedantic)
endif()
# Console conservée sous Windows pour les sorties qDebug dans VS Code.
include(CTest)
if(BUILD_TESTING)
 find_package(Qt6 REQUIRED COMPONENTS Test)
 find_package(Threads REQUIRED)
 qt_add_executable(core_tests tests/CoreTests.cpp)
 qt_add_resources(core_tests "state_test_definitions" PREFIX "/" BASE "resources" FILES resources/etats.csv resources/protocol.json resources/config.ini resources/machines.json resources/codes.csv resources/courbe.json resources/debug.json)
 target_link_libraries(core_tests PRIVATE blum_core Qt6::Test Threads::Threads)
 add_test(NAME core_tests COMMAND core_tests)
 qt_add_executable(integration_tests tests/IntegrationTests.cpp
  src/gui/widgets/StatusWidget.h src/gui/widgets/StatusWidget.cpp
 src/gui/components/StatusIndicator.h
 src/gui/components/ColorCheckBox.h src/gui/components/ColorCheckBox.cpp
 src/gui/components/CalibrationButton.h src/gui/components/CalibrationButton.cpp
 src/gui/components/TwoStateButton.h src/gui/components/TwoStateButton.cpp
 src/gui/widgets/Page.h
  src/gui/widgets/GraphWidget.h src/gui/widgets/GraphWidget.cpp
 src/gui/widgets/VisualisationPage.h src/gui/widgets/VisualisationPage.cpp
  src/gui/MainWindow.h src/gui/MainWindow.cpp src/gui/widgets/MeasurePage.h src/gui/widgets/MeasurePage.cpp
 src/gui/widgets/StateGraphWidget.h src/gui/widgets/StateGraphWidget.cpp
 src/gui/widgets/PilotagePage.h src/gui/widgets/PilotagePage.cpp
 src/gui/widgets/ConsigneWidget.h src/gui/widgets/ConsigneWidget.cpp
 src/gui/widgets/PIDWidget.h src/gui/widgets/PIDWidget.cpp
 src/gui/widgets/DebugPage.h src/gui/widgets/DebugPage.cpp
 src/gui/widgets/SerialWidget.h src/gui/widgets/SerialWidget.cpp src/gui/components/StyleHelpers.h
  src/serial/SerialController.h src/serial/SerialController.cpp src/serial/SerialWorker.h src/serial/SerialWorker.cpp
  resources/resources.qrc)
 target_link_libraries(integration_tests PRIVATE blum_core Qt6::Widgets Qt6::SerialPort Qt6::Test)
 add_dependencies(integration_tests SoftPorteBlum)
 add_test(NAME integration_tests COMMAND integration_tests)
 set_tests_properties(integration_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
 if(MSVC)
  target_compile_options(integration_tests PRIVATE /utf-8)
 endif()
endif()
