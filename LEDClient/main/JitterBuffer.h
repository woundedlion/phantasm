#pragma once
#include "RingBuffer.h"
#include "SPI.h"
#include "Types.h"


const int JITTER_BUFFER_DEPTH = 16;
const int W = 288;
const int H = 144;
const int STRIP_H = 48;

typedef RingBuffer<RGB[W * STRIP_H], JITTER_BUFFER_DEPTH> JitterBuffer;