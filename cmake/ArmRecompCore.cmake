set(ARM_RECOMP_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

function(arm_recomp_core_sources out_var profile)
    set(common_sources
        "${ARM_RECOMP_CORE_ROOT}/common/arm_decode.cpp"
        "${ARM_RECOMP_CORE_ROOT}/common/thumb_decode.cpp"
        "${ARM_RECOMP_CORE_ROOT}/common/arm_ir.cpp"
        "${ARM_RECOMP_CORE_ROOT}/common/interpreter.cpp")

    if(profile STREQUAL "armv4t")
        set(sources ${common_sources})
    elseif(profile STREQUAL "armv4t_gba")
        set(sources
            "${ARM_RECOMP_CORE_ROOT}/profiles/armv4t_gba/arm_decode.cpp"
            "${ARM_RECOMP_CORE_ROOT}/profiles/armv4t_gba/thumb_decode.cpp"
            "${ARM_RECOMP_CORE_ROOT}/profiles/armv4t_gba/arm_ir.cpp"
            "${ARM_RECOMP_CORE_ROOT}/profiles/armv4t_gba/interpreter.cpp"
            "${ARM_RECOMP_CORE_ROOT}/profiles/armv4t_gba/arm_codegen.cpp")
    elseif(profile STREQUAL "armv5te_nds")
        set(sources
            ${common_sources}
            "${ARM_RECOMP_CORE_ROOT}/profiles/armv5te_nds/arm_codegen.cpp")
    else()
        message(FATAL_ERROR
            "Unknown arm-recomp-core profile '${profile}'; expected armv4t, armv4t_gba, or armv5te_nds")
    endif()

    set(${out_var} "${sources}" PARENT_SCOPE)
endfunction()

function(arm_recomp_core_include_dirs out_var profile)
    if(profile STREQUAL "armv4t")
        set(include_dirs "${ARM_RECOMP_CORE_ROOT}/common")
    elseif(profile STREQUAL "armv4t_gba")
        set(include_dirs "${ARM_RECOMP_CORE_ROOT}/profiles/armv4t_gba")
    elseif(profile STREQUAL "armv5te_nds")
        set(include_dirs
            "${ARM_RECOMP_CORE_ROOT}/common"
            "${ARM_RECOMP_CORE_ROOT}/profiles/armv5te_nds")
    else()
        message(FATAL_ERROR
            "Unknown arm-recomp-core profile '${profile}'; expected armv4t, armv4t_gba, or armv5te_nds")
    endif()

    set(${out_var} "${include_dirs}" PARENT_SCOPE)
endfunction()
