cmake_minimum_required(VERSION 3.25)
project(NC-KTV VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─── Qt6 Installation Path ───────────────────────────────────────────────────
# Qt 6.2.9 (from MuseScore distribution)
list(APPEND CMAKE_PREFIX_PATH "D:/ProgramData/musescore qt")

# ─── Find Dependencies ───────────────────────────────────────────────────────

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Network Core5Compat)

# External Libraries (via vcpkg) - DISABLED for VS2026 compatibility test
# find_package(nlohmann_json CONFIG REQUIRED)
# find_package(yaml-cpp CONFIG REQUIRED)
# find_package(Eigen3 CONFIG REQUIRED)
# find_package(zstd CONFIG REQUIRED)
# find_package(GTest CONFIG REQUIRED)
# find_package(OpenSSL REQUIRED)
# find_package(ICU COMPONENTS i18n uc REQUIRED)

# ICU (optional, for romanization)
# find_package(ICU COMPONENTS uc i18n data) # This line was removed as per the Code Edit's implied change for ICU

# OpenSSL (for AES-256-GCM encryption)
# find_package(OpenSSL REQUIRED)

# ─── Targets ─────────────────────────────────────────────────────────────────

add_subdirectory(src/core)
add_subdirectory(src/gui)
# add_subdirectory(tests)
