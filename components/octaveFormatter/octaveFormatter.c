//--------------------------------------------------------------------------------------------------
/**
 * Snapshot formatter producing Octave CBOR output.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------
#include "octaveFormatter.h"

#include "interfaces.h"

#include "dataHub.h"
#include "snapshot.h"
#include "cbor_utils.h"


/// Encoded bytes threshold under which formatter keeps buffering before sending data
#define STREAMING_THRESHOLD_BYTES   HUB_MAX_STRING_BYTES

/// Use query API custom flag as full tree encoding request
#define OCTAVE_FLAG_FULL_TREE QUERY_SNAPSHOT_FLAG_CUSTOM

/// Filter bitmask for live node detection.
#define LIVE_FILTERS    (SNAPSHOT_FILTER_CREATED | SNAPSHOT_FILTER_NORMAL)
/// Filter bitmask for all possible filters.
#define ALL_FILTERS     (LIVE_FILTERS | SNAPSHOT_FILTER_DELETED)

/// Internal formatter states.
typedef enum
{
    STATE_START = 0,            ///< Beginning of the document.
    STATE_SNAPSHOT_STEP,        ///< Trigger next outer state machine step.
    STATE_NODE_NAME,            ///< Output node name.
    STATE_NODE_OPEN,            ///< Output node opening and metadata.
    STATE_NODE_VALUES,          ///< Output node timestamp and format for value.
    STATE_NODE_VALUE_BODY,      ///< Output node value.
    STATE_NODE_DEFAULT,         ///< Output formatting for default value.
    STATE_NODE_DEFAULT_BODY,    ///< Output node default value.
    STATE_JSON_EX,              ///< Output formatting for JSON example (JSON nodes only).
    STATE_JSON_EX_BODY,         ///< Output node JSON example (JSON nodes only).
    STATE_MAX                   ///< One larger than highest state value.
} OctaveFormatterState_t;

/// Octave formatter state.
typedef struct OctaveFormatter
{
    snapshot_Formatter_t    base;           ///< Base type containing tree handling callbacks.
    uint8_t                 buffer[HUB_MAX_STRING_BYTES * 2];   ///< Buffer for preparing formatted
                                                                ///< output.
    size_t                  remaining;      ///< Number of bytes available for encoding in buffer.
    size_t                  encodedBytes;   ///< Number of bytes used in encoding buffer.
    size_t                  next;           ///< Offset of the next byte to send.
    size_t                  available;      ///< Number of bytes available to be sent.
    bool                    isFullDump;     ///< Is the current dataHub dump a full or a diff one
    bool                    skipNode;       ///< Does formatter need to skip content for this node?
    OctaveFormatterState_t  nextState;      ///< Next state to transition to once currently buffered
                                            ///< data is sent.
    le_fdMonitor_Ref_t      monitor;        ///< FD monitor for output stream.
} OctaveFormatter_t;

//--------------------------------------------------------------------------------------------------
/*
 * Callback for an internal formatter state machine step.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*OctaveFormatterStep_t)
(
    OctaveFormatter_t *octaveFormatter ///< Formatter instance.
);

// Forward reference.
static void Step(OctaveFormatter_t *octaveFormatter);


//--------------------------------------------------------------------------------------------------
/*
 * Get the string representation of a filter.
 *
 * @return const char * representing filter
 */
//--------------------------------------------------------------------------------------------------
static const char * FilterToString
(
    uint32_t filter ///< filter value to translate
)
{
    if (LIVE_FILTERS == (filter & ALL_FILTERS))
    {
        return "LIVE";
    }
    else if (SNAPSHOT_FILTER_CREATED == (filter & ALL_FILTERS))
    {
        return "NEW";
    }
    else if (SNAPSHOT_FILTER_NORMAL == (filter & ALL_FILTERS))
    {
        return "MODIFIED";
    }
    else if (SNAPSHOT_FILTER_DELETED == (filter & ALL_FILTERS))
    {
        return "DELETED";
    }
    else
    {
        return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
/*
 * Send some data from the buffer to the output stream.
 *
 * @return -1 on error, 0 if all data was sent, or 1 if more data remains to be sent.
 */
//--------------------------------------------------------------------------------------------------
static int SendData
(
    OctaveFormatter_t   *octaveFormatter,   ///< Formatter instance.
    int                  stream             ///< Output stream FD.
)
{
    const uint8_t *start = &octaveFormatter->buffer[octaveFormatter->next];
    ssize_t      count;

    if (octaveFormatter->available == 0)
    {
        LE_DEBUG("Nothing to send");
        return 1;
    }

    count = le_fd_Write(stream, start, octaveFormatter->available);
    if (count < 0)
    {
        if (EAGAIN == errno)
        {
            // Data not read yet by other side, keep new data in buffer and wait
            return 1;
        }
        else
        {
            // other errors are fatal
            LE_ERROR("Failed to write to stream with errno: %d", errno);
            return -1;
        }
    }
    else if (count < (ssize_t)octaveFormatter->available)
    {
        // Didn't send all of the available data.
        octaveFormatter->next += count;
        octaveFormatter->available -= count;
        LE_ASSERT(octaveFormatter->next < sizeof(octaveFormatter->buffer));
        return 1;
    }
    else
    {
        // We've sent everything in the buffer.
        octaveFormatter->next = 0;
        octaveFormatter->available = 0;
        le_fdMonitor_Disable(octaveFormatter->monitor, POLLOUT);
        return 0;
    }
}

//--------------------------------------------------------------------------------------------------
/*
 * Handle an FD or manually triggered event on the formatted output stream.
 */
//--------------------------------------------------------------------------------------------------
static void HandleEvents
(
    OctaveFormatter_t   *octaveFormatter,  ///< Formatter instance.
    int                 fd,                ///< Output stream file descriptor.
    short               events             ///< FD event bitfield.
)
{
    int status;

    LE_DEBUG("Handling events 0x%04X", events);
    if (events & POLLOUT)
    {
        // Can send more data, so do it.
        status = SendData(octaveFormatter, fd);
        if (status < 0)
        {
            LE_ERROR("Failed to send data");
            // Error sending data, so abort the snapshot.
            snapshot_End(LE_CLOSED);
            return;
        }
        else if (status == 0)
        {
            LE_DEBUG("Wrote all %zu bytes to pipe", octaveFormatter->encodedBytes);
            // Sent all the available data, take action to get more.
            octaveFormatter->remaining = sizeof(octaveFormatter->buffer);
            octaveFormatter->encodedBytes = 0;
            Step(octaveFormatter);
            return;
        }
        else
        {
            LE_DEBUG("Wrote %zu bytes to pipe, %zu remaining",
                     octaveFormatter->encodedBytes - octaveFormatter->available,
                     octaveFormatter->available);
        }

        // If we are here there is still more data to send from the buffer, so wait
        // until the next POLLOUT.
    }

    if (events & POLLHUP)
    {
        LE_ERROR("Stream closed unexpectedly");
        // Stream was closed for some reason, nothing we can do except terminate the snapshot.
        snapshot_End(LE_CLOSED);
    }
    else if (events & ~POLLOUT)
    {
        LE_ERROR("Unsupported event received");
        // Any other condition is an error, so terminate the snapshot.
        snapshot_End(LE_FAULT);
    }
}

//--------------------------------------------------------------------------------------------------
/*
 * Handle an event on the formatted output stream.
 */
//--------------------------------------------------------------------------------------------------
static void StreamHandler
(
    int     fd,     ///< Output stream file descriptor.
    short   events  ///< Event bitmask.
)
{
    OctaveFormatter_t *octaveFormatter = le_fdMonitor_GetContextPtr();

    LE_DEBUG("Stream event");
    HandleEvents(octaveFormatter, fd, events);
}

//--------------------------------------------------------------------------------------------------
/*
 * Handle an explicitly triggered event to write to the formatted output stream.
 */
//--------------------------------------------------------------------------------------------------
static void ExplicitSendHandler
(
    OctaveFormatter_t   *octaveFormatter,  ///< Formatter instance.
    void                *unused            ///< Unused.
)
{
    int fd = le_fdMonitor_GetFd(octaveFormatter->monitor);

    LE_UNUSED(unused);

    LE_DEBUG("Explicit send");
    HandleEvents(octaveFormatter, fd, POLLOUT);
}

//--------------------------------------------------------------------------------------------------
/*
 * (Re)enable events on formatted output stream.
 */
//--------------------------------------------------------------------------------------------------
static void EnableSend
(
    OctaveFormatter_t   *octaveFormatter,   ///< Formatter instance.
    size_t               available          ///< Number of available bytes to send.
)
{
    LE_ASSERT(octaveFormatter->next == 0);
    LE_DEBUG("Sending %zu bytes", available);
    octaveFormatter->available = available;
    le_fdMonitor_Enable(octaveFormatter->monitor, POLLOUT);

    // Explicitly trigger an attempt to send, since the stream might be sitting ready and therefore
    // not generate a new POLLOUT.
    le_event_QueueFunction((le_event_DeferredFunc_t) &ExplicitSendHandler, octaveFormatter, NULL);
}

//--------------------------------------------------------------------------------------------------
/*
 * Send data in encoded buffer if above a given threshold,
 * otherwise advance formatter state machine.
 * Sending data may be forced (ignoring threshold, useful when no more operation to perform).
 */
//--------------------------------------------------------------------------------------------------
static void SendOrAdvance
(
    OctaveFormatter_t   *octaveFormatter,   ///< Formatter instance.
    bool                forceSend           ///< Force send even if conditions are not met.
)
{
    if (forceSend || (octaveFormatter->encodedBytes >= STREAMING_THRESHOLD_BYTES))
    {
        EnableSend(octaveFormatter, octaveFormatter->encodedBytes);
    }
    else
    {
        Step(octaveFormatter);
    }
}

//--------------------------------------------------------------------------------------------------
/*
 * Octave creates internal nodes for observation.
 * These are:
 *  - direct children of /app/cloudInterface node
 *  - of type INPUT/OUTPUT/OBSERVATION
 * They must not be reported as part of the tree.
 *
 * @return True if node identified as internal Octave node.
 */
//--------------------------------------------------------------------------------------------------
static bool IsInternalNode(
    resTree_EntryRef_t node ///< Node to check for
)
{
    admin_EntryType_t   entryType = resTree_GetEntryType(node);

    if ((ADMIN_ENTRY_TYPE_INPUT == entryType) ||
        (ADMIN_ENTRY_TYPE_OUTPUT == entryType) ||
        (ADMIN_ENTRY_TYPE_OBSERVATION == entryType))
    {
        resTree_EntryRef_t parentNode = resTree_GetParent(node);
        if (NULL != parentNode && (0 == strcmp("cloudInterface", resTree_GetEntryName(parentNode))))
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/*
 * Begin formatting the overall resource tree.
 */
//--------------------------------------------------------------------------------------------------
static void StartTree
(
    snapshot_Formatter_t *formatter ///< Formatter instance
)
{
    OctaveFormatter_t *octaveFormatter = CONTAINER_OF(formatter, OctaveFormatter_t, base);

    le_result_t res = LE_OK;
    size_t encoded = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;

    LE_ASSERT(formatter->filter & ALL_FILTERS);

    LE_DEBUG("Encode tree for filter %s", FilterToString(formatter->filter));

    // 1st pass for full/diff tree: open array, log timestamp and open map for all/new items
    if (SNAPSHOT_FILTER_CREATED == (formatter->filter & LIVE_FILTERS) ||
        LIVE_FILTERS == (formatter->filter & LIVE_FILTERS))
    {
        if (LE_OK != (res = cbor_utils_EncodeArrayStart(octaveFormatter->buffer + encoded,
                                                        &remaining, &encoded,
                                                        octaveFormatter->isFullDump ? 2 : 4)))
        {
            goto cborerror;
        }
        /* Octave uses the time in milliseconds as an unsigned int, whereas snapshot_GetTimestamp()
        * returns whole seconds as double
        */
        struct timeval tv;
        gettimeofday(&tv, NULL);
        uint64_t ts = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;

        if (LE_OK != (res = cbor_utils_EncodePositiveInt(octaveFormatter->buffer + encoded,
                                                         &remaining, &encoded, ts)))
        {
            goto cborerror;
        }
        if (LE_OK != (res = cbor_utils_EncodeIndefMapStart(octaveFormatter->buffer + encoded,
                                                           &remaining, &encoded)))
        {
            goto cborerror;
        }
    }
    // 2nd pass for diff tree: open map for modified items
    else if (SNAPSHOT_FILTER_NORMAL == (formatter->filter & LIVE_FILTERS))
    {
        if (LE_OK != (res = cbor_utils_EncodeIndefMapStart(octaveFormatter->buffer + encoded,
                                                           &remaining, &encoded)))
        {
            goto cborerror;
        }
    }
    // 3rd pass for diff tree: open array for deleted items
    else if (SNAPSHOT_FILTER_DELETED == (formatter->filter & ALL_FILTERS))
    {
        if (LE_OK != (res = cbor_utils_EncodeIndefArrayStart(octaveFormatter->buffer + encoded,
                                                             &remaining, &encoded)))
        {
            goto cborerror;
        }
    }
    else
    {
        LE_FATAL("Unexpected filter requested");
    }

    // Now we wait for the buffer to drain and call snapshot_Step() when it is done.
    octaveFormatter->skipNode = true;
    octaveFormatter->nextState = STATE_SNAPSHOT_STEP;
    octaveFormatter->encodedBytes = encoded;
    octaveFormatter->remaining = remaining;
    SendOrAdvance(octaveFormatter, false);
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Begin formatting a resource tree node.
 */
//--------------------------------------------------------------------------------------------------
static void BeginNode
(
    snapshot_Formatter_t *formatter ///< Formatter instance.
)
{
    OctaveFormatter_t *octaveFormatter = CONTAINER_OF(formatter, OctaveFormatter_t, base);

    LE_ASSERT(formatter->filter & ALL_FILTERS);

    // evaluate if node content should be skipped (move on to child node):
    //  - root node is skipped
    //  - Octave internal node are skipped
    //  - added/modified nodes that are not input/ouput/observation are skipped
    //  - for deleted nodes tracking: only the actually deleted node is considered
    resTree_EntryRef_t  node = snapshot_GetNode();
    resTree_EntryRef_t  root = snapshot_GetRoot();
    admin_EntryType_t   entryType = resTree_GetEntryType(node);
    octaveFormatter->skipNode = ((root == node) ||
                                 IsInternalNode(node) ||
                                 (!(formatter->filter & SNAPSHOT_FILTER_DELETED) &&
                                  !((ADMIN_ENTRY_TYPE_INPUT == entryType) ||
                                    (ADMIN_ENTRY_TYPE_OUTPUT == entryType) ||
                                    (ADMIN_ENTRY_TYPE_OBSERVATION == entryType))) ||
                                 ((formatter->filter & SNAPSHOT_FILTER_DELETED) &&
                                  !resTree_IsDeleted(node)));

    if (octaveFormatter->skipNode)
    {
        // Move directly to opening node as this one's content is irrelevant
        LE_DEBUG("Skip node '%s'", resTree_GetEntryName(node));
        octaveFormatter->nextState = STATE_NODE_OPEN;
        Step(octaveFormatter);
    }
    else
    {
        // Relevant node, move on to its name
        octaveFormatter->nextState = STATE_NODE_NAME;
        Step(octaveFormatter);
    }
}

//--------------------------------------------------------------------------------------------------
/*
 * Write the node name part of the object key to the buffer.
 */
//--------------------------------------------------------------------------------------------------
static void NodeName
(
    OctaveFormatter_t *octaveFormatter  ///< Formatter instance.
)
{
    resTree_EntryRef_t node = snapshot_GetNode();
    char path[HUB_MAX_RESOURCE_PATH_BYTES] = {0};
    ssize_t pathLen = 0;

    le_result_t res = LE_OK;
    size_t encodedBytes = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;


    LE_ASSERT(octaveFormatter->base.filter & ALL_FILTERS);
    LE_ASSERT(!octaveFormatter->skipNode);

    /* path returned below does not begin with '/', whereas the backend expects
     * this to be included in the cbor message
     */
    path[0] = '/';
    /* Get node's full path from root. If this fails, abort snapshot. resTree_GetPath() returns
     * -ve legato error code on failure
     */
    if (0 > (pathLen = resTree_GetPath((char *)(path + 1), HUB_MAX_RESOURCE_PATH_BYTES - 1,
                                       snapshot_GetRoot(), node)))
    {
        LE_ERROR("Failed to retrieve node's path for node '%s'", resTree_GetEntryName(node));
        res = (le_result_t)pathLen;
        goto cborerror;
    }
    if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                &remaining, &encodedBytes, path)))
    {
        goto cborerror;
    }
    octaveFormatter->nextState = STATE_NODE_OPEN;
    octaveFormatter->encodedBytes = encodedBytes;
    octaveFormatter->remaining = remaining;
    SendOrAdvance(octaveFormatter, false);
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Write the node opening preamble and its metadata to the buffer.
 */
//--------------------------------------------------------------------------------------------------
static void NodeOpen
(
    OctaveFormatter_t *octaveFormatter  ///< Formatter instance.
)
{
    resTree_EntryRef_t  node = snapshot_GetNode();
    admin_EntryType_t   entryType = resTree_GetEntryType(node);

    le_result_t res = LE_OK;
    size_t encodedBytes = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;

    LE_ASSERT(octaveFormatter->base.filter & ALL_FILTERS);

    // for added/modified nodes: open a map to dump their content
    if (octaveFormatter->base.filter & LIVE_FILTERS && !octaveFormatter->skipNode)
    {
        io_DataType_t dataType = resTree_GetDataType(node);
        int64_t metadata = 0;
        metadata += entryType;
        metadata += 10*dataType;
        metadata += 100*resTree_IsMandatory(node);

        LE_DEBUG("Open node '%s'", resTree_GetEntryName(node));
        if (LE_OK != (res = cbor_utils_EncodeIndefMapStart(octaveFormatter->buffer + encodedBytes,
                                                           &remaining, &encodedBytes)))
        {
            goto cborerror;
        }
        if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                    &remaining, &encodedBytes, (char*)"y")))
        {
            goto cborerror;
        }
        if (LE_OK != (res = cbor_utils_EncodeInt(octaveFormatter->buffer + encodedBytes,
                                                 &remaining, &encodedBytes, metadata)))
        {
            goto cborerror;
        }
    }
    // skipped nodes need no formatting and deleted ones are dumped by name only
    else
    {
        octaveFormatter->skipNode = false;
        octaveFormatter->nextState = STATE_SNAPSHOT_STEP;
        Step(octaveFormatter);
        return;
    }

    octaveFormatter->skipNode = false;

    switch (entryType)
    {
        case ADMIN_ENTRY_TYPE_INPUT:
        case ADMIN_ENTRY_TYPE_OUTPUT:
        case ADMIN_ENTRY_TYPE_OBSERVATION:
            if (snapshot_IsTimely(node))
            {
                // Node values have been set, output them
                octaveFormatter->nextState = STATE_NODE_VALUES;
            }
            else
            {
                // Node is still empty, move on to next one
                octaveFormatter->nextState = STATE_SNAPSHOT_STEP;
            }
            break;
        case ADMIN_ENTRY_TYPE_NAMESPACE:
        case ADMIN_ENTRY_TYPE_PLACEHOLDER:
        default:
            LE_FATAL("Unexpected entry type: %d", entryType);
            break;
    }
    octaveFormatter->encodedBytes = encodedBytes;
    octaveFormatter->remaining = remaining;
    SendOrAdvance(octaveFormatter, false);
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Format node's timestamp and prepare for its value.
 */
//--------------------------------------------------------------------------------------------------
static void NodeValues
(
    OctaveFormatter_t *octaveFormatter  ///< Formatter instance.
)
{
    resTree_EntryRef_t  node = snapshot_GetNode();
    dataSample_Ref_t    sample = resTree_GetCurrentValue(node);
    io_DataType_t       dataType = resTree_GetDataType(node);

    le_result_t res = LE_OK;
    size_t encodedBytes = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;

    LE_ASSERT(octaveFormatter->base.filter & LIVE_FILTERS);

    if (NULL == sample)
    {
        // nodes with no value should not have their timestamp set
        // NodeOpen call should have jumped directly to next node
        // allow snapshot to continue as it does not creates issue but warn about it
        LE_WARN("Node '%s' has no value, should not have reached this function",
                resTree_GetEntryName(snapshot_GetNode()));
        octaveFormatter->nextState = STATE_SNAPSHOT_STEP;
        Step(octaveFormatter);
        return;
    }

    LE_DEBUG("Node timestamp for '%s'", resTree_GetEntryName(node));

    double timestampDouble = dataSample_GetTimestamp(sample);
    uint64_t timestamp = (uint64_t)timestampDouble;
    // Handle clients who use milliseconds, not seconds
    if (timestamp >= 10000000000){
        timestamp = timestamp / 1000;
    }

    if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                &remaining, &encodedBytes, (char*)"t")))
    {
        goto cborerror;
    }
    if (LE_OK != (res = cbor_utils_EncodePositiveInt(octaveFormatter->buffer + encodedBytes,
                                                     &remaining, &encodedBytes, timestamp)))
    {
        goto cborerror;
    }

    switch (dataType)
    {
        case IO_DATA_TYPE_TRIGGER:
            octaveFormatter->nextState = STATE_SNAPSHOT_STEP;
            break;

        case IO_DATA_TYPE_BOOLEAN:
        case IO_DATA_TYPE_NUMERIC:
        case IO_DATA_TYPE_STRING:
        case IO_DATA_TYPE_JSON:
            if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                        &remaining, &encodedBytes, (char*)"v")))
            {
                goto cborerror;
            }
            octaveFormatter->nextState = STATE_NODE_VALUE_BODY;
            break;
    }
    octaveFormatter->encodedBytes = encodedBytes;
    octaveFormatter->remaining = remaining;
    SendOrAdvance(octaveFormatter, false);
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Write the node value to the buffer.
 */
//--------------------------------------------------------------------------------------------------
static void NodeValueBody
(
    OctaveFormatter_t *octaveFormatter  ///< Formatter instance.
)
{
    resTree_EntryRef_t  node = snapshot_GetNode();
    dataSample_Ref_t    sample = resTree_GetCurrentValue(node);
    io_DataType_t       dataType = resTree_GetDataType(node);

    le_result_t res = LE_OK;
    size_t encodedBytes = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;

    LE_ASSERT(octaveFormatter->base.filter & LIVE_FILTERS);
    LE_ASSERT(sample != NULL);

    LE_DEBUG("Node value for '%s'", resTree_GetEntryName(node));

    // The string/JSON copied in should never be larger than sizeof(octaveFormatter->buffer), so we
    // can assert if this overflows.
    if (IO_DATA_TYPE_BOOLEAN == dataType)
    {
        if (LE_OK != (res = cbor_utils_EncodeBool(octaveFormatter->buffer + encodedBytes,
                                                  &remaining, &encodedBytes,
                                                  dataSample_GetBoolean(sample))))
        {
            goto cborerror;
        }
    }
    else if (IO_DATA_TYPE_NUMERIC == dataType)
    {
        if (LE_OK != (res = cbor_utils_EncodeDouble(octaveFormatter->buffer + encodedBytes,
                                                    &remaining, &encodedBytes,
                                                    dataSample_GetNumeric(sample))))
        {
            goto cborerror;
        }
    }
    else if (IO_DATA_TYPE_STRING == dataType)
    {
        if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                    &remaining, &encodedBytes,
                                                    dataSample_GetString(sample))))
        {
            goto cborerror;
        }
    }
    else if (IO_DATA_TYPE_JSON == dataType)
    {
        if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                    &remaining, &encodedBytes,
                                                    dataSample_GetJson(sample))))
        {
            goto cborerror;
        }
    }
    else
    {
        // no other data type should end up in this function
        LE_FATAL("Unexpected data type %d", dataType);
    }

    octaveFormatter->nextState = STATE_NODE_DEFAULT;
    octaveFormatter->encodedBytes = encodedBytes;
    octaveFormatter->remaining = remaining;
    SendOrAdvance(octaveFormatter, false);
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Checks for default value and proceed accordingly.
 */
//--------------------------------------------------------------------------------------------------
static void NodeDefaultValue
(
    OctaveFormatter_t *octaveFormatter  ///< Formatter instance.
)
{
    resTree_EntryRef_t  node = snapshot_GetNode();
    io_DataType_t       dataType = resTree_GetDataType(node);
    admin_EntryType_t   entryType = resTree_GetEntryType(node);

    le_result_t res = LE_OK;
    size_t encodedBytes = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;

    LE_ASSERT(octaveFormatter->base.filter & LIVE_FILTERS);

    if (resTree_HasDefault(node))
    {
        if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                    &remaining, &encodedBytes, (char*)"d")))
        {
            goto cborerror;
        }
        octaveFormatter->nextState = STATE_NODE_DEFAULT_BODY;
        octaveFormatter->encodedBytes = encodedBytes;
        octaveFormatter->remaining = remaining;
        SendOrAdvance(octaveFormatter, false);
    }
    else if (IO_DATA_TYPE_JSON == dataType && ADMIN_ENTRY_TYPE_INPUT == entryType &&
             resTree_IsJsonExampleChanged(node))
    {
        octaveFormatter->nextState = STATE_JSON_EX;
        Step(octaveFormatter);
    }
    else
    {
        octaveFormatter->nextState = STATE_SNAPSHOT_STEP;
        Step(octaveFormatter);
    }
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Write the node default value to the buffer.
 */
//--------------------------------------------------------------------------------------------------
static void NodeDefaultValueBody
(
    OctaveFormatter_t *octaveFormatter  ///< Formatter instance.
)
{
    resTree_EntryRef_t  node = snapshot_GetNode();
    io_DataType_t       dataType = resTree_GetDefaultDataType(node);
    dataSample_Ref_t    sample = resTree_GetDefaultValue(node);

    le_result_t res = LE_OK;
    size_t encodedBytes = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;

    LE_ASSERT(octaveFormatter->base.filter & LIVE_FILTERS);
    LE_ASSERT(resTree_HasDefault(node));

    LE_DEBUG("Node default value for '%s'", resTree_GetEntryName(node));

    if (IO_DATA_TYPE_BOOLEAN == dataType)
    {
        if (LE_OK != (res = cbor_utils_EncodeBool(octaveFormatter->buffer + encodedBytes,
                                                  &remaining, &encodedBytes,
                                                  dataSample_GetBoolean(sample))))
        {
            goto cborerror;
        }
    }
    else if (IO_DATA_TYPE_NUMERIC == dataType)
    {
        if (LE_OK != (res = cbor_utils_EncodeDouble(octaveFormatter->buffer + encodedBytes,
                                                    &remaining, &encodedBytes,
                                                    dataSample_GetNumeric(sample))))
        {
            goto cborerror;
        }
    }
    else if (IO_DATA_TYPE_STRING == dataType)
    {
        if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                    &remaining, &encodedBytes,
                                                    dataSample_GetString(sample))))
        {
            goto cborerror;
        }
    }
    else if (IO_DATA_TYPE_JSON == dataType)
    {
        if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                    &remaining, &encodedBytes,
                                                    dataSample_GetJson(sample))))
        {
            goto cborerror;
        }
    }
    else
    {
        // no other data type should end up in this function
        LE_FATAL("Unexpected data type %d", dataType);
    }

    if (IO_DATA_TYPE_JSON == resTree_GetDataType(node) &&
        ADMIN_ENTRY_TYPE_INPUT == resTree_GetEntryType(node) &&
        resTree_IsJsonExampleChanged(node))
    {
        octaveFormatter->nextState = STATE_JSON_EX;
    }
    else
    {
        octaveFormatter->nextState = STATE_SNAPSHOT_STEP;
    }
    octaveFormatter->encodedBytes = encodedBytes;
    octaveFormatter->remaining = remaining;
    SendOrAdvance(octaveFormatter, false);
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Formatting for JSON example.
 */
//--------------------------------------------------------------------------------------------------
static void NodeJsonExample
(
    OctaveFormatter_t *octaveFormatter  ///< Formatter instance.
)
{
    resTree_EntryRef_t  node = snapshot_GetNode();
    io_DataType_t       dataType = resTree_GetDataType(node);

    le_result_t res = LE_OK;
    size_t encodedBytes = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;

    LE_ASSERT(octaveFormatter->base.filter & LIVE_FILTERS);
    LE_ASSERT(IO_DATA_TYPE_JSON == dataType);
    LE_ASSERT(resTree_IsJsonExampleChanged(node));

    if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                &remaining, &encodedBytes, (char*)"s")))
    {
        goto cborerror;
    }
    octaveFormatter->nextState = STATE_JSON_EX_BODY;
    octaveFormatter->encodedBytes = encodedBytes;
    octaveFormatter->remaining = remaining;
    SendOrAdvance(octaveFormatter, false);
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Write the node JSON example.
 */
//--------------------------------------------------------------------------------------------------
static void NodeJsonExampleBody
(
    OctaveFormatter_t *octaveFormatter  ///< Formatter instance.
)
{
    resTree_EntryRef_t  node = snapshot_GetNode();
    io_DataType_t       dataType = resTree_GetDataType(node);

    le_result_t res = LE_OK;
    size_t encodedBytes = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;

    LE_ASSERT(octaveFormatter->base.filter & LIVE_FILTERS);
    LE_ASSERT(IO_DATA_TYPE_JSON == dataType);
    LE_ASSERT(resTree_IsJsonExampleChanged(node));

    LE_DEBUG("Node JSON example for '%s'", resTree_GetEntryName(node));

    // The string/JSON copied in should never be larger than sizeof(octaveFormatter->buffer), so we
    // can assert if this overflows.
    if (LE_OK != (res = cbor_utils_EncodeString(octaveFormatter->buffer + encodedBytes,
                                                &remaining, &encodedBytes,
                                                dataSample_GetJson(resTree_GetJsonExample(node)))))
    {
        goto cborerror;
    }
    resTree_ClearJsonExampleChanged(node);

    octaveFormatter->nextState = STATE_SNAPSHOT_STEP;
    octaveFormatter->encodedBytes = encodedBytes;
    octaveFormatter->remaining = remaining;
    SendOrAdvance(octaveFormatter, false);
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Finish formatting an object.
 */
//--------------------------------------------------------------------------------------------------
static void EndObject
(
    snapshot_Formatter_t *formatter ///< Formatter instance.
)
{
    OctaveFormatter_t *octaveFormatter = CONTAINER_OF(formatter, OctaveFormatter_t, base);

    le_result_t res = LE_OK;
    size_t encodedBytes = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;

    LE_ASSERT(formatter->filter & ALL_FILTERS);

    LE_DEBUG("Closing node");

    // Now we wait for the buffer to drain and call snapshot_Step() when it is done.
    octaveFormatter->nextState = STATE_SNAPSHOT_STEP;

    // evaluate if node content was skipped (overwritten when accessed its child)
    //  - root node is skipped
    //  - added/modified nodes that are not input/ouput/observation are skipped
    //  - for deleted nodes tracking: only the actually deleted node is considered
    resTree_EntryRef_t  node = snapshot_GetNode();
    resTree_EntryRef_t  root = snapshot_GetRoot();
    admin_EntryType_t   entryType = resTree_GetEntryType(node);
    octaveFormatter->skipNode = ((root == node) ||
                                 (!(formatter->filter & SNAPSHOT_FILTER_DELETED) &&
                                  !((ADMIN_ENTRY_TYPE_INPUT == entryType) ||
                                    (ADMIN_ENTRY_TYPE_OUTPUT == entryType) ||
                                    (ADMIN_ENTRY_TYPE_OBSERVATION == entryType))) ||
                                 ((formatter->filter & SNAPSHOT_FILTER_DELETED) &&
                                  !resTree_IsDeleted(node)));

    // for added/modified nodes that were not skipped: close map
    if (formatter->filter & LIVE_FILTERS && !octaveFormatter->skipNode)
    {
        if (LE_OK != (res = cbor_utils_EncodeBreak(octaveFormatter->buffer + encodedBytes,
                                                   &remaining, &encodedBytes)))
        {
            goto cborerror;
        }
        octaveFormatter->encodedBytes = encodedBytes;
        octaveFormatter->remaining = remaining;
        SendOrAdvance(octaveFormatter, false);
    }
    // for skipped/deleted nodes, no formatting needed
    else
    {
        Step(octaveFormatter);
    }
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Finish formatting a tree.
 */
//--------------------------------------------------------------------------------------------------
static void EndTree
(
    snapshot_Formatter_t *formatter ///< Formatter instance.
)
{
    OctaveFormatter_t *octaveFormatter = CONTAINER_OF(formatter, OctaveFormatter_t, base);

    le_result_t res = LE_OK;
    size_t encodedBytes = octaveFormatter->encodedBytes;
    size_t remaining = octaveFormatter->remaining;

    LE_ASSERT(formatter->filter & ALL_FILTERS);

    LE_DEBUG("Finished encoding tree for filter %s", FilterToString(formatter->filter));
    octaveFormatter->nextState = STATE_SNAPSHOT_STEP;

    // full tree dump ends after 1 pass: close nodes map
    if (octaveFormatter->isFullDump)
    {
        if (LE_OK != (res = cbor_utils_EncodeBreak(octaveFormatter->buffer + encodedBytes,
                                                   &remaining, &encodedBytes)))
        {
            goto cborerror;
        }
        formatter->scan = false;
    }
    // diff tree 1st pass end (added nodes): close nodes map and move on to modified nodes
    else if (SNAPSHOT_FILTER_CREATED == (formatter->filter & LIVE_FILTERS))
    {
        if (LE_OK != (res = cbor_utils_EncodeBreak(octaveFormatter->buffer + encodedBytes,
                                                   &remaining, &encodedBytes)))
        {
            goto cborerror;
        }
        formatter->scan = true;
        formatter->filter = SNAPSHOT_FILTER_NORMAL;
    }
    // diff tree 2nd pass end (modified nodes): close nodes map and move on to deleted nodes
    else if (SNAPSHOT_FILTER_NORMAL == (formatter->filter & LIVE_FILTERS))
    {
        if (LE_OK != (res = cbor_utils_EncodeBreak(octaveFormatter->buffer + encodedBytes,
                                                   &remaining, &encodedBytes)))
        {
            goto cborerror;
        }
        formatter->scan = true;
        formatter->filter = SNAPSHOT_FILTER_DELETED;
    }
    // diff tree 3rd pass end (deleted nodes): close nodes array
    else if (SNAPSHOT_FILTER_DELETED == (formatter->filter & ALL_FILTERS))
    {
        formatter->scan = false;
        if (LE_OK != (res = cbor_utils_EncodeBreak(octaveFormatter->buffer + encodedBytes,
                                                   &remaining, &encodedBytes)))
        {
            goto cborerror;
        }
    }
    else
    {
        LE_FATAL("Unexpected filter requested");
    }
    octaveFormatter->encodedBytes = encodedBytes;
    octaveFormatter->remaining = remaining;
    SendOrAdvance(octaveFormatter, !formatter->scan); // force sending if no more pass to perform
    return;

cborerror:
    LE_ERROR("Failed to encode data with error %s", LE_RESULT_TXT(res));
    snapshot_End(res);
}

//--------------------------------------------------------------------------------------------------
/*
 * Close and clean up the formatter instance.
 */
//--------------------------------------------------------------------------------------------------
static void Close
(
    snapshot_Formatter_t *formatter ///< Formatter instance.
)
{
    OctaveFormatter_t *octaveFormatter = CONTAINER_OF(formatter, OctaveFormatter_t, base);

    LE_DEBUG("Closing formatter");
    le_fdMonitor_Delete(octaveFormatter->monitor);
}

//--------------------------------------------------------------------------------------------------
/*
 * Simple wrapper to step the greater state machine.
 */
//--------------------------------------------------------------------------------------------------
static void SnapshotStep
(
    OctaveFormatter_t *octaveFormatter  ///< Formatter instance.
)
{
    LE_UNUSED(octaveFormatter);

    LE_DEBUG("Stepping snapshot state machine");
    snapshot_Step();
}

//--------------------------------------------------------------------------------------------------
/*
 * Transition the formatter state machine to the next state.
 */
//--------------------------------------------------------------------------------------------------
static void Step
(
    OctaveFormatter_t *octaveFormatter  ///< Formatter instance.
)
{
    static const OctaveFormatterStep_t steps[STATE_MAX - 1] =
    {
        &SnapshotStep,          // STATE_SNAPSHOT_STEP
        &NodeName,              // STATE_NODE_NAME
        &NodeOpen,              // STATE_NODE_OPEN
        &NodeValues,            // STATE_NODE_VALUES
        &NodeValueBody,         // STATE_NODE_VALUE_BODY
        &NodeDefaultValue,      // STATE_NODE_DEFAULT
        &NodeDefaultValueBody,  // STATE_NODE_DEFAULT_BODY
        &NodeJsonExample,       // STATE_JSON_EX
        &NodeJsonExampleBody    // STATE_JSON_EX_BODY
    };
#if LE_DEBUG_ENABLED
    const char *stepNames[STATE_MAX - 1] =
    {
        "STATE_SNAPSHOT_STEP",
        "STATE_NODE_NAME",
        "STATE_NODE_OPEN",
        "STATE_NODE_VALUES",
        "STATE_NODE_VALUE_BODY",
        "STATE_NODE_DEFAULT",
        "STATE_NODE_DEFAULT_BODY",
        "STATE_NODE_JSON_EX",
        "STATE_NODE_JSON_EX_BODY"
    };
#endif /* end LE_DEBUG_ENABLED */

    if (octaveFormatter->nextState == STATE_START)
    {
        // If things haven't started yet, just wait until they do.
        return;
    }

    LE_ASSERT(octaveFormatter->nextState > STATE_START && octaveFormatter->nextState < STATE_MAX);
#if LE_DEBUG_ENABLED
    LE_DEBUG("Octave formatter transition: -> %s", stepNames[octaveFormatter->nextState - 1]);
#endif /* end LE_DEBUG_ENABLED */
    steps[octaveFormatter->nextState - 1](octaveFormatter);
}

//--------------------------------------------------------------------------------------------------
/*
 * Initialise and return the Octave CBOR snapshot formatter instance.
 *
 * @return LE_OK on success, otherwise an appropriate error code.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t GetOctaveSnapshotFormatter
(
    uint32_t                      flags,    ///< [IN]  Flags that were passed to the snapshot
                                            ///<       request.
    int                           stream,   ///< [IN]  File descriptor to write formatted output to.
    struct snapshot_Formatter   **formatter ///< [OUT] Returned formatter instance.
)
{
    static OctaveFormatter_t octaveFormatter =
    {
        {
            .startTree  = &StartTree,
            .beginNode  = &BeginNode,
            .endNode    = &EndObject,
            .endTree    = &EndTree,
            .close      = &Close
        }
    };

    LE_ASSERT(formatter != NULL);
    *formatter = &octaveFormatter.base;

    memset(octaveFormatter.buffer, 0, sizeof(octaveFormatter.buffer));
    octaveFormatter.remaining     = sizeof(octaveFormatter.buffer);
    octaveFormatter.encodedBytes  = 0;
    octaveFormatter.next          = 0;
    octaveFormatter.available     = 0;
    octaveFormatter.isFullDump    = (flags & OCTAVE_FLAG_FULL_TREE);
    octaveFormatter.skipNode      = true;
    octaveFormatter.nextState     = STATE_START;

    if (octaveFormatter.isFullDump)
    {
        octaveFormatter.base.filter   = LIVE_FILTERS;
        LE_DEBUG("Octave formatter: full tree. Transition to STATE_START");
    }
    else
    {
        octaveFormatter.base.filter   = SNAPSHOT_FILTER_CREATED;
        LE_DEBUG("Octave formatter: diff tree. Transition to STATE_START");
    }
    octaveFormatter.base.scan     = true;

    // Configure event handler for outputting formatted data.
    octaveFormatter.monitor = le_fdMonitor_Create(
                                "OctaveSnapshotStream",
                                stream,
                                &StreamHandler,
                                POLLOUT
                            );
    le_fdMonitor_SetContextPtr(octaveFormatter.monitor, &octaveFormatter);
    le_fdMonitor_Disable(octaveFormatter.monitor, POLLOUT);

    return LE_OK;
}

/// Component initialisation.
COMPONENT_INIT
{
    // Do nothing.
}
