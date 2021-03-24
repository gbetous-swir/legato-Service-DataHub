//--------------------------------------------------------------------------------------------------
/**
 * @file configService_parse.c
 *
 * Implementation of the Data Hub Config Parser.
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

#include "configService.h"
#include "parser.h"

//--------------------------------------------------------------------------------------------------
/**
 *  Hold the config parse environment parameters.
 */
//--------------------------------------------------------------------------------------------------
typedef struct ParseContext
{
    bool validateOnly;                      ///< Only validate the config, do not apply
    le_result_t result;                     ///< Overall parse result.
    parseError_t* parserErrorPtr;           ///< Pointer to parser error structure passed to us.
} ParseContext_t;


//--------------------------------------------------------------------------------------------------
/**
 * Function to Handle Parser Errors.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void HandleError
(
    ParseContext_t* parseContextPtr,    ///< [IN] Pointer to the Parser Context structure
    le_result_t result,                 ///< [IN] Result code
    const char* msg                     ///< [IN] Error Message string
)
{
    // pass the error to higher levels:
    LE_ERROR("Error when parsing config file, error code :[%d]", result);
    LE_ERROR("Error message: [%s]", msg);
    strncpy(parseContextPtr->parserErrorPtr->errorMsg, msg, CONFIG_MAX_ERROR_MSG_LEN);

    parseContextPtr->parserErrorPtr->fileLoc = parser_GetNumBytesRead();

    // Set the Parse Context result code
    parseContextPtr->result = result;

    parser_StopParse(parser_GetParseSessionRef());
}


//--------------------------------------------------------------------------------------------------
/**
 * Error event callback given to octave parser.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void ErrorEventCb
(
    le_result_t error,     ///< [IN] Result code
    const char* msg,       ///< [IN] Error Message string
    void* context          ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;
    if (!parseContextPtr->validateOnly)
    {
        // errors in the apply phase are reported as LE_FAULT.
        error = LE_FAULT;
    }
    HandleError(context, error, msg);
}


//--------------------------------------------------------------------------------------------------
/**
 * Helper functions for processing an observation.
 *
 * Each function processes one field.
 * Return value is a boolean. true indicates that field was successfully processed and parser can
 * move forward to other fields, false means there was an error and parser should stop.
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Helper function for creating an Observation.
 *
 * @return
 *      - true            The function succeeded.
 *      - false           The function failed.
 */
