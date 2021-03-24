//--------------------------------------------------------------------------------------------------
/**
 * @file config_load.c
 *
 * Testing Load API of the config.api
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

#include "config_test.h"

#define TEST_CALLBACK_TIMEOUT 5000

/*
 * Timer to trigger timeout if expected event is not received.
 */
static le_timer_Ref_t TestTimeoutTimerRef;


//--------------------------------------------------------------------------------------------------
/**
 *  A result callback that is used when we expect LE_OK
 */
//--------------------------------------------------------------------------------------------------
static void ResCallback
(
    le_result_t res,
    const char* errorMsg,
    uint32_t fileLoc,
    void* context
)
{
    le_result_t expectedFinalRes = (int)(intptr_t) context;
    le_timer_Stop(TestTimeoutTimerRef);
    LE_UNUSED(context);
    LE_UNUSED(errorMsg);
    LE_UNUSED(fileLoc);
    LE_TEST_OK(res == expectedFinalRes, "load final result code, got %s, expected %s",
        LE_RESULT_TXT(res), LE_RESULT_TXT(expectedFinalRes));
    LE_TEST_INFO("======== END Parser TEST ========");
    LE_TEST_EXIT;
}


//--------------------------------------------------------------------------------------------------
/**
 *  A structure that holds config test case information.
 */
//--------------------------------------------------------------------------------------------------
typedef struct Config_testcase
{
    char filepath[IO_MAX_RESOURCE_PATH_LEN+1];          ///< config file path.
    char configFormat[CONFIG_MAX_ENCODED_TYPE_LEN+1];   ///< config file's encoding format.
    le_result_t expectedLoadRes;                        ///< expected result for the Load function.
    le_result_t expectedFinalRes;                               ///< result from callback.
} config_testcase_t;


//--------------------------------------------------------------------------------------------------
/**
 *  List of config test cases. Each test case is a list of config files that will be applied.
 */
//--------------------------------------------------------------------------------------------------
static config_testcase_t config_testcases[][8] =
{
    {
        {"nonExistingConfig.json", "json", LE_NOT_FOUND, LE_FAULT}, {{0}}
    },
    {
        {"validConfig1.cbor", "cbor", LE_UNSUPPORTED, LE_FAULT} ,{{0}}
    },
    {
        {"validConfig1.json", "json", LE_OK, LE_OK}, {{0}}
    },
    {
        {"wrongFromatConfig1.json", "json", LE_OK, LE_FORMAT_ERROR}, {{0}}
    },
    {
        {"wrongFromatConfig2.json", "json", LE_OK, LE_FORMAT_ERROR}, {{0}}
    },
    {
        {"wrongFromatConfig3.json", "json", LE_OK, LE_FORMAT_ERROR}, {{0}}
    },
    {
        {"wrongFromatConfig4.json", "json", LE_OK, LE_FORMAT_ERROR}, {{0}}
    },
    {
        {"wrongFromatConfig5.json", "json", LE_OK, LE_FORMAT_ERROR}, {{0}}
    },
    {
        {"wrongParameterConfig1.json", "json", LE_OK, LE_BAD_PARAMETER}, {{0}}
    },
    {
        {"wrongParameterConfig2.json", "json", LE_OK, LE_BAD_PARAMETER}, {{0}}
    },
    {
        {"wrongParameterConfig3.json", "json", LE_OK, LE_BAD_PARAMETER}, {{0}}
    },
#if LE_CONFIG_RTOS
    {
        {"tooLargeConfig1.json", "json", LE_OK, LE_FAULT}, {{0}} // will only pass on RTOS
    },
    {
        {"tooLargeConfig2.json", "json", LE_OK, LE_FAULT}, {{0}} // will only pass on RTOS
    },
#endif
    {
        {"validConfig1.json" , "json", LE_OK, LE_OK},
        {"wrongFromatConfig1.json", "json", LE_OK, LE_FORMAT_ERROR}, {{0}}
    },
    {
        {"validConfig1.json" , "json", LE_OK, LE_OK},
        {"wrongParameterConfig2.json", "json", LE_OK, LE_BAD_PARAMETER}, {{0}}
    },
    {
        {"validConfig1.json" , "json", LE_OK, LE_OK},
        {"tooLargeConfig1.json", "json", LE_OK, LE_FAULT}, {{0}} // will only pass on RTOS
    }
};


static const bool datahub_empty[] = {true, false};


static const char NumPath[] = "myresources/nums/this";
static const char JsonPath[] = "myresources/json/this";
static const char StrPath[] = "myresources/strs/this";

//--------------------------------------------------------------------------------------------------
/**
 *  Fill datahub with some resource.
 */
//--------------------------------------------------------------------------------------------------
static void FillDatahub
(
)
{
    // io resources:
    io_CreateInput(NumPath, IO_DATA_TYPE_NUMERIC, "k");
    io_CreateInput(JsonPath, IO_DATA_TYPE_JSON, "");
    io_CreateInput(StrPath, IO_DATA_TYPE_STRING, "s");

    io_PushNumeric(NumPath, 0, 1.5);
    io_PushJson(JsonPath, 0, "{\"key\": 2}");
    io_PushString(JsonPath, 0, "a string");


    // observations:
    admin_CreateObs("prebuiltobs");
    admin_SetSource("/apps/config_test/myresources/nums/this", "/obs/prebuiltobs");
}

//--------------------------------------------------------------------------------------------------
/**
 *  Loads a particular config file.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t LoadThisConfig
(
    const char* filepath,
    const char* format,
    le_result_t expectedFinalRes
)
{
    char fileurl[IO_MAX_RESOURCE_PATH_LEN+1] = "test/configTest/configFiles/";
    strcat(fileurl, filepath);
    LE_INFO("Loading config file: %s", filepath);

    return config_Load(fileurl, format, ResCallback, (void*)(intptr_t)expectedFinalRes);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Time out callback.
 */
//--------------------------------------------------------------------------------------------------
static void CallbackTimeout
(
    le_timer_Ref_t timerRef                   ///< [IN] Timer pointer
)
{
    LE_UNUSED(timerRef);
    LE_TEST_OK(false, "Did not get result callback in time.");
    LE_TEST_INFO("======== END Parser TEST ========");
    LE_TEST_EXIT;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Run a particular config test case.
 */
//--------------------------------------------------------------------------------------------------
static bool ConfigParserRunTest
(
    int dhub_state,
    int config_arr_idx
)
{
    if (((unsigned int) dhub_state >= NUM_ARRAY_MEMBERS(datahub_empty)) ||
        ((unsigned int) config_arr_idx >= NUM_ARRAY_MEMBERS(config_testcases)))
        return false;
    if (!datahub_empty[dhub_state])
    {
        FillDatahub();
    }

    int i = 0;

    while (strcmp(config_testcases[config_arr_idx][i].filepath, "") != 0)
    {

        if (config_testcases[config_arr_idx][i].expectedLoadRes == LE_OK)
        {

            // start the timer
            TestTimeoutTimerRef = le_timer_Create("TestTimeout");

            le_timer_SetHandler(TestTimeoutTimerRef, CallbackTimeout);
            le_timer_SetMsInterval(TestTimeoutTimerRef, TEST_CALLBACK_TIMEOUT);
            le_timer_Start(TestTimeoutTimerRef);
        }

        // load this config:
        le_result_t loadRes = LoadThisConfig(config_testcases[config_arr_idx][i].filepath,
                            config_testcases[config_arr_idx][i].configFormat,
                            config_testcases[config_arr_idx][i].expectedFinalRes);

        LE_TEST_OK(config_testcases[config_arr_idx][i].expectedLoadRes == loadRes,
            "initial load function result: %s, expected %s", LE_RESULT_TXT(loadRes),
            LE_RESULT_TXT(config_testcases[config_arr_idx][i].expectedLoadRes));
        i++;
    }
    return true;
}



//--------------------------------------------------------------------------------------------------
/**
 *  Run a config test
 */
//--------------------------------------------------------------------------------------------------
void config_parser_test
(
)
{
    int dhStatusIdx = 0;
    int configIdx = 0;

    LE_ASSERT(le_arg_GetIntOption(&dhStatusIdx, "d", "datahub") == LE_OK);
    LE_ASSERT(le_arg_GetIntOption(&configIdx, "c", "config") == LE_OK);

    LE_TEST_INFO("======== BEGIN Parser TEST [%d][%d]========", dhStatusIdx, configIdx);
    LE_TEST_PLAN(2);

    LE_ASSERT(ConfigParserRunTest(dhStatusIdx, configIdx));
}
