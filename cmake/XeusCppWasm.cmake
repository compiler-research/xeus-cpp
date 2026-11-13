# Append one method to the EXPORTED_RUNTIME_METHODS link option already present
# on a target. This preserves methods exported by xeus_wasm_link_options and
# avoids maintaining a duplicate copy of the baseline list.
#
# Sets XEUS_CPP_WASM_EXPORTED_RUNTIME_METHODS in the caller scope so other
# targets (for example tests) can reuse the rewritten option.
function(xeus_cpp_wasm_append_runtime_method target method)
    get_target_property(link_options ${target} LINK_OPTIONS)
    if(NOT link_options)
        message(FATAL_ERROR
            "xeus-cpp: ${target} has no LINK_OPTIONS; "
            "xeus_wasm_link_options() must run first"
        )
    endif()

    set(updated_options)
    set(runtime_methods_option "")
    foreach(option IN LISTS link_options)
        if(option MATCHES "EXPORTED_RUNTIME_METHODS='\\[(.*)\\]'")
            if(runtime_methods_option)
                message(FATAL_ERROR
                    "xeus-cpp: ${target} has multiple EXPORTED_RUNTIME_METHODS options"
                )
            endif()

            # CMAKE_MATCH_1 is the JSON array contents without the brackets.
            string(REPLACE "\"" "" methods "${CMAKE_MATCH_1}")
            string(REPLACE "," ";" methods "${methods}")
            list(TRANSFORM methods STRIP)
            list(APPEND methods "${method}")
            list(REMOVE_DUPLICATES methods)
            string(REPLACE ";" "\",\"" methods "${methods}")
            set(runtime_methods_option
                "SHELL: -s EXPORTED_RUNTIME_METHODS='[\"${methods}\"]'"
            )
            list(APPEND updated_options "${runtime_methods_option}")
        else()
            list(APPEND updated_options "${option}")
        endif()
    endforeach()

    if(NOT runtime_methods_option)
        message(FATAL_ERROR
            "xeus-cpp: ${target} does not export EXPORTED_RUNTIME_METHODS, the xeus wasm link options changed"
        )
    endif()

    set_target_properties(${target} PROPERTIES LINK_OPTIONS "${updated_options}")
    set(XEUS_CPP_WASM_EXPORTED_RUNTIME_METHODS
        "${runtime_methods_option}"
        PARENT_SCOPE
    )
endfunction()
