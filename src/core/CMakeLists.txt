# The core library: all business logic, no user interface.
# It links only QtCore and QtSql, so it cannot accidentally use any GUI class.
add_library(inventory_core STATIC
    Types.h
    Database.h          Database.cpp
    InventoryService.h  InventoryService.cpp
    AlertService.h      AlertService.cpp
    InputClassifier.h   InputClassifier.cpp
    Backup.h            Backup.cpp
)

target_include_directories(inventory_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(inventory_core PUBLIC Qt6::Core Qt6::Sql)
