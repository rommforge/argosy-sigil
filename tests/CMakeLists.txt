# SPDX-License-Identifier: MPL-2.0
# Test harness — both synthetic unit tests and real-ROM integration tests.

# Integration tests are gated on SIGIL_ROM_DIR (and SIGIL_PROD_KEYS for Switch);
# they skip cleanly when the env vars aren't set so the public CI without
# ROMs can still pass.

# Helper: define a test that links against sigil and reports skip vs fail
# uniformly via exit code conventions.
function(sigil_add_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE sigil)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME ${name} COMMAND ${name})
    # Skip is signaled by exit code 77 (autotools convention).
    set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
endfunction()

sigil_add_test(integration_psp)
sigil_add_test(integration_psx)
sigil_add_test(integration_ps2)
sigil_add_test(integration_wii)
sigil_add_test(integration_gamecube)
sigil_add_test(integration_psvita)
