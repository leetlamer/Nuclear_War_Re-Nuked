set(NWR_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding")
if(WIN32)
  enable_language(RC)
  target_sources(nuclearwar PRIVATE packaging/windows/application.rc)
  # Resource compilers resolve ICON paths using their include search path.
  target_include_directories(nuclearwar PRIVATE "${NWR_ICON_DIR}")
elseif(APPLE)
  set(NWR_ICNS "${NWR_ICON_DIR}/nuclear-war-renuked.icns")
  target_sources(nuclearwar PRIVATE "${NWR_ICNS}")
  set_source_files_properties("${NWR_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(nuclearwar PROPERTIES MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Nuclear War: Re-Nuked"
    MACOSX_BUNDLE_ICON_FILE nuclear-war-renuked.icns
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in")
elseif(UNIX)
  install(FILES packaging/linux/nuclear-war-renuked.desktop DESTINATION share/applications)
  foreach(size 16 32 48 64 128 256 512)
    install(FILES "${NWR_ICON_DIR}/linux/${size}x${size}/nuclear-war-renuked.png"
      DESTINATION "share/icons/hicolor/${size}x${size}/apps")
  endforeach()
endif()
