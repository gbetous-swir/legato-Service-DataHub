
/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */
#ifndef QUERY_COMMON_H_INCLUDE_GUARD
#define QUERY_COMMON_H_INCLUDE_GUARD


#include "legato.h"

// Interface specific includes
#include "io_common.h"

#define IFGEN_QUERY_PROTOCOL_ID "bddfa3a9a8e7e2216b61441a8dc6f987"
#define IFGEN_QUERY_MSG_SIZE 50024



//--------------------------------------------------------------------------------------------------
/**
 */
//--------------------------------------------------------------------------------------------------
#define QUERY_BEGINNING_OF_TIME 0

//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'query_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct query_TriggerPushHandler* query_TriggerPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'query_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct query_BooleanPushHandler* query_BooleanPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'query_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct query_NumericPushHandler* query_NumericPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'query_StringPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct query_StringPushHandler* query_StringPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'query_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct query_JsonPushHandler* query_JsonPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    QUERY_SNAPSHOT_FORMAT_JSON = 0,
        ///< Basic JSON representation of the resource tree.
    QUERY_SNAPSHOT_FORMAT_OCTAVE = 1,
        ///< Format for the payload of an Octave "tree" or "tree-diff" message.
    QUERY_SNAPSHOT_FORMAT_CUSTOM = 2
        ///< First available custom format type value.
}
query_SnapshotFormat_t;


//--------------------------------------------------------------------------------------------------
/**
 */
//--------------------------------------------------------------------------------------------------/// After taking this snapshot, flush accumulated deletion
/// records.
#define QUERY_SNAPSHOT_FLAG_FLUSH_DELETIONS 0x1/// First available custom control flag.
#define QUERY_SNAPSHOT_FLAG_CUSTOM 0x2
typedef uint32_t query_SnapshotFlag_t;


//--------------------------------------------------------------------------------------------------
/**
 * Completion callbacks for query_ReadBufferJson() must look like this.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*query_ReadCompletionFunc_t)
(
        le_result_t result,
        ///< LE_OK if successful, LE_COMM_ERROR if write to outputFile failed.
        void* contextPtr
        ///<
);

//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing triggers to an output
 */
//--------------------------------------------------------------------------------------------------
typedef void (*query_TriggerPushHandlerFunc_t)
(
        double timestamp,
        ///< Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        void* contextPtr
        ///<
);

//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing Boolean values to an output
 */
//--------------------------------------------------------------------------------------------------
typedef void (*query_BooleanPushHandlerFunc_t)
(
        double timestamp,
        ///< Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        bool value,
        ///<
        void* contextPtr
        ///<
);

//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing numeric values to an output
 */
//--------------------------------------------------------------------------------------------------
typedef void (*query_NumericPushHandlerFunc_t)
(
        double timestamp,
        ///< Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        double value,
        ///<
        void* contextPtr
        ///<
);

//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing string values to an output
 */
//--------------------------------------------------------------------------------------------------
typedef void (*query_StringPushHandlerFunc_t)
(
        double timestamp,
        ///< Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        const char* LE_NONNULL value,
        ///<
        void* contextPtr
        ///<
);

//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing JSON values to an output
 */
//--------------------------------------------------------------------------------------------------
typedef void (*query_JsonPushHandlerFunc_t)
(
        double timestamp,
        ///< Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        const char* LE_NONNULL value,
        ///<
        void* contextPtr
        ///<
);

//--------------------------------------------------------------------------------------------------
/**
 */
//--------------------------------------------------------------------------------------------------
typedef void (*query_HandleSnapshotResultFunc_t)
(
        le_result_t status,
        ///< LE_OK when content is complete. Any other value indicates an error.
        void* contextPtr
        ///<
);


