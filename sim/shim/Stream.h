// Minimal Arduino Stream base class (Android build).
#pragma once
#ifndef SIM_STREAM_H
#define SIM_STREAM_H

#include "Print.h"

class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() {}
};

#endif
