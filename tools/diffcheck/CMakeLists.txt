# Opt-in (-DMSLC_DIFFCHECK=ON). Every test here exits 77 (SKIP) when the GPU side it
# needs is not on the machine, so enabling the option never makes a build red.
find_library(DIFFCHECK_VULKAN_LIB vulkan HINTS /opt/homebrew/lib /usr/local/lib)
find_path(DIFFCHECK_VULKAN_INCLUDE vulkan/vulkan.h HINTS /opt/homebrew/include /usr/local/include)

set(DIFFCHECK_SRC ${CMAKE_SOURCE_DIR}/tools/diffcheck)
set(DIFFCHECK_TESTS ${CMAKE_SOURCE_DIR}/tests)

add_test(NAME diffcheck/compare COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_compare_test.py)
add_test(NAME diffcheck/inputs COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_inputs_test.py)
add_test(NAME diffcheck/checks COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_checks_test.py)
add_test(NAME diffcheck/graphics COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_graphics_test.py)
add_test(NAME diffcheck/kernel-resources COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_kernel_resources_test.py)

if(DIFFCHECK_VULKAN_LIB AND DIFFCHECK_VULKAN_INCLUDE)
	add_executable(diffcheck-vkhost ${DIFFCHECK_SRC}/vkhost.c)
	target_include_directories(diffcheck-vkhost PRIVATE ${DIFFCHECK_VULKAN_INCLUDE})
	target_link_libraries(diffcheck-vkhost PRIVATE ${DIFFCHECK_VULKAN_LIB})
	add_test(NAME diffcheck/vkhost COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_vkhost_test.py
		$<TARGET_FILE:mslc-cli> $<TARGET_FILE:diffcheck-vkhost> ${DIFFCHECK_TESTS}/diffcheck/fma_kernel.metal)
	set_tests_properties(diffcheck/vkhost PROPERTIES SKIP_RETURN_CODE 77)
	# Half fragment fwidth against host-computed values, with planted miscompiles that must be caught (#173).
	add_test(NAME diffcheck/half-fwidth-expected COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/half_fwidth_expected_test.py
		$<TARGET_FILE:mslc-cli> $<TARGET_FILE:diffcheck-vkhost>)
	set_tests_properties(diffcheck/half-fwidth-expected PROPERTIES SKIP_RETURN_CODE 77)
else()
	message(STATUS "diffcheck: no Vulkan loader or headers, the Vulkan host is not built")
endif()

if(APPLE)
	enable_language(OBJCXX)
	add_executable(diffcheck-metalhost ${DIFFCHECK_SRC}/metalhost.mm)
	target_compile_options(diffcheck-metalhost PRIVATE -fobjc-arc)
	target_link_libraries(diffcheck-metalhost PRIVATE "-framework Metal" "-framework Foundation")
	add_test(NAME diffcheck/metalhost COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_metalhost_test.py
		$<TARGET_FILE:diffcheck-metalhost> ${DIFFCHECK_TESTS}/diffcheck/fma_kernel.metal
		${DIFFCHECK_TESTS}/diffcheck/struct_kernel.metal)
	set_tests_properties(diffcheck/metalhost PROPERTIES SKIP_RETURN_CODE 77)
	add_test(NAME diffcheck/hosts COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_hosts_test.py
		$<TARGET_FILE:diffcheck-metalhost>)
	set_tests_properties(diffcheck/hosts PROPERTIES SKIP_RETURN_CODE 77)
else()
	message(STATUS "diffcheck: not macOS, the Metal host is not built")
endif()

if(TARGET diffcheck-vkhost AND TARGET diffcheck-metalhost)
	add_test(NAME diffcheck/driver COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_driver_test.py
		$<TARGET_FILE:mslc-cli> $<TARGET_FILE:diffcheck-metalhost> $<TARGET_FILE:diffcheck-vkhost>)
	set_tests_properties(diffcheck/driver PROPERTIES SKIP_RETURN_CODE 77)

	# A probe with a "// RUN:" line is also run on both backends. Unlike the disassembly
	# needles, a wrong decoration, an inverted select or a swapped lane changes the result.
	# Graphics probes (tests/diffcheck/graphics) are drawn on both backends the same way.
	function(diffcheck_probe_tests directory prefix)
		file(GLOB diffcheck_probes CONFIGURE_DEPENDS ${directory}/*.metal)
		foreach(probe IN LISTS diffcheck_probes)
			file(STRINGS ${probe} run_line LIMIT_COUNT 1 REGEX "^// RUN:")
			if(run_line)
				get_filename_component(name ${probe} NAME_WE)
				add_test(NAME diffcheck/${prefix}/${name} COMMAND ${PYTHON3} ${DIFFCHECK_SRC}/diffcheck.py ${probe}
					--mslc $<TARGET_FILE:mslc-cli> --metalhost $<TARGET_FILE:diffcheck-metalhost>
					--vkhost $<TARGET_FILE:diffcheck-vkhost>)
				set_tests_properties(diffcheck/${prefix}/${name} PROPERTIES SKIP_RETURN_CODE 77)
			endif()
		endforeach()
	endfunction()
	diffcheck_probe_tests(${CMAKE_SOURCE_DIR}/tests/probes probe)
	diffcheck_probe_tests(${DIFFCHECK_TESTS}/diffcheck/graphics graphics)
	diffcheck_probe_tests(${DIFFCHECK_TESTS}/diffcheck/kernel kernel)

	add_test(NAME diffcheck/kernel-driver COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_kernel_driver_test.py
		$<TARGET_FILE:mslc-cli> $<TARGET_FILE:diffcheck-metalhost> $<TARGET_FILE:diffcheck-vkhost>)
	set_tests_properties(diffcheck/kernel-driver PROPERTIES SKIP_RETURN_CODE 77)

	add_test(NAME diffcheck/denormal COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_denormal_test.py
		$<TARGET_FILE:mslc-cli> $<TARGET_FILE:diffcheck-metalhost> $<TARGET_FILE:diffcheck-vkhost>)
	set_tests_properties(diffcheck/denormal PROPERTIES SKIP_RETURN_CODE 77)

	add_test(NAME diffcheck/graphics-driver COMMAND ${PYTHON3} ${DIFFCHECK_TESTS}/diffcheck_graphics_driver_test.py
		$<TARGET_FILE:mslc-cli> $<TARGET_FILE:diffcheck-metalhost> $<TARGET_FILE:diffcheck-vkhost>)
	set_tests_properties(diffcheck/graphics-driver PROPERTIES SKIP_RETURN_CODE 77)
endif()
