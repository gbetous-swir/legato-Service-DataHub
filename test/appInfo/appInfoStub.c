#include "legato.h"
#include "interfaces.h"

static int SensorPid = 0;
static int ActuatorPid = 0;
static int ConfigTestPid = 0;

COMPONENT_INIT
{
    le_arg_SetIntVar(&SensorPid, "s", "sensor");
    le_arg_SetIntVar(&ActuatorPid, "a", "actuator");
    le_arg_SetIntVar(&ConfigTestPid, "c", "configTest");
    le_arg_Scan();

    LE_INFO("Starting le_appInfo API stub server.");
}

//--------------------------------------------------------------------------------------------------
/**
 * Gets the state of the specified application.  The state of unknown applications is STOPPED.
 *
 * @return
 *      The state of the specified application.
 */
//--------------------------------------------------------------------------------------------------
le_appInfo_State_t le_appInfo_GetState
(
    const char* appName
        ///< [IN] Application name.
)
{
    LE_WARN("Was asked for the state of app '%s'.", appName);

    return LE_APPINFO_STOPPED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Gets the state of the specified process in an application.  This function only works for
 * configured processes that the Supervisor starts directly.
 *
 * @return
 *      The state of the specified process.
 */
//--------------------------------------------------------------------------------------------------
le_appInfo_ProcState_t le_appInfo_GetProcState
(
    const char* appName,
        ///< [IN] Application name.

    const char* procName
        ///< [IN] Process name.
)
{
    LE_WARN("Was asked for the state of process '%s' in app '%s'.", procName, appName);

    return LE_APPINFO_PROC_STOPPED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Gets the application name of the process with the specified PID.
 *
 * @return
 *      LE_OK if the application name was successfully found.
 *      LE_OVERFLOW if the application name could not fit in the provided buffer.
 *      LE_NOT_FOUND if the process is not part of an application.
 *      LE_FAULT if there was an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_appInfo_GetName
(
    int32_t pid,
        ///< [IN] PID of the process.

    char* appName,
        ///< [OUT] Application name.

    size_t appNameNumElements
        ///< [IN]
)
{
    if (pid == SensorPid)
    {
        LE_DEBUG("Was asked for the name of the sensor process.");
        return le_utf8_Copy(appName, "sensor", appNameNumElements, NULL);
    }

    if (pid == ActuatorPid)
    {
        LE_DEBUG("Was asked for the name of the actuator process.");
        return le_utf8_Copy(appName, "actuator", appNameNumElements, NULL);
    }

    if (pid == ConfigTestPid)
    {
        LE_DEBUG("Was asked for the name of the configTest process.");
        return le_utf8_Copy(appName, "configTest", appNameNumElements, NULL);
    }

    LE_ERROR("Was asked for the name of app running unknown process with PID %d.", pid);

    return LE_NOT_FOUND;
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the application hash as a hexidecimal string.  The application hash is a unique hash of the
 * current version of the application.
 *
 * @return
 *      LE_OK if the application has was successfully retrieved.
 *      LE_OVERFLOW if the application hash could not fit in the provided buffer.
 *      LE_NOT_FOUND if the application is not installed.
 *      LE_FAULT if there was an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_appInfo_GetHash
(
    const char* appName,
        ///< [IN] Application name.

    char* hashStr,
        ///< [OUT] Hash string.

    size_t hashStrNumElements
        ///< [IN]
)
{
    LE_ERROR("Was asked for an app's hash.");

    return LE_FAULT;
}

