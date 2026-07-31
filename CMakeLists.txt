cmake_minimum_required(VERSION 3.24)

project(filemanager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets Test)

qt_standard_project_setup()

add_library(filemanager_settings STATIC
    src/services/SettingsStore.cpp
    src/services/SettingsStore.h
    src/services/AppPaths.h
)
target_include_directories(filemanager_settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(filemanager_settings PUBLIC Qt6::Core)

add_library(filemanager_ui STATIC
    src/MainWindow.cpp
    src/MainWindow.h
    src/models/FavoritesModel.cpp
    src/models/FavoritesModel.h
    src/models/FileSystemSortProxyModel.cpp
    src/models/FileSystemSortProxyModel.h
    src/models/FileMetadata.cpp
    src/models/FileMetadata.h
    src/ui/FavoritesSidebar.cpp
    src/ui/FavoritesSidebar.h
    src/ui/MetadataPanel.cpp
    src/ui/MetadataPanel.h
    src/services/TabManager.cpp
    src/services/TabManager.h
    src/services/FileOperationService.cpp
    src/services/FileOperationService.h
    src/services/PlatformServices.cpp
    src/services/PlatformServices.h
    src/services/SearchService.cpp
    src/services/SearchService.h
    src/services/GitService.cpp
    src/services/GitService.h
    src/services/ThemeManager.cpp
    src/services/ThemeManager.h
    src/services/TerminalService.cpp
    src/services/TerminalService.h
    src/ui/FileBrowserWidget.cpp
    src/ui/FileBrowserWidget.h
    src/ui/TabStrip.cpp
    src/ui/TabStrip.h
)
target_include_directories(filemanager_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(filemanager_ui PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets filemanager_settings)

qt_add_resources(filemanager_ui "filemanager_icons"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/filemanager.png
)

add_executable(filemanager_app
    src/main.cpp
)
target_link_libraries(filemanager_app PRIVATE filemanager_ui)

if(WIN32)
    target_sources(filemanager_app PRIVATE resources/filemanager.rc)
    set_target_properties(filemanager_app PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

enable_testing()
add_subdirectory(tests)
