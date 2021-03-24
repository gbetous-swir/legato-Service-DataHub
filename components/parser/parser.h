//--------------------------------------------------------------------------------------------------
/**
 * @file parser.h
 *
 *  File Parser Library:
 */
//--------------------------------------------------------------------------------------------------
#include "legato.h"
#include "interfaces.h"

#ifndef PARSER_H_INCLUDE_GUARD
#define PARSER_H_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Maximum allowed size for string fields
 */
//--------------------------------------------------------------------------------------------------
#define PARSER_MAX_ERROR_MSG_BYTES              (CONFIG_MAX_ERROR_MSG_LEN + 1)

#define PARSER_OBSNAME_MAX_BYTES                (IO_MAX_RESOURCE_PATH_LEN + 1)
#define PARSER_OBS_DEST_MAX_BYTES               (IO_MAX_RESOURCE_PATH_LEN + 1)
#define PARSER_OBS_RES_MAX_BYTES                (IO_MAX_RESOURCE_PATH_LEN + 1)

#define PARSER_OBS_TRANSFORM_MAX_BYTES          (7)
#define PARSER_OBS_JSON_EX_MAX_BYTES            (ADMIN_MAX_JSON_EXTRACTOR_LEN + 1)

#define PARSER_STATE_MAX_STRING_BYTES           (IO_MAX_STRING_VALUE_LEN + 1)
#define PARSER_STATE_MAX_PATH_BYTES             (IO_MAX_RESOURCE_PATH_LEN + 1)


//--------------------------------------------------------------------------------------------------
/**
 * Mask and position for individual observation fields.
 */
//--------------------------------------------------------------------------------------------------
#define PARSER_OBS_RESOURCE_POS                 (0)
#define PARSER_OBS_RESOURCE_MASK                (0x1)
#define PARSER_OBS_DEST_POS                     (1)
#define PARSER_OBS_DEST_MASK                    (0x2)
#define PARSER_OBS_PERIOD_POS                   (2)
#define PARSER_OBS_PERIOD_MASK                  (0x4)
#define PARSER_OBS_CHANGEBY_POS                 (3)
#define PARSER_OBS_CHANGEBY_MASK                (0x8)
#define PARSER_OBS_LOWERTHAN_POS                (4)
#define PARSER_OBS_LOWERTHAN_MASK               (0x10)
#define PARSER_OBS_GREATERTHAN_POS              (5)
#define PARSER_OBS_GREATERTHAN_MASK             (0x20)
#define PARSER_OBS_BUFFER_POS                   (6)
#define PARSER_OBS_BUFFER_MASK                  (0x40)
#define PARSER_OBS_TRANSFORM_POS                (7)
#define PARSER_OBS_TRANSFORM_MASK               (0x80)
#define PARSER_OBS_JSON_EXT_POS                 (8)
#define PARSER_OBS_JSON_EXT_MASK                (0x100)


//--------------------------------------------------------------------------------------------------
/**
 * Mask and position for individual state fields.
 */
//--------------------------------------------------------------------------------------------------
#define PARSER_STATE_VALUE_POS                  (0)
#define PARSER_STATE_VALUE_MASK                 (0x1)
#define PARSER_STATE_DATATYPE_POS               (1)
#define PARSER_STATE_DATATYPE_MASK              (0x2)


//--------------------------------------------------------------------------------------------------
/**
 * This reference is used by clients to point to a particular parse session.
 */
//--------------------------------------------------------------------------------------------------
typedef struct ParseEnv* Parser_ParseSessionRef_t;

//--------------------------------------------------------------------------------------------------
/**
 * This structure holds data that is found in an observation.
 * obsName, resourcePath, and destination will always be present and valid. Other members will be
 * set to a default value if they are missing from the file. Bellow is a list of members and their
 * default value:
 * minPeriod, changeBy, lowerThan, and greaterThan: NAN
 * bufferMaxCount: 0
 * transform: ADMIN_OBS_TRANSFORM_TYPE_NONE
 * jsonExtraction: '/0'
 */
//--------------------------------------------------------------------------------------------------
typedef struct parser_ObsData
{
    uint32_t bitmask;                                   ///< bitmask indicating which fields
                                                        /// were present in the file.
    char obsName[PARSER_OBSNAME_MAX_BYTES];             ///< Name of observation.
    char resourcePath[PARSER_OBS_RES_MAX_BYTES];        ///< Value of "r"
    char destination[PARSER_OBS_DEST_MAX_BYTES];        ///< Value of "d"
    double minPeriod;                                   ///< Value of "p"
    double changeBy;                                    ///< Value of "st"
    double lowerThan;                                   ///< Value of "lt"
    double greaterThan;                                 ///< Value of "gt"
    uint32_t bufferMaxCount;                            ///< Value of "b"
    admin_TransformType_t transform;                    ///< Value of "f"
    char jsonExtraction[PARSER_OBS_JSON_EX_MAX_BYTES];  ///< Value of "s"
} parser_ObsData_t;


