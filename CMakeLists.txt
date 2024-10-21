# CMakeLists.txt for RL-AQM example

# Minimum CMake sürümü
cmake_minimum_required(VERSION 3.10)

# Proje adı
project(rl-aqm CXX)

# Kaynak dosyaları
set(source_files
    rl-aqm-env.cc
    rl-aqm.cc
)

# Başlık dosyaları
set(header_files
    rl-aqm-env.h
)

# Çalıştırılabilir dosya oluştur
build_lib_example(
    NAME rl-aqm
    SOURCE_FILES ${source_files}
    HEADER_FILES ${header_files}
    LIBRARIES_TO_LINK ${libcore}
                      ${libnetwork}
                      ${libinternet}
                      ${libpoint-to-point}
                      ${libpoint-to-point-layout}
                      ${libapplications}
                      ${libtraffic-control}
                      ${libflow-monitor}
                      ${libai}

)

