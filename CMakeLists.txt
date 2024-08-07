project(STDEXEC_SANDBOX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/3rd-party/libuv-cmake-src)
add_subdirectory(3rd-party/stdexec-src)
add_subdirectory(3rd-party/optick-src)
add_subdirectory(3rd-party/tbb-src)

find_package(libuv REQUIRED)

add_executable(sandbox
    main.cpp
    util.hpp
    LibuvThreadPool.hpp
    FakeServerDemo.hpp
    LibuvFakeServer.cpp
    Backend.cpp
    LibuvFakeServer.hpp
    Context.hpp
    SingleShotEvent.hpp
    AsyncReader.hpp
    QueueScheduler.hpp
    Input.cpp 
    Output.cpp
    Transformator.cpp
    Image.cpp)

target_link_libraries(sandbox PUBLIC STDEXEC::stdexec TBB::tbb uv OptickCore)