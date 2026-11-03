cmake_minimum_required(VERSION 3.21)
project(ShopInventory VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Lets Qt generate the extra code needed by classes that use Q_OBJECT.
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql Widgets Test)

enable_testing()

add_subdirectory(src/core)   # business logic, no GUI
add_subdirectory(src/app)    # Qt Widgets user interface + ShopInventory.exe
add_subdirectory(tests)
