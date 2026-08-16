if(NOT DEFINED STAGE_DIR)
    message(FATAL_ERROR "STAGE_DIR is required")
endif()
if(NOT DEFINED SLUG)
    message(FATAL_ERROR "SLUG is required")
endif()
if(NOT DEFINED EXPECT_AU)
    set(EXPECT_AU OFF)
endif()
if(NOT DEFINED EXPECT_VERSION)
    set(EXPECT_VERSION "0.1.1")
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
foreach(token IN ITEMS "Product: DeltaSpine" "Version: ${EXPECT_VERSION}" "Bundle ID: jp.ehl.deltaspine" "Plugin Code: DlSp")
    string(FIND "${manifest}" "${token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR "Manifest missing token: ${token}")
    endif()
endforeach()

if(EXPECT_AU)
    set(au_info_plist "${STAGE_DIR}/au/${SLUG}_au_plugin.component/Contents/Info.plist")
    if(NOT EXISTS "${au_info_plist}")
        message(FATAL_ERROR "Missing AU Info.plist: ${au_info_plist}")
    endif()

    file(READ "${au_info_plist}" au_plist)
    foreach(token IN ITEMS
            "<key>sandboxSafe</key>"
            "<key>CFBundleShortVersionString</key>"
            "<string>${EXPECT_VERSION}</string>"
            "<key>CFBundleVersion</key>")
        string(FIND "${au_plist}" "${token}" token_index)
        if(token_index EQUAL -1)
            message(FATAL_ERROR "AU Info.plist missing token: ${token}")
        endif()
    endforeach()

    foreach(forbidden IN ITEMS
            "<key>resourceUsage</key>"
            "<key>network.client</key>"
            "<key>temporary-exception.files.all.read-write</key>")
        string(FIND "${au_plist}" "${forbidden}" forbidden_index)
        if(NOT forbidden_index EQUAL -1)
            message(FATAL_ERROR "AU Info.plist contains over-broad resourceUsage token: ${forbidden}")
        endif()
    endforeach()
endif()
