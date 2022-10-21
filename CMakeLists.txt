# CMakeList.txt: projeto de CMake para CleanCMakeProject, incluir origem e definir
# lógica específica a um projeto aqui.
#
cmake_minimum_required (VERSION 3.8)

set(VCPKG_TARGET_TRIPLET x64-windows)

project ("five-words")

# Adicione a origem ao executável deste projeto.
add_executable (five-words "main.cpp" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET five-words PROPERTY CXX_STANDARD 23)
endif()

# TODO: Adicione testes e instale destinos, se necessário.
find_package(Boost COMPONENTS iostreams REQUIRED)

target_include_directories(five-words PUBLIC ${Boost_INCLUDE_DIR})

message(STATUS "Boost_LIBRARIES = ${Boost_LIBRARIES}")

target_link_libraries(five-words 
  ${Boost_LIBRARIES}
)