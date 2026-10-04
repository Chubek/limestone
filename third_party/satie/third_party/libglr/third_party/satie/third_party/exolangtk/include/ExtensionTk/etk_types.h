/**
 * @file etk_types.h
 * @brief Common ExtensionTk status and boolean types.
 * @stability stable
 * @depends ExtensionTk::platform
 */
#ifndef ETK_TYPES_H
#define ETK_TYPES_H
#include "etk_platform.h"
typedef enum etk_status { ETK_OK = 0, ETK_EINVAL, ETK_ENOMEM, ETK_ENOSYS,
                          ETK_ENOTFOUND, ETK_EVERSION, ETK_EFAIL } etk_status;
typedef itk_bool etk_bool;
#define ETK_TRUE ITK_TRUE
#define ETK_FALSE ITK_FALSE
#endif
