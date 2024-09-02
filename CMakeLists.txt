project(STDEXEC_SANDBOX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TBB_STRICT OFF CACHE BOOL "disable werror due to std23 deprications" FORCE)
set(TBB_TEST OFF CACHE BOOL "no test needed" FORCE)
set(TBB_INSTALL OFF CACHE BOOL "no install is required" FORCE)
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/3rd-party/libuv-cmake-src)
add_subdirectory(3rd-party/stdexec-src)
add_subdirectory(3rd-party/optick-src)
add_subdirectory(3rd-party/tbb-src)
add_subdirectory(3rd-party/libuv-src)

#find_package(libuv REQUIRED)

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
    Retry.hpp
    AsyncReader.hpp
    TaskStream.hpp
    Input.cpp
    Output.cpp
    Transformator.cpp
    Image.cpp)

target_link_libraries(sandbox PUBLIC STDEXEC::stdexec TBB::tbb libuv::libuv OptickCore)
