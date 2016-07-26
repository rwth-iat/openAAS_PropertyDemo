#include <signal.h>

#include "ua_config_standard.h"
#include "ua_network_tcp.h"
#include "ua_log_stdout.h"

/* files nodeset.h and nodeset.c are created from server_nodeset.xml in the /src_generated directory by CMake */
#include "nodeset.h"

#include "pi_dht_read.h"
#include "pi_mmio.h"

#define SENSOR_TYPE                       DHT22
#define SENSOR_GPIO_PIN                   17

#define DIAGNOSELED_GPIO_PIN              13
UA_Guid diagnosisJobId;
#define LED_REC_NAME                    "LED"

#define READ_TEMPERATURE    1
#define READ_HUMIDITY       2

UA_Boolean running = true;
UA_Logger logger = UA_Log_Stdout;

typedef struct Readings {
    UA_DateTime readTime;
    UA_Float    humidity;
    UA_Float    temperature;
    UA_Int32    status;
} Readings;
Readings readings = {.readTime = 0, .humidity = 0.0f, .temperature = 0.0f, .status=DHT_ERROR_GPIO};

UA_Boolean ledOn = false;

static void stopHandler(int sign) {
    UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = false;
}

static void readSensorJob(UA_Server *server, void *data) {
    Readings tempReadings;
    tempReadings.status = pi_dht_read(SENSOR_TYPE, SENSOR_GPIO_PIN, &tempReadings.humidity, &tempReadings.temperature);
    tempReadings.readTime = UA_DateTime_now();
    //update a reading if its successful or the old reading is 60 seconds old
    if(tempReadings.status == DHT_SUCCESS || readings.status != DHT_SUCCESS || tempReadings.readTime - readings.readTime > 60 * 1000 * UA_MSEC_TO_DATETIME){
        readings = tempReadings;
    }
}

static UA_StatusCode
readSensor(void *handle, const UA_NodeId nodeid, UA_Boolean sourceTimeStamp,
            const UA_NumericRange *range, UA_DataValue *dataValue) {
    dataValue->hasValue = true;
    if((UA_Int32)handle == READ_TEMPERATURE){
        UA_Variant_setScalarCopy(&dataValue->value, &readings.temperature, &UA_TYPES[UA_TYPES_FLOAT]);
    } else {
        UA_Variant_setScalarCopy(&dataValue->value, &readings.humidity, &UA_TYPES[UA_TYPES_FLOAT]);
    }
    dataValue->hasSourceTimestamp = true;
    UA_DateTime_copy(&readings.readTime, &dataValue->sourceTimestamp);
    dataValue->hasStatus = true;
    switch(readings.status){
        case DHT_SUCCESS:
            dataValue->status = UA_STATUSCODE_GOOD;
            break;
        case DHT_ERROR_TIMEOUT:
            dataValue->status = UA_STATUSCODE_BADTIMEOUT;
            break;
        case DHT_ERROR_GPIO:
            dataValue->status = UA_STATUSCODE_BADCOMMUNICATIONERROR;
            break;
        default:
            dataValue->status = UA_STATUSCODE_BADNOTFOUND;
            break;
    }
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
readLed(void *handle, const UA_NodeId nodeid, UA_Boolean sourceTimeStamp,
            const UA_NumericRange *range, UA_DataValue *dataValue) {
    dataValue->hasValue = true;
    UA_Variant_setScalarCopy(&dataValue->value, &ledOn, &UA_TYPES[UA_TYPES_BOOLEAN]);
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
diagnosisMethod(void *handle, const UA_NodeId objectId, size_t inputSize, const UA_Variant *input,
                 size_t outputSize, UA_Variant *output) {

        if(inputSize != 2){
            return UA_STATUSCODE_BADMETHODINVALID;
        }

        if(input[0].type != &UA_TYPES[UA_TYPES_STRING] && input[1].type != &UA_TYPES[UA_TYPES_STRING]){
            return UA_STATUSCODE_BADMETHODINVALID;
        }

        UA_String* receiver = (UA_String*)input[0].data;
        UA_String* msg = (UA_String*)input[1].data;

        UA_String rec = UA_STRING(LED_REC_NAME);
        if(!UA_String_equal(receiver, &rec)){
            UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "Wrong receiver");
            return UA_STATUSCODE_BADMETHODINVALID;
        }

        if(pi_mmio_init()==MMIO_SUCCESS){
            pi_mmio_set_output(DIAGNOSELED_GPIO_PIN);
        }

        UA_String on = UA_STRING("ON");
        if(UA_String_equal(msg, &on)){
            if(pi_mmio_init()==MMIO_SUCCESS)
                pi_mmio_set_high(DIAGNOSELED_GPIO_PIN);
            return UA_STATUSCODE_GOOD;
        }

        UA_String off = UA_STRING("OFF");
        if(UA_String_equal(msg, &off)){
            if(pi_mmio_init()==MMIO_SUCCESS)
                pi_mmio_set_low(DIAGNOSELED_GPIO_PIN);
            return UA_STATUSCODE_GOOD;
        }

        UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "Wrong message");
        return UA_STATUSCODE_BADMETHODINVALID;
}

