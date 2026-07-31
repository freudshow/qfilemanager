add_executable(filemanager_smoke_test test_smoke.cpp)
target_link_libraries(filemanager_smoke_test PRIVATE Qt6::Widgets Qt6::Test filemanager_ui)
add_dependencies(filemanager_smoke_test filemanager_app)
add_test(NAME filemanager_smoke_test COMMAND filemanager_smoke_test)
set_tests_properties(filemanager_smoke_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(filemanager_settings_store_test test_settings_store.cpp)
target_link_libraries(filemanager_settings_store_test PRIVATE Qt6::Core Qt6::Test filemanager_settings)
add_test(NAME SettingsStoreTest COMMAND filemanager_settings_store_test)

add_executable(filemanager_mainwindow_test test_mainwindow.cpp)
target_link_libraries(filemanager_mainwindow_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test filemanager_ui)
add_test(NAME MainWindowTest COMMAND filemanager_mainwindow_test)
set_tests_properties(MainWindowTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(filemanager_filebrowser_test test_filebrowser.cpp)
target_link_libraries(filemanager_filebrowser_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test filemanager_ui)
add_test(NAME FileBrowserWidgetTest COMMAND filemanager_filebrowser_test)
set_tests_properties(FileBrowserWidgetTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(filemanager_favorites_model_test test_favorites_model.cpp)
target_link_libraries(filemanager_favorites_model_test PRIVATE Qt6::Core Qt6::Test filemanager_ui)
add_test(NAME FavoritesModelTest COMMAND filemanager_favorites_model_test)
set_tests_properties(FavoritesModelTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(filemanager_favorites_sidebar_test test_favorites_sidebar.cpp)
target_link_libraries(filemanager_favorites_sidebar_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test filemanager_ui)
add_test(NAME FavoritesSidebarTest COMMAND filemanager_favorites_sidebar_test)
set_tests_properties(FavoritesSidebarTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(filemanager_metadata_test test_metadata.cpp)
target_link_libraries(filemanager_metadata_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test filemanager_ui)
add_test(NAME MetadataTest COMMAND filemanager_metadata_test)
set_tests_properties(MetadataTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(filemanager_file_operations_test test_file_operations.cpp)
target_link_libraries(filemanager_file_operations_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test filemanager_ui)
add_test(NAME FileOperationServiceTest COMMAND filemanager_file_operations_test)
set_tests_properties(FileOperationServiceTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(filemanager_terminal_service_test test_terminal_service.cpp)
target_link_libraries(filemanager_terminal_service_test PRIVATE Qt6::Core Qt6::Test filemanager_ui)
add_test(NAME TerminalServiceTest COMMAND filemanager_terminal_service_test)

add_executable(filemanager_theme_manager_test test_theme_manager.cpp)
target_link_libraries(filemanager_theme_manager_test PRIVATE Qt6::Core Qt6::Test filemanager_ui)
add_test(NAME ThemeManagerTest COMMAND filemanager_theme_manager_test)

add_executable(filemanager_restore_test test_restore.cpp)
target_link_libraries(filemanager_restore_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test filemanager_ui)
add_test(NAME RestoreTest COMMAND filemanager_restore_test)
set_tests_properties(RestoreTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(filemanager_git_service_test test_git_service.cpp)
target_link_libraries(filemanager_git_service_test PRIVATE Qt6::Core Qt6::Test filemanager_ui)
add_test(NAME GitServiceTest COMMAND filemanager_git_service_test)
