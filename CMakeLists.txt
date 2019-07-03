cmake_minimum_required (VERSION 3.10)
project ("IIO Example")

find_library(IIO_LIB iio)
add_executable(ad9361 test.cpp)
target_link_libraries(ad9361 ${IIO_LIB})