//--------------------------------------------------------------------------------------------------
static bool ObsNameHelper
(
    const char* obsName,    ///< [IN] Observation name string
    bool IsANewObs,         ///< [IN] Is this a new observation?
    void* context           ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;
    LE_ASSERT(!hub_IsResourcePathMalformed(obsName));

    if (!parseContextPtr->validateOnly)
    {
        // if we're applying configs, we can create the observation now:
        // admin_CreateObs will return LE_OK even if it already exists.
        le_result_t obsCreateRes = admin_CreateObs(obsName);

        if (obsCreateRes != LE_OK)
        {

            char msg[CONFIG_MAX_ERROR_MSG_LEN] = {0};
            snprintf(msg,
                     CONFIG_MAX_ERROR_MSG_LEN,
                     "Error in Creating Observation %s, error: %s",
                     obsName,
                     LE_RESULT_TXT(obsCreateRes));

            HandleError(parseContextPtr, LE_FAULT, msg);
            return false;
        }

        // Set the relevance flag so we know this obs was visited during application of this config
        resTree_EntryRef_t entryRef = resTree_FindEntry(resTree_GetObsNamespace(), obsName);
        LE_ASSERT(entryRef); // we just created it, so it must exist.
        resTree_SetRelevance(entryRef, true);

        // Mark as config if it's a new observation:
        if (IsANewObs)
        {
            resTree_MarkObservationAsConfig(entryRef);
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Helper function to set the Observation Source.
 *
 * @return
 *      - true            The function succeeded.
 *      - false           The function failed.
 */
//--------------------------------------------------------------------------------------------------
static bool ObsResourceHelper
(
    const char* obsName,        ///< [IN] Observation name string
    const char* resourcePath,   ///< [IN] Resource path string
    void* context               ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;
    LE_ASSERT(!hub_IsResourcePathMalformed(resourcePath));

    if (!parseContextPtr->validateOnly)
    {
        char absPath[HUB_MAX_RESOURCE_PATH_BYTES]= {};
        snprintf(absPath, HUB_MAX_RESOURCE_PATH_BYTES, "/obs/%s", obsName);

        // Set the Observation Source
        le_result_t result = admin_SetSource(absPath, resourcePath);
        if (result != LE_OK)
        {
            char msg[CONFIG_MAX_ERROR_MSG_LEN] = {0};
            snprintf(msg,
                     CONFIG_MAX_ERROR_MSG_LEN,
                     "failed to set source for obs %s, error: %s",
                     obsName,
                     LE_RESULT_TXT(result));

            HandleError(parseContextPtr, LE_FAULT, msg);
            return false;
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Helper function to set the Observation Resource path or Destination Name.
 *
 * @return
 *      - true            The function succeeded.
 *      - false           The function failed.
 */
//--------------------------------------------------------------------------------------------------
static bool ObsDestinationHelper
(
    const char* obsName,     ///< [IN] Observation name string
    const char* dest,        ///< [IN] Destination name string
    void* context            ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;

    // first determine if value is a destination string or a resource path.
    if (dest[0] == '/')
    {
        // value is a resource path:
        if (!parseContextPtr->validateOnly)
        {
            char absPath[HUB_MAX_RESOURCE_PATH_BYTES]= {};
            snprintf(absPath, HUB_MAX_RESOURCE_PATH_BYTES, "/obs/%s", obsName);

            // Set the Observation Source
            le_result_t result = admin_SetSource(dest, absPath);
            if (result != LE_OK)
            {
                char msg[CONFIG_MAX_ERROR_MSG_LEN] = {0};
                snprintf(msg,
                         CONFIG_MAX_ERROR_MSG_LEN,
                         "failed to set destination for obs %s, error: %s",
                         obsName,
                         LE_RESULT_TXT(result));

                HandleError(parseContextPtr, LE_FAULT, msg);
                return false;
            }
        }
    }
    else
    {
        // value is not a resource path, in this case, there's no validation to be done.
        if (!parseContextPtr->validateOnly)
        {
            // value is a destination string:
            resTree_EntryRef_t entryRef = resTree_FindEntry(resTree_GetObsNamespace(), obsName);
            LE_ASSERT(entryRef); // we wouldn't have come this far if this obs didn't exist.

            // Set the Observation Destination name
            resTree_SetDestination(entryRef, dest);
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Helper function to set the Observation Minimum Period value.
 *
 * @return
 *      - true            The function succeeded.
 *      - false           The function failed.
 */
//--------------------------------------------------------------------------------------------------
static bool ObsMinPeriodHelper
(
    parser_ObsData_t* obsDataPtr,     ///< [IN] Pointer to Observation Data structure
    bool IsANewObs,                   ///< [IN] Is this a new observation?
    void* context                     ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;
    if (!parseContextPtr->validateOnly)
    {
        if ((obsDataPtr->bitmask & PARSER_OBS_PERIOD_MASK) || !IsANewObs)
        {
            // Set the Observation Minimum period
            le_result_t result = admin_SetMinPeriod(obsDataPtr->obsName, obsDataPtr->minPeriod);
            if (result != LE_OK)
            {
                char msg[CONFIG_MAX_ERROR_MSG_LEN] = {0};
                snprintf(msg,
                         CONFIG_MAX_ERROR_MSG_LEN,
                         "Failed to set Min Period for obs %s, error: %s",
                         obsDataPtr->obsName,
                         LE_RESULT_TXT(result));

                HandleError(parseContextPtr, LE_FAULT, msg);
                return false;
            }
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Helper function to set the Observation Change By value.
 *
 * @return
 *      - true            The function succeeded.
 *      - false           The function failed.
 */
//--------------------------------------------------------------------------------------------------
static bool ObsChangeByHelper
(
    parser_ObsData_t* obsDataPtr,     ///< [IN] Pointer to Observation Data structure
    bool IsANewObs,                   ///< [IN] Is this a new observation?
    void* context                     ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;
    if (!parseContextPtr->validateOnly)
    {
        if ((obsDataPtr->bitmask & PARSER_OBS_CHANGEBY_MASK) || !IsANewObs)
        {
            // Set the Observation Change By field
            le_result_t result = admin_SetChangeBy(obsDataPtr->obsName, obsDataPtr->changeBy);
            if (result != LE_OK)
            {
                char msg[CONFIG_MAX_ERROR_MSG_LEN] = {0};
                snprintf(msg,
                         CONFIG_MAX_ERROR_MSG_LEN,
                         "Failed to set Changeby for obs %s, error: %s",
                         obsDataPtr->obsName,
                         LE_RESULT_TXT(result));

                HandleError(parseContextPtr, LE_FAULT, msg);
                return false;
            }
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Helper function to set the Observation High Limit threshold.
 *
 * @return
 *      - true            The function succeeded.
 *      - false           The function failed.
 */
//--------------------------------------------------------------------------------------------------
static bool ObsLowerThanHelper
(
    parser_ObsData_t* obsDataPtr,     ///< [IN] Pointer to Observation Data structure
    bool IsANewObs,                   ///< [IN] Is this a new observation?
    void* context                     ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;

    if (!parseContextPtr->validateOnly)
    {
        if ((obsDataPtr->bitmask & PARSER_OBS_LOWERTHAN_MASK) || !IsANewObs)
        {
            // Set the Observation High Limit
            le_result_t result = admin_SetHighLimit(obsDataPtr->obsName, obsDataPtr->lowerThan);
            if (result != LE_OK)
            {
                char msg[CONFIG_MAX_ERROR_MSG_LEN] = {0};
                snprintf(msg,
                         CONFIG_MAX_ERROR_MSG_LEN,
                         "Failed to set high limit for obs %s, error: %s",
                         obsDataPtr->obsName,
                         LE_RESULT_TXT(result));

                HandleError(parseContextPtr, LE_FAULT, msg);
                return false;
            }
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Helper function to set the Observation Low Limit threshold.
 *
 * @return
 *      - true            The function succeeded.
 *      - false           The function failed.
 */
//--------------------------------------------------------------------------------------------------
static bool ObsGreaterThanHelper
(
    parser_ObsData_t* obsDataPtr,     ///< [IN] Pointer to Observation Data structure
    bool IsANewObs,                   ///< [IN] Is this a new observation?
    void* context                     ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;

    if (!parseContextPtr->validateOnly)
    {
        if ((obsDataPtr->bitmask & PARSER_OBS_GREATERTHAN_MASK) || !IsANewObs)
        {
            // Set the Observation Low Limit
            le_result_t result = admin_SetLowLimit(obsDataPtr->obsName, obsDataPtr->greaterThan);
            if (result != LE_OK)
            {
                char msg[CONFIG_MAX_ERROR_MSG_LEN] = {0};
                snprintf(msg,
                         CONFIG_MAX_ERROR_MSG_LEN,
                         "Failed to set low limit for obs %s, error: %s",
                         obsDataPtr->obsName,
                         LE_RESULT_TXT(result));

                HandleError(parseContextPtr, LE_FAULT, msg);
                return false;
            }
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Helper function to set the Observation Maximum Buffer count.
 *
 * @return
 *      - true            The function succeeded.
 *      - false           The function failed.
 */
//--------------------------------------------------------------------------------------------------
static bool ObsMaxBufferHelper
(
    parser_ObsData_t* obsDataPtr,     ///< [IN] Pointer to Observation Data structure
    bool IsANewObs,                   ///< [IN] Is this a new observation?
    void* context                     ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;

    if (!parseContextPtr->validateOnly)
    {
        if ((obsDataPtr->bitmask & PARSER_OBS_BUFFER_MASK) || !IsANewObs)
        {
            // Set the Observation Max Buffer Count
            le_result_t result = admin_SetBufferMaxCount(obsDataPtr->obsName,
                obsDataPtr->bufferMaxCount);
            if (result != LE_OK)
            {
                char msg[CONFIG_MAX_ERROR_MSG_LEN] = {0};
                snprintf(msg,
                         CONFIG_MAX_ERROR_MSG_LEN,
                         "Failed to set buffer maxCount for obs %s, error: %s",
                         obsDataPtr->obsName,
                         LE_RESULT_TXT(result));

                HandleError(parseContextPtr, LE_FAULT, msg);
                return false;
            }
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Helper function to set the Observation Transform.
 *
 * @return
 *      - true            The function succeeded.
 *      - false           The function failed.
 */
//--------------------------------------------------------------------------------------------------
static bool ObsTransformFunctionHelper
(
    parser_ObsData_t* obsDataPtr,     ///< [IN] Pointer to Observation Data structure
    bool IsANewObs,                   ///< [IN] Is this a new observation?
    void* context                     ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;

    if (!parseContextPtr->validateOnly)
    {
        if ((obsDataPtr->bitmask & PARSER_OBS_TRANSFORM_MASK) || !IsANewObs)
        {
            // Set the Observation transform
            le_result_t result = admin_SetTransform(obsDataPtr->obsName,
                (obsDataPtr->transform), NULL, 0);
            if (result != LE_OK)
            {
                char msg[CONFIG_MAX_ERROR_MSG_LEN] = {0};
                snprintf(msg,
                         CONFIG_MAX_ERROR_MSG_LEN,
                         "Failed to set obs transform for obs %s, error: %s",
                         obsDataPtr->obsName,
                         LE_RESULT_TXT(result));

                HandleError(parseContextPtr, LE_FAULT, msg);
                return false;
            }
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Helper function to set the Observation JSON extraction.
 *
 * @return
 *      - true            The function succeeded.
 *      - false           The function failed.
 */
//--------------------------------------------------------------------------------------------------
static bool ObsJsonExtractionHelper
(
    parser_ObsData_t* obsDataPtr,     ///< [IN] Pointer to Observation Data structure
    bool IsANewObs,                   ///< [IN] Is this a new observation?
    void* context                     ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;

    if (!parseContextPtr->validateOnly)
    {
        if ((obsDataPtr->bitmask & PARSER_OBS_JSON_EXT_MASK) || !IsANewObs)
        {
            // Set the JSON Extraction
            le_result_t result = admin_SetJsonExtraction(obsDataPtr->obsName,
                obsDataPtr->jsonExtraction);
            if (result != LE_OK)
            {
                char msg[CONFIG_MAX_ERROR_MSG_LEN] = {0};
                snprintf(msg,
                         CONFIG_MAX_ERROR_MSG_LEN,
                         "Failed to set JSON extraction for obs %s, error: %s",
                         obsDataPtr->obsName,
                         LE_RESULT_TXT(result));

                HandleError(parseContextPtr, LE_FAULT, msg);
                return false;
            }
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Callback function to process an 'observation' entry.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void OneObservationCb
(
    parser_ObsData_t* obsDataPtr,  ///< [IN] Pointer to Observation Data structure
    void* context                  ///< [IN] Context pointer
)
{
    bool newObs = false;
    resTree_EntryRef_t entryRef = resTree_FindEntry(resTree_GetObsNamespace(), obsDataPtr->obsName);
    if (!entryRef || resTree_IsResource(entryRef))
    {
        newObs = true;
    }
    if (!ObsNameHelper(obsDataPtr->obsName, newObs, context))
    {
        return;
    }

    if (!ObsResourceHelper(obsDataPtr->obsName, obsDataPtr->resourcePath, context))
    {
        return;
    }

    if (!ObsDestinationHelper(obsDataPtr->obsName, obsDataPtr->destination, context))
    {
        return;
    }

    if (!ObsMinPeriodHelper(obsDataPtr, newObs, context))
    {
        return;
    }

    if (!ObsChangeByHelper(obsDataPtr, newObs, context))
    {
        return;
    }

    if (!ObsLowerThanHelper(obsDataPtr, newObs, context))
    {
        return;
    }

    if (!ObsGreaterThanHelper(obsDataPtr, newObs, context))
    {
        return;
    }

    if (!ObsMaxBufferHelper(obsDataPtr, newObs, context))
    {
        return;
    }

    if (!ObsTransformFunctionHelper(obsDataPtr, newObs, context))
    {
        return;
    }

    if (!ObsJsonExtractionHelper(obsDataPtr, newObs, context))
    {
        return;
    }
}



//--------------------------------------------------------------------------------------------------
/**
 * Callback function to process a 'state' entry.
 *
 *
 * States are values which are pushed to resources and set as default value of those resources.
 * CP must set the value as default using admin_Set*Default functions and then push the value to
 * the resource using admin_Push* APIs.
 * The resource may not exist at the time that the state is being parsed in which case the act of
 * setting the default will create a placeholder resource for it.
 * Return code of both setting the default and pushing the value must be ignored to keep the
 * behaviour the same as today.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void StateCb
(
    parser_StateData_t* stateDataPtr,  ///< [IN] Pointer to State Data structure
    void* context                      ///< [IN] Context pointer
)
{
    ParseContext_t* parseContextPtr = (ParseContext_t*) context;
    const char* resPath = stateDataPtr->resourcePath;
    LE_ASSERT (!hub_IsResourcePathMalformed(resPath));
    if (!parseContextPtr->validateOnly)
    {
        le_result_t ret = LE_OK;

        // set default and push
        switch(stateDataPtr->dataType)
        {
            case (IO_DATA_TYPE_TRIGGER):
            {
                ret = admin_PushTrigger(resPath, 0);
                break;
            }
            case (IO_DATA_TYPE_NUMERIC):
            {
                ret = admin_SetNumericDefault(resPath, stateDataPtr->value.number);
                if (ret == LE_OK)
                {
                    ret = admin_PushNumeric(resPath, 0, stateDataPtr->value.number);
                }
                break;
            }
            case (IO_DATA_TYPE_BOOLEAN):
            {
                ret = admin_SetBooleanDefault(resPath, stateDataPtr->value.boolean);
                if (ret == LE_OK)
                {
                    ret = admin_PushBoolean(resPath, 0, stateDataPtr->value.boolean);
                }
                break;
            }
            case (IO_DATA_TYPE_STRING):
            {
                ret = admin_SetStringDefault(resPath, stateDataPtr->value.string);

                if (ret == LE_OK)
                {
                    ret = admin_PushString(resPath, 0, stateDataPtr->value.string);
                };
                break;
            }
            case (IO_DATA_TYPE_JSON):
            {
                ret = admin_SetJsonDefault(resPath, stateDataPtr->value.string);
                if (ret == LE_OK)
                {
                    ret = admin_PushJson(resPath, 0, stateDataPtr->value.string);
                }
                break;
            }
            default:
            {
                //this should not happen
                LE_FATAL("Unexpected DataType in State");
            }
        }
        if (ret != LE_OK)
        {
            // ignoring the result code to keep the current behavior
            // processing a state could fail due to lack of memory for placeholder resources or
            // lack of memory for data samples or type mismatch
            LE_WARN("Problem in processing state for resource at %s, result: %d", resPath, ret);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Callback function to indicate the end of 'state' parsing.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void StatesEndCb
(
    void* context    ///< [IN] Context pointer
)
{
    LE_UNUSED(context);
    // we finished parsing states, which is the last thing we care about.
    // so we can stop the parse here.
    parser_StopParse(parser_GetParseSessionRef());
}


//--------------------------------------------------------------------------------------------------
/**
 * Callback function to indicate the start of 'state' parsing.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void StatesStartCb
(
    void* context    ///< [IN] Context pointer
)
{
    LE_UNUSED(context);
    // we've found observations, so we can move on with states.
    parser_Callbacks_t* callbacksPtr = parser_GetCallbacks(
            parser_GetParseSessionRef());

    // Add callbacks for parsing states
    callbacksPtr->state = StateCb;
    callbacksPtr->sObjectEnd = StatesEndCb;
}


//--------------------------------------------------------------------------------------------------
/**
 * Callback function to indicate the end of 'observation' parsing.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void ObservationsEndCb
(
    void* context    ///< [IN] Context pointer
)
{
    LE_UNUSED(context);

    // Observations ended, stop the parser now. We will start it again for states.
    parser_StopParse(parser_GetParseSessionRef());
}


//--------------------------------------------------------------------------------------------------
/**
 * Callback function to indicate the start of 'observation' parsing.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void ObservationsStartCb
(
    void* context    ///< [IN] Context pointer
)
{
    LE_UNUSED(context);
    parser_Callbacks_t* callbacksPtr = parser_GetCallbacks(
            parser_GetParseSessionRef());

    // Add callbacks for parsing observations
    callbacksPtr->observation = OneObservationCb;
    callbacksPtr->oObjectEnd = ObservationsEndCb;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to parse the specified configuration.
 *
 * @return
 *      - LE_OK            The function succeeded.
 *      - LE_FORMAT_ERROR  The configuration file has an unrecoverable format error.
 *      - LE_BAD_PARAMETER There is invalid parameters in the configuration file.
 *      - LE_IO_ERROR      Parser failed to read from file.
 *      - LE_FAULT         The configuration cannot be applied successfully.
 */
//--------------------------------------------------------------------------------------------------
le_result_t configService_ParseConfig
(
    int fd,                                ///< [IN] File descriptor of the configuration file.
    bool validateOnly,                     ///< [IN] Boolean to indicate if this is a validate or
                                            /// apply operation.
    parseError_t* parseErrorPtr            ///< [OUT] Pointer to the Parse Error structure.
)
{
    ParseContext_t parseContext = {};
    parseContext.validateOnly = validateOnly;
    parseContext.parserErrorPtr = parseErrorPtr;
    parseContext.result = LE_OK;

    parser_Callbacks_t callbacks = {};

    // At the beginning the only thing we care about is the start of the "o" section.
    // so we'll register a callback for that event and then take it from there.
    callbacks.oObject = ObservationsStartCb;
    callbacks.error = ErrorEventCb;

    // Start parsing the config file to process observations ("o")
    parser_Parse(fd, &callbacks, &parseContext);

    if (parseContext.result == LE_OK)
    {
        // At this point observations are parsed but states are not.
        // so we'll parse again but with state callbacks enabled.
        // need to empty the callbacks because we don't want to get callbacks for observations for
        // a second time.
        memset(&callbacks, 0, sizeof(parser_Callbacks_t));

        callbacks.sObject = StatesStartCb;
        callbacks.error = ErrorEventCb;
        lseek(fd, 0, SEEK_SET);
        // Start parsing the config file to process states ("s")
        parser_Parse(fd, &callbacks, &parseContext);
    }

    return parseContext.result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Traverse Datahub Tree
 *
 * Does a post order traversal of the data hub resource tree.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
void configService_TraverseDatahubResourceTree
(
    resTree_EntryRef_t currEntry,                         ///< [IN] current resource tree entry
    configService_traversalCallbacks_t* callbacksPtr,     ///< [IN] pointer callback structure
    void* context                                         ///< [IN] caller context.
)
{
    static bool underObsTree = false;
    if (!currEntry)
    {
        return;
    }

    if (currEntry == resTree_GetObsNamespace())
    {
        underObsTree = true;
    }

    // visit children:
    resTree_EntryRef_t childEntry = resTree_GetFirstChild(currEntry);
    while(childEntry)
    {
        resTree_EntryRef_t tempEntry = resTree_GetNextSibling(childEntry);
        configService_TraverseDatahubResourceTree(childEntry, callbacksPtr, context);
        childEntry = tempEntry;
    }

    if (currEntry == resTree_GetObsNamespace())
    {
        underObsTree = false;
    }

    // visit this node:
    switch(resTree_GetEntryType(currEntry))
    {
        case (ADMIN_ENTRY_TYPE_NAMESPACE):
        {
            if (callbacksPtr->nameSpaceCb)
            {
                callbacksPtr->nameSpaceCb(currEntry, context);
            }
            break;
        }
        case (ADMIN_ENTRY_TYPE_PLACEHOLDER):
        {
            if (underObsTree)
            {
                if (callbacksPtr->observationCb)
                {
                    callbacksPtr->observationCb(currEntry, context);
                }
            }
            else
            {
                if (callbacksPtr->ioResourceCb)
                {
                    callbacksPtr->ioResourceCb(currEntry, context);
                }
            }
            break;
        }
        case (ADMIN_ENTRY_TYPE_INPUT):
        case (ADMIN_ENTRY_TYPE_OUTPUT):
        {
            if (callbacksPtr->ioResourceCb)
            {
                callbacksPtr->ioResourceCb(currEntry, context);
            }
            break;
        }
        case (ADMIN_ENTRY_TYPE_OBSERVATION):
        {
            if (callbacksPtr->observationCb)
            {
                callbacksPtr->observationCb(currEntry, context);
            }
            break;
        }
        default:
        {
            LE_ERROR("Unexpected case in entry type");
            underObsTree = false;
            return;
        }
    }
}
