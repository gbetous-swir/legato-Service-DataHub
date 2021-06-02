#include "legato.h"

char* simulateAppName;

//--------------------------------------------------------------------------------------------------
/**
 * Get the client session reference for the current message
 */
//--------------------------------------------------------------------------------------------------
struct le_msg_Session
{
    enum
    {
        LE_MSG_SESSION_UNIX_SOCKET,
        LE_MSG_SESSION_LOCAL
    } type;
};

struct le_msg_Session sessionIo;
le_msg_SessionRef_t io_GetClientSessionRef(void)
{
    sessionIo.type = LE_MSG_SESSION_LOCAL;
    le_msg_SessionRef_t sessionRef = &sessionIo;
    return sessionRef;
}

struct le_msg_Session sessionQuery;
le_msg_SessionRef_t query_GetClientSessionRef(void)
{
    sessionQuery.type = LE_MSG_SESSION_LOCAL;
    le_msg_SessionRef_t sessionRef = &sessionQuery;
    return sessionRef;
}

le_result_t le_appInfo_GetName
(
    int32_t  pid,           ///< [IN]  PID of the process.
    char    *appNameStr,    ///< [OUT] Application name buffer.
    size_t   appNameSize    ///< [IN]  Buffer size.
)
{
    (void)pid;
    (void)appNameSize;
    strcpy(appNameStr, simulateAppName);
    return LE_OK;
}
