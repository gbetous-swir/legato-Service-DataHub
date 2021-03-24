//--------------------------------------------------------------------------------------------------
/**
 * @file parser_json.c
 *
 *  Parsing a file with json format.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "dataHub.h"
#include "resource.h"
#include "obs.h"
#include "dataSample.h"
#include "ioService.h"

#include "parser.h"

//--------------------------------------------------------------------------------------------------
/**
 *  Temporary storage to use during parse.
 */
//--------------------------------------------------------------------------------------------------
typedef union TempStorage
{
    parser_ObsData_t o;
    parser_StateData_t s;
} tempStorage_t;

//--------------------------------------------------------------------------------------------------
/**
 *  Holds the parse environment parameters.
 */
//--------------------------------------------------------------------------------------------------
typedef struct ParseEnv
{
    int fd;                                 ///< File descriptor.
    parser_Callbacks_t* callbacksPtr;       ///< Callbacks structure
    tempStorage_t tempStorage;              ///< Temp storage for observation or state data.
    bool stopped;                           ///< Whether the parse has been stopped by the client.
    le_json_EventHandler_t fallbackHandler; ///< Sometimes we have to ignore an entire json object,
                                            /// this field holds a handler to use when we are
                                            /// finished with that object.
    void* context;                          ///< User context.
} ParseEnv_t;

//--------------------------------------------------------------------------------------------------
/**
 * Reference to current parse session.
 *
 * Only support one parse session now.
 */
//--------------------------------------------------------------------------------------------------
static Parser_ParseSessionRef_t CurrParseSessionRef;

//--------------------------------------------------------------------------------------------------
/**
 *  Prototypes for le_json handlers:
 */
//--------------------------------------------------------------------------------------------------
static void IgnoreValueHandler        (le_json_Event_t event);
static void ExpectObsResourcePath     (le_json_Event_t event);
static void ExpectObsDestination      (le_json_Event_t event);
static void ExpectObsMinPeriod        (le_json_Event_t event);
static void ExpectObsChangeBy         (le_json_Event_t event);
static void ExpectObsLowerThan        (le_json_Event_t event);
static void ExpectObsGreaterThan      (le_json_Event_t event);
static void ExpectObsMaxBuffer        (le_json_Event_t event);
static void ExpectObsTransformFunction(le_json_Event_t event);
static void ExpectObsJsonExtraction   (le_json_Event_t event);
static void ExpectObsMember           (le_json_Event_t event);
static void ExpectOneObsStart         (le_json_Event_t event);
static void ExpectObsName             (le_json_Event_t event);
static void ExpectObservationsStart   (le_json_Event_t event);
static void ExpectStateValue          (le_json_Event_t event);
static void ExpectStateDataType       (le_json_Event_t event);
static void ExpectStateMember         (le_json_Event_t event);
static void ExpectOneStateStart       (le_json_Event_t event);
static void ExpectStateName           (le_json_Event_t event);
static void ExpectStatesStart         (le_json_Event_t event);
static void ExpectConfigStart         (le_json_Event_t event);
static void ExpectRootMemberName      (le_json_Event_t event);


//--------------------------------------------------------------------------------------------------
/**
 *  parser helper function for going to next state
 */
//--------------------------------------------------------------------------------------------------
static void GoToNextState
(
    le_json_EventHandler_t nextHandler                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (parseEnvPtr->stopped)
    {
        // if we're stopped, clean up the parser, that also stops it.
        le_json_Cleanup(le_json_GetSession());
    }
    else
    {
        le_json_SetEventHandler(nextHandler);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Ignore next value:
 */
//--------------------------------------------------------------------------------------------------
static void IgnoreNextValue
(
    le_json_EventHandler_t fallbackHandler      ///< [IN] The le_json event hander to fall back on.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    parseEnvPtr->fallbackHandler = fallbackHandler;
    GoToNextState(IgnoreValueHandler);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Handles an error that happened during parse
 */
//--------------------------------------------------------------------------------------------------
static void HandleError
(
    le_result_t error,                          ///< [IN] result code
    const char* msg                             ///< [IN] error message
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (parseEnvPtr->callbacksPtr->error)
    {
        parseEnvPtr->callbacksPtr->error(error, msg, parseEnvPtr->context);
    }
    // end the parse:
    le_json_Cleanup(le_json_GetSession());
}

//--------------------------------------------------------------------------------------------------
/**
 *  Clear the temporary storage.
 */
//--------------------------------------------------------------------------------------------------
static void ClearTempStorage
(
    tempStorage_t* tempStrPtr                          ///< [IN] pointer to temp storage structure
)
{
    memset(tempStrPtr, 0, sizeof(tempStorage_t));
}

//--------------------------------------------------------------------------------------------------
/**
 *  Error handler given to le_json for error events.
 */
//--------------------------------------------------------------------------------------------------
static void ErrorEventHandler
(
    le_json_Error_t error,
    const char* msg
)
{
    HandleError((error == LE_JSON_READ_ERROR)? LE_IO_ERROR: LE_FORMAT_ERROR, msg);
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that ignores the value.
 */
//--------------------------------------------------------------------------------------------------
static void IgnoreValueHandler
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    static int collectionLayer = 0;
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();

    if (event == LE_JSON_OBJECT_START)
    {
        collectionLayer++;
    }
    else if (event == LE_JSON_OBJECT_END)
    {
        collectionLayer--;
    }

    if (collectionLayer == 0)
    {
        // reached the end of this object:
        //get back to the same state
        GoToNextState(parseEnvPtr->fallbackHandler);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "r" member of an observation.
 * This field holds the source resource.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsResourcePath
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_STRING)
    {
        const char* resourcePath = le_json_GetString();
        if (hub_IsResourcePathMalformed(resourcePath) ||
            strnlen(resourcePath, PARSER_OBS_RES_MAX_BYTES)>=PARSER_OBS_RES_MAX_BYTES)
        {
            HandleError(LE_BAD_PARAMETER, "resource path is invalid");
        }
        else
        {
            // set the bitmask to we know we've received this field:
            parseEnvPtr->tempStorage.o.bitmask |= PARSER_OBS_RESOURCE_MASK;
            // cache the value in temp storage:
            strncpy(parseEnvPtr->tempStorage.o.resourcePath,
                    resourcePath,
                    PARSER_OBS_RES_MAX_BYTES);

            GoToNextState(ExpectObsMember);
        }
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "d" member of an observation.
 * This field holds the destination
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsDestination
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_STRING)
    {
        const char* destination = le_json_GetString();

        if (destination[0] == '/' && (hub_IsResourcePathMalformed(destination) ||
                                      strnlen(destination, PARSER_OBS_DEST_MAX_BYTES) >=
                                            PARSER_OBS_DEST_MAX_BYTES))
        {
            // if destination start with '/', then it's going to be interpreted as a resource path.
            // so if the path is invalid then destination cannot be set.
            HandleError(LE_BAD_PARAMETER, "obs destination is invalid");
        }
        else
        {
            // set the bitmask to we know we've received this field:
            parseEnvPtr->tempStorage.o.bitmask |= PARSER_OBS_DEST_MASK;
            // cache the value in temp storage:
            strncpy(parseEnvPtr->tempStorage.o.destination,
                    destination,
                    PARSER_OBS_DEST_MAX_BYTES);

            GoToNextState(ExpectObsMember);
        }
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "p" member of an observation.
 * This field holds the minimum period of an observation.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsMinPeriod
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_NUMBER)
    {
        // set the bitmask to we know we've received this field:
        parseEnvPtr->tempStorage.o.bitmask |= PARSER_OBS_PERIOD_MASK;
        // cache the value in temp storage:
        parseEnvPtr->tempStorage.o.minPeriod = le_json_GetNumber();

        GoToNextState(ExpectObsMember);
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "st" member of an observation.
 * This field holds the change by value of an observation.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsChangeBy
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_NUMBER)
    {
        // set the bitmask to we know we've received this field:
        parseEnvPtr->tempStorage.o.bitmask |= PARSER_OBS_CHANGEBY_MASK;
        // cache the value in temp storage:
        parseEnvPtr->tempStorage.o.changeBy = le_json_GetNumber();

        GoToNextState(ExpectObsMember);
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "lt" member of an observation.
 * This field holds the High Limit of an observation(must be lower than this value)
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsLowerThan
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_NUMBER)
    {
        // set the bitmask to we know we've received this field:
        parseEnvPtr->tempStorage.o.bitmask |= PARSER_OBS_LOWERTHAN_MASK;
        // cache the value in temp storage:
        parseEnvPtr->tempStorage.o.lowerThan = le_json_GetNumber();

        GoToNextState(ExpectObsMember);
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "gt" member of an observation.
 * This field holds the Low Limit of an observation(must be greater than this value)
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsGreaterThan
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_NUMBER)
    {
        // set the bitmask to we know we've received this field:
        parseEnvPtr->tempStorage.o.bitmask |= PARSER_OBS_GREATERTHAN_MASK;
        // cache the value in temp storage:
        parseEnvPtr->tempStorage.o.greaterThan = le_json_GetNumber();

        GoToNextState(ExpectObsMember);
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "b" member of an observation.
 * This field holds the maximum buffer count value of an observation.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsMaxBuffer
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_NUMBER)
    {
        // set the bitmask to we know we've received this field:
        parseEnvPtr->tempStorage.o.bitmask |= PARSER_OBS_BUFFER_MASK;
        // cache the value in temp storage:
        parseEnvPtr->tempStorage.o.bufferMaxCount = le_json_GetNumber();

        GoToNextState(ExpectObsMember);
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert function text to transform type:
 *
 * @note:
 * we won't validate the function to keep the current behavior, if text is unknown,
 * ADMIN_OBS_TRANSFORM_TYPE_NONE will be applied.
 * @return
 *      obs transform type.
 */
//--------------------------------------------------------------------------------------------------
static admin_TransformType_t FunctionToTransformType
(
    const char* function            ///< [IN] transform function name in the config file.
)
{
    if (strncmp(function, "mean", PARSER_OBS_TRANSFORM_MAX_BYTES) == 0)
    {
        return ADMIN_OBS_TRANSFORM_TYPE_MEAN;
    }
    else if (strncmp(function, "stddev", PARSER_OBS_TRANSFORM_MAX_BYTES) == 0)
    {
        return ADMIN_OBS_TRANSFORM_TYPE_STDDEV;
    }
    else if (strncmp(function, "min", PARSER_OBS_TRANSFORM_MAX_BYTES) == 0)
    {
        return ADMIN_OBS_TRANSFORM_TYPE_MIN;
    }
    else if (strncmp(function, "max", PARSER_OBS_TRANSFORM_MAX_BYTES) == 0)
    {
        return ADMIN_OBS_TRANSFORM_TYPE_MAX;
    }
    return ADMIN_OBS_TRANSFORM_TYPE_NONE;
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "f" member of an observation.
 * This field holds the transform value of an observation.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsTransformFunction
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_STRING)
    {
        const char* transform = le_json_GetString();

        if (strnlen(transform, PARSER_OBS_TRANSFORM_MAX_BYTES) >=
            PARSER_OBS_TRANSFORM_MAX_BYTES)
        {
            HandleError(LE_BAD_PARAMETER, "obs transform is invalid");
            return;
        }

        // set the bitmask so we know we've received this field:
        parseEnvPtr->tempStorage.o.bitmask |= PARSER_OBS_TRANSFORM_MASK;
        // cache the value in temp storage:
        parseEnvPtr->tempStorage.o.transform = FunctionToTransformType(transform);

        GoToNextState(ExpectObsMember);
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "s" member of an observation.
 * This field holds the json extraction of an observation.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsJsonExtraction
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_STRING)
    {
        const char* jsonExtraction = le_json_GetString();
        if (strnlen(jsonExtraction, PARSER_OBS_JSON_EX_MAX_BYTES) >=
            PARSER_OBS_JSON_EX_MAX_BYTES)
        {
            HandleError(LE_BAD_PARAMETER, "jsonExtraction is too long");
            return;
        }
        // set the bitmask to we know we've received this field:
        parseEnvPtr->tempStorage.o.bitmask |= PARSER_OBS_JSON_EXT_MASK;
        // cache the value in temp storage:
        le_utf8_Copy(parseEnvPtr->tempStorage.o.jsonExtraction,
                     jsonExtraction,
                     PARSER_OBS_JSON_EX_MAX_BYTES, NULL);

        GoToNextState(ExpectObsMember);
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Helper function to handle the observation member.
 */
//--------------------------------------------------------------------------------------------------
static void ObsMemberHelper
(
    const char* memberName                          ///< [IN] obs member key.
)
{
    if (strcmp(memberName, "r") == 0)
    {
       GoToNextState(ExpectObsResourcePath);
    }
    else if (strcmp(memberName, "d") == 0)
    {
       GoToNextState(ExpectObsDestination);
    }
    else if (strcmp(memberName, "p") == 0)
    {
        GoToNextState(ExpectObsMinPeriod);
    }
    else if (strcmp(memberName, "st") == 0)
    {
        GoToNextState(ExpectObsChangeBy);
    }
    else if (strcmp(memberName, "lt") == 0)
    {
        GoToNextState(ExpectObsLowerThan);
    }
    else if (strcmp(memberName, "gt") == 0)
    {
        GoToNextState(ExpectObsGreaterThan);
    }
    else if (strcmp(memberName, "b") == 0)
    {
        GoToNextState(ExpectObsMaxBuffer);
    }
    else if (strcmp(memberName, "f") == 0)
    {
        GoToNextState(ExpectObsTransformFunction);
    }
    else if (strcmp(memberName, "s") == 0)
    {
        GoToNextState(ExpectObsJsonExtraction);
    }
    else
    {
        //unexpected member, ignore this member:
        IgnoreNextValue(ExpectObsMember);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects a member key in an observation object
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsMember
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_OBJECT_MEMBER)
    {
        const char* memberName = le_json_GetString(); // Name tells you which field it is.
        ObsMemberHelper(memberName);
    }
    else if (event == LE_JSON_OBJECT_END)
    {
        // finished with this obs:
        // did we get all we expect:
        if ((parseEnvPtr->tempStorage.o.bitmask & PARSER_OBS_RESOURCE_MASK) &&
            (parseEnvPtr->tempStorage.o.bitmask & PARSER_OBS_DEST_MASK))
        {
            // have what we need:
            // set missing fields:
            if (!(parseEnvPtr->tempStorage.o.bitmask & PARSER_OBS_PERIOD_MASK))
            {
                parseEnvPtr->tempStorage.o.minPeriod = NAN;
            }
            if (!(parseEnvPtr->tempStorage.o.bitmask & PARSER_OBS_CHANGEBY_MASK))
            {
                parseEnvPtr->tempStorage.o.changeBy = NAN;
            }
            if (!(parseEnvPtr->tempStorage.o.bitmask & PARSER_OBS_LOWERTHAN_MASK))
            {
                parseEnvPtr->tempStorage.o.lowerThan = NAN;
            }
            if (!(parseEnvPtr->tempStorage.o.bitmask & PARSER_OBS_GREATERTHAN_MASK))
            {
                parseEnvPtr->tempStorage.o.greaterThan = NAN;
            }
            if (!(parseEnvPtr->tempStorage.o.bitmask & PARSER_OBS_BUFFER_MASK))
            {
                parseEnvPtr->tempStorage.o.bufferMaxCount = 0;
            }
            if (!(parseEnvPtr->tempStorage.o.bitmask & PARSER_OBS_TRANSFORM_MASK))
            {
                parseEnvPtr->tempStorage.o.transform = ADMIN_OBS_TRANSFORM_TYPE_NONE;
            }
            if (!(parseEnvPtr->tempStorage.o.bitmask & PARSER_OBS_JSON_EXT_MASK))
            {
                parseEnvPtr->tempStorage.o.jsonExtraction[0] = '\0';
            }
            // call the callback:
            if (parseEnvPtr->callbacksPtr->observation)
            {
                parseEnvPtr->callbacksPtr->observation(&(parseEnvPtr->tempStorage.o),
                        parseEnvPtr->context);
            }
            // just need to clear the temp storage for the next observation.
            ClearTempStorage(&(parseEnvPtr->tempStorage));
            GoToNextState(ExpectObsName);
        }
        else
        {
            // did not get all mandatory fields.
            char msg[PARSER_MAX_ERROR_MSG_BYTES] = {0};
            snprintf(msg, PARSER_MAX_ERROR_MSG_BYTES,
                     "observation %.10s did not have both r and d",
                     parseEnvPtr->tempStorage.o.obsName);
            HandleError(LE_FORMAT_ERROR, msg);

        }
   }
   else
   {
       // unexpected case:
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
   }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the start an observation object
 */
//--------------------------------------------------------------------------------------------------
static void ExpectOneObsStart
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
   if (event == LE_JSON_OBJECT_START)
   {
       // got what we expect, move on to next step:
       GoToNextState(ExpectObsMember);
   }
   else
   {
       // unexpected case:
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
   }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the name of an observation which is the key for the
 * observation object.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObsName
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_OBJECT_MEMBER)
    {
        const char* obsName = le_json_GetString();

        if (hub_IsResourcePathMalformed(obsName) ||
            strnlen(obsName, PARSER_OBSNAME_MAX_BYTES) >= PARSER_OBSNAME_MAX_BYTES)
        {
            // obsName is malformed:
            HandleError(LE_BAD_PARAMETER, "observation name is invalid");
        }
        else
        {
            // cache obsName in temp storage:
            le_utf8_Copy(parseEnvPtr->tempStorage.o.obsName, obsName,
                PARSER_OBSNAME_MAX_BYTES, NULL);
            // got what we expect, move on to next step:
            GoToNextState(ExpectOneObsStart);
        }
    }
    else if (event == LE_JSON_OBJECT_END)
    {
        // finished with all observations:
        if (parseEnvPtr->callbacksPtr->oObjectEnd)
        {
            parseEnvPtr->callbacksPtr->oObjectEnd(parseEnvPtr->context);
        }
        GoToNextState(ExpectRootMemberName);
    }
    else
    {
        // unexpected case:
        //HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the start of the "o" object.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectObservationsStart
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_OBJECT_START)
    {
        if (parseEnvPtr->callbacksPtr->oObject)
        {
            parseEnvPtr->callbacksPtr->oObject(parseEnvPtr->context);
        }
        // got what we expect, move on to next step:
        GoToNextState(ExpectObsName);
    }
    else
    {
        // unexpected case:
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the file's type, "t" in root.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectType
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_NUMBER)
    {
        if (parseEnvPtr->callbacksPtr->type)
        {
            parseEnvPtr->callbacksPtr->type(le_json_GetNumber(), parseEnvPtr->context);
        }
        // got what we expect, move on to next step:
        GoToNextState(ExpectRootMemberName);
    }
    else
    {
        // unexpected case:
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the file's version, "v" in root.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectVersion
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_STRING)
    {
        if (parseEnvPtr->callbacksPtr->version)
        {
            parseEnvPtr->callbacksPtr->version(le_json_GetString(), parseEnvPtr->context);
        }
        // got what we expect, move on to next step:
        GoToNextState(ExpectRootMemberName);
    }
    else
    {
        // unexpected case:
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the file's timestamp, "ts" in root.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectTimeStamp
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_NUMBER)
    {
        if (parseEnvPtr->callbacksPtr->timeStamp)
        {
            parseEnvPtr->callbacksPtr->timeStamp(le_json_GetNumber(), parseEnvPtr->context);
        }
        // got what we expect, move on to next step:
        GoToNextState(ExpectRootMemberName);
    }
    else
    {
        // unexpected case:
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects a key in the root config file
 */
//--------------------------------------------------------------------------------------------------
static void ExpectRootMemberName
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_OBJECT_MEMBER)
    {
        const char* memberName = le_json_GetString();
        if (strcmp(memberName, "o") == 0)
        {
            // observations:
            GoToNextState(ExpectObservationsStart);
        }
        else if (strcmp(memberName, "s") == 0)
        {
            // states:
            GoToNextState(ExpectStatesStart);
        }
        else if (strcmp(memberName, "a") == 0)
        {
            // actions
            // not supported yet, ignoring for now.
            IgnoreNextValue(ExpectRootMemberName);
        }
        else if (strcmp(memberName, "t") == 0)
        {
            // type
            IgnoreNextValue(ExpectType);
        }
        else if (strcmp(memberName, "v") == 0)
        {
            // version:
            IgnoreNextValue(ExpectVersion);
        }
        else if (strcmp(memberName, "ts") == 0)
        {
            // timestamp
            IgnoreNextValue(ExpectTimeStamp);
        }
        else
        {
            // unknown root object member, we will ignore this entire object.
            IgnoreNextValue(ExpectRootMemberName);
        }
    }
    else if (event == LE_JSON_OBJECT_END)
    {
        if (parseEnvPtr->callbacksPtr->endOfParse)
        {
            parseEnvPtr->callbacksPtr->endOfParse(parseEnvPtr->context);
        }
        le_json_Cleanup(le_json_GetSession());
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "v" member of a state.
 * This field holds the value of a state.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectStateValue
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    switch(event)
    {
        case (LE_JSON_NUMBER):
        {
            parseEnvPtr->tempStorage.s.dataType = IO_DATA_TYPE_NUMERIC;
            parseEnvPtr->tempStorage.s.value.number = le_json_GetNumber();
            break;
        }
        case (LE_JSON_TRUE):
        case (LE_JSON_FALSE):
        {
            parseEnvPtr->tempStorage.s.dataType = IO_DATA_TYPE_BOOLEAN;
            parseEnvPtr->tempStorage.s.value.boolean = (event == LE_JSON_TRUE);
            break;
        }
        case (LE_JSON_STRING):
        {
            if (parseEnvPtr->tempStorage.s.dataType != IO_DATA_TYPE_JSON)
            {
                // if we haven't gotten the "dt" field yet, then we'll assume this string is just
                // a string type. If there was a "dt" later, then dataType will be corrected.
                parseEnvPtr->tempStorage.s.dataType = IO_DATA_TYPE_STRING;
            }
            if (strnlen(le_json_GetString(), PARSER_STATE_MAX_STRING_BYTES) >=
                PARSER_STATE_MAX_STRING_BYTES)
            {
                HandleError(LE_BAD_PARAMETER, "String value is too long.");
                return;
            }
            le_utf8_Copy(parseEnvPtr->tempStorage.s.value.string,
                         le_json_GetString(),
                         PARSER_STATE_MAX_STRING_BYTES, NULL);
            break;
        }
        default:
        {
            HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
            return;
        }
    }
    parseEnvPtr->tempStorage.s.bitmask |= PARSER_STATE_VALUE_MASK;
    GoToNextState(ExpectStateMember);
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the "dt" member of a state.
 * This field holds the data type of a state.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectStateDataType
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_STRING)
    {
        // anything other than "json" will be ignored.
        if (strcmp(le_json_GetString(), "json") == 0)
        {
            parseEnvPtr->tempStorage.s.dataType = IO_DATA_TYPE_JSON;
            parseEnvPtr->tempStorage.s.bitmask |= PARSER_STATE_DATATYPE_MASK;
            GoToNextState(ExpectStateMember);
        }
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Helper function to handle a field in the state object.
 */
//--------------------------------------------------------------------------------------------------
static void StateMemberHelper
(
    const char* memberName                          ///< [IN] The state member key
)
{
    if (strcmp(memberName, "v") == 0)
    {
        GoToNextState(ExpectStateValue);
    }
    else if (strcmp(memberName, "dt") == 0)
    {
        GoToNextState(ExpectStateDataType);
    }
    else
    {
        IgnoreNextValue(ExpectStateMember);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects a key in a state object.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectStateMember
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_OBJECT_MEMBER)
    {
        const char* memberName = le_json_GetString(); // Name tells you which field it is.
        StateMemberHelper(memberName);
    }
    else if (event == LE_JSON_OBJECT_END)
    {
        // finished with this state:
        // did we get all the required fields for a state:
        if (parseEnvPtr->tempStorage.s.bitmask & PARSER_STATE_VALUE_MASK)
        {
            // we have all we need:
            // call the callback:
            if (parseEnvPtr->callbacksPtr->state)
            {
                parseEnvPtr->callbacksPtr->state(&(parseEnvPtr->tempStorage.s),
                    parseEnvPtr->context);
            }

            // just need to clear the temp storage for the next observation.
            ClearTempStorage(&(parseEnvPtr->tempStorage));

            // Getting the name of next state:
            GoToNextState(ExpectStateName);
        }
        else
        {
            // we don't have all the fields:
            HandleError(LE_FORMAT_ERROR, "state did not have v");
        }
    }
    else
    {
        // unexpected case:
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects start of a state's object.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectOneStateStart
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    if (event == LE_JSON_OBJECT_START)
    {
        // got what we expect, move on to next step:
        GoToNextState(ExpectStateMember);
    }
    else
    {
         // unexpected case:
         HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the key for a state which holds the resource path.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectStateName
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();

    if (event == LE_JSON_OBJECT_MEMBER)
    {
        const char* stateName = le_json_GetString();

        if (hub_IsResourcePathMalformed(stateName) ||
            strnlen(stateName, PARSER_STATE_MAX_PATH_BYTES) >=
                PARSER_STATE_MAX_PATH_BYTES)
        {
            // stateName is malformed:
            HandleError(LE_BAD_PARAMETER, "state key is invalid");
        }
        else
        {
            strncpy(parseEnvPtr->tempStorage.s.resourcePath,
                    stateName,
                    PARSER_STATE_MAX_PATH_BYTES);
            // got what we expect, move on to next step:
            GoToNextState(ExpectOneStateStart);
        }
    }
    else if (event == LE_JSON_OBJECT_END)
    {
        // finished with all states, which means we're finished with parsing too.
        if (parseEnvPtr->callbacksPtr->sObjectEnd)
        {
            parseEnvPtr->callbacksPtr->sObjectEnd(parseEnvPtr->context);
        }
        GoToNextState(ExpectRootMemberName);
    }
    else
    {
        // unexpected case:
         HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects start of the "s" object.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectStatesStart
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    ParseEnv_t* parseEnvPtr = le_json_GetOpaquePtr();
    if (event == LE_JSON_OBJECT_START)
    {
        if (parseEnvPtr->callbacksPtr->sObject)
        {
            parseEnvPtr->callbacksPtr->sObject(parseEnvPtr->context);
        }
        // got what we expect, move on to next step:
        GoToNextState(ExpectStateName);
    }
    else
    {
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  le_json event handler that expects the start of the root object.
 */
//--------------------------------------------------------------------------------------------------
static void ExpectConfigStart
(
    le_json_Event_t event                          ///< [IN] The le_json event.
)
{
    if (event == LE_JSON_OBJECT_START)
    {
        // got what we expect, move on to next step:
        GoToNextState(ExpectRootMemberName);
    }
    else
    {
        // unexpected case:
        HandleError(LE_FORMAT_ERROR, "Unexpected JSON element found");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Public functions:
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 *  Set parser callbacks.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void parser_SetCallbacks
(
    Parser_ParseSessionRef_t parseSessionRef,  ///< [IN] Parse session reference.
    parser_Callbacks_t* callbacksPtr                 ///< [IN] Pointer to callback structure
)
{
    if (parseSessionRef)
    {
        parseSessionRef->callbacksPtr = callbacksPtr;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Get the current parse session reference.
 *
 * @note:
 *      Can be only called from inside a callback.
 *
 * @return:
 *     Current parse session reference.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED Parser_ParseSessionRef_t parser_GetParseSessionRef
(
)
{
    return CurrParseSessionRef;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Stop the parser.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void parser_StopParse
(
    Parser_ParseSessionRef_t parseSessionRef         ///< [IN] Parse session reference.
)
{
    if (parseSessionRef)
    {
        parseSessionRef->stopped = true;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Get number of bytes read from file so far.
 *
 * @note:
 *      Can be only called from inside a callback.
 *
 * @return:
 *      Number of bytes read from file so far.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED size_t parser_GetNumBytesRead
(
)
{
    if (CurrParseSessionRef)
    {
        return le_json_GetBytesRead(le_json_GetSession());
    }
    else
    {
        return 0;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Get pointer to parser callbacks structure.
 *
 * @return
 *      Pointer to callback structure
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED parser_Callbacks_t* parser_GetCallbacks
(
    Parser_ParseSessionRef_t parseSessionRef         ///< [IN] Parse session reference.
)
{
    if (parseSessionRef)
    {
        return parseSessionRef->callbacksPtr;
    }
    else
    {
        return NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Parse a file.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void parser_Parse
(
    int fd,                                          ///< [IN] file descriptor to be parsed.
    parser_Callbacks_t* callbacksPtr,                ///< [IN] Pointer to callback structure
    void* context                                    ///< [IN] Context to provide to callbacks
)
{
    if (CurrParseSessionRef)
    {
        // there is another parse ongoing,
        // currently we only support one parse at the time.
        callbacksPtr->error(LE_BUSY, "Another parse is ongoing", context);
    }
    else if (fd < 0)
    {
        callbacksPtr->error(LE_IO_ERROR, "Invalid Fd", context);
    }
    else
    {
        parser_Callbacks_t emptyCallbacks = {0};
        ParseEnv_t parseEnv = {0};
        parseEnv.fd = fd;
        parseEnv.callbacksPtr = (callbacksPtr)? callbacksPtr : (&emptyCallbacks);
        parseEnv.context = context;
        CurrParseSessionRef = &parseEnv;
        le_json_SyncParse(fd, ExpectConfigStart, ErrorEventHandler, &parseEnv);
        CurrParseSessionRef = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Component initializer.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("Default Parser Started.");
}
