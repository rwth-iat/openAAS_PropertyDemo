#include <signal.h>

/* we need to use some internal open62541 headers due to usage of experimental node generation feature */
//# include "ua_types.h"
//# include "ua_server.h"
//# include "ua_config_standard.h"
//# include "ua_network_tcp.h"
//# include "ua_log_stdout.h"

/* files nodeset.h and nodeset.c are created from server_nodeset.xml in the /src_generated directory by CMake */
#include "nodeset.h"

#include "pi_dht_read.h"
#include "pi_mmio.h"

#define SENSOR_TYPE                       DHT22
#define SENSOR_GPIO_PIN                   4

#define DIAGNOSELED_GPIO_PIN              14
#define DIAGNOSIS_DURATION      5000 //ms
UA_Guid diagnosisJobId;

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

static void diagnosisJob(UA_Server *server, void *data) {
    //cleanup
    UA_Server_removeRepeatedJob(server, diagnosisJobId);
    UA_Guid_init(&diagnosisJobId);
    //set led to low
    if(pi_mmio_init()==MMIO_SUCCESS)
        pi_mmio_set_low(DIAGNOSELED_GPIO_PIN);
    UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "Diagnosis finished");
    return;
}
static UA_StatusCode
diagnosisMethod(void *handle, const UA_NodeId objectId, size_t inputSize, const UA_Variant *input,
                 size_t outputSize, UA_Variant *output) {
        if(!UA_Guid_equal(&diagnosisJobId, &UA_GUID_NULL)){
            return UA_STATUSCODE_BADNOTHINGTODO;
        }

        UA_Server* server = (UA_Server*)handle;

        if(pi_mmio_init()==MMIO_SUCCESS){
            pi_mmio_set_output(DIAGNOSELED_GPIO_PIN);
            pi_mmio_set_high(DIAGNOSELED_GPIO_PIN);
        }

        UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "Diagnosis started");
        UA_Job job = {.type = UA_JOBTYPE_METHODCALL,
                      .job.methodCall = {.method = diagnosisJob, .data = NULL} };
        UA_Server_addRepeatedJob(server, job, DIAGNOSIS_DURATION, &diagnosisJobId);

        return UA_STATUSCODE_GOOD;
}

int main(int argc, char** argv) {
    signal(SIGINT, stopHandler); /* catches ctrl-c */

    UA_Guid_init(&diagnosisJobId);

    UA_ServerConfig config = UA_ServerConfig_standard;
    UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, 16664);
    config.networkLayers = &nl;
    config.networkLayersSize = 1;
    UA_Server *server = UA_Server_new(config);

    /* create nodes from nodeset */
    nodeset(server);

    /* add a sensor sampling job to the server */
    UA_Job job = {.type = UA_JOBTYPE_METHODCALL,
                  .job.methodCall = {.method = readSensorJob, .data = NULL} };
    UA_Server_addRepeatedJob(server, job, 2500, NULL);


    /* adding temperature node */
    UA_NodeId temperatureNodeId = UA_NODEID_STRING(1, "temperature");
    UA_QualifiedName temperatureNodeName = UA_QUALIFIEDNAME(1, "temperature");
    UA_DataSource temperatureDataSource = (UA_DataSource) {
        .handle = (void*)READ_TEMPERATURE, .read = readSensor, .write = NULL};
    UA_VariableAttributes temperatureAttr;
    UA_VariableAttributes_init(&temperatureAttr);
    temperatureAttr.description = UA_LOCALIZEDTEXT("en_US","temperature");
    temperatureAttr.displayName = UA_LOCALIZEDTEXT("en_US","temperature");

    UA_Server_addDataSourceVariableNode(server, temperatureNodeId,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                        temperatureNodeName, UA_NODEID_NULL, temperatureAttr, temperatureDataSource, NULL);

    /* add temperature datasource to a generated node */
    UA_Server_setVariableNode_dataSource(server, UA_NODEID_NUMERIC(2, 6002), temperatureDataSource);

    /* adding humidity node */
    UA_NodeId humidityNodeId = UA_NODEID_STRING(1, "humidity");
    UA_QualifiedName humidityNodeName = UA_QUALIFIEDNAME(1, "humidity");
    UA_DataSource humidityDataSource = (UA_DataSource) {
        .handle = (void*)READ_HUMIDITY, .read = readSensor, .write = NULL};
    UA_VariableAttributes humidityAttr;
    UA_VariableAttributes_init(&humidityAttr);
    humidityAttr.description = UA_LOCALIZEDTEXT("en_US","humidity");
    humidityAttr.displayName = UA_LOCALIZEDTEXT("en_US","humidity");

    UA_Server_addDataSourceVariableNode(server, humidityNodeId,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                        humidityNodeName, UA_NODEID_NULL, humidityAttr, humidityDataSource, NULL);

    /* adding diagnosis method node */
    UA_MethodAttributes diagnosisAttr;
    UA_MethodAttributes_init(&diagnosisAttr);
    diagnosisAttr.description = UA_LOCALIZEDTEXT("en_US","diagnosis");
    diagnosisAttr.displayName = UA_LOCALIZEDTEXT("en_US","diagnosis");
    diagnosisAttr.executable = true;
    diagnosisAttr.userExecutable = true;
    UA_Server_addMethodNode(server, UA_NODEID_NULL,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASORDEREDCOMPONENT),
                            UA_QUALIFIEDNAME(1, "diagnosis"),
                            diagnosisAttr, &diagnosisMethod, server,
                            0, NULL, 0, NULL, NULL);

    UA_StatusCode retval = UA_Server_run(server, &running);
    UA_Server_delete(server);
    nl.deleteMembers(&nl);
    return (int)retval;
}
