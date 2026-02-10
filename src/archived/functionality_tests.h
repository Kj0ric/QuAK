#ifndef QUAK_FUNCTIONALITY_TESTS_H_
#define QUAK_FUNCTIONALITY_TESTS_H_

#include <string>
#include "Automaton.h"

// Basic testing functions
void testReadDomain();
void testSilent(std::string filename);
void testNonNestedRead(const std::string filepath);
void testNestedRead(std::string filepath);
void testNestedConstruction(const std::string& filepath);
void testSilentTransformationNonNested(const std::string& filepath);
void testSilentTransformationNested(const std::string& filepath);

// S_ij construction and analysis functions
void testS_ijConstruction(
    const std::string& filepath, 
    std::size_t child_index, 
    weight_t j, 
    value_function_t g, 
    weight_t bound = -1
);

// Return values computation functions
void testComputeChildReturnValues(const std::string& filepath, value_function_t finVal, weight_t bound);
void testCompareOldVsNewReturnValues(const std::string& filepath, value_function_t finVal, weight_t bound = -1);


// Monitor construction and testing functions
void testConstructMonitors(const std::string& filepath, value_function_t finVal, weight_t bound = -1);
void testAllMonitorsConstruction(const std::string& filepath, value_function_t finVal, weight_t bound = -1);

// Büchi transformation testing function
void testTransformToBuchi(const std::string& filepath, value_function_t finVal, weight_t bound = -1);

void testGenerateMacro(const std::string& filepath);

#endif /* QUAK_FUNCTIONALITY_TESTS_H_ */