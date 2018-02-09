//--------------------------------------------------------------------------------------------------
/**
 * Implementation of Namespaces.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "interfaces.h"
#include "dataHub.h"
#include "dataSample.h"
#include "resource.h"
#include "resTree.h"


//--------------------------------------------------------------------------------------------------
/**
 * Resource tree entry.
 *
 * @warning The members of this structure must not be accessed outside resTree.c.
 */
//--------------------------------------------------------------------------------------------------
typedef struct resTree_Entry
{
    le_dls_Link_t link;  ///< Used to link into parent's list of children.
    struct resTree_Entry* parentPtr; ///< Ptr to the parent entry (NULL if the root entry).
    char name[HUB_MAX_ENTRY_NAME_BYTES]; ///< Name of the entry.
    le_dls_List_t childList;  ///< List of child entries.
    admin_EntryType_t type; ///< The type of entry.
    res_Resource_t* resourcePtr;    ///< Ptr to the Resource object or NULL if just a Namespace.
}
Entry_t;


/// Pointer to the Root object (the root of the resource tree).
static Entry_t* RootPtr;

/// Pool of Entry objects.
static le_mem_PoolRef_t EntryPool = NULL;


//--------------------------------------------------------------------------------------------------
/**
 * Create an entry object (defaults to a Namespace type entry) as a child of another entry.
 *
 * @return A pointer to the new child object.
 */
//--------------------------------------------------------------------------------------------------
static Entry_t* AddChild
(
    Entry_t* parentPtr, ///< Ptr to the parent entry (NULL if creating the Root).
    const char* name    ///< Name of the new child ("" if creating the Root).
)
//--------------------------------------------------------------------------------------------------
{
    Entry_t* entryPtr = le_mem_ForceAlloc(EntryPool);

    entryPtr->link = LE_DLS_LINK_INIT;

    if (LE_OK != le_utf8_Copy(entryPtr->name, name, sizeof(entryPtr->name), NULL))
    {
        LE_ERROR("Resource tree entry name longer than %zu bytes max. Truncated to '%s'.",
                 sizeof(entryPtr->name),
                 name);
    }

    entryPtr->childList = LE_DLS_LIST_INIT;
    entryPtr->type = ADMIN_ENTRY_TYPE_NAMESPACE;
    entryPtr->resourcePtr = NULL;

    if (parentPtr != NULL)
    {
        LE_ASSERT(resTree_FindChild(parentPtr, name) == NULL);

        // Increment the reference count on the parent.
        le_mem_AddRef(parentPtr);

        // Link to the parent entry.
        entryPtr->parentPtr = parentPtr;
        le_dls_Queue(&parentPtr->childList, &entryPtr->link);
    }

    return entryPtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Destructor function for Entry objects.
 */
//--------------------------------------------------------------------------------------------------
static void EntryDestructor
(
    void* objPtr
)
//--------------------------------------------------------------------------------------------------
{
    Entry_t* entryPtr = objPtr;

    LE_ASSERT(entryPtr->parentPtr != NULL);
    LE_ASSERT(le_dls_IsEmpty(&entryPtr->childList));
    LE_ASSERT(entryPtr->resourcePtr == NULL);

    // Remove from parent's list of children.
    le_dls_Remove(&entryPtr->parentPtr->childList, &entryPtr->link);

    // Release the reference to the parent.
    le_mem_Release(entryPtr->parentPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Resource Tree module.
 *
 * @warning Must be called before any other functions in this module.
 */
//--------------------------------------------------------------------------------------------------
void resTree_Init
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    // Create the Namespace Pool (note: Namespaces are just instances of Entry_t).
    EntryPool = le_mem_CreatePool("Res Tree Entry", sizeof(Entry_t));
    le_mem_SetDestructor(EntryPool, EntryDestructor);

    // Create the Root Namespace.
    RootPtr = AddChild(NULL, "");
}


//--------------------------------------------------------------------------------------------------
/**
 * Check whether a given resource tree Entry is a Resource.
 *
 * @return true if it is a resource. false if not.
 */
//--------------------------------------------------------------------------------------------------
bool resTree_IsResource
(
    resTree_EntryRef_t entryRef
)
//--------------------------------------------------------------------------------------------------
{
    return (entryRef->resourcePtr != NULL);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to the root namespace.
 *
 * @return Reference to the object.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetRoot
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    return RootPtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Find a child entry with a given name.
 *
 * @return Reference to the object or NULL if not found.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_FindChild
(
    resTree_EntryRef_t nsRef, ///< Namespace entry to search.
    const char* name    ///< Name of the child entry.
)
//--------------------------------------------------------------------------------------------------
{
    le_dls_Link_t* linkPtr = le_dls_Peek(&nsRef->childList);

    while (linkPtr != NULL)
    {
        Entry_t* childPtr = CONTAINER_OF(linkPtr, Entry_t, link);

        if (strcmp(name, childPtr->name) == 0)
        {
            return childPtr;
        }

        linkPtr = le_dls_PeekNext(&nsRef->childList, linkPtr);
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Go to the entry at a given resource path.
 *
 * Optionally will create a missing entry if doCreate is true.  If doCreate is false, won't
 * create any entries at all and will just return NULL if the entry doesn't exist.
 *
 * @return Reference to the object, or
 *         NULL if path malformed or if doCreate == false and entry doesn't exist.
 */
//--------------------------------------------------------------------------------------------------
static resTree_EntryRef_t GoToEntry
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path,   ///< Path.
    bool doCreate       ///< true = create if missing, false = return NULL if missing.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t currentEntry = baseNamespace;

    size_t i = 0;   // Index into path.

    while (path[i] != '\0')
    {
        char entryName[HUB_MAX_ENTRY_NAME_BYTES];

        // If we're at a slash, skip it.
        if (path[i] == '/')
        {
            i++;
        }

        // Look for a slash or the end of the string as the terminator of the next entry name.
        const char* terminatorPtr = strchrnul(path + i, '/');

        // Compute the length of the entry name in this path element.
        size_t nameLen = terminatorPtr - (path + i);

        // Sanity check the length.
        if (nameLen == 0)
        {
            LE_ERROR("Resource path element missing in path '%s'.", path);
            return NULL;
        }
        if (nameLen >= sizeof(entryName))
        {
            LE_ERROR("Resource path element too long in path '%s'.", path);
            return NULL;
        }

        // Copy the name out and null terminate it.
        (void)strncpy(entryName, path + i, nameLen);
        entryName[nameLen] = '\0';

        // Look up the entry name in the list of children of the current entry.
        // If found, this becomes the new current entry.
        Entry_t* childPtr = resTree_FindChild(currentEntry, entryName);

        if (childPtr == NULL)
        {
            // If we're supposed to create a missing entry, create one now.
            // Otherwise, return NULL.
            if (doCreate)
            {
                childPtr = AddChild(currentEntry, entryName);
            }
            else
            {
                return NULL;
            }
        }

        // The child is now the base for the rest of the path.
        currentEntry = childPtr;

        // Advance the index past the name.
        i += nameLen;
    }

    return currentEntry;
}


//--------------------------------------------------------------------------------------------------
/**
 * Replace the resource attached to an entry with another resource.
 * The original resource is deleted.
 */
//--------------------------------------------------------------------------------------------------
static void ReplaceResource
(
    resTree_EntryRef_t entryRef,    ///< Replace the resource attached to this entry
    res_Resource_t* replacementPtr, ///< With this resource
    admin_EntryType_t replacementType ///< The type of the replacement resource.
)
//--------------------------------------------------------------------------------------------------
{
    // If we're replacing an existing Resource with another type, move Resource settings over.
    if (entryRef->resourcePtr != NULL)
    {
        // Note that this may result in lost settings. For example, Placeholders don't have
        // filter settings, but Observations do, so moving settings from an Observation to a
        // Placeholder will lose the Observation's filter settings.
        res_MoveAdminSettings(entryRef->resourcePtr, replacementPtr);

        // Delete the original resource.
        le_mem_Release(entryRef->resourcePtr);
    }

    entryRef->resourcePtr = replacementPtr;
    entryRef->type = replacementType;
}


//--------------------------------------------------------------------------------------------------
/**
 * Find an entry at a given resource path.
 *
 * @return Reference to the object, or NULL if not found.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_FindEntry
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path    ///< Path.
)
//--------------------------------------------------------------------------------------------------
{
    return GoToEntry(baseNamespace, path, /* doCreate = */ false);
}


//--------------------------------------------------------------------------------------------------
/**
 * Find an entry in the resource tree that resides at a given absolute path.
 *
 * @return a pointer to the entry or NULL if not found (including if the path is malformed).
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_FindEntryAtAbsolutePath
(
    const char* path
)
//--------------------------------------------------------------------------------------------------
{
    // Path must be absolute.
    if (path[0] != '/')
    {
        LE_ERROR("Path not absolute.");
        return NULL;
    }
    path++; // Skip the leading '/'.

    return resTree_FindEntry(resTree_GetRoot(), path);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the name of an entry.
 *
 * @return Ptr to the name. Only valid while the entry exists.
 */
//--------------------------------------------------------------------------------------------------
const char* resTree_GetEntryName
(
    resTree_EntryRef_t entryRef
)
//--------------------------------------------------------------------------------------------------
{
    return entryRef->name;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the type of an entry.
 *
 * @return The entry type.
 */
//--------------------------------------------------------------------------------------------------
admin_EntryType_t resTree_GetEntryType
(
    resTree_EntryRef_t entryRef
)
//--------------------------------------------------------------------------------------------------
{
    return entryRef->type;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the Units of a resource.
 *
 * @return Pointer to the units string.  Valid as long as the resource exists.
 */
//--------------------------------------------------------------------------------------------------
const char* resTree_GetUnits
(
    resTree_EntryRef_t resRef
)
//--------------------------------------------------------------------------------------------------
{
    LE_ASSERT(resTree_IsResource(resRef));

    return res_GetUnits(resRef->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Find out what data type a given resource currently has.
 *
 * Note that the data type of Inputs and Outputs are set by the app that creates those resources.
 * All other resources will change data types as values are pushed to them.
 *
 * @return the data type.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t resTree_GetDataType
(
    resTree_EntryRef_t resRef
)
//--------------------------------------------------------------------------------------------------
{
    LE_ASSERT(resTree_IsResource(resRef));

    return res_GetDataType(resRef->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets a reference to an entry at a given path in the resource tree.
 * Creates a Namespace if nothing exists at that path.
 * Also creates parent, grandparent, etc. Namespaces, as needed.
 *
 * @return Reference to the object, or NULL if the path is malformed.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetEntry
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path    ///< Path.
)
//--------------------------------------------------------------------------------------------------
{
    return GoToEntry(baseNamespace, path, /* doCreate = */ true);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to a resource at a given path.
 * Creates a Placeholder resource if nothing exists at that path.
 * Also creates parent, grandparent, etc. Namespaces, as needed.
 *
 * If there's already a Namespace at the given path, it will be deleted and replaced by a
 * Placeholder.
 *
 * @return Reference to the object, or NULL if the path is malformed.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetResource
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path    ///< Path.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entryRef = GoToEntry(baseNamespace, path, /* doCreate = */ true);

    if (entryRef == NULL)
    {
        return NULL;
    }

    // If a Namespace currently resides at that spot in the tree, replace it with a Placeholder.
    if (entryRef->type == ADMIN_ENTRY_TYPE_NAMESPACE)
    {
        res_Resource_t* placeholderPtr = res_CreatePlaceholder(entryRef);

        ReplaceResource(entryRef, placeholderPtr, ADMIN_ENTRY_TYPE_PLACEHOLDER);
    }

    return entryRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to an Input resource at a given path.
 * Creates a new Input resource if nothing exists at that path.
 * Also creates parent, grandparent, etc. Namespaces, as needed.
 *
 * If there's already a Namespace or Placeholder at the given path, it will be deleted and
 * replaced by an Input.
 *
 * @return Reference to the object, or NULL if the path is malformed, an Output or Observation
 *         already exists at that location, or an Input with different units or data type already
 *         exists at that location.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetInput
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path,       ///< Path.
    io_DataType_t dataType, ///< The data type.
    const char* units       ///< Units string, e.g., "degC" (see senml); "" = unspecified.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entryRef = GoToEntry(baseNamespace, path, /* doCreate = */ true);

    if (entryRef == NULL)
    {
        return NULL;
    }

    switch (entryRef->type)
    {
        // If a Namespace or Placeholder currently resides at that spot in the tree, replace it with
        // an Input.
        case ADMIN_ENTRY_TYPE_NAMESPACE:
        case ADMIN_ENTRY_TYPE_PLACEHOLDER:
        {
            res_Resource_t* resPtr = res_CreateInput(entryRef, dataType, units);
            ReplaceResource(entryRef, resPtr, ADMIN_ENTRY_TYPE_INPUT);
            return entryRef;
        }
        case ADMIN_ENTRY_TYPE_INPUT:

            LE_ERROR("Attempt to replace an Input with another Input.");
            return NULL;

        case ADMIN_ENTRY_TYPE_OUTPUT:

            LE_ERROR("Attempt to replace an Output with an Input.");
            return NULL;

        case ADMIN_ENTRY_TYPE_OBSERVATION:

            LE_ERROR("Attempt to replace an Observation with an Input.");
            return NULL;

        case ADMIN_ENTRY_TYPE_NONE:

            // This should never happen.
            break;
    }

    LE_FATAL("Unexpected entry type %d", entryRef->type);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to an Output resource at a given path.
 * Creates a new Output resource if nothing exists at that path.
 * Also creates parent, grandparent, etc. Namespaces, as needed.
 *
 * If there's already a Namespace or Placeholder at the given path, it will be deleted and
 * replaced by an Output.
 *
 * @return Reference to the object, or NULL if the path is malformed, an Input or Observation
 *         already exists at that location, or an Output with different units or data type already
 *         exists at that location.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetOutput
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path,       ///< Path.
    io_DataType_t dataType, ///< The data type.
    const char* units       ///< Units string, e.g., "degC" (see senml); "" = unspecified.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entryRef = GoToEntry(baseNamespace, path, /* doCreate = */ true);

    if (entryRef == NULL)
    {
        return NULL;
    }

    switch (entryRef->type)
    {
        // If a Namespace or Placeholder currently resides at that spot in the tree, replace it with
        // an Input.
        case ADMIN_ENTRY_TYPE_NAMESPACE:
        case ADMIN_ENTRY_TYPE_PLACEHOLDER:
        {
            res_Resource_t* resPtr = res_CreateOutput(entryRef, dataType, units);
            ReplaceResource(entryRef, resPtr, ADMIN_ENTRY_TYPE_OUTPUT);
            return entryRef;
        }
        case ADMIN_ENTRY_TYPE_INPUT:

            LE_ERROR("Attempt to replace an Input with an Output.");
            return NULL;

        case ADMIN_ENTRY_TYPE_OUTPUT:

            LE_ERROR("Attempt to replace an Output with another Output.");
            return NULL;

        case ADMIN_ENTRY_TYPE_OBSERVATION:

            LE_ERROR("Attempt to replace an Observation with an Output.");
            return NULL;

        case ADMIN_ENTRY_TYPE_NONE:

            // This should never happen.
            break;
    }

    LE_FATAL("Unexpected entry type %d", entryRef->type);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to an Observation resource at a given path.
 * Creates a new Observation resource if nothing exists at that path.
 * Also creates parent, grandparent, etc. Namespaces, as needed.
 *
 * If there's already a Namespace or Placeholder at the given path, it will be deleted and
 * replaced by an Observation.
 *
 * @return Reference to the object, or NULL if the path is malformed or an Input or Output already
 *         exists at that location.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetObservation
(
    resTree_EntryRef_t baseNamespace, ///< Reference to an entry the path is relative to.
    const char* path    ///< Path.
)
//--------------------------------------------------------------------------------------------------
{
    resTree_EntryRef_t entryRef = GoToEntry(baseNamespace, path, /* doCreate = */ true);

    if (entryRef == NULL)
    {
        return NULL;
    }

    // If a Namespace or Placeholder currently resides at that spot in the tree, replace it with
    // an Observation.
    switch (entryRef->type)
    {
        case ADMIN_ENTRY_TYPE_NAMESPACE:
        case ADMIN_ENTRY_TYPE_PLACEHOLDER:
        {
            res_Resource_t* obsPtr = res_CreateObservation(entryRef);
            ReplaceResource(entryRef, obsPtr, ADMIN_ENTRY_TYPE_OBSERVATION);
            return entryRef;
        }
        case ADMIN_ENTRY_TYPE_INPUT:

            LE_ERROR("Attempt to replace an Input with an Observation.");
            return NULL;

        case ADMIN_ENTRY_TYPE_OUTPUT:

            LE_ERROR("Attempt to replace an Output with an Observation.");
            return NULL;

        case ADMIN_ENTRY_TYPE_OBSERVATION:

            // Nothing needs to be done here.
            return entryRef;

        case ADMIN_ENTRY_TYPE_NONE:

            // This should never happen.
            break;
    }

    LE_FATAL("Unexpected entry type %d", entryRef->type);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the path of a given resource tree entry relative to a given namespace.
 *
 * @return
 *  - Number of bytes written to the string buffer (excluding null terminator) if successful.
 *  - LE_OVERFLOW if the string doesn't have space for the path.
 *  - LE_NOT_FOUND if the resource is not in the given namespace.
 */
//--------------------------------------------------------------------------------------------------
ssize_t resTree_GetPath
(
    char* stringBuffPtr,  ///< Ptr to where the path should be written.
    size_t stringBuffSize,  ///< Size of the string buffer, in bytes.
    resTree_EntryRef_t baseNamespace,
    resTree_EntryRef_t entryRef
)
//--------------------------------------------------------------------------------------------------
{
    size_t bytesWritten = 0;

    // Corner case: If the entry is the same as the base namespace,
    // just null terminate, if there's space for that.
    if (entryRef == baseNamespace)
    {
        LE_ASSERT(stringBuffSize > 0);
        stringBuffPtr[0] = '\0';
        return LE_OK;
    }

    // If the parent is the base namespace, just print the name of the current entry.
    if (entryRef->parentPtr == baseNamespace)
    {
        size_t len;

        // If the base namespace is the Root namespace, then prefix with a leading '/'.
        if (baseNamespace == RootPtr)
        {
            if (LE_OK != le_utf8_Copy(stringBuffPtr, "/", stringBuffSize, NULL))
            {
                return LE_OVERFLOW;
            }
            stringBuffPtr++;
            stringBuffSize--;
            bytesWritten = 1;
        }
        if (LE_OK != le_utf8_Copy(stringBuffPtr, entryRef->name, stringBuffSize, &len))
        {
            return LE_OVERFLOW;
        }
        bytesWritten += len;

        return bytesWritten;
    }

    // If we've reached the Root namespace, then the entry is not in the base namespace.
    if (entryRef == RootPtr)
    {
        return LE_NOT_FOUND;
    }

    // Otherwise, recursively traverse up the tree to the child of the base namespace and
    // print entry names and '/' separators as we unwind back to the current entry.
    size_t len;
    ssize_t result = resTree_GetPath(stringBuffPtr,
                                     stringBuffSize,
                                     baseNamespace,
                                     entryRef->parentPtr);

    if (result < 0) // All the le_result_t codes (other than LE_OK) are negative.
    {
        return result;
    }
    len = result;

    bytesWritten += len;
    stringBuffPtr += len;
    stringBuffSize -= len;

    if (LE_OK != le_utf8_Copy(stringBuffPtr, "/", stringBuffSize, &len))
    {
        return LE_OVERFLOW;
    }

    bytesWritten += len;
    stringBuffPtr += len;
    stringBuffSize -= len;

    if (LE_OK != le_utf8_Copy(stringBuffPtr, entryRef->name, stringBuffSize, &len))
    {
        return LE_OVERFLOW;
    }

    bytesWritten += len;

    return bytesWritten;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the first child of a given entry.
 *
 * @return Reference to the first child entry, or NULL if the entry has no children.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetFirstChild
(
    resTree_EntryRef_t entryRef
)
//--------------------------------------------------------------------------------------------------
{
    le_dls_Link_t* linkPtr = le_dls_Peek(&entryRef->childList);

    if (linkPtr != NULL)
    {
        return CONTAINER_OF(linkPtr, Entry_t, link);
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the next sibling (child of the same parent) of a given entry.
 *
 * @return Reference to the next entry in the parent's child list, or
 *         NULL if already at the last child.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetNextSibling
(
    resTree_EntryRef_t entryRef
)
//--------------------------------------------------------------------------------------------------
{
    if (entryRef->parentPtr == NULL)
    {
        // Someone called this function for the Root entry.
        return NULL;
    }

    le_dls_Link_t* linkPtr = le_dls_PeekNext(&entryRef->parentPtr->childList, &entryRef->link);

    if (linkPtr != NULL)
    {
        return CONTAINER_OF(linkPtr, Entry_t, link);
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a data sample to a resource.
 *
 * @note Takes ownership of the data sample reference.
 */
//--------------------------------------------------------------------------------------------------
void resTree_Push
(
    resTree_EntryRef_t entryRef,    ///< The entry to push to.
    io_DataType_t dataType,         ///< The data type.
    dataSample_Ref_t dataSample     ///< The data sample (timestamp + value).
)
//--------------------------------------------------------------------------------------------------
{
    switch (entryRef->type)
    {
        case ADMIN_ENTRY_TYPE_INPUT:
        case ADMIN_ENTRY_TYPE_OUTPUT:
        case ADMIN_ENTRY_TYPE_OBSERVATION:
        case ADMIN_ENTRY_TYPE_PLACEHOLDER:

            res_Push(entryRef->resourcePtr, dataType, NULL, dataSample);
            break;

        case ADMIN_ENTRY_TYPE_NAMESPACE:

            // Throw away the data sample.
            le_mem_Release(dataSample);
            break;

        case ADMIN_ENTRY_TYPE_NONE:
            LE_FATAL("Unexpected entry type.");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Add a Push Handler to an Output resource.
 *
 * @return Reference to the handler added.
 */
//--------------------------------------------------------------------------------------------------
hub_HandlerRef_t resTree_AddPushHandler
(
    resTree_EntryRef_t resRef, ///< Reference to the Output resource.
    io_DataType_t dataType,
    void* callbackPtr,
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    return res_AddPushHandler(resRef->resourcePtr,
                              dataType,
                              callbackPtr,
                              contextPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove a Push Handler from an Output resource.
 */
//--------------------------------------------------------------------------------------------------
void resTree_RemovePushHandler
(
    hub_HandlerRef_t handlerRef
)
//--------------------------------------------------------------------------------------------------
{
    res_RemovePushHandler(handlerRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the current value of a resource.
 *
 * @return Reference to the Data Sample object or NULL if the resource doesn't have a current value.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t resTree_GetCurrentValue
(
    resTree_EntryRef_t resRef
)
//--------------------------------------------------------------------------------------------------
{
    if (!resTree_IsResource(resRef))
    {
        return NULL;
    }

    return res_GetCurrentValue(resRef->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Creates a data flow route from one resource to another by setting the data source for the
 * destination resource.  If the destination resource already has a source resource, it will be
 * replaced. Does nothing if the route already exists.
 *
 * @return
 *  - LE_OK if route already existed or new route was successfully created.
 *  - LE_DUPLICATE if the addition of this route would result in a loop.
 */
//--------------------------------------------------------------------------------------------------
le_result_t resTree_SetSource
(
    resTree_EntryRef_t destEntry,
    resTree_EntryRef_t srcEntry     ///< Source entry ref, or NULL to clear the source.
)
//--------------------------------------------------------------------------------------------------
{
    LE_ASSERT(destEntry->type != ADMIN_ENTRY_TYPE_NAMESPACE);
    LE_ASSERT(destEntry->type != ADMIN_ENTRY_TYPE_NONE);

    return res_SetSource(destEntry->resourcePtr, (srcEntry != NULL ? srcEntry->resourcePtr : NULL));
}


//--------------------------------------------------------------------------------------------------
/**
 * Fetches the data flow source resource entry from which a given resource expects to receive data
 * samples.
 *
 * @return Reference to the source entry or NULL if none configured.
 */
//--------------------------------------------------------------------------------------------------
resTree_EntryRef_t resTree_GetSource
(
    resTree_EntryRef_t destEntry
)
//--------------------------------------------------------------------------------------------------
{
    if (resTree_IsResource(destEntry))
    {
        return res_GetSource(destEntry->resourcePtr);
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete an Input or Output resource.
 *
 * Converts the resource into a Placeholder if it still has configuration settings.
 */
//--------------------------------------------------------------------------------------------------
void resTree_DeleteIO
(
    resTree_EntryRef_t entryRef
)
//--------------------------------------------------------------------------------------------------
{
    res_Resource_t* ioPtr = entryRef->resourcePtr;

    if (res_HasAdminSettings(ioPtr))
    {
        // There are still administrative settings present on this resource, so replace it
        // with a Placeholder.
        res_Resource_t* placeholderPtr = res_CreatePlaceholder(entryRef);
        ReplaceResource(entryRef, placeholderPtr, ADMIN_ENTRY_TYPE_PLACEHOLDER);
    }
    else
    {
        entryRef->resourcePtr = NULL;
        entryRef->type = ADMIN_ENTRY_TYPE_NAMESPACE;
        le_mem_Release(entryRef);
        le_mem_Release(ioPtr);
    }

}


//--------------------------------------------------------------------------------------------------
/**
 * Delete an Observation.
 */
//--------------------------------------------------------------------------------------------------
void resTree_DeleteObservation
(
    resTree_EntryRef_t obsEntry
)
//--------------------------------------------------------------------------------------------------
{
    res_Resource_t* obsPtr = obsEntry->resourcePtr;
    obsEntry->resourcePtr = NULL;
    res_DeleteObservation(obsPtr);

    obsEntry->type = ADMIN_ENTRY_TYPE_NAMESPACE;

    le_mem_Release(obsEntry);
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum period between data samples accepted by a given Observation.
 *
 * This is used to throttle the rate of data passing into and through an Observation.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetMinPeriod
(
    resTree_EntryRef_t obsEntry,
    double minPeriod
)
//--------------------------------------------------------------------------------------------------
{
    res_SetMinPeriod(obsEntry->resourcePtr, minPeriod);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum period between data samples accepted by a given Observation.
 *
 * @return The value, or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
double resTree_GetMinPeriod
(
    resTree_EntryRef_t obsEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_GetMinPeriod(obsEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the highest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetHighLimit
(
    resTree_EntryRef_t obsEntry,
    double highLimit
)
//--------------------------------------------------------------------------------------------------
{
    res_SetHighLimit(obsEntry->resourcePtr, highLimit);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the highest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double resTree_GetHighLimit
(
    resTree_EntryRef_t obsEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_GetHighLimit(obsEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the lowest value in a range that will be accepted by a given Observation.
 *
 * Ignored for all non-numeric types except Boolean for which non-zero = true and zero = false.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetLowLimit
(
    resTree_EntryRef_t obsEntry,
    double lowLimit
)
//--------------------------------------------------------------------------------------------------
{
    res_SetLowLimit(obsEntry->resourcePtr, lowLimit);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the lowest value in a range that will be accepted by a given Observation.
 *
 * @return The value, or NAN (not a number) if not set.
 */
//--------------------------------------------------------------------------------------------------
double resTree_GetLowLimit
(
    resTree_EntryRef_t obsEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_GetLowLimit(obsEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the magnitude that a new value must vary from the current value to be accepted by
 * a given Observation.
 *
 * Ignored for trigger types.
 *
 * For all other types, any non-zero value means accept any change, but drop if the same as current.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetChangeBy
(
    resTree_EntryRef_t obsEntry,
    double change
)
//--------------------------------------------------------------------------------------------------
{
    res_SetChangeBy(obsEntry->resourcePtr, change);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the magnitude that a new value must vary from the current value to be accepted by
 * a given Observation.
 *
 * @return The value, or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
double resTree_GetChangeBy
(
    resTree_EntryRef_t obsEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_GetChangeBy(obsEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the maximum number of data samples to buffer in a given Observation.  Buffers are FIFO
 * circular buffers. When full, the buffer drops the oldest value to make room for a new addition.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetBufferMaxCount
(
    resTree_EntryRef_t obsEntry,
    uint32_t count
)
//--------------------------------------------------------------------------------------------------
{
    res_SetBufferMaxCount(obsEntry->resourcePtr, count);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the buffer size setting for a given Observation.
 *
 * @return The buffer size (in number of samples) or 0 if not set.
 */
//--------------------------------------------------------------------------------------------------
uint32_t resTree_GetBufferMaxCount
(
    resTree_EntryRef_t obsEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_GetBufferMaxCount(obsEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum time between backups of an Observation's buffer to non-volatile storage.
 * If the buffer's size is non-zero and the backup period is non-zero, then the buffer will be
 * backed-up to non-volatile storage when it changes, but never more often than this period setting
 * specifies.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetBufferBackupPeriod
(
    resTree_EntryRef_t obsEntry,
    uint32_t seconds
)
//--------------------------------------------------------------------------------------------------
{
    res_SetBufferBackupPeriod(obsEntry->resourcePtr, seconds);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the minimum time between backups of an Observation's buffer to non-volatile storage.
 * See admin_SetBufferBackupPeriod() for more information.
 *
 * @return The buffer backup period (in seconds) or 0 if backups are disabled or the Observation
 *         does not exist.
 */
//--------------------------------------------------------------------------------------------------
uint32_t resTree_GetBufferBackupPeriod
(
    resTree_EntryRef_t obsEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_GetBufferBackupPeriod(obsEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the default value of a resource.
 *
 * @note Default will be discarded by an Input or Output resource if the default's data type
 *       does not match the data type of the Input or Output.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetDefault
(
    resTree_EntryRef_t resEntry,
    io_DataType_t dataType,
    dataSample_Ref_t value
)
//--------------------------------------------------------------------------------------------------
{
    res_SetDefault(resEntry->resourcePtr, dataType, value);
}


//--------------------------------------------------------------------------------------------------
/**
 * Discover whether a given resource has a default value.
 *
 * @return true if there is a default value set, false if not.
 */
//--------------------------------------------------------------------------------------------------
bool resTree_HasDefault
(
    resTree_EntryRef_t resEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_HasDefault(resEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of the default value that is currently set on a given resource.
 *
 * @return The data type, or IO_DATA_TYPE_TRIGGER if not set.
 */
//--------------------------------------------------------------------------------------------------
io_DataType_t resTree_GetDefaultDataType
(
    resTree_EntryRef_t resEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_GetDefaultDataType(resEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the default value of a resource.
 *
 * @return the default value, or NULL if not set.
 */
//--------------------------------------------------------------------------------------------------
dataSample_Ref_t resTree_GetDefaultValue
(
    resTree_EntryRef_t resEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_GetDefaultValue(resEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove any default value that might be set on a given resource.
 */
//--------------------------------------------------------------------------------------------------
void resTree_RemoveDefault
(
    resTree_EntryRef_t resEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_RemoveDefault(resEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Set an override on a given resource.
 *
 * @note Override will be discarded by an Input or Output resource if the override's data type
 *       does not match the data type of the Input or Output.
 */
//--------------------------------------------------------------------------------------------------
void resTree_SetOverride
(
    resTree_EntryRef_t resEntry,
    io_DataType_t dataType,
    dataSample_Ref_t value
)
//--------------------------------------------------------------------------------------------------
{
    res_SetOverride(resEntry->resourcePtr, dataType, value);
}


//--------------------------------------------------------------------------------------------------
/**
 * Find out whether the resource currently has an override in effect.
 *
 * @return true if the resource is overridden, false otherwise.
 */
//--------------------------------------------------------------------------------------------------
bool resTree_IsOverridden
(
    resTree_EntryRef_t resEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_IsOverridden(resEntry->resourcePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove any override that might be in effect for a given resource.
 */
//--------------------------------------------------------------------------------------------------
void resTree_RemoveOverride
(
    resTree_EntryRef_t resEntry
)
//--------------------------------------------------------------------------------------------------
{
    return res_RemoveOverride(resEntry->resourcePtr);
}