//--------------------------------------------------------------------------------------------------
/**
 * Get if this client bound locally.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool ifgen_query_HasLocalBinding
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Init data that is common across all threads
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_query_InitCommonData
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Perform common initialization and open a session
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_OpenSession
(
    le_msg_SessionRef_t _ifgen_sessionRef,
    bool isBlocking
);

//--------------------------------------------------------------------------------------------------
/**
 * Read data out of a buffer.  Data is written to a given file descriptor in JSON-encoded format
 * as an array of objects containing a timestamp and a value (or just a timestamp for triggers).
 * E.g.,
 *
 * @code
 * [{"t":1537483647.125,"v":true},{"t":1537483657.128,"v":true}]
 * @endcode
 *
 * @return
 *  - LE_OK if the read operation started successfully.
 *  - LE_NOT_FOUND if the Observation doesn't exist.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_ReadBufferJson
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL obsPath,
        ///< [IN] Observation path. Can be absolute
        ///< (beginning with a '/') or relative to /obs/.
        double startAfter,
        ///< [IN] Start after this many seconds ago,
        ///< or after an absolute number of seconds since the Epoch
        ///< (if startafter > 30 years).
        ///< Use NAN (not a number) to read the whole buffer.
        int outputFile,
        ///< [IN] File descriptor to write the data to.
        query_ReadCompletionFunc_t completionFuncPtr,
        ///< [IN] Completion callback to be called when operation finishes.
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Read the timestamp of a single sample from a buffer.
 *
 * @note This can be used with any type of sample.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the Observation doesn't exist or does not have a sample newer than the given
 *                 startAfter timestamp.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_ReadBufferSampleTimestamp
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL obsPath,
        ///< [IN] Observation path. Can be absolute
        ///< (beginning with a '/') or relative to /obs/.
        double startAfter,
        ///< [IN] Start after this many seconds ago,
        ///< or after an absolute number of seconds since the Epoch
        ///< (if startafter > 30 years).
        ///< Use NAN (not a number) to read the oldest sample in the buffer.
        double* timestampPtr
        ///< [OUT] Timestamp of the sample, if LE_OK returned.
);

//--------------------------------------------------------------------------------------------------
/**
 * Read a single Boolean sample from a buffer.
 *
 * @warning This can only be used with Boolean type samples.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the Observation doesn't exist or does not have a Boolean sample newer than
 *                 the given startAfter timestamp.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_ReadBufferSampleBoolean
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL obsPath,
        ///< [IN] Observation path. Can be absolute
        ///< (beginning with a '/') or relative to /obs/.
        double startAfter,
        ///< [IN] Start after this many seconds ago,
        ///< or after an absolute number of seconds since the Epoch
        ///< (if startafter > 30 years).
        ///< Use NAN (not a number) to read the oldest sample in the buffer.
        double* timestampPtr,
        ///< [OUT] Timestamp of the sample, if LE_OK returned.
        bool* valuePtr
        ///< [OUT] Value of the sample, if LE_OK returned.
);

//--------------------------------------------------------------------------------------------------
/**
 * Read a single numeric sample from a buffer.
 *
 * @warning This can only be used with numeric type samples.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the Observation doesn't exist or does not have a Numeric sample newer than
 *                 the given startAfter timestamp.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_ReadBufferSampleNumeric
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL obsPath,
        ///< [IN] Observation path. Can be absolute
        ///< (beginning with a '/') or relative to /obs/.
        double startAfter,
        ///< [IN] Start after this many seconds ago,
        ///< or after an absolute number of seconds since the Epoch
        ///< (if startafter > 30 years).
        ///< Use NAN (not a number) to read the oldest sample in the buffer.
        double* timestampPtr,
        ///< [OUT] Timestamp of the sample, if LE_OK returned.
        double* valuePtr
        ///< [OUT] Value of the sample, if LE_OK returned.
);

//--------------------------------------------------------------------------------------------------
/**
 * Read a single sample from a buffer as a string.
 *
 * @note This can be used with any type of sample.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the Observation doesn't exist or does not have a sample newer than the given
 *                 startAfter timestamp.
 *  - LE_OVERFLOW if the buffer provided is too small to hold the value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_ReadBufferSampleString
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL obsPath,
        ///< [IN] Observation path. Can be absolute
        ///< (beginning with a '/') or relative to /obs/.
        double startAfter,
        ///< [IN] Start after this many seconds ago,
        ///< or after an absolute number of seconds since the Epoch
        ///< (if startafter > 30 years).
        ///< Use NAN (not a number) to read the oldest sample in the buffer.
        double* timestampPtr,
        ///< [OUT] Timestamp of the sample, if LE_OK returned.
        char* value,
        ///< [OUT] Value of the sample, if LE_OK returned.
        size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Read a single sample from a buffer as JSON.
 *
 * @note This can be used with any type of sample.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the Observation doesn't exist or does not have a sample newer than the given
 *                 startAfter timestamp.
 *  - LE_OVERFLOW if the buffer provided is too small to hold the value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_ReadBufferSampleJson
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL obsPath,
        ///< [IN] Observation path. Can be absolute
        ///< (beginning with a '/') or relative to /obs/.
        double startAfter,
        ///< [IN] Start after this many seconds ago,
        ///< or after an absolute number of seconds since the Epoch
        ///< (if startafter > 30 years).
        ///< Use NAN (not a number) to read the oldest sample in the buffer.
        double* timestampPtr,
        ///< [OUT] Timestamp of the sample, if LE_OK returned.
        char* value,
        ///< [OUT] Value of the sample, if LE_OK returned.
        size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum value found in an Observation's data set within a given time span.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED double ifgen_query_GetMin
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL obsPath,
        ///< [IN] Observation path. Can be absolute
        ///< (beginning with a '/') or relative to /obs/.
        double startTime
        ///< [IN] If < 30 years then seconds before now; else seconds since the Epoch.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the maximum value found within a given time span in an Observation's buffer.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED double ifgen_query_GetMax
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL obsPath,
        ///< [IN] Observation path. Can be absolute
        ///< (beginning with a '/') or relative to /obs/.
        double startTime
        ///< [IN] If < 30 years then seconds before now; else seconds since the Epoch.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the mean (average) of all values found within a given time span in an Observation's buffer.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED double ifgen_query_GetMean
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL obsPath,
        ///< [IN] Observation path. Can be absolute
        ///< (beginning with a '/') or relative to /obs/.
        double startTime
        ///< [IN] If < 30 years then seconds before now; else seconds since the Epoch.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the standard deviation of all values found within a given time span in an
 * Observation's buffer.
 *
 * @return The value, or NAN (not-a-number) if there's no numerical data in the Observation's
 *         buffer (if the buffer size is zero, the buffer is empty, or the buffer contains data
 *         of a non-numerical type).
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED double ifgen_query_GetStdDev
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL obsPath,
        ///< [IN] Observation path. Can be absolute
        ///< (beginning with a '/') or relative to /obs/.
        double startTime
        ///< [IN] If < 30 years then seconds before now; else seconds since the Epoch.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current data type of a resource.
 *
 * @note Observations and Placeholders will default to IO_DATA_TYPE_TRIGGER, but will change
 *       types as other types of data are pushed to them.  The data types of Inputs and Outputs
 *       are decided by the apps that create them.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource was not found.
 *  - LE_UNSUPPORTED if the path refers to a namespace (which can't have a data type).
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_GetDataType
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path. Can be absolute (beginning
        ///< with a '/') or relative to the namespace of
        ///< the calling app (/app/<app-name>/).
        io_DataType_t* dataTypePtr
        ///< [OUT] The fetched data type, if LE_OK is returned.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current units of a resource.
 *
 * @note Observations and Placeholders will default to "", but will change units as data is
 *       pushed to them.  The units of Inputs and Outputs are decided by the apps that create them.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource was not found.
 *  - LE_UNSUPPORTED if the path refers to a namespace (which can't have a data type).
 *  - LE_OVERFLOW if the units string was truncated because it is larger than the buffer provided.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_GetUnits
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path. Can be absolute (beginning
        ///< with a '/') or relative to the namespace of
        ///< the calling app (/app/<app-name>/).
        char* units,
        ///< [OUT] The fetched units, if LE_OK is returned.
        size_t unitsSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the timestamp of the current value of a resource.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource was not found.
 *  - LE_UNSUPPORTED if the path refers to a namespace (which can't have a data type).
 *  - LE_UNAVAILABLE if the resource doesn't have a current value (yet).
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_GetTimestamp
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path. Can be absolute (beginning
        ///< with a '/') or relative to the namespace of
        ///< the calling app (/app/<app-name>/).
        double* timestampPtr
        ///< [OUT] The fetched timestamp (in seconds since the Epoch), if LE_OK returned.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current value of a resource, if it's Boolean type.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource was not found.
 *  - LE_UNSUPPORTED if the path refers to a namespace (which can't have a data type).
 *  - LE_UNAVAILABLE if the resource doesn't have a current value (yet).
 *  - LE_FORMAT_ERROR if the resource has another data type.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_GetBoolean
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path. Can be absolute (beginning
        ///< with a '/') or relative to the namespace of
        ///< the calling app (/app/<app-name>/).
        double* timestampPtr,
        ///< [OUT] Fetched timestamp (in seconds since the Epoch), if LE_OK returned.
        bool* valuePtr
        ///< [OUT] Fetched value, if LE_OK returned.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current value of a resource, if it's numeric type.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource was not found.
 *  - LE_UNSUPPORTED if the path refers to a namespace (which can't have a data type).
 *  - LE_UNAVAILABLE if the resource doesn't have a current value (yet).
 *  - LE_FORMAT_ERROR if the resource has another data type.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_GetNumeric
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path. Can be absolute (beginning
        ///< with a '/') or relative to the namespace of
        ///< the calling app (/app/<app-name>/).
        double* timestampPtr,
        ///< [OUT] Fetched timestamp (in seconds since the Epoch), if LE_OK returned.
        double* valuePtr
        ///< [OUT] Fetched value, if LE_OK returned.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current value of a resource, if it's a string type.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource was not found.
 *  - LE_UNSUPPORTED if the path refers to a namespace (which can't have a data type).
 *  - LE_UNAVAILABLE if the resource doesn't have a current value (yet).
 *  - LE_FORMAT_ERROR if the resource has another data type.
 *  - LE_OVERFLOW if the value was truncated because it is larger than the buffer provided.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_GetString
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path. Can be absolute (beginning
        ///< with a '/') or relative to the namespace of
        ///< the calling app (/app/<app-name>/).
        double* timestampPtr,
        ///< [OUT] Fetched timestamp (in seconds since the Epoch), if LE_OK returned.
        char* value,
        ///< [OUT] Fetched value, if LE_OK returned.
        size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current value of a resource of any type, in JSON format.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource was not found.
 *  - LE_UNSUPPORTED if the path refers to a namespace (which can't have a data type).
 *  - LE_UNAVAILABLE if the resource doesn't have a current value (yet).
 *  - LE_OVERFLOW if the value was truncated because it is larger than the buffer provided.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_GetJson
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path. Can be absolute (beginning
        ///< with a '/') or relative to the namespace of
        ///< the calling app (/app/<app-name>/).
        double* timestampPtr,
        ///< [OUT] Fetched timestamp (in seconds since the Epoch), if LE_OK returned.
        char* value,
        ///< [OUT] Fetched value, if LE_OK returned.
        size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the example JSON value string for a given Input resource.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource was not found.
 *  - LE_UNSUPPORTED if the path refers to something that doesn't have a JSON type.
 *  - LE_UNAVAILABLE if the JSON-type resource doesn't have an example value.
 *  - LE_OVERFLOW if the value was truncated because it is larger than the buffer provided.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_query_GetJsonExample
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path. Can be absolute (beginning
        ///< with a '/') or relative to the namespace of
        ///< the calling app (/app/<app-name>/).
        char* example,
        ///< [OUT] Example will be put here, if LE_OK returned.
        size_t exampleSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'query_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED query_TriggerPushHandlerRef_t ifgen_query_AddTriggerPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Absolute path of resource.
        query_TriggerPushHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'query_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_query_RemoveTriggerPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        query_TriggerPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'query_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED query_BooleanPushHandlerRef_t ifgen_query_AddBooleanPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Absolute path of resource.
        query_BooleanPushHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'query_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_query_RemoveBooleanPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        query_BooleanPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'query_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED query_NumericPushHandlerRef_t ifgen_query_AddNumericPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Absolute path of resource.
        query_NumericPushHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'query_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_query_RemoveNumericPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        query_NumericPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'query_StringPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED query_StringPushHandlerRef_t ifgen_query_AddStringPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Absolute path of resource.
        query_StringPushHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'query_StringPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_query_RemoveStringPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        query_StringPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'query_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED query_JsonPushHandlerRef_t ifgen_query_AddJsonPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Absolute path of resource.
        query_JsonPushHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'query_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_query_RemoveJsonPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        query_JsonPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_query_TakeSnapshot
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        uint32_t format,
        ///< [IN] Snapshot output data format.
        query_SnapshotFlag_t flags,
        ///< [IN] Flags controlling the
        ///< snapshot action.
        const char* LE_NONNULL path,
        ///< [IN] Tree path to use as the root.
        double since,
        ///< [IN] Request only values that have
        ///< changed since this time (in s).
        ///< Use BEGINNING_OF_TIME to
        ///< request the full tree.
        query_HandleSnapshotResultFunc_t callbackPtr,
        ///< [IN] Completion callback to indicate
        ///< the end of the streamed
        ///< snapshot, or an error.
        void* contextPtr,
        ///< [IN]
        int* snapshotStreamPtr
        ///< [OUT] File descriptor to which the
        ///< encoded snapshot data will be
        ///< streamed.
);

//--------------------------------------------------------------------------------------------------
/**
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_query_TrackDeletions
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        bool on
        ///< [IN] If true, start tracking deletions; if false stop tracking and flush records.
);

#endif // QUERY_COMMON_H_INCLUDE_GUARD
