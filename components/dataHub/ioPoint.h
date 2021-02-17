//--------------------------------------------------------------------------------------------------
/**
 * Interface provided by the I/O Point module to other modules within the Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef IO_POINT_H_INCLUDE_GUARD
#define IO_POINT_H_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the I/O Point module.  This function MUST be called before any others in this module.
 */
//--------------------------------------------------------------------------------------------------
void ioPoint_Init
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Create an Input Resource.
 *
 * @return Pointer to the Resource or NULL if it failed to create a resource.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* ioPoint_CreateInput
(
    io_DataType_t dataType,
    resTree_EntryRef_t entryRef ///< The resource tree entry to attach this Resource to.
);


//--------------------------------------------------------------------------------------------------
/**
 * Create an Output Resource.
 *
 * @return Pointer to the Resource or NULL if it failed to create a resource.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* ioPoint_CreateOutput
(
    io_DataType_t dataType,
    resTree_EntryRef_t entryRef ///< The resource tree entry to attach this Resource to.
);


//--------------------------------------------------------------------------------------------------
/**
 * Create a placeholder IO Resource.
 *
 * @return Pointer to the Resource or NULL if it failed to create a resource.
 */
//--------------------------------------------------------------------------------------------------
res_Resource_t* ioPoint_CreatePlaceholderIO
(
    resTree_EntryRef_t entryRef ///< The resource tree entry to attach this Resource to.
);


//--------------------------------------------------------------------------------------------------
/**
 * Make a placeholder resource an input
 */
//--------------------------------------------------------------------------------------------------
void ioPoint_MakeResourceInput
(
    res_Resource_t* resPtr,   ///< Pointer to resource
    io_DataType_t dataType    ///< Io resource data type.
);


//--------------------------------------------------------------------------------------------------
/**
 * Make a placeholder resource an output
 */
//--------------------------------------------------------------------------------------------------
void ioPoint_MakeResourceOutput
(
    res_Resource_t* resPtr,   ///< Pointer to resource
    io_DataType_t dataType    ///< Io resource data type.
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of an Input or Output resource.
 *
 * @return The data type.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t ioPoint_GetDataType
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Add a Push Handler.
 *
 * @return Reference to the handler added.
 */
//--------------------------------------------------------------------------------------------------
hub_HandlerRef_t ioPoint_AddPushHandler
(
    res_Resource_t* resPtr,
    io_DataType_t dataType,
    void* callbackPtr,
    void* contextPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Remove a Push Handler.
 */
//--------------------------------------------------------------------------------------------------
void ioPoint_RemovePushHandler
(
    hub_HandlerRef_t handlerRef
);


//--------------------------------------------------------------------------------------------------
/**
 * Perform type coercion, replacing a data sample with another of a different type, if necessary,
 * to make the data compatible with the data type of a given Input or Output resource.
 *
 * @return
 *      - LE_OK If coercion happened successfully.
 *      - LE_NO_MEMORY If could not coerce to a new type because failed to allocate a new datasample
 */
//--------------------------------------------------------------------------------------------------
le_result_t ioPoint_DoTypeCoercion
(
    res_Resource_t* resPtr,
    io_DataType_t* dataTypePtr,     ///< [INOUT] the data type, may be changed by type coercion
    dataSample_Ref_t* valueRefPtr   ///< [INOUT] the data sample, may be replaced by type coercion
);


//--------------------------------------------------------------------------------------------------
/**
 * Mark an Output resource "optional".  (By default, they are marked "mandatory".)
 */
//--------------------------------------------------------------------------------------------------
void ioPoint_MarkOptional
(
    res_Resource_t* resPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Check if a given resource is a mandatory output.  If so, it means that this is an output resource
 * that must have a value before the related app function will begin working.
 *
 * @return true if a mandatory output, false if it's an optional output or not an output at all.
 */
//--------------------------------------------------------------------------------------------------
bool ioPoint_IsMandatory
(
    res_Resource_t* resPtr
);


#endif // IO_POINT_H_INCLUDE_GUARD
