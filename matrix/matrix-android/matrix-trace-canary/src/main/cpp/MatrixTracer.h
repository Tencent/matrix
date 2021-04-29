
// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// MatrixTracer.h

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_MatrixTracer_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_MatrixTracer_H_

bool anrDumpCallback(int status, const char *dumpFile);
bool anrDumpTraceCallback();
bool printTraceCallback();
#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_MatrixTracer_H_