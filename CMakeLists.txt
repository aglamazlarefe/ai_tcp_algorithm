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
                      ${libapplications}
                      ${libtraffic-control}
                      ${libflow-monitor}
                      ${libai}
)

# Python binding modülü oluştur (isteğe bağlı)
if(${ENABLE_PYTHON_BINDINGS})
    set(py_source_files
        rl-aqm-py.cc
    )

    build_lib_example_python_bindings(
        NAME rl_aqm
        SOURCE_FILES ${py_source_files}
        LIBRARIES_TO_LINK ${libai}
    )

    # Python binding'i C++ kütüphanesiyle birlikte derle
    add_dependencies(rl-aqm rl_aqm_py)
endif()