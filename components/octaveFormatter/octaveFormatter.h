
//--------------------------------------------------------------------------------------------------
/**
 * @file octaveFormatter.h
 *
 * Plugin interface for Octave CBOR snapshot formatter.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef OCTAVEFORMATTER_H_INCLUDE_GUARD
#define OCTAVEFORMATTER_H_INCLUDE_GUARD

#include "legato.h"

// Forward reference.
struct snapshot_Formatter;

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
);

#endif /* end OCTAVEFORMATTER_H_INCLUDE_GUARD */