static UA_UInt32 currentId = 1000;
static UA_NodeId getNewNodeId(UA_UInt16 ns) {
    return UA_NODEID_NUMERIC(ns, currentId++);

}

static void createComponent(UA_ObjectAttributes* objAtrC1, UA_NodeId newNodeId,
        char* componentName, UA_Server* server, UA_NodeId parentNodeId) {
    UA_QualifiedName browseNameC1 = UA_QUALIFIEDNAME(0, componentName);
    objAtrC1->description = UA_LOCALIZEDTEXT("en", componentName);
    objAtrC1->displayName = UA_LOCALIZEDTEXT("en", componentName);
    UA_NodeId nodeId = newNodeId;
    UA_Server_addObjectNode(server, nodeId, parentNodeId,
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), browseNameC1,
            UA_NODEID_NUMERIC(0, 58), *objAtrC1, NULL, &nodeId);
    parentNodeId = nodeId;
    UA_ObjectAttributes objAtrInbox;
    UA_ObjectAttributes_init(&objAtrInbox);
    UA_QualifiedName browseNameInbox = UA_QUALIFIEDNAME(0, "Inbox");
    objAtrInbox.description = UA_LOCALIZEDTEXT("en", "Inbox");
    objAtrInbox.displayName = UA_LOCALIZEDTEXT("en", "Inbox");
    UA_NodeId inboxNodeId = UA_NODEID_NUMERIC(nodeId.namespaceIndex,
            nodeId.identifier.numeric + 1);
    UA_Server_addObjectNode(server, getNewNodeId(nodeId.namespaceIndex),
            parentNodeId, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            browseNameInbox, UA_NODEID_NUMERIC(0, 61), objAtrInbox, NULL,
            &inboxNodeId);
}

