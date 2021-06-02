
/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */
#ifndef IO_COMMON_H_INCLUDE_GUARD
#define IO_COMMON_H_INCLUDE_GUARD


#include "legato.h"

#define IFGEN_IO_PROTOCOL_ID "a2dafb94eca240de32c6cbd223a593ba"
#define IFGEN_IO_MSG_SIZE 50103



//--------------------------------------------------------------------------------------------------
/**
 * Constant used in place of a timestamp, when pushing samples to the Data Hub, to ask the Data Hub
 * to generate a timestamp for the sample.
 */
//--------------------------------------------------------------------------------------------------
#define IO_NOW 0

//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of bytes (excluding null terminator) in an I/O resource's path within its
 * namespace in the Data Hub's resource tree.
 */
//--------------------------------------------------------------------------------------------------
#ifndef IO_MAX_RESOURCE_PATH_LEN
#define IO_MAX_RESOURCE_PATH_LEN 79
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of bytes (excluding terminator) in the value of a string type data sample.
 */
//--------------------------------------------------------------------------------------------------
#ifndef IO_MAX_STRING_VALUE_LEN
#define IO_MAX_STRING_VALUE_LEN 50000
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of bytes (excluding terminator) in the units string of a numeric I/O resource.
 */
//--------------------------------------------------------------------------------------------------
#define IO_MAX_UNITS_NAME_LEN 23

//--------------------------------------------------------------------------------------------------
/**
 * Enumerates the data types supported.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    IO_DATA_TYPE_TRIGGER = 0,
        ///< trigger
    IO_DATA_TYPE_BOOLEAN = 1,
        ///< Boolean
    IO_DATA_TYPE_NUMERIC = 2,
        ///< numeric (floating point number)
    IO_DATA_TYPE_STRING = 3,
        ///< string
    IO_DATA_TYPE_JSON = 4
        ///< JSON
}
io_DataType_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct io_TriggerPushHandler* io_TriggerPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct io_BooleanPushHandler* io_BooleanPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct io_NumericPushHandler* io_NumericPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_StringPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct io_StringPushHandler* io_StringPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
typedef struct io_JsonPushHandler* io_JsonPushHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'io_UpdateStartEnd'
 */
//--------------------------------------------------------------------------------------------------
typedef struct io_UpdateStartEndHandler* io_UpdateStartEndHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Callback function for pushing triggers to an output
 */
//--------------------------------------------------------------------------------------------------
typedef void (*io_TriggerPushHandlerFunc_t)
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
typedef void (*io_BooleanPushHandlerFunc_t)
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
typedef void (*io_NumericPushHandlerFunc_t)
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
typedef void (*io_StringPushHandlerFunc_t)
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
typedef void (*io_JsonPushHandlerFunc_t)
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
 * Callback function for notification that a Data Hub reconfiguration is beginning or ending.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*io_UpdateStartEndHandlerFunc_t)
(
        bool isStarting,
        ///< true = an update is starting, false = the update has ended.
        void* contextPtr
        ///<
);


