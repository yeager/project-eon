# Validate runtime-read PO catalogs without modifying the source tree.

foreach(required PO_DIRECTORY OUTPUT_DIRECTORY MSGFMT_EXECUTABLE MSGCMP_EXECUTABLE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "l10n validation is missing ${required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
set(catalogs ar de el en_GB es fi fr hi it ja ko nl no pl pt_BR ru sv tr uk zh_CN)
foreach(catalog IN LISTS catalogs)
    set(catalog_path "${PO_DIRECTORY}/${catalog}.po")
    if(NOT EXISTS "${catalog_path}")
        message(FATAL_ERROR "missing required Project Eon catalog: ${catalog_path}")
    endif()
    execute_process(
        COMMAND "${MSGFMT_EXECUTABLE}" --check -o "${OUTPUT_DIRECTORY}/${catalog}.mo" "${catalog_path}"
        RESULT_VARIABLE msgfmt_status)
    if(NOT msgfmt_status EQUAL 0)
        message(FATAL_ERROR "invalid PO syntax or metadata: ${catalog_path}")
    endif()
    execute_process(
        COMMAND "${MSGCMP_EXECUTABLE}" --use-fuzzy "${catalog_path}" "${PO_DIRECTORY}/ProjectEon.pot"
        RESULT_VARIABLE msgcmp_status)
    if(NOT msgcmp_status EQUAL 0)
        message(FATAL_ERROR "catalog does not match ProjectEon.pot: ${catalog_path}")
    endif()
endforeach()

message(STATUS "Project Eon l10n validated: 20 catalogs")