int main(int argc, char** argv) {
    signal(SIGINT, stopHandler); /* catches ctrl-c */

    UA_Guid_init(&diagnosisJobId);

    UA_ServerConfig config = UA_ServerConfig_standard;
    UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, 16664);
    config.networkLayers = &nl;
    config.networkLayersSize = 1;

    //default applications "sees" NS 0 and NS1
    UA_ApplicationDescription* app1 = &config.applicationDescription;
    app1->applicationName.text = UA_STRING_ALLOC("Message");
    app1->discoveryUrlsSize = 1;
    app1->discoveryUrls = UA_Array_new(1, &UA_TYPES[UA_TYPES_STRING]);
    app1->discoveryUrls[0] = UA_STRING_ALLOC("/app0");

    UA_Server *server = UA_Server_new(config);

    /* create nodes from nodeset */
    nodeset(server);

    /* add a sensor sampling job to the server */
    UA_Job job = {.type = UA_JOBTYPE_METHODCALL,
                  .job.methodCall = {.method = readSensorJob, .data = NULL} };
    UA_Server_addRepeatedJob(server, job, 2500, NULL);


    /* adding temperature node */
    UA_DataSource temperatureDataSource = (UA_DataSource) {
            .handle = (void*)READ_TEMPERATURE, .read = readSensor, .write = NULL};

    /*UA_NodeId temperatureNodeId = UA_NODEID_STRING(1, "temperature");
    UA_QualifiedName temperatureNodeName = UA_QUALIFIEDNAME(1, "temperature");
    UA_VariableAttributes temperatureAttr;
    UA_VariableAttributes_init(&temperatureAttr);
    temperatureAttr.description = UA_LOCALIZEDTEXT("en_US","temperature");
    temperatureAttr.displayName = UA_LOCALIZEDTEXT("en_US","temperature");

    UA_Server_addDataSourceVariableNode(server, temperatureNodeId,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                        temperatureNodeName, UA_NODEID_NULL, temperatureAttr, temperatureDataSource, NULL); */

    /* add temperature datasource to a generated node */
    UA_Server_setVariableNode_dataSource(server, UA_NODEID_NUMERIC(4, 6001), temperatureDataSource);

    /* adding humidity node */
    UA_DataSource humidityDataSource = (UA_DataSource) {
        .handle = (void*)READ_HUMIDITY, .read = readSensor, .write = NULL};
    /* UA_NodeId humidityNodeId = UA_NODEID_STRING(1, "humidity");
    UA_QualifiedName humidityNodeName = UA_QUALIFIEDNAME(1, "humidity");

    UA_VariableAttributes humidityAttr;
    UA_VariableAttributes_init(&humidityAttr);
    humidityAttr.description = UA_LOCALIZEDTEXT("en_US","humidity");
    humidityAttr.displayName = UA_LOCALIZEDTEXT("en_US","humidity");

    UA_Server_addDataSourceVariableNode(server, humidityNodeId,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                        humidityNodeName, UA_NODEID_NULL, humidityAttr, humidityDataSource, NULL); */

    /* add temperature datasource to a generated node */
    UA_Server_setVariableNode_dataSource(server, UA_NODEID_NUMERIC(4, 6247), humidityDataSource);

    /* LMSR */
    UA_ObjectAttributes objAtrPeerManager;
    UA_ObjectAttributes_init(&objAtrPeerManager);
    UA_NodeId peerManagerNodeId = UA_NODEID_NUMERIC(1, 5000);
    char* peerManager = "LMSR";
    createComponent(&objAtrPeerManager, peerManagerNodeId, peerManager, server, UA_NODEID_NUMERIC(0, 85));

    /* Array with registered components */
    UA_NodeId registeredComponentsId = UA_NODEID_NUMERIC(1, 5001);
    UA_QualifiedName registeredComponentsNodeName = UA_QUALIFIEDNAME(1, "registeredComponents");
    UA_VariableAttributes registeredComponentsAttr;
    UA_VariableAttributes_init(&registeredComponentsAttr);
    UA_String* registeredComponentsArray = UA_Array_new(1, &UA_TYPES[UA_TYPES_STRING]);
    UA_String_init(&registeredComponentsArray[0]);
    registeredComponentsArray[0] = UA_STRING_ALLOC(LED_REC_NAME);
    UA_Variant_setArray(&registeredComponentsAttr.value, registeredComponentsArray, 1, &UA_TYPES[UA_TYPES_STRING]);
    registeredComponentsAttr.description = UA_LOCALIZEDTEXT("en_US","registeredComponents");
    registeredComponentsAttr.displayName = UA_LOCALIZEDTEXT("en_US","registeredComponents");

    UA_Server_addVariableNode(server, registeredComponentsId, peerManagerNodeId,
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), registeredComponentsNodeName,
                                  UA_NODEID_NULL, registeredComponentsAttr, NULL, NULL);

    /* adding dropMessage method node */
    UA_MethodAttributes diagnosisAttr;
    UA_MethodAttributes_init(&diagnosisAttr);
    diagnosisAttr.description = UA_LOCALIZEDTEXT("en_US","dropMessage");
    diagnosisAttr.displayName = UA_LOCALIZEDTEXT("en_US","dropMessage");
    diagnosisAttr.executable = true;
    diagnosisAttr.userExecutable = true;

    /* add the method node with the callback */
    UA_Argument* inputArguments = UA_Array_new(2,&UA_TYPES[UA_TYPES_ARGUMENT]);
    UA_Argument_init(&inputArguments[0]);
    inputArguments[0].dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    inputArguments[0].description = UA_LOCALIZEDTEXT("en_US", "Receiver");
    inputArguments[0].name = UA_STRING("Receiver");
    inputArguments[0].valueRank = -1;
    UA_Argument_init(&inputArguments[1]);
    inputArguments[1].dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    inputArguments[1].description = UA_LOCALIZEDTEXT("en_US", "Message");
    inputArguments[1].name = UA_STRING("Message");
    inputArguments[1].valueRank = -1;

    UA_Server_addMethodNode(server, UA_NODEID_NUMERIC(1, 5002),
                            peerManagerNodeId,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASORDEREDCOMPONENT),
                            UA_QUALIFIEDNAME(1, "dropMessage"),
                            diagnosisAttr, &diagnosisMethod, server,
                            2, inputArguments, 0, NULL, NULL);


    {
        UA_ApplicationDescription app0;
        UA_ApplicationDescription_copy(&config.applicationDescription, &app0);
        UA_String_deleteMembers(&app0.applicationName.text);
        app0.applicationName.text = UA_STRING_ALLOC("AssetShell");
        app0.discoveryUrlsSize = 1;
        app0.discoveryUrls = UA_Array_new(1, &UA_TYPES[UA_TYPES_STRING]);
        app0.discoveryUrls[0] = UA_STRING_ALLOC("/app1");
        UA_UInt16 ns[4];
        ns[0] = 0;
        ns[1] = 2;
        ns[2] = 3;
        ns[3] = 4;
        UA_Server_addApplication(server, &app0, ns, 4);
    }

    {
        UA_ApplicationDescription app0;
        UA_ApplicationDescription_copy(&config.applicationDescription, &app0);
        UA_String_deleteMembers(&app0.applicationName.text);
        app0.applicationName.text = UA_STRING_ALLOC("Engineering");
        app0.discoveryUrlsSize = 1;
        app0.discoveryUrls = UA_Array_new(1, &UA_TYPES[UA_TYPES_STRING]);
        app0.discoveryUrls[0] = UA_STRING_ALLOC("/app2");
        UA_UInt16 ns[5];
        ns[0] = 0;
        ns[1] = 1;
        ns[2] = 2;
        ns[3] = 3;
        ns[4] = 4;
        UA_Server_addApplication(server, &app0, ns, 5);
    }

    UA_StatusCode retval = UA_Server_run(server, &running);
    UA_Server_delete(server);
    nl.deleteMembers(&nl);
    return (int)retval;
}
