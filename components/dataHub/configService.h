//--------------------------------------------------------------------------------------------------
/**
 * @file configService.h
 *
 * Config parser header.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "resource.h"

#ifndef CONFIG_SERVICE_H_INCLUDE_GUARD
#define CONFIG_SERVICE_H_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 *  Hold info about a failure during parse.
 */
//--------------------------------------------------------------------------------------------------
typedef struct ParseError
{
    size_t fileLoc;          ///< Holds number of bytes read from file when the error happened.
    char errorMsg[CONFIG_MAX_ERROR_MSG_BYTES]; ///< Holds error message.
} parseError_t;


//--------------------------------------------------------------------------------------------------
/**
 *  Callbacks for configService_TraverseDatahubResourceTree
 */
//--------------------------------------------------------------------------------------------------
// found io resource callback
typedef void (*FoundIoResourceCallback_t)
(
    resTree_EntryRef_t entryRef,
    void* context
);

// found observation callback
typedef void (*FoundObservationCallback_t)
(
    resTree_EntryRef_t entryRef,
    void* context
);

// found namespace callback
typedef void (*FoundNamespaceCallback_t)
(
    resTree_EntryRef_t entryRef,
    void* context
);

//--------------------------------------------------------------------------------------------------
/**
 *  Callback structure for datahub resource tree traversal.
 */
//--------------------------------------------------------------------------------------------------
typedef struct TraversalCallbacks
{
    FoundIoResourceCallback_t ioResourceCb;      /// called when an io resource entry is found.
    FoundObservationCallback_t observationCb;    /// called when an observation entry is found.
    FoundNamespaceCallback_t nameSpaceCb;        /// called when a namespace entry is found.
} configService_traversalCallbacks_t;


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
);


//--------------------------------------------------------------------------------------------------
/**
 *  Parse a config file.
 *
 *  @return
 *      - LE_OK if successful.
 *      - LE_BAD_PARAMETER parsing failed because a parameter was invalid.
 *      - LE_FORMAT_ERROR parsing failed because of a format error in file.
 *      - LE_IO_ERROR parsing failed because cannot read the file.
 *      - LE_FAULT parsing failed in apply phase.
 */
//--------------------------------------------------------------------------------------------------
le_result_t configService_ParseConfig
(
    int fd,                                ///< [IN] File descriptor of the configuration file.
    bool validateOnly,                     ///< [IN] Boolean to indicate if this is a validate or
                                           /// apply operation.
    parseError_t* parseErrorPtr            ///< [OUT] Pointer to the Parse Error structure.
);

//--------------------------------------------------------------------------------------------------
/**
 * Trigger the registered callback for the specified destination, if available
 *
 * @return
 *      - LE_OK                Function succeeded.
 *      - LE_BAD_PARAMETER     Invalid destination record variable.
 *      - LE_NOUT_FOUND        Unable to find matching destination name
 */
//--------------------------------------------------------------------------------------------------
le_result_t configService_TriggerDestinationPushHandler
(
    const char* destination,     ///< [IN] Destination path of resource
    const char* obsName,         ///< [IN] Observation Name
    const char* srcPath,         ///< [IN] Source path + JSON extraction, if applicable
    io_DataType_t dataType,      ///< [IN] Data type of the data sample
    dataSample_Ref_t dataSample  ///< [IN] Data sample
);

#endif // CONFIG_SERVICE_H_INCLUDE_GUARD
