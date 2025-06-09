add_library(mongoose STATIC
    ${CMAKE_SOURCE_DIR}/Libraries/mongoose/mongoose.c
    ${CMAKE_SOURCE_DIR}/Libraries/mongoose/mongoose.h
)

target_include_directories(mongoose PRIVATE ${CMAKE_SOURCE_DIR}/Libraries/mongoose)