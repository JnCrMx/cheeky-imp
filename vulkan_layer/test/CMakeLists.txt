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

file(STRINGS rules.txt RULES)
set(COUNTER 1)
foreach(RULE ${RULES})
    add_test(rule_parser_${COUNTER} test_rule_parser ${RULE})
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
