
#include "estimated.h"
unsigned long estimatedCounter_seconds = 0;

void setEstimatedTime(unsigned long seconds) {
    estimatedCounter_seconds = seconds;
}
unsigned long getEstimatedTime() {
    return estimatedCounter_seconds;
}
