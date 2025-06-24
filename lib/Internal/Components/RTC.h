#ifndef IRTC_H
#define IRTC_H

#include "Base.h"

class IRTC : public Base {
    public:
        virtual uint32_t now() = 0;
        virtual void setTime(char* isoDate) = 0;
};

#endif