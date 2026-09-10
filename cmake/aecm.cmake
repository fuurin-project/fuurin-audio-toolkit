# SPDX-License-Identifier: Apache-2.0
set(ZAT_AECM_DIR "${FUURIN_DEP_ROOT}/modules/lib/webrtc-aecm" CACHE PATH "cpuimage WebRTC_AECM source")
if(NOT EXISTS "${ZAT_AECM_DIR}/aecm/echo_control_mobile.cc")
  message(FATAL_ERROR "Missing WebRTC_AECM at ${ZAT_AECM_DIR}; run west update webrtc-aecm")
endif()

# Apply the port in the build tree, keeping pinned upstream sources untouched.
# Prefix every WebRtc symbol: libfvad's similarly named SPL functions do not
# necessarily have the same ABI and must not resolve to this implementation.
set(ZAT_AECM_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/aecm-port")
file(MAKE_DIRECTORY "${ZAT_AECM_GENERATED}")
function(zat_aecm_replace variable before after)
  string(FIND "${${variable}}" "${before}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "AECM port patch no longer matches upstream: ${before}")
  endif()
  string(REPLACE "${before}" "${after}" value "${${variable}}")
  set(${variable} "${value}" PARENT_SCOPE)
endfunction()

file(GLOB aecm_files CONFIGURE_DEPENDS "${ZAT_AECM_DIR}/aecm/*.h"
  "${ZAT_AECM_DIR}/aecm/*.c" "${ZAT_AECM_DIR}/aecm/*.cc")
set(aecm_sources)
foreach(path IN LISTS aecm_files)
  get_filename_component(name "${path}" NAME)
  if(name MATCHES "aecm_core_(neon|mips)\\.cc$")
    continue()
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
  file(READ "${path}" content)
  string(REGEX REPLACE "WebRtc[A-Za-z0-9_]+" "ZatAecm_\\0" content "${content}")
  if(name STREQUAL "echo_control_mobile.cc")
    zat_aecm_replace(content "calloc(1, sizeof(AecMobile)));"
      "calloc(1, sizeof(AecMobile)));\n    if (!aecm) return NULL;")
  elseif(name STREQUAL "aecm_core.cc")
    zat_aecm_replace(content "calloc(1, sizeof(AecmCore)));"
      "calloc(1, sizeof(AecmCore)));\n    if (!aecm) return NULL;")
    # Creation is serialized by the adapter. Reset must not rewrite shared
    # function pointers while another initialized instance processes audio.
    zat_aecm_replace(content
      "ZatAecm_WebRtcAecm_CalcLinearEnergies = CalcLinearEnergiesC;\n    ZatAecm_WebRtcAecm_StoreAdaptiveChannel = StoreAdaptiveChannelC;\n    ZatAecm_WebRtcAecm_ResetAdaptiveChannel = ResetAdaptiveChannelC;"
      "static bool scalar_functions_initialized = false;\n    if (!scalar_functions_initialized) {\n        ZatAecm_WebRtcAecm_CalcLinearEnergies = CalcLinearEnergiesC;\n        ZatAecm_WebRtcAecm_StoreAdaptiveChannel = StoreAdaptiveChannelC;\n        ZatAecm_WebRtcAecm_ResetAdaptiveChannel = ResetAdaptiveChannelC;\n        scalar_functions_initialized = true;\n    }")
  elseif(name STREQUAL "delay_estimator.cc")
    zat_aecm_replace(content "#include <algorithm>" "#include \"aecm_compat.h\"")
    zat_aecm_replace(content "std::any_of" "zat_any_of")
  endif()
  if(name MATCHES "\\.(c|cc)$")
    foreach(fn malloc calloc realloc free)
      string(REPLACE "${fn}(" "zat_aecm_${fn}(" content "${content}")
    endforeach()
    string(PREPEND content "/* Generated port: see toolkit cmake/aecm.cmake. */\n#include \"aecm_alloc.h\"\n")
    list(APPEND aecm_sources "${ZAT_AECM_GENERATED}/${name}")
  endif()
  file(WRITE "${ZAT_AECM_GENERATED}/${name}.in" "${content}")
  configure_file("${ZAT_AECM_GENERATED}/${name}.in"
    "${ZAT_AECM_GENERATED}/${name}" COPYONLY)
endforeach()

add_library(zat_webrtc_aecm STATIC ${aecm_sources} ${ZAT_ROOT}/src/aecm_alloc.c)
target_include_directories(zat_webrtc_aecm PUBLIC "${ZAT_AECM_GENERATED}"
  PRIVATE ${ZAT_ROOT}/src ${CMAKE_CURRENT_LIST_DIR})
target_compile_features(zat_webrtc_aecm PRIVATE c_std_11 cxx_std_11)
target_compile_options(zat_webrtc_aecm PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>)
