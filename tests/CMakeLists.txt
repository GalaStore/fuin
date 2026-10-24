# SPDX-License-Identifier: MIT
find_package(Qt6 REQUIRED COMPONENTS Test DBus)

set(S ${PROJECT_SOURCE_DIR}/src)
add_library(fuin_foldermodel STATIC ${S}/fs/FolderModel.cpp ${S}/fs/FileKind.cpp ${S}/app/Config.cpp ${S}/ai/SecretGuard.cpp)
target_include_directories(fuin_foldermodel PUBLIC ${S})
target_compile_definitions(fuin_foldermodel PUBLIC QT_NO_CAST_FROM_ASCII QT_NO_URL_CAST_FROM_STRING QT_NO_KEYWORDS)
target_compile_options(fuin_foldermodel PUBLIC -Wall -Wextra -Wpedantic)
target_link_libraries(fuin_foldermodel PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent KF6::KIOCore)

add_executable(tst_foldermodel tst_foldermodel.cpp)
target_link_libraries(tst_foldermodel PRIVATE fuin_foldermodel Qt6::Test Qt6::DBus)
add_test(NAME tst_foldermodel COMMAND tst_foldermodel)

add_executable(bench_foldermodel bench_foldermodel.cpp)
target_link_libraries(bench_foldermodel PRIVATE fuin_foldermodel)
add_test(NAME bench_foldermodel COMMAND bench_foldermodel)
set_tests_properties(bench_foldermodel PROPERTIES LABELS bench)
