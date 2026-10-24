if (APPLE)
    set (MACOS ON)

    enable_language (OBJCXX)

    add_library (
        platform_dependencies INTERFACE
    )

    # CGEvent synthesis, CGWindowList and AXIsProcessTrusted all live in
    # ApplicationServices; Cocoa is needed for the status item.
    target_link_libraries (
        platform_dependencies INTERFACE
        "-framework ApplicationServices"
        "-framework CoreFoundation"
        "-framework Cocoa"
        pthread
    )

    add_library (Platform::Dependencies ALIAS platform_dependencies)

    install (
        FILES ${PROJECT_SOURCE_DIR}/JoyShockMapper/gyro_icon.png
        DESTINATION share/icons/JoyShockMapper.png
    )

    install (
        DIRECTORY ${PROJECT_SOURCE_DIR}/dist/GyroConfigs
        DESTINATION ../etc/JoyShockMapper/
    )
endif ()
