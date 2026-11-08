cmake_minimum_required(VERSION 3.24)

cmake_policy(SET CMP0091 NEW)

project(Quickdraw VERSION 1.0.0 LANGUAGES CXX RC)

add_executable(Quickdraw WIN32
    src/main.cpp
    src/app_controller.cpp
    src/app_controller.h
    src/display/display_backend.cpp
    src/display/display_backend.h
    src/display/display_manager.cpp
    src/display/display_manager.h
    src/display/display_types.h
    src/display/mode_catalog.cpp
    src/display/mode_catalog.h
    src/display/mode_transaction.cpp
    src/display/mode_transaction.h
    src/display/monitor_matcher.cpp
    src/display/monitor_matcher.h
    src/platform/single_instance.cpp
    src/platform/single_instance.h
    src/platform/logger.cpp
    src/platform/logger.h
    src/platform/brand_migration.cpp
    src/platform/brand_migration.h
    src/platform/startup_manager.cpp
    src/platform/startup_manager.h
    src/storage/recovery_store.cpp
    src/storage/recovery_store.h
    src/storage/settings_store.cpp
    src/storage/settings_store.h
    src/ui/tray_icon.cpp
    src/ui/tray_icon.h
    src/ui/confirmation_dialog.cpp
    src/ui/confirmation_dialog.h
    src/ui/settings_dialog.cpp
    src/ui/settings_dialog.h
    src/ui/tray_menu.cpp
    src/ui/tray_menu.h
    resources/app.rc
    resources/app.manifest
    resources/quickdraw.ico
    resources/branding/quickdraw-icon-256.png
    resources/resource.h
)

target_compile_features(Quickdraw PRIVATE cxx_std_20)
target_compile_definitions(Quickdraw PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
target_include_directories(Quickdraw PRIVATE src resources)

if(MSVC)
    set_property(TARGET Quickdraw PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_options(Quickdraw PRIVATE
        /W4
        /WX
        /permissive-
        /utf-8
        /EHsc
        /guard:cf
    )
    target_link_options(Quickdraw PRIVATE
        /DYNAMICBASE
        /NXCOMPAT
        /HIGHENTROPYVA
        /GUARD:CF
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
elseif(MINGW)
    target_compile_options(Quickdraw PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_link_options(Quickdraw PRIVATE -municode)
endif()

target_link_libraries(Quickdraw PRIVATE
    advapi32
    comctl32
    ole32
    shell32
    user32
    uuid
)

set_target_properties(Quickdraw PROPERTIES
    OUTPUT_NAME "Quickdraw"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Quickdraw>"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(QuickdrawModeCatalogTests
        tests/mode_catalog_tests.cpp
        src/display/mode_catalog.cpp
        src/display/mode_catalog.h
        src/display/display_types.h
    )
    target_compile_features(QuickdrawModeCatalogTests PRIVATE cxx_std_20)
    target_compile_definitions(QuickdrawModeCatalogTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    target_include_directories(QuickdrawModeCatalogTests PRIVATE src)
    if(MSVC)
        set_property(TARGET QuickdrawModeCatalogTests PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(QuickdrawModeCatalogTests PRIVATE
            /W4 /WX /permissive- /utf-8 /EHsc
        )
    elseif(MINGW)
        target_compile_options(QuickdrawModeCatalogTests PRIVATE
            -Wall -Wextra -Wpedantic -Werror
        )
    endif()
    add_test(NAME mode_catalog COMMAND QuickdrawModeCatalogTests)

    add_executable(QuickdrawModeTransactionTests
        tests/mode_transaction_tests.cpp
        src/display/mode_transaction.cpp
        src/display/mode_transaction.h
        src/display/display_backend.h
        src/display/display_types.h
    )
    target_compile_features(QuickdrawModeTransactionTests PRIVATE cxx_std_20)
    target_compile_definitions(QuickdrawModeTransactionTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    target_include_directories(QuickdrawModeTransactionTests PRIVATE src)
    if(MSVC)
        set_property(TARGET QuickdrawModeTransactionTests PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(QuickdrawModeTransactionTests PRIVATE
            /W4 /WX /permissive- /utf-8 /EHsc
        )
    elseif(MINGW)
        target_compile_options(QuickdrawModeTransactionTests PRIVATE
            -Wall -Wextra -Wpedantic -Werror
        )
    endif()
    add_test(NAME mode_transaction COMMAND QuickdrawModeTransactionTests)

    add_executable(QuickdrawMonitorMatcherTests
        tests/monitor_matcher_tests.cpp
        src/display/monitor_matcher.cpp
        src/display/monitor_matcher.h
        src/display/display_types.h
    )
    target_compile_features(QuickdrawMonitorMatcherTests PRIVATE cxx_std_20)
    target_compile_definitions(QuickdrawMonitorMatcherTests PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
        WINVER=0x0A00 _WIN32_WINNT=0x0A00
    )
    target_include_directories(QuickdrawMonitorMatcherTests PRIVATE src)
    if(MSVC)
        set_property(TARGET QuickdrawMonitorMatcherTests PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(QuickdrawMonitorMatcherTests PRIVATE
            /W4 /WX /permissive- /utf-8 /EHsc
        )
    elseif(MINGW)
        target_compile_options(QuickdrawMonitorMatcherTests PRIVATE
            -Wall -Wextra -Wpedantic -Werror
        )
    endif()
    add_test(NAME monitor_matcher COMMAND QuickdrawMonitorMatcherTests)
endif()
