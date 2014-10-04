#ifndef TI99_LOGGER_H
#define TI99_LOGGER_H

#include <android/log.h>

#define LOG_INFO(format, args...)    __android_log_print(ANDROID_LOG_INFO, "And99", format, ## args)
#define LOG_ERROR(format, args...)   __android_log_print(ANDROID_LOG_ERROR, "And99", format, ## args)

#endif
