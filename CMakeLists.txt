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

set(TUXELS_CORE_SOURCES
  src/core/SelectionMask.cpp
  src/layers/PixelLayer.cpp
  src/compositor/compose.cpp
  src/brush/RoundBrush.cpp
  src/brush/BrushEngine.cpp
  src/fill/FloodFill.cpp
  src/geom/Resample.cpp
  src/history/CropCommand.cpp
  src/history/MoveLayerCommand.cpp
  src/history/TransformCommand.cpp
  src/io/TxlIO.cpp
  src/tools/BrushTool.cpp
  src/tools/BucketTool.cpp
  src/tools/CropTool.cpp
  src/tools/LassoTool.cpp
  src/tools/MagicWandTool.cpp
  src/tools/MarqueeTool.cpp
  src/tools/MoveTool.cpp
  src/tools/PolyLassoTool.cpp
  src/tools/SelectByColorTool.cpp
  src/tools/TransformTool.cpp
)

add_library(tuxels_core STATIC ${TUXELS_CORE_SOURCES})
set_target_properties(tuxels_core PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(tuxels_core PUBLIC src)

add_library(tuxels_io STATIC src/io/PngIO.cpp)
set_target_properties(tuxels_io PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(tuxels_io PUBLIC src)
target_link_libraries(tuxels_io PUBLIC tuxels_core Qt6::Gui)

add_executable(tuxels
  src/app/main.cpp
  src/app/MainWindow.h
  src/app/MainWindow.cpp
  src/ui/CanvasView.h
  src/ui/CanvasView.cpp
  src/ui/LayerRowWidget.h
  src/ui/LayerRowWidget.cpp
  src/ui/LayersPanel.h
  src/ui/LayersPanel.cpp
  src/ui/ToolsPanel.h
  src/ui/ToolsPanel.cpp
  resources/tuxels.qrc
)
if(WIN32)
  target_sources(tuxels PRIVATE packaging/tuxels.rc)
endif()
target_include_directories(tuxels PRIVATE src)
target_link_libraries(tuxels PRIVATE
  tuxels_core
  tuxels_io
  Qt6::Widgets
  Qt6::Gui
  Qt6::OpenGL
  Qt6::OpenGLWidgets
)

enable_testing()

function(tuxels_add_test name)
  add_executable(${name} tests/${name}.cpp)
  set_target_properties(${name} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
  target_include_directories(${name} PRIVATE src tests)
  target_link_libraries(${name} PRIVATE tuxels_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

tuxels_add_test(test_tile)
tuxels_add_test(test_blend_modes)
tuxels_add_test(test_compositor)
tuxels_add_test(test_brush)
tuxels_add_test(test_undo)
tuxels_add_test(test_selection)
tuxels_add_test(test_fill)
tuxels_add_test(test_crop)
tuxels_add_test(test_txl_io)
tuxels_add_test(test_layer_origin)
tuxels_add_test(test_place_image)
tuxels_add_test(test_move_layer)
tuxels_add_test(test_resample)
tuxels_add_test(test_transform_command)
tuxels_add_test(test_transform_tool)
tuxels_add_test(test_polygon_fill)
tuxels_add_test(test_lasso_tool)
tuxels_add_test(test_select_by_color)
tuxels_add_test(test_brush_dynamics)

add_executable(test_png_io tests/test_png_io.cpp)
set_target_properties(test_png_io PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(test_png_io PRIVATE src tests)
target_link_libraries(test_png_io PRIVATE tuxels_io)
add_test(NAME test_png_io COMMAND test_png_io)