//--------------------------------------------------------------------------------------------------
/**
 * Get if this client bound locally.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool ifgen_io_HasLocalBinding
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Init data that is common across all threads
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_InitCommonData
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Perform common initialization and open a session
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_io_OpenSession
(
    le_msg_SessionRef_t _ifgen_sessionRef,
    bool isBlocking
);

//--------------------------------------------------------------------------------------------------
/**
 * Create an input resource, which is used to push data into the Data Hub.
 *
 * Does nothing if the resource already exists.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_DUPLICATE if a resource by that name exists but with different direction, type or units.
 *  - LE_NO_MEMORY if the client is not permitted to create that many resources.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_io_CreateInput
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Path within the client app's resource namespace.
        io_DataType_t dataType,
        ///< [IN] The data type.
        const char* LE_NONNULL units
        ///< [IN] e.g., "degC" (see senml); "" = unspecified.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the example value for a JSON-type Input resource.
 *
 * Does nothing if the resource is not found, is not an input, or doesn't have a JSON type.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_SetJsonExample
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Path within the client app's resource namespace.
        const char* LE_NONNULL example
        ///< [IN] The example JSON value string.
);

//--------------------------------------------------------------------------------------------------
/**
 * Create an output resource, which is used to receive data output from the Data Hub.
 *
 * Does nothing if the resource already exists.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_DUPLICATE if a resource by that name exists but with different direction, type or units.
 *  - LE_NO_MEMORY if the client is not permitted to create that many resources.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_io_CreateOutput
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Path within the client app's resource namespace.
        io_DataType_t dataType,
        ///< [IN] The data type.
        const char* LE_NONNULL units
        ///< [IN] e.g., "degC" (see senml); "" = unspecified.
);

//--------------------------------------------------------------------------------------------------
/**
 * Delete a resource.
 *
 * Does nothing if the resource doesn't exist.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_DeleteResource
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path
        ///< [IN] Resource path within the client app's namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a trigger type data sample.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_PushTrigger
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double timestamp
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< IO_NOW = now (i.e., generate a timestamp for me).
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a Boolean type data sample.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_PushBoolean
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< IO_NOW = now (i.e., generate a timestamp for me).
        bool value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a numeric type data sample.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_PushNumeric
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< IO_NOW = now (i.e., generate a timestamp for me).
        double value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a string type data sample.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_PushString
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< IO_NOW = now (i.e., generate a timestamp for me).
        const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Push a JSON data sample.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_PushJson
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double timestamp,
        ///< [IN] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        ///< IO_NOW = now (i.e., generate a timestamp for me).
        const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED io_TriggerPushHandlerRef_t ifgen_io_AddTriggerPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        io_TriggerPushHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_TriggerPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_RemoveTriggerPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        io_TriggerPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED io_BooleanPushHandlerRef_t ifgen_io_AddBooleanPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        io_BooleanPushHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_BooleanPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_RemoveBooleanPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        io_BooleanPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED io_NumericPushHandlerRef_t ifgen_io_AddNumericPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        io_NumericPushHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_NumericPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_RemoveNumericPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        io_NumericPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_StringPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED io_StringPushHandlerRef_t ifgen_io_AddStringPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        io_StringPushHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_StringPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_RemoveStringPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        io_StringPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED io_JsonPushHandlerRef_t ifgen_io_AddJsonPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        io_JsonPushHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_JsonPush'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_RemoveJsonPushHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        io_JsonPushHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Mark an Output resource "optional".  (By default, they are marked "mandatory".)
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_MarkOptional
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path
        ///< [IN] Resource path within the client app's namespace.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set a Boolean type value as the default value of a given resource.
 *
 * @note This will be ignored if the resource already has a default value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_SetBooleanDefault
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        bool value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set a numeric type value as the default value of a given resource.
 *
 * @note This will be ignored if the resource already has a default value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_SetNumericDefault
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set a string type value as the default value of a given resource.
 *
 * @note This will be ignored if the resource already has a default value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_SetStringDefault
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set a JSON type value as the default value of a given resource.
 *
 * @note This will be ignored if the resource already has a default value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_SetJsonDefault
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        const char* LE_NONNULL value
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the timestamp of the current value of an Input or Output resource with any data type.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource does not exist.
 *  - LE_UNAVAILABLE if the resource does not currently have a value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_io_GetTimestamp
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double* timestampPtr
        ///< [OUT] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the current value of a Boolean type Input or Output resource.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource does not exist.
 *  - LE_UNAVAILABLE if the resource does not currently have a value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_io_GetBoolean
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double* timestampPtr,
        ///< [OUT] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        bool* valuePtr
        ///< [OUT]
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the current value of a numeric type Input or Output resource.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_NOT_FOUND if the resource does not exist.
 *  - LE_UNAVAILABLE if the resource does not currently have a value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_io_GetNumeric
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double* timestampPtr,
        ///< [OUT] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        double* valuePtr
        ///< [OUT]
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the current value of a string type Input or Output resource.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_OVERFLOW if the value buffer was too small to hold the value.
 *  - LE_NOT_FOUND if the resource does not exist.
 *  - LE_UNAVAILABLE if the resource does not currently have a value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_io_GetString
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double* timestampPtr,
        ///< [OUT] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        char* value,
        ///< [OUT]
        size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Fetch the current value of an Input or Output resource (of any data type) in JSON format.
 *
 * @return
 *  - LE_OK if successful.
 *  - LE_OVERFLOW if the value buffer was too small to hold the value.
 *  - LE_NOT_FOUND if the resource does not exist.
 *  - LE_UNAVAILABLE if the resource does not currently have a value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_io_GetJson
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        const char* LE_NONNULL path,
        ///< [IN] Resource path within the client app's namespace.
        double* timestampPtr,
        ///< [OUT] Timestamp in seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC).
        char* value,
        ///< [OUT]
        size_t valueSize
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'io_UpdateStartEnd'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED io_UpdateStartEndHandlerRef_t ifgen_io_AddUpdateStartEndHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        io_UpdateStartEndHandlerFunc_t callbackPtr,
        ///< [IN]
        void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'io_UpdateStartEnd'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_io_RemoveUpdateStartEndHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        io_UpdateStartEndHandlerRef_t handlerRef
        ///< [IN]
);

#endif // IO_COMMON_H_INCLUDE_GUARD