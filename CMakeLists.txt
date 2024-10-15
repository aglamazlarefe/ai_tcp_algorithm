cmake_minimum_required(VERSION 3.10)

# Proje adı
project(RLAQM-Simulation)

# Gerekli paketler
find_package(NS3 REQUIRED)
find_package(OpenGymAi REQUIRED)

# C++ standardı
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Kütüphaneler ve dizinler
include_directories(${NS3_INCLUDE_DIRS} ${OpenGymAi_INCLUDE_DIRS})

link_directories(${NS3_LIBRARY_DIRS} ${OpenGymAi_LIBRARY_DIRS})

# Kaynak dosyalar
set(SOURCES
    rl-aqm-env.cc
    rl-aqm.cc
)

# Çıktı dosyası
add_executable(RLAqmSimulation ${SOURCES})

# Kütüphanelerle bağlantı
target_link_libraries(RLAqmSimulation ${NS3_LIBRARIES} ${OpenGymAi_LIBRARIES})

# Debug log ayarı
add_definitions(-DNS3_LOG_ENABLE)


