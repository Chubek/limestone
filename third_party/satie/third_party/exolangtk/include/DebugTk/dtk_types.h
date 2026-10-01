/**
 * @file dtk_types.h
 * @brief Common DebugTk status values.
 * @stability stable
 * @depends DebugTk::platform
 */
#ifndef DTK_TYPES_H
#define DTK_TYPES_H
#include "dtk_platform.h"
typedef enum dtk_status { DTK_OK = 0, DTK_EINVAL, DTK_ENOMEM, DTK_ENOSYS,
                          DTK_ENOTFOUND, DTK_EFAIL } dtk_status;
typedef itk_bool dtk_bool;
#define DTK_TRUE ITK_TRUE
#define DTK_FALSE ITK_FALSE
#endif
