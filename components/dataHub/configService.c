//--------------------------------------------------------------------------------------------------
/**
 * @file configService.c
 *
 * Implementation of the Data Hub Config API.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "dataHub.h"
#include "ioService.h"

#include "configService.h"
#include "parser.h"

#define CONFIG_DESTINATION_MAX_NUM  6

//--------------------------------------------------------------------------------------------------
/**
 * holds context pointer given to Load function.
 */
//--------------------------------------------------------------------------------------------------
static void* ContextPtr;


// Config Service Destination Callback structure
typedef struct config_DestinationStructure
{
    char destination[CONFIG_MAX_DESTINATION_NAME_BYTES];   ///< Destination string.
    config_DestinationPushHandlerFunc_t callbackPtr;       ///< handler provided by client.
    void* contextPtr;                                      ///< client context.

} config_DestinationStructure_t;


// Static Destination Record
static config_DestinationStructure_t destinationRecord[CONFIG_DESTINATION_MAX_NUM];


//--------------------------------------------------------------------------------------------------
/**
 * Callback given to configService_TraverseDatahubResourceTree to be called whenever an observation
 * is found.
 * If an observation is created by a previous config file is absent in a new config file, it must
 * be deleted.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void FoundObservation
(
    resTree_EntryRef_t entryRef,      ///< [IN] observation entry
    void* context                     ///< [IN] context pointer, holds considerRelevance.
)
{
    bool considerRelevance = (bool) (int)(intptr_t) context;

    if (resTree_IsObservationConfig(entryRef))
    {
        // this entry is a config observation. It must be deleted if either of below conditions is
        // true:
        // 1. considerRelevance is false, meaning we're not considering the relevance of resources
        // during clean up, essentially deleting all config observations.
        // 2. This observations is not relevant.
        if (!considerRelevance || !resTree_IsRelevant(entryRef))
        {
            resTree_DeleteObservation(entryRef);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Callback given to configService_TraverseDatahubResourceTree to be called whenever an observation
 * is found.
 * This callback clears the relevance flag of an observation.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void ClearRelevanceFlag
(
    resTree_EntryRef_t entryRef,      ///< [IN] observation entry
    void* context                     ///< [IN] context pointer
)
{
    LE_UNUSED(context);
    resTree_SetRelevance(entryRef, false);
}


//--------------------------------------------------------------------------------------------------
/**
 * Clean up the datahub tree by deleting observations.
 *
 * If considerRelevance is true, only deletes config resources that do not have the relevance flag.
 * If considerRelevance is false, essentially deletes all config observations.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void CleanupTree
(
    bool considerRelevance   ///< [IN] whether or not to consider the relevance flag when
                             ///< cleaning up the tree.
)
{
    //traverse entire datahub tree and delete config stuff
    configService_traversalCallbacks_t callbacks = {};
    callbacks.observationCb = FoundObservation;
    configService_TraverseDatahubResourceTree(resTree_GetRoot(), &callbacks,
            (void*)(intptr_t)(int) considerRelevance);

    // reset the relevance flag
    callbacks.observationCb = ClearRelevanceFlag;
    configService_TraverseDatahubResourceTree(resTree_GetRoot(), &callbacks, NULL);
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove all observations marked as config.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void RemoveAllConfigObservations
(
)
{
    // CleanupTree removes all config observations that are not relevant. Giving false removes
    // all config observations.
    CleanupTree(false);
}


//--------------------------------------------------------------------------------------------------
/**
 * Validate a config.
 *
 * @return
 *      - LE_OK            If config was successfully validated.
 *      - LE_FORMAT_ERROR  The configuration file has an unrecoverable format error.
 *      - LE_BAD_PARAMETER There is invalid parameters in the configuration file.
 *      - LE_IO_ERROR      Parser failed to read from file.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ValidateConfig
(
    int fd,                      ///< [IN] File descriptor holding the config.
    parseError_t* parseErrorPtr  ///< [OUT] pointer to a parse error structure
)
{
    // Validate the configuration
    return configService_ParseConfig(fd, true, parseErrorPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Apply a config.
 *
 * @return
 *      - LE_OK     If config was successfully applied.
 *      - LE_FAULT  error while applying configuration
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ApplyConfig
(
    int fd,                      ///< [IN] File descriptor holding the config.
    parseError_t* parseErrorPtr  ///< [OUT] pointer to a parse error structure
)
{
    // Apply the configuration
    le_result_t overallResult =
        configService_ParseConfig(
            fd,
            false,
            parseErrorPtr);

    // Remove old config observations that were not applied in the configuration file
    CleanupTree(true);

    return overallResult;
}

//--------------------------------------------------------------------------------------------------
/**
 * Load a config file.
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void DoLoad
(
    void* fd_,                      ///< File descriptor holding the config.
    void* resultCallback_           ///< Callback function to call once load is done.
)
{
    int fd = (int)(intptr_t) fd_;

    config_LoadResultHandlerFunc_t resultCallback =
        (config_LoadResultHandlerFunc_t) resultCallback_;

    parseError_t parseError = {};

    // Validate Configuration file
    le_result_t overallResult = ValidateConfig(fd, &parseError);

    if (overallResult != LE_OK)
    {
        LE_ERROR("Config Validation failed! at file location: %" PRIuS, parseError.fileLoc);
        LE_ERROR("Error message: %s", parseError.errorMsg);

        // Trigger LoadResultHandler callback
        resultCallback(overallResult,
                       parseError.errorMsg,
                       parseError.fileLoc,
                       ContextPtr);
        return;
    }

    LE_INFO("Config is Valid, Moving on to Apply step");

    lseek(fd, 0, SEEK_SET);

    // Apply Configuration file
    overallResult = ApplyConfig(fd, &parseError);

    if (overallResult != LE_OK)
    {
        LE_ERROR("Applying Config failed at file location: %" PRIuS, parseError.fileLoc);
        LE_ERROR("Error message: %s", parseError.errorMsg);
        RemoveAllConfigObservations();
        // Failure in apply is always reported as LE_FAULT because that is the error code that the
        // client recognizes for failure in this phase.
        overallResult = LE_FAULT;
    }
    else
    {
        LE_INFO("Config successfully Applied");
    }

    // Trigger LoadResultHandler callback
    resultCallback(overallResult,
                   parseError.errorMsg,
                   parseError.fileLoc,
                   ContextPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Function that causes the Datahub to load a configuration from a file.
 * Any existing configuration will be removed and replaced with the incoming one.
 *
 * @return
 *  - LE_OK           : Configuration successfully loaded
 *  - LE_NOT_FOUND    : Unable to locate or retrieve configuration file
 *  - LE_UNSUPPORTED  : Configuration encoding format is not supported.
 */
