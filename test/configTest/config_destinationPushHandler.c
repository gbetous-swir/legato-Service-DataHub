//--------------------------------------------------------------------------------------------------
/**
 * @file config_destinationPushHandler.c
 *
 * Testing destination push handler for the config.api
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
 * Test Resource and Observation Names and Paths
 */
//--------------------------------------------------------------------------------------------------
#define CONFIG_TEST_DESTINATION_MAX  4

#define RESOURCE_NAME_1 "resource1/value"
#define RESOURCE_NAME_2 "resource2/value"
#define RESOURCE_NAME_3 "resource3/value"
#define RESOURCE_NAME_4 "resource4/value"

#define ADMIN_RESOURCE_NAME_1 "/app/configTest/" RESOURCE_NAME_1
#define ADMIN_RESOURCE_NAME_2 "/app/configTest/" RESOURCE_NAME_2
#define ADMIN_RESOURCE_NAME_3 "/app/configTest/" RESOURCE_NAME_3
#define ADMIN_RESOURCE_NAME_4 "/app/configTest/" RESOURCE_NAME_4


#define OBS_NAME_1      "obs1"
#define OBS_NAME_2      "obs2"
#define OBS_NAME_3      "obs3"
#define OBS_NAME_4      "obs4"

#define ADMIN_OBS_NAME_1      "/obs/" OBS_NAME_1
#define ADMIN_OBS_NAME_2      "/obs/" OBS_NAME_2
#define ADMIN_OBS_NAME_3      "/obs/" OBS_NAME_3
#define ADMIN_OBS_NAME_4      "/obs/" OBS_NAME_4


//--------------------------------------------------------------------------------------------------
/**
 * Test Logic and Control Variables
 */
//--------------------------------------------------------------------------------------------------
static size_t Count = 0;
static size_t PushCount = 0;
static size_t TestIndex = 0;
static const char* TestDestination[] =
{
    "destination1",
    "destination2",
    "destination3",
    "destination4",
};


//--------------------------------------------------------------------------------------------------
/**
 * Pre-Defined Test Data Variables
 */
//--------------------------------------------------------------------------------------------------
static bool TestValueBoolean = true;
static double TestValueNumeric = 12345.6789;
static double TestValueNumeric2 = 9876.54321;
static const char* TestValueString = "Destination Test String";
static const char* TestValueString2 = "Test String # 2";

static const char* TestValueJsonString = "{\"devs\":[{"
            "\"dev\":\"UART1\","
            "\"timeout\":0.5,"
            "\"framing\":{"
                "\"lenfield\":{"
                    "\"start\":2,"
                    "\"size\":4,"
                    "\"bigendian\":false,"
                    "\"offset\":1"
                "}"
            "}"
        "}],"
        "\"baud\":\"19200\","
        "\"databits\":\"8\","
        "\"parity\":\"even\","
        "\"stopbit\":\"1\""
        "}";

static const char* TestValueJsonExtractionString = "devs";
static const char* TestValueJsonStringExpected = "[{"
            "\"dev\":\"UART1\","
            "\"timeout\":0.5,"
            "\"framing\":{"
                "\"lenfield\":{"
                    "\"start\":2,"
                    "\"size\":4,"
                    "\"bigendian\":false,"
                    "\"offset\":1"
                "}"
            "}"
        "}]";

static const char* TestValueJsonExtractionString2 = "baud";
static const char* TestValueJsonStringExpected2 = "19200";


//--------------------------------------------------------------------------------------------------
/**
 * Private function prototypes:
 */
//--------------------------------------------------------------------------------------------------
static void PushValues(void);

//--------------------------------------------------------------------------------------------------
/**
 *  timeout handler
 */
//--------------------------------------------------------------------------------------------------
static void CallbackTimeout
(
    le_timer_Ref_t timerRef                   ///< [IN] Timer pointer
)
{
    LE_UNUSED(timerRef);
    LE_INFO("WHAT?");
    //LE_FATAL("Did not get result callback");
}


//--------------------------------------------------------------------------------------------------
/**
 * Destination Push Handler callback
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void DestinationPushHandler
(
    double timestamp,
    ///< Seconds since the Epoch 1970-01-01 00:00:00 +0000 (UTC)
    const char* LE_NONNULL obsName,
    ///< Name of observation from configuration
    const char* LE_NONNULL srcPath,
    ///< Source path + JSON extraction, if applicable
    io_DataType_t dataType,
    ///< Type of data being returned
    bool boolValue,
    ///< Boolean value
    double numericValue,
    ///< Numeric value
    const char* LE_NONNULL stringValue,
    ///< String or JSON string value
    void* contextPtr
    ///<
)
{
    LE_UNUSED(timestamp);
    le_timer_Stop(TestTimeoutTimerRef);

    LE_DEBUG("[%s] - obsName [%s], srcPath [%s]",
             __FUNCTION__,
             obsName,
             srcPath);

    switch (dataType)
    {
        case IO_DATA_TYPE_BOOLEAN:
            LE_TEST_OK(((strcmp(obsName, OBS_NAME_1) == 0) &&
                       (strcmp(srcPath, ADMIN_RESOURCE_NAME_1) == 0) &&
                       (strcmp(contextPtr, TestDestination[0]) == 0) &&
                       (boolValue == TestValueBoolean)),
                       "[Destination Name: %s] Obs Name: %s, Boolean result: %s",
                       (const char*) contextPtr,
                       obsName,
                       (boolValue) ? "true" : "false");
            break;

        case IO_DATA_TYPE_NUMERIC:
            LE_TEST_OK(((strcmp(obsName, OBS_NAME_2) == 0) &&
                       (strcmp(srcPath, ADMIN_RESOURCE_NAME_2) == 0) &&
                       (strcmp(contextPtr, TestDestination[1]) == 0) &&
                       ((numericValue == TestValueNumeric) ||
                        (numericValue == TestValueNumeric2))),
                       "[Destination Name: %s] Obs Name: %s, Numeric result: %f",
                       (const char*) contextPtr,
                       obsName,
                       numericValue);
            break;

        case IO_DATA_TYPE_STRING:
            if (strcmp(obsName, OBS_NAME_3) == 0)
            {
                LE_TEST_OK(((strcmp(srcPath, ADMIN_RESOURCE_NAME_3) == 0) &&
                            (strcmp(contextPtr, TestDestination[2]) == 0) &&
                            ((strcmp(stringValue, TestValueString) == 0) |
                            (strcmp(stringValue, TestValueString2) == 0) |
                            (strcmp(stringValue, TestValueJsonStringExpected2) == 0))),
                            "[Destination Name: %s] Obs Name: %s, String result: %s",
                            (const char*) contextPtr,
                            obsName,
                            stringValue);
            }
            else
            {
                char sourcePathStr[IO_MAX_RESOURCE_PATH_LEN + 1];
                sprintf(sourcePathStr, "%s/%s", ADMIN_RESOURCE_NAME_4, TestValueJsonExtractionString2);

                LE_TEST_OK(((strcmp(obsName, OBS_NAME_4) == 0) &&
                            (strcmp(srcPath, sourcePathStr) == 0) &&
                            (strcmp(contextPtr, TestDestination[3]) == 0) &&
                            (strcmp(stringValue, TestValueJsonStringExpected2) == 0)),
                            "[Destination Name: %s] Obs Name: %s, String result: %s",
                            (const char*) contextPtr,
                            obsName,
                            stringValue);
            }
            break;

        case IO_DATA_TYPE_JSON:
        {
            char sourcePathStr[IO_MAX_RESOURCE_PATH_LEN + 1];
            sprintf(sourcePathStr, "%s/%s", ADMIN_RESOURCE_NAME_4, TestValueJsonExtractionString);

            LE_TEST_OK(((strcmp(obsName, OBS_NAME_4) == 0) &&
                       (strcmp(srcPath, sourcePathStr) == 0) &&
                       (strcmp(contextPtr, TestDestination[3]) == 0) &&
                       (strcmp(stringValue, TestValueJsonStringExpected) == 0)),
                       "[Destination Name: %s] Obs Name: %s, JSON String result: %s",
                       (const char*) contextPtr,
                       obsName,
                       stringValue);
            break;
        }

        default:
            LE_TEST_OK(false,
                       "[%s] - Unsupported dataType [%d]",
                       __FUNCTION__,
                       dataType);
            break;
    }

    Count++;

    if (Count == PushCount)
    {
        LE_TEST_INFO("======== END DestinationPushHandler TEST ========");
        LE_TEST_EXIT;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Test functon to create dataHub Resources
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void CreateIOResources
(
    void
)
{
    le_result_t result;

    // Create a Boolean Resource type
    result = io_CreateOutput(RESOURCE_NAME_1, IO_DATA_TYPE_BOOLEAN, "");
    LE_TEST_OK((result == LE_OK), "Created Boolean Resource 1: %s", LE_RESULT_TXT(result));

    // Create a Numeric Resource type
    result = io_CreateOutput(RESOURCE_NAME_2, IO_DATA_TYPE_NUMERIC, "");
    LE_TEST_OK((result == LE_OK), "Created Numeric Resource 2: %s", LE_RESULT_TXT(result));

    // Create a String Resource type
    result = io_CreateOutput(RESOURCE_NAME_3, IO_DATA_TYPE_STRING, "");
    LE_TEST_OK((result == LE_OK), "Created String Resource 3: %s", LE_RESULT_TXT(result));

    // Create a Json Resource type
    result = io_CreateOutput(RESOURCE_NAME_4, IO_DATA_TYPE_JSON, "");
    LE_TEST_OK((result == LE_OK), "Created JSON String Resource 4: %s", LE_RESULT_TXT(result));
}


//--------------------------------------------------------------------------------------------------
/**
 *  config load result callback.
 */
//--------------------------------------------------------------------------------------------------
static void ConfigLoadResCallback
(
    le_result_t res,
    const char* errorMsg,
    uint32_t fileLoc,
    void* context
)
{
    LE_UNUSED(context);
    LE_UNUSED(errorMsg);
    LE_UNUSED(fileLoc);
    le_timer_Stop(TestTimeoutTimerRef);
    LE_TEST_OK(res == LE_OK, "Config file final load result: %d", res);

    if (res == LE_OK)
    {
        PushValues();
    }
}


#if !LE_CONFIG_LINUX
// DestinationPushHandler JSON Configuration
static const char* destinationPushHandlerJson = "{"
    "\"t\":0,"
    "\"v\":\"1.0.0\","
    "\"ts\":1614208658764,"
    "\"s\":{"
    "},"
    "\"o\":{"
        "\"obs1\": {"
            "\"r\":\"/app/configTest/resource1/value\","
            "\"d\":\"destination1\""
        "},"
        "\"obs2\": {"
            "\"r\":\"/app/configTest/resource2/value\","
            "\"d\":\"destination2\""
        "},"
        "\"obs3\": {"
            "\"r\":\"/app/configTest/resource3/value\","
            "\"d\":\"destination3\""
        "},"
        "\"obs4\": {"
            "\"r\":\"/app/configTest/resource4/value\","
            "\"d\":\"destination4\","
            "\"s\":\"devs\""
        "}"
    "},"
    "\"a\":{"
    "}"
"}";

#define CONFIG_TEST_APP_PATH            "/app"
#define CONFIG_TEST_DATAHUB_PATH        CONFIG_TEST_APP_PATH "/dataHub"
#define CONFIG_TEST_DATAHUB_TEST_PATH   CONFIG_TEST_DATAHUB_PATH "/test"
#define CONFIG_TEST_DEST_PUSH_HANDLER_FILE_NAME \
    CONFIG_TEST_DATAHUB_TEST_PATH "/config_destinationPushHandler.json"


//--------------------------------------------------------------------------------------------------
/**
 * Test functon to generate test configuratoin file(s)
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void GenerateConfig
(
    void
)
{
    mkdir(CONFIG_TEST_APP_PATH, S_IRWXU | S_IRWXG | S_IRWXO);
    mkdir(CONFIG_TEST_DATAHUB_PATH, S_IRWXU | S_IRWXG | S_IRWXO);
    mkdir(CONFIG_TEST_DATAHUB_TEST_PATH, S_IRWXU | S_IRWXG | S_IRWXO);

    // Open the Destination Push Handler JSON configuration file
    FILE* fd = fopen(CONFIG_TEST_DEST_PUSH_HANDLER_FILE_NAME, "w");

    LE_INFO("[%s] Creating %s",
            __FUNCTION__,
            CONFIG_TEST_DEST_PUSH_HANDLER_FILE_NAME);

    // Write the Destination Push Handler to flash
    fwrite(&destinationPushHandlerJson[0], strlen(destinationPushHandlerJson), 1, fd);

    // Close the file
    fclose(fd);
}
#endif


//--------------------------------------------------------------------------------------------------
/**
 * Test functon to create and configure dataHub Observations
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void LoadConfig
(
    void
)
{
    config_DestinationPushHandlerRef_t refPtr;

#if !LE_CONFIG_LINUX
    le_result_t res =
        config_Load(
            CONFIG_TEST_DEST_PUSH_HANDLER_FILE_NAME,
            "json",
            ConfigLoadResCallback,
            NULL);
#else
    le_result_t res =
        config_Load(
            "test/configTest/configFiles/config_destinationPushHandler.json",
            "json",
            ConfigLoadResCallback,
            NULL);
#endif

    LE_TEST_OK(res == LE_OK, "config_Load return value is %d", res);
    for (int i = 0; i < CONFIG_TEST_DESTINATION_MAX; i++)
    {
        // Register Destination Push Handler for Observation 1
        refPtr = config_AddDestinationPushHandler(
                    TestDestination[i],
                    DestinationPushHandler,
                    (void*) TestDestination[i]);

        LE_TEST_OK((refPtr != NULL),
                   "Registering Destination Push Handler for Obs %d",
                   i + 1);
        TestIndex++;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Test functon to push pre-defined test values to each of the configured resources
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void PushValues
(
    void
)
{
    le_result_t result;

    // Test Case # 1 - Push Boolean value to Resource 1
    result = io_PushBoolean(RESOURCE_NAME_1, IO_NOW, TestValueBoolean);
    LE_TEST_OK((result == LE_OK),
               "Pushed update (%s) to Boolean Resource 1: %s",
               (TestValueBoolean) ? "true" : "false",
               LE_RESULT_TXT(result));
    PushCount++;

    // Test Case # 2 - Push Numeric value to Resource 2
    result = io_PushNumeric(RESOURCE_NAME_2, IO_NOW, TestValueNumeric);
    LE_TEST_OK((result == LE_OK),
               "Pushed update (%f) to Numeric Resource 2: %s",
               TestValueNumeric,
               LE_RESULT_TXT(result));
    PushCount++;

    // Test Case # 3 - Push String value to Resource 3
    result = io_PushString(RESOURCE_NAME_3, IO_NOW, TestValueString);
    LE_TEST_OK((result == LE_OK),
               "Pushed update (%s) to String Resource 3: %s",
               TestValueString,
               LE_RESULT_TXT(result));
    PushCount++;

    // Test Case # 4 - Push Numeric value to Resource 2
    result = io_PushNumeric(RESOURCE_NAME_2, IO_NOW, TestValueNumeric2);
    LE_TEST_OK((result == LE_OK),
               "Pushed update (%f) to Numeric Resource 2: %s",
               TestValueNumeric2,
               LE_RESULT_TXT(result));
    PushCount++;

    // Test Case # 5 - Push JSON String value to Resource 4
    result = io_PushJson(RESOURCE_NAME_4, IO_NOW, TestValueJsonString);
    LE_TEST_OK((result == LE_OK),
               "Pushed update (%s) to JSON String Resource 4: %s",
               TestValueJsonString,
               LE_RESULT_TXT(result));
    PushCount++;

    // Test Case # 6 - Push String value to Resource 3
    result = io_PushString(RESOURCE_NAME_3, IO_NOW, TestValueString2);
    LE_TEST_OK((result == LE_OK),
               "Pushed update (%s) to String Resource 3: %s",
               TestValueString2,
               LE_RESULT_TXT(result));
    PushCount++;

    // Test Case # 7 - Push JSON String value to Resource 4
    // Set the JSON Extraction string
    result = admin_SetJsonExtraction(OBS_NAME_4, TestValueJsonExtractionString2);
    LE_TEST_OK((result == LE_OK),
               "Set Json Extraction for Observation 4: %s",
               LE_RESULT_TXT(result));

    // Push the JSON Test String
    result = io_PushJson(RESOURCE_NAME_4, IO_NOW, TestValueJsonString);
    LE_TEST_OK((result == LE_OK),
               "Pushed update (%s) to JSON String Resource 4: %s",
               TestValueJsonString,
               LE_RESULT_TXT(result));
    PushCount++;
}


//--------------------------------------------------------------------------------------------------
/**
 * Main Test function to create, configure, and push test data values
 * to different dataHub resources
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
void config_destinationPush_test
(
    void
)
{
    LE_TEST_INFO("======== BEGIN DestinationPushHandler TEST ========");
    LE_TEST_PLAN(25);

    CreateIOResources();
#if !LE_CONFIG_LINUX
    GenerateConfig();
#endif
    LoadConfig();


    TestTimeoutTimerRef = le_timer_Create("TestTimeout");

    le_timer_SetHandler(TestTimeoutTimerRef, CallbackTimeout);
    le_timer_SetMsInterval(TestTimeoutTimerRef, TEST_CALLBACK_TIMEOUT);
    le_timer_Start(TestTimeoutTimerRef);
}
