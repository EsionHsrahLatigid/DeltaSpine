if(NOT DEFINED STAGE_DIR)
    message(FATAL_ERROR "STAGE_DIR is required")
endif()
if(NOT DEFINED SLUG)
    message(FATAL_ERROR "SLUG is required")
endif()
if(NOT DEFINED EXPECT_AU)
    set(EXPECT_AU OFF)
endif()

set(required
    "${STAGE_DIR}/ARTIFACTS.txt"
    "${STAGE_DIR}/vst3/${SLUG}_vst3_plugin.vst3")

if(APPLE)
    list(APPEND required "${STAGE_DIR}/standalone/${SLUG}_standalone_plugin.app")
elseif(WIN32)
    list(APPEND required "${STAGE_DIR}/standalone/${SLUG}_standalone_plugin.exe")
else()
    list(APPEND required "${STAGE_DIR}/standalone/${SLUG}_standalone_plugin")
endif()

if(EXPECT_AU)
    list(APPEND required "${STAGE_DIR}/au/${SLUG}_au_plugin.component")
endif()

foreach(path IN LISTS required)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing staged artifact: ${path}")
    endif()
endforeach()

file(READ "${STAGE_DIR}/ARTIFACTS.txt" manifest)
foreach(token IN ITEMS "Product: DeltaSpine" "Bundle ID: jp.ehl.deltaspine" "Plugin Code: DlSp")
    string(FIND "${manifest}" "${token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR "Manifest missing token: ${token}")
    endif()
endforeach()
