# One executable per test class, as Qt Test expects.
function(add_core_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE inventory_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_core_test(tst_database)
add_core_test(tst_inventory)
add_core_test(tst_alerts)
add_core_test(tst_inputclassifier)
add_core_test(tst_backup)

# The GUI test needs the Widgets UI library. "offscreen" lets it run
# without a visible screen (for example on a build server).
add_executable(tst_mainwindow tst_mainwindow.cpp)
target_link_libraries(tst_mainwindow PRIVATE inventory_ui Qt6::Test)
add_test(NAME tst_mainwindow COMMAND tst_mainwindow)
set_tests_properties(tst_mainwindow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
