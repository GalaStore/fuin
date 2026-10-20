# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)
project(archivos VERSION 0.1.0 DESCRIPTION "Keyboard-first, AI-aware file manager (KIO + Qt Quick)" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(ARCHIVOS_WITH_PARQUET "Parquet preview via Apache Arrow C++" ON)
option(ARCHIVOS_WITH_SYNTAX "Syntax highlighting via KSyntaxHighlighting" ON)

# KDE's find modules (LibMount, UDev) are needed by KF6Solid's package config.
find_package(ECM 6.0 NO_MODULE QUIET)
if(ECM_FOUND)
  list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_FIND_MODULE_DIR})
endif()

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Concurrent Network)
find_package(KF6KIO 6.0 REQUIRED)
find_package(KF6Solid 6.0 REQUIRED)
find_package(KF6CoreAddons 6.0 REQUIRED)
find_package(KF6Service 6.0 REQUIRED)

qt_standard_project_setup(REQUIRES 6.6)

add_executable(archivos
  src/main.cpp
  src/app/Config.cpp src/app/Theme.cpp src/app/AppInfo.cpp
  src/fs/FileKind.cpp src/fs/FolderModel.cpp src/fs/FileOps.cpp src/fs/PlacesModel.cpp src/fs/ThumbnailProvider.cpp
  src/preview/Inspector.cpp src/preview/Previewer.cpp src/preview/CodeHighlighter.cpp
  src/ai/SecretGuard.cpp src/ai/ContextPacker.cpp src/ai/McpServer.cpp
  src/ipc/Ipc.cpp
)
target_compile_definitions(archivos PRIVATE ARCHIVOS_VERSION="${PROJECT_VERSION}" QT_NO_CAST_FROM_ASCII QT_NO_URL_CAST_FROM_STRING QT_NO_KEYWORDS)
target_compile_options(archivos PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(archivos PRIVATE
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent Qt6::Network
  KF6::KIOCore KF6::KIOGui KF6::KIOWidgets KF6::Solid KF6::CoreAddons KF6::Service)

if(ARCHIVOS_WITH_SYNTAX)
  find_package(KF6SyntaxHighlighting 6.0)
  if(KF6SyntaxHighlighting_FOUND)
    target_link_libraries(archivos PRIVATE KF6::SyntaxHighlighting)
    target_compile_definitions(archivos PRIVATE ARCHIVOS_HAVE_KSYNTAX)
  endif()
endif()

if(ARCHIVOS_WITH_PARQUET)
  find_package(Arrow CONFIG QUIET)
  find_package(Parquet CONFIG QUIET)
  if(Arrow_FOUND AND Parquet_FOUND)
    target_link_libraries(archivos PRIVATE Parquet::parquet_shared Arrow::arrow_shared)
    target_compile_definitions(archivos PRIVATE ARCHIVOS_HAVE_PARQUET)
    message(STATUS "Parquet preview: enabled (Arrow ${Arrow_VERSION})")
  else()
    message(STATUS "Parquet preview: disabled (Apache Arrow/Parquet C++ not found)")
  endif()
endif()

file(GLOB ARCHIVOS_QML RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS qml/*.qml qml/*.js)
qt_add_resources(archivos archivos_ui
  PREFIX "/"
  FILES ${ARCHIVOS_QML} assets/tomoe.svg assets/konoha.svg assets/leaf.svg assets/archivos.svg)

include(GNUInstallDirs)
install(TARGETS archivos RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/archivos.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/archivos.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES packaging/io.github.galactuuus.archivos.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
