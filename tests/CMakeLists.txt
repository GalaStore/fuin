# SPDX-License-Identifier: MIT
find_package(Qt6 REQUIRED COMPONENTS Test DBus)

set(S ${PROJECT_SOURCE_DIR}/src)
add_library(fuin_foldermodel STATIC ${S}/fs/FolderModel.cpp ${S}/fs/FileKind.cpp ${S}/app/Config.cpp ${S}/ai/SecretGuard.cpp ${S}/platform/Terminal.cpp)
target_include_directories(fuin_foldermodel PUBLIC ${S})
target_compile_definitions(fuin_foldermodel PUBLIC QT_NO_CAST_FROM_ASCII QT_NO_URL_CAST_FROM_STRING QT_NO_KEYWORDS)
target_compile_options(fuin_foldermodel PUBLIC ${FUIN_WARNING_FLAGS})
target_link_libraries(fuin_foldermodel PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent KF6::KIOCore)

add_executable(tst_foldermodel tst_foldermodel.cpp)
target_link_libraries(tst_foldermodel PRIVATE fuin_foldermodel Qt6::Test Qt6::DBus)
add_test(NAME tst_foldermodel COMMAND tst_foldermodel)

add_executable(tst_secretguard tst_secretguard.cpp)
target_link_libraries(tst_secretguard PRIVATE fuin_foldermodel Qt6::Test)
add_test(NAME tst_secretguard COMMAND tst_secretguard)

add_executable(tst_terminal tst_terminal.cpp)
target_link_libraries(tst_terminal PRIVATE fuin_foldermodel Qt6::Test)
add_test(NAME tst_terminal COMMAND tst_terminal)

add_executable(tst_icons tst_icons.cpp)
target_link_libraries(tst_icons PRIVATE Qt6::Gui Qt6::Test)
target_compile_definitions(tst_icons PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_KEYWORDS
    FUIN_ICON_FONT="${PROJECT_SOURCE_DIR}/assets/fonts/FuinIcons.ttf"
    FUIN_ICON_LIST="${PROJECT_SOURCE_DIR}/assets/fonts/icons.txt"
    FUIN_QML_DIR="${PROJECT_SOURCE_DIR}/qml")
add_test(NAME tst_icons COMMAND tst_icons)
set_tests_properties(tst_icons PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_config tst_config.cpp)
target_link_libraries(tst_config PRIVATE fuin_foldermodel Qt6::Test)
add_test(NAME tst_config COMMAND tst_config)

if(FUIN_WITH_PORTAL)
    add_executable(tst_portallook tst_portallook.cpp ${S}/portal/PortalLook.cpp)
    target_include_directories(tst_portallook PRIVATE ${S})
    target_compile_definitions(tst_portallook PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_KEYWORDS)
    target_link_libraries(tst_portallook PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME tst_portallook COMMAND tst_portallook)
    set_tests_properties(tst_portallook PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=generic;QT_NO_XDG_DESKTOP_PORTAL=1")

    add_test(NAME smoke_portal
             COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/portal_startup.sh $<TARGET_FILE:fuin> ${PROJECT_SOURCE_DIR}/packaging/fuin.portal
                     ${CMAKE_CURRENT_BINARY_DIR}/smoke_portal)
    set_tests_properties(smoke_portal PROPERTIES LABELS smoke SKIP_RETURN_CODE 77 TIMEOUT 60)
endif()

add_library(fuin_inspector STATIC ${S}/preview/Inspector.cpp)
target_link_libraries(fuin_inspector PUBLIC fuin_foldermodel)
if(FUIN_WITH_PARQUET AND Arrow_FOUND AND Parquet_FOUND)
    target_link_libraries(fuin_inspector PUBLIC Parquet::parquet_shared Arrow::arrow_shared)
    target_compile_definitions(fuin_inspector PUBLIC FUIN_HAVE_PARQUET)
endif()

add_executable(tst_inspector tst_inspector.cpp)
target_link_libraries(tst_inspector PRIVATE fuin_inspector Qt6::Test)
add_test(NAME tst_inspector COMMAND tst_inspector)

add_executable(bench_foldermodel bench_foldermodel.cpp)
target_link_libraries(bench_foldermodel PRIVATE fuin_foldermodel)
add_test(NAME bench_foldermodel COMMAND bench_foldermodel)
set_tests_properties(bench_foldermodel PROPERTIES LABELS bench)

foreach(mode version context mcp)
    add_test(NAME smoke_${mode}
             COMMAND ${CMAKE_COMMAND} -DFUIN=$<TARGET_FILE:fuin> -DMODE=${mode} -DVERSION=${PROJECT_VERSION}
                     -DWORK=${CMAKE_CURRENT_BINARY_DIR}/smoke_${mode} -P ${CMAKE_CURRENT_SOURCE_DIR}/smoke.cmake)
    set_tests_properties(smoke_${mode} PROPERTIES LABELS smoke ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endforeach()

add_test(NAME release_notes COMMAND ${PROJECT_SOURCE_DIR}/.github/scripts/release-notes.sh 0.1.0 ${PROJECT_SOURCE_DIR}/CHANGELOG.md)
set_tests_properties(release_notes PROPERTIES PASS_REGULAR_EXPRESSION "^First public release\\.\n")
