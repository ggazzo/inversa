#include "semver.h"
#include <stdlib.h>
#include <string.h>

// Helper function to compare pre-release identifiers
int comparePreRelease(const char* pre1, const char* pre2) {
    // If one has pre-release and other doesn't, the one without is greater
    if (!pre1 && !pre2) return 0;
    if (!pre1) return 1;
    if (!pre2) return -1;
    
    // Split identifiers by dots
    char* pre1_copy = strdup(pre1);
    char* pre2_copy = strdup(pre2);
    
    char* token1 = strtok(pre1_copy, ".");
    char* token2 = strtok(pre2_copy, ".");
    
    while (token1 && token2) {
        // Try to compare as numbers first
        char* end1, *end2;
        long num1 = strtol(token1, &end1, 10);
        long num2 = strtol(token2, &end2, 10);
        
        // If both are numbers, compare numerically
        if (*end1 == '\0' && *end2 == '\0') {
            if (num1 != num2) {
                free(pre1_copy);
                free(pre2_copy);
                return num1 > num2 ? 1 : -1;
            }
        } else {
            // Compare as strings
            int cmp = strcmp(token1, token2);
            if (cmp != 0) {
                free(pre1_copy);
                free(pre2_copy);
                return cmp;
            }
        }
        
        token1 = strtok(NULL, ".");
        token2 = strtok(NULL, ".");
    }
    
    // If one has more identifiers, it's greater
    if (token1 && !token2) {
        free(pre1_copy);
        free(pre2_copy);
        return 1;
    }
    if (!token1 && token2) {
        free(pre1_copy);
        free(pre2_copy);
        return -1;
    }
    
    free(pre1_copy);
    free(pre2_copy);
    return 0;
}

int compareSemVer(const char* version1, const char* version2) {
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    const char* pre1 = NULL;
    const char* pre2 = NULL;
    
    // Parse version1
    char* end;
    major1 = strtol(version1, &end, 10);
    if (*end != '.') return 0;
    minor1 = strtol(end + 1, &end, 10);
    if (*end != '.') return 0;
    patch1 = strtol(end + 1, &end, 10);
    
    // Check for pre-release
    if (*end == '-') {
        pre1 = end + 1;
    }
    
    // Parse version2
    major2 = strtol(version2, &end, 10);
    if (*end != '.') return 0;
    minor2 = strtol(end + 1, &end, 10);
    if (*end != '.') return 0;
    patch2 = strtol(end + 1, &end, 10);
    
    // Check for pre-release
    if (*end == '-') {
        pre2 = end + 1;
    }
    
    // Compare major version
    if (major1 != major2) {
        return major1 > major2 ? 1 : -1;
    }
    
    // Compare minor version
    if (minor1 != minor2) {
        return minor1 > minor2 ? 1 : -1;
    }
    
    // Compare patch version
    if (patch1 != patch2) {
        return patch1 > patch2 ? 1 : -1;
    }
    
    // Compare pre-release versions
    return comparePreRelease(pre1, pre2);
} 