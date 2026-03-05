cmake_minimum_required(VERSION 3.25)
project(NC-KTV VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─── Qt6 Installation Path ───────────────────────────────────────────────────
# Qt 6.10.2 (official Qt installation)
list(APPEND CMAKE_PREFIX_PATH "D:/ProgramData/Qt/6.10.2/msvc2022_64")

# ─── Find Dependencies ───────────────────────────────────────────────────────

# Qt6 (required)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Network)

# OpenSSL (optional - enables encrypted project files)
find_package(OpenSSL QUIET)

# ─── Targets ─────────────────────────────────────────────────────────────────

add_subdirectory(src/core)
add_subdirectory(src/gui)
add_subdirectory(tests)
