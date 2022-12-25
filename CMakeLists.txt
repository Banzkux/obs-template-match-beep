cmake_minimum_required(VERSION 3.16...3.21)

project(obs-template-match-beep VERSION 1.0.0)
add_library(${CMAKE_PROJECT_NAME} MODULE)

set(PLUGIN_AUTHOR "Janne Pitkänen")

# Replace `com.example.obs-plugin-template` with a unique Bundle ID for macOS releases (used both in
# the installer and when submitting the installer for notarization)
set(MACOS_BUNDLEID "com.example.${CMAKE_PROJECT_NAME}")

set(LINUX_MAINTAINER_EMAIL "acebanzkux@gmail.com")

set(OBS_DIRECTORY
    ""
    CACHE PATH "Directory to OBS installation.")

set(OBS_PLATFORM "")

# Add your custom source files here - header files are optional and only required for visibility
# e.g. in Xcode or Visual Studio

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/template-match-beep.cpp src/vendor/LiveVisionKit/FrameIngest.cpp
          src/vendor/abeep/sintable.cpp)
set(ABEEP_H src/vendor/abeep/abeep.h src/vendor/abeep/sintable.h)
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/template-match-beep.generated.h src/vendor/LiveVisionKit/FrameIngest.hpp
          src/vendor/beep/beep.h ${ABEEP_H})

# Import libobs as main plugin dependency
find_package(libobs REQUIRED)
include(cmake/ObsPluginHelpers.cmake)

# Uncomment these lines if you want to use the OBS Frontend API in your plugin
#[[
find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
#]]

# Uncomment those lines if you want to use Qt in your plugin
#[[
find_qt(COMPONENTS Widgets Core)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)
#]]

if(OS_WINDOWS)
  find_package(OpenCV REQUIRED world)
else()
  find_package(OpenCV REQUIRED core highgui imgcodecs imgproc)
endif()
if(NOT OpenCV_FOUND)
  message(FATAL_ERROR "Failed to find the required OpenCV libaries!")
else()
  message(STATUS "${MI}Found OpenCV: YES")
  message(STATUS "${MI}OpenCV Version: ${OpenCV_VERSION}")
  message(STATUS "${MI}OpenCV Modules: ${OpenCV_LIBS}")
endif()
message(STATUS ${OpenCV_LIBS})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${OpenCV_LIBS})

configure_file(src/template-match-beep.h.in ${CMAKE_SOURCE_DIR}/src/template-match-beep.generated.h)

# /!\ TAKE NOTE: No need to edit things past this point /!\

# --- Platform-independent build settings ---

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(OS_WINDOWS)
  configure_file(cmake/bundle/windows/installer-Windows.iss.in
                 ${CMAKE_BINARY_DIR}/installer-Windows.generated.iss)

  configure_file(cmake/bundle/windows/resource.rc.in ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.rc)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.rc)

  if(MSVC)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4)
    set_target_properties(
      ${CMAKE_PROJECT_NAME}
      PROPERTIES VS_DEBUGGER_COMMAND
                 "${OBS_DIRECTORY}/bin/${_ARCH_SUFFIX}bit/obs${_ARCH_SUFFIX}.exe"
                 VS_DEBUGGER_WORKING_DIRECTORY "${OBS_DIRECTORY}/bin/${_ARCH_SUFFIX}bit")
  endif()

  if(OBS_DIRECTORY)
    add_custom_command(
      TARGET ${CMAKE_PROJECT_NAME}
      POST_BUILD
      COMMAND
        ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        ${OBS_DIRECTORY}/obs-plugins/${_ARCH_SUFFIX}bit/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}>
      COMMAND
        ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${CMAKE_PROJECT_NAME}.pdb
        ${OBS_DIRECTORY}/obs-plugins/${_ARCH_SUFFIX}bit/${CMAKE_PROJECT_NAME}.pdb)
  endif()
  # --- End of section ---

  # -- macOS specific build settings and tasks --
elseif(OS_MACOS)
  configure_file(cmake/bundle/macos/installer-macos.pkgproj.in
                 ${CMAKE_BINARY_DIR}/installer-macos.generated.pkgproj)

  set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
  set(MACOSX_PLUGIN_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}")
  set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")

  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall)
  
  find_library(AUDIO_UNIT AudioUnit)
  if(NOT AUDIO_UNIT)
    message(FATAL_ERROR "AudioUnit not found")
  endif()
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${AUDIO_UNIT})
  # --- End of section ---

  # --- Linux-specific build settings and tasks ---
else()
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall)

  find_package(ALSA REQUIRED)
  if(ALSA_FOUND)
    include_directories(${ALSA_INCLUDE_DIRS})
    message(STATUS ${ALSA_LIBRARIES})
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${ALSA_LIBRARIES})
  endif(ALSA_FOUND)
endif()
# --- End of section ---
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
setup_plugin_target(${CMAKE_PROJECT_NAME})
