cmake_minimum_required(VERSION 3.21)
project(tuxels LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Gui OpenGL OpenGLWidgets)

option(TUXELS_SANITIZE "Enable AddressSanitizer + UBSan" OFF)
if(TUXELS_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)

add_executable(tuxels
  src/app/main.cpp
  src/app/MainWindow.h
  src/app/MainWindow.cpp
)
target_include_directories(tuxels PRIVATE src)
target_link_libraries(tuxels PRIVATE
  Qt6::Widgets
  Qt6::Gui
  Qt6::OpenGL
  Qt6::OpenGLWidgets
)

enable_testing()