//--------------------------------------------------------------------------------------------------
le_result_t config_Load
(
    const char* LE_NONNULL filePath,            ///< [IN] Path of configuration file.
    const char* LE_NONNULL encodedType,         ///< [IN] Type of encoding used in the file
    config_LoadResultHandlerFunc_t callbackPtr, ///< [IN] Callback to notify caller of result
    void* contextPtr                            ///< [IN]
)
{
    LE_INFO("Loading Config, file path is %s" , filePath);
    int fd = -1;
    if (strcmp(encodedType, "json") == 0)
    {
        // open the file now so you don't have to copy the file path.
        fd = open(filePath, O_RDONLY);
        if (fd < 0)
        {
            return LE_NOT_FOUND;
        }
    }
    else
    {
        return LE_UNSUPPORTED;
    }

    ContextPtr = contextPtr;

    le_event_QueueFunction(DoLoad, (void*)(intptr_t)fd, (void*)callbackPtr);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'config_DestinationPush'
 */
//--------------------------------------------------------------------------------------------------
config_DestinationPushHandlerRef_t config_AddDestinationPushHandler
(
    const char* destination,                           ///< [IN] Absolute path of resource.
    config_DestinationPushHandlerFunc_t callbackPtr,   ///< [IN]
    void* contextPtr                                   ///< [IN]
)
{
    // Traverse destination array looking for free entry
    for (int i = 0; i < CONFIG_DESTINATION_MAX_NUM; i++)
    {
        if (destinationRecord[i].callbackPtr == NULL)
        {
            strncpy(destinationRecord[i].destination, destination, CONFIG_MAX_DESTINATION_NAME_LEN);
            destinationRecord[i].callbackPtr = callbackPtr;
            destinationRecord[i].contextPtr = contextPtr;

            return (config_DestinationPushHandlerRef_t)(&destinationRecord[i]);
        }
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'config_DestinationPush'
 */
//--------------------------------------------------------------------------------------------------
void config_RemoveDestinationPushHandler
(
    config_DestinationPushHandlerRef_t handlerRef
        ///< [IN]
)
{
    // Look-up AT Command Register entry using handlerRef
    config_DestinationStructure_t* destinationRecordPtr =
        (config_DestinationStructure_t*)handlerRef;

    memset(destinationRecordPtr->destination, 0, CONFIG_MAX_DESTINATION_NAME_BYTES);
    destinationRecordPtr->callbackPtr = NULL;
    destinationRecordPtr->contextPtr = NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Trigger destination push handler for the specified destination name, if registered
 *
 * @return
 *      - LE_OK                Function succeeded.
 *      - LE_BAD_PARAMETER     Invalid destination record variable.
 *      - LE_NOT_FOUND         Unable to find matching destination name
 */
//--------------------------------------------------------------------------------------------------
le_result_t configService_TriggerDestinationPushHandler
(
    const char* destination,     ///< [IN] Destination path of resource
    const char* obsName,         ///< [IN] Observation Name
    const char* srcPath,         ///< [IN] Source path + JSON extraction, if applicable
    io_DataType_t dataType,      ///< [IN] Data type of the data sample
    dataSample_Ref_t dataSample  ///< [IN] Data sample
)
{
    LE_DEBUG("[%s] destination [%s], obsName [%s]",
             __FUNCTION__,
             destination,
             obsName);

    LE_DEBUG("[%s] srcPath [%s], dataType [%d]",
             __FUNCTION__,
             srcPath,
             dataType);

    // Traverse destination array searching for matching destination name
    for (int i = 0; i < CONFIG_DESTINATION_MAX_NUM; i++)
    {
        if (strncmp(destinationRecord[i].destination, destination,
                    sizeof(destinationRecord[i].destination)) == 0)
        {
            bool valueBool = false;
            double valueNumeric = 0.0;
            const char* valueString = "";

            if (destinationRecord[i].callbackPtr == NULL)
            {
                LE_ERROR("Destination PushHandler callback is NULL!");
                return LE_BAD_PARAMETER;
            }

            double timestamp = dataSample_GetTimestamp(dataSample);

            switch (dataType)
            {
                case IO_DATA_TYPE_TRIGGER:
                {
                    break;
                }

                case IO_DATA_TYPE_BOOLEAN:
                {
                    // Set the valueBool
                    valueBool = dataSample_GetBoolean(dataSample);
                    break;
                }

                case IO_DATA_TYPE_NUMERIC:
                {
                    // Set the valueNumeric
                    valueNumeric = dataSample_GetNumeric(dataSample);
                    break;
                }

                case IO_DATA_TYPE_STRING:
                {
                    // Set the valueString
                    valueString = dataSample_GetString(dataSample);
                    break;
                }

                case IO_DATA_TYPE_JSON:
                {
                    // Set the valueString using the JSON string
                    valueString = dataSample_GetJson(dataSample);
                    break;
                }
            }

            LE_DEBUG("[%s] Calling push handler, destination [%s]",
                     __FUNCTION__,
                     destinationRecord[i].destination);

            // Trigger the Destination PushHandler callback registered
            // for this destination name
            destinationRecord[i].callbackPtr(
                timestamp,
                obsName,
                srcPath,
                dataType,
                valueBool,
                valueNumeric,
                valueString,
                destinationRecord[i].contextPtr);

            return LE_OK;
        }
    }

    LE_ERROR("[%s] Unable to find matching push handler, destination [%s]",
             __FUNCTION__,
             destination);

    return LE_NOT_FOUND;
}
