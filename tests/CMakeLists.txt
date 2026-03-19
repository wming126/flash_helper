find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test REQUIRED)

add_executable(test_smartmerge test_smartmerge.cpp ../src/smartmerge.cpp)
target_include_directories(test_smartmerge PRIVATE ..)
target_link_libraries(test_smartmerge PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME test_smartmerge COMMAND test_smartmerge)

add_executable(test_localflashutils test_localflashutils.cpp ../src/localflashutils.cpp)
target_include_directories(test_localflashutils PRIVATE ..)
target_link_libraries(test_localflashutils PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME test_localflashutils COMMAND test_localflashutils)
