include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(test_reflection reflection.cpp)
target_link_libraries(test_reflection PUBLIC cheeky_layer)
add_test(reflection test_reflection)

add_executable(struct_tree struct_tree.cpp)
target_link_libraries(struct_tree PUBLIC cheeky_layer)
add_test(struct_tree struct_tree VkGraphicsPipelineCreateInfo)

add_executable(test_enum_reflection enum_reflection.cpp)
target_link_libraries(test_enum_reflection PUBLIC cheeky_layer)
add_test(enum_reflection test_enum_reflection)

add_executable(test_rule_parser rule_parser.cpp)
target_link_libraries(test_rule_parser PUBLIC cheeky_layer)
add_backward(test_rule_parser)

file(STRINGS rules.txt RULES)
set(COUNTER 1)
foreach(RULE ${RULES})
    add_test(rule_parser_${COUNTER} test_rule_parser "${RULE}")
    MATH(EXPR COUNTER "${COUNTER}+1")
endforeach()

file(STRINGS rules_fail.txt RULES_FAIL)
set(COUNTER 1)
foreach(RULE ${RULES_FAIL})
    add_test(rule_parser_fail_${COUNTER} test_rule_parser ${RULE})
    set_tests_properties(rule_parser_fail_${COUNTER} PROPERTIES WILL_FAIL TRUE)
    MATH(EXPR COUNTER "${COUNTER}+1")
endforeach()

add_executable(test_custom_structs custom_structs.cpp)
target_link_libraries(test_custom_structs PUBLIC cheeky_layer)
add_test(custom_structs test_custom_structs)

add_executable(test_reflection_string reflection_string.cpp)
target_link_libraries(test_reflection_string PUBLIC cheeky_layer)
add_test(reflection_string test_reflection_string)

add_executable(test_reflection_array reflection_array.cpp)
target_link_libraries(test_reflection_array PUBLIC cheeky_layer)
add_test(reflection_array test_reflection_array)

add_executable(test_reflection_flags reflection_flags.cpp)
target_link_libraries(test_reflection_flags PUBLIC cheeky_layer gtest_main)
gtest_discover_tests(test_reflection_flags)

add_executable(test_rule_parser_multiline rule_parser_multiline.cpp)
target_link_libraries(test_rule_parser_multiline PUBLIC cheeky_layer)
add_test(rule_parser_multiline1 test_rule_parser_multiline ${CMAKE_CURRENT_SOURCE_DIR}/rules.txt)
add_test(rule_parser_multiline2 test_rule_parser_multiline ${CMAKE_CURRENT_SOURCE_DIR}/rules_multiline.txt)
add_backward(test_rule_parser_multiline)

add_executable(test_server_socket server_socket.cpp)
target_link_libraries(test_server_socket PUBLIC cheeky_layer)
add_backward(test_server_socket)

add_executable(test_vulkan vulkan_test.cpp)
target_link_libraries(test_vulkan PUBLIC Vulkan::Vulkan)
