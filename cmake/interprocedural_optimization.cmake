function(enable_lto target)
    if(KB_ENABLE_LTO)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
        
        if(ipo_supported)
            set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
            message(STATUS "LTO enabled for ${target}")
        else()
            message(WARNING "LTO is not supported: ${ipo_error}")
        endif()
        
        # Set linker preference as a link option on the interface target
        target_link_options(${target} INTERFACE
            $<$<CXX_COMPILER_ID:Clang,GNU>:-fuse-ld=gold>
        )
    endif()
endfunction()