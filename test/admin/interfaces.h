#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include "io_interface.h"
#include "admin_interface.h"
#include "query_interface.h"

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of application names.
 */
//--------------------------------------------------------------------------------------------------
#define LE_LIMIT_APP_NAME_LEN 47

//--------------------------------------------------------------------------------------------------
/**
 * Get the client session reference for the current message
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionRef_t io_GetClientSessionRef(void);
le_msg_SessionRef_t query_GetClientSessionRef(void);

le_result_t le_appInfo_GetName
(
    int32_t  pid,           ///< [IN]  PID of the process.
    char    *appNameStr,    ///< [OUT] Application name buffer.
    size_t   appNameSize    ///< [IN]  Buffer size.
);

#endif