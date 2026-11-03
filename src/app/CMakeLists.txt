# The user interface is a library too, so the GUI test can use MainWindow.
add_library(inventory_ui STATIC
    MainWindow.h     MainWindow.cpp
    Dialogs.h        Dialogs.cpp
    KeyRedirector.h  KeyRedirector.cpp
)
target_include_directories(inventory_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(inventory_ui PUBLIC inventory_core Qt6::Widgets)

# WIN32: on Windows, start without a black console window.
add_executable(ShopInventory WIN32 main.cpp)
target_link_libraries(ShopInventory PRIVATE inventory_ui)