//--------------------------------------------------------------------------------------------------
/**
 * This structure holds data that is found in a state.
 * @note:
 * Regardless of whether the "dt" key was found in the state object or not(The bistmaks bit for
 * datatype), the dataType field of this strucutre is always valid. The value of this field will be
 * set according to the json value's type. A JSON string will set this field to IO_DATA_TYPE_STRING
 * unless "dt":"json" is also present in the state object.
 */
//--------------------------------------------------------------------------------------------------
typedef struct parser_StateData
{
    uint32_t bitmask;                                        ///< bitmask indicating which fields
                                                             /// were present in the file.
    union Value                                              ///< Value of "v"
    {
        double number;
        bool boolean;
        char string[PARSER_STATE_MAX_STRING_BYTES];
    } value;
    io_DataType_t dataType;                                  ///< Type of value
    char resourcePath[PARSER_STATE_MAX_PATH_BYTES];          ///< Key of state
} parser_StateData_t;


//--------------------------------------------------------------------------------------------------
/**
 *  Callback function prototypes:
 *  They provide the value and the context.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*parser_StringCb_fpt)  (const char*, void*);
typedef void (*parser_NumericCb_fpt) (double, void*);
typedef void (*parser_IntCb_fpt)     (int, void*);
typedef void (*parser_BooleanCb_fpt) (bool, void*);
typedef void (*parser_SimpleCb_fpt)  (void*);

typedef void (*parser_ObsCb_fpt)     (parser_ObsData_t* , void*);
typedef void (*parser_StateCb_fpt)   (parser_StateData_t* , void*);

//--------------------------------------------------------------------------------------------------
/**
 *  Error event callback:
 * The first argument is an le_result_t indicating the error that happened. Here's a list of
 * possible error codes:
 *  - LE_BAD_PARAMETER : a parameter was invalid.
 *  - LE_FORMAT_ERROR: JSON or octave file schema error.
 *  - LE_IO_ERROR: reading from file resulted in error.
 *  - LE_BUSY: Parser cannot accept another parse session.
 * second argument is the error message.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*parser_ErrorCb_fpt)   (le_result_t, const char* ,  void*);


//--------------------------------------------------------------------------------------------------
/**
 *  Parser file callbacks:
 */
//--------------------------------------------------------------------------------------------------
typedef struct parser_Callbacks
{
    parser_IntCb_fpt    type;       ///< called with the int value of "t" section at the root.

    parser_StringCb_fpt version;    ///< called with value of the "v" section at the root.

    parser_NumericCb_fpt timeStamp; ///< called with value of "ts" section at the root.

    parser_SimpleCb_fpt oObject;    ///< called when the "o" object is seen at the root.

    parser_ObsCb_fpt  observation;  ///< called with data of an observation.

    parser_SimpleCb_fpt oObjectEnd; ///< called when finished reading all observations.

    parser_SimpleCb_fpt sObject;    ///< called when the "s" object is seen.

    parser_StateCb_fpt state;       ///< called with data of a state.

    parser_SimpleCb_fpt sObjectEnd; ///< called when finished reading all states.

    parser_SimpleCb_fpt aObject;    ///< called when the "a" object is seen.

    parser_StringCb_fpt actionId;   ///< called with action Id of an action.

    parser_SimpleCb_fpt endOfParse; ///< called when entire file is parsed and there will no
                                    //longer be any callbacks.

    parser_ErrorCb_fpt error;       ///< called when parses faces an error. parsing stops
                                    //after this.
} parser_Callbacks_t;


//--------------------------------------------------------------------------------------------------
/**
 *  Set parser callbacks.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void parser_SetCallbacks
(
    Parser_ParseSessionRef_t parseSessionRef,        ///< [IN] Parse session reference.
    parser_Callbacks_t* callbacksPtr                 ///< [IN] Pointer to callback structure
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 *  Stop the parser.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void parser_StopParse
(
    Parser_ParseSessionRef_t parseSessionRef         ///< [IN] Parse session reference.
);

//--------------------------------------------------------------------------------------------------
/**
 *  Get number of bytes read from file so far.
 *
 * @return:
 *      Number of bytes read from file so far.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED size_t parser_GetNumBytesRead
(
);

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
);

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
);

#endif // PARSER_H_INCLUDE_GUARD
