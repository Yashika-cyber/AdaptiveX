cmake_minimum_required(VERSION 3.20)
project(adaptivex CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
enable_testing()

# ── Sources ───────────────────────────────────────────────────
set(ENGINE_SOURCES
    src/generator.cpp
    src/scheduler.cpp
    src/controller.cpp
    src/order_book.cpp
    src/fairness.cpp
    src/cache.cpp
    src/wal.cpp
    src/metrics.cpp
    src/main.cpp
)

add_executable(adaptivex ${ENGINE_SOURCES})

target_include_directories(adaptivex PRIVATE include)
target_link_libraries(adaptivex Threads::Threads)

target_compile_options(adaptivex PRIVATE
    -O2
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

# ── ThreadSanitizer build ─────────────────────────────────────
add_executable(adaptivex_tsan ${ENGINE_SOURCES})
target_include_directories(adaptivex_tsan PRIVATE include)
target_link_libraries(adaptivex_tsan Threads::Threads)
target_compile_options(adaptivex_tsan PRIVATE -fsanitize=thread -g -O1)
target_link_options(adaptivex_tsan PRIVATE -fsanitize=thread)

# ── AddressSanitizer build ────────────────────────────────────
add_executable(adaptivex_asan ${ENGINE_SOURCES})
target_include_directories(adaptivex_asan PRIVATE include)
target_link_libraries(adaptivex_asan Threads::Threads)
target_compile_options(adaptivex_asan PRIVATE -fsanitize=address -g -O1)
target_link_options(adaptivex_asan PRIVATE -fsanitize=address)

# ── Tests ─────────────────────────────────────────────────────
set(COMMON_TEST_SRCS src/scheduler.cpp src/fairness.cpp src/metrics.cpp)

add_executable(test_scheduler tests/test_scheduler.cpp ${COMMON_TEST_SRCS})
target_include_directories(test_scheduler PRIVATE include)
target_link_libraries(test_scheduler Threads::Threads)
target_compile_options(test_scheduler PRIVATE -g -O0 -Wall)

add_executable(test_fairness tests/test_fairness.cpp ${COMMON_TEST_SRCS})
target_include_directories(test_fairness PRIVATE include)
target_link_libraries(test_fairness Threads::Threads)
target_compile_options(test_fairness PRIVATE -g -O0 -Wall)

add_executable(test_generator tests/test_generator.cpp src/generator.cpp)
target_include_directories(test_generator PRIVATE include)
target_link_libraries(test_generator Threads::Threads)
target_compile_options(test_generator PRIVATE -g -O0 -Wall)

add_executable(test_order_book tests/test_order_book.cpp src/order_book.cpp)
target_include_directories(test_order_book PRIVATE include)
target_link_libraries(test_order_book Threads::Threads)
target_compile_options(test_order_book PRIVATE -g -O0 -Wall)

add_executable(test_sync_queue tests/test_sync_queue.cpp)
target_include_directories(test_sync_queue PRIVATE include)
target_link_libraries(test_sync_queue Threads::Threads)
target_compile_options(test_sync_queue PRIVATE -g -O0 -Wall)

add_test(NAME scheduler COMMAND test_scheduler)
add_test(NAME fairness COMMAND test_fairness)
add_test(NAME generator COMMAND test_generator)
add_test(NAME order_book COMMAND test_order_book)
add_test(NAME sync_queue COMMAND test_sync_queue)

add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_scheduler test_fairness test_generator test_order_book test_sync_queue
    COMMENT "Running all unit tests"
)

# Install
install(TARGETS adaptivex DESTINATION bin)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
