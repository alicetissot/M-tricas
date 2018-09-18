#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h> // open
#include <sys/stat.h>  // open
#include <fcntl.h>     // open
#include <unistd.h>    // read/write usleep
#include <inttypes.h>  // uint8_t, etc
#include <open62541.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <netinet/ip_icmp.h>

int fd;
// Note PCF8591 defaults to 0x48!
int ads_address = 0x48;
int16_t val;

uint8_t writeBuf[3];
uint8_t readBuf[2];

float myfloat;

/*
The resolution of the ADC in single ended 
mode we have 15 bit rather than 16 bit resolution, 
the 16th bit being the sign of the differential reading.
*/

// Define the Packet Constants
// ping packet size
#define PING_PKT_S 64
 
// Automatic port number
#define PORT_NO 0 

// Automatic port number
#define PING_SLEEP_RATE 1000000 

// Gives the timeout delay for receiving packets
// in seconds
#define RECV_TIMEOUT 1 

// Define the Ping Loop
int pingloop=1;


static UA_StatusCode readRAM(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                         const UA_NodeId *nodeId, void *nodeContext, UA_Boolean sourceTimeStamp,
                                         const UA_NumericRange *range, UA_DataValue *dataValue) {

    struct sysinfo sys_info;

    if(sysinfo(&sys_info) != 0)
	perror("sysinfo");

    // Total and free ram.
    printf("Total RAM: %ldk\n", sys_info.totalram / 1024);
    printf("Free RAM: %ldk\n", sys_info.freeram / 1024);
	  
    // Shared and buffered ram.
    printf("Shared RAM: %ldk\n", sys_info.sharedram / 1024);
    printf("Buffered RAM: %ldk\n", sys_info.bufferram / 1024);
	
    return 0;
	
    UA_Int16 level = val;
    UA_Variant_setScalarCopy(&dataValue->value, &level,
                             &UA_TYPES[UA_TYPES_UINT16]);
    dataValue->hasValue = true;
    return UA_STATUSCODE_GOOD;
}


static UA_StatusCode writeRAM(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                      const UA_NodeId *nodeId, void *nodeContext, const UA_NumericRange *range,
                                      const UA_DataValue *data) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Manually changing sensor value NOT implemented!");
    return UA_STATUSCODE_BADINTERNALERROR;
}

static void addRAMDataSourceVariable(UA_Server *server) {

UA_NodeId LevelId; /* get the nodeid assigned by the server */
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    oAttr.displayName = UA_LOCALIZEDTEXT("en-US", "RAM");
    UA_Server_addObjectNode(server, UA_NODEID_NULL,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                            UA_QUALIFIEDNAME(1, "RAM"), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            oAttr, NULL, &LevelId);

    UA_VariableAttributes mnAttr = UA_VariableAttributes_default;
    UA_String manufacturerName = UA_STRING("???");
    UA_Variant_setScalar(&mnAttr.value, &manufacturerName, &UA_TYPES[UA_TYPES_STRING]);
    mnAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Total_RAM");
    UA_Server_addVariableNode(server, UA_NODEID_NULL, LevelId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "Total_RAM"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), mnAttr, NULL, NULL);

    UA_VariableAttributes modelAttr = UA_VariableAttributes_default;
    UA_String modelName = UA_STRING("???");
    UA_Variant_setScalar(&modelAttr.value, &modelName, &UA_TYPES[UA_TYPES_STRING]);
    modelAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Free_RAM");
    UA_Server_addVariableNode(server, UA_NODEID_NULL, LevelId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "Free_RAM"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), modelAttr, NULL, NULL);

    UA_VariableAttributes tagAttr = UA_VariableAttributes_default;
    UA_String tag = UA_STRING("???");
    UA_Variant_setScalar(&tagAttr.value, &tag, &UA_TYPES[UA_TYPES_STRING]);
    tagAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Shared_RAM");
    UA_Server_addVariableNode(server, UA_NODEID_NULL, LevelId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "Shared_RAM"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), tagAttr, NULL, NULL);

    UA_VariableAttributes tagAttr = UA_VariableAttributes_default;
    UA_String tag = UA_STRING("???");
    UA_Variant_setScalar(&tagAttr.value, &tag, &UA_TYPES[UA_TYPES_STRING]);
    tagAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Buffer_RAM");
    UA_Server_addVariableNode(server, UA_NODEID_NULL, LevelId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "Buffer_RAM"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), tagAttr, NULL, NULL);
							  
							  
							  
    UA_NodeId currentNodeId = UA_NODEID_STRING(1, "RAM");
    UA_QualifiedName currentName = UA_QUALIFIEDNAME(1, "RAM");
	
    UA_DataSource levelDataSource;
    UA_VariableAttributes vAttr = UA_VariableAttributes_default;
    levelDataSource.read = readRAM;
    levelDataSource.write = writeRAM;
    UA_Variant_setScalar(&vAttr.value, &levelDataSource, &UA_TYPES[UA_TYPES_STRING]);
    vAttr.displayName = UA_LOCALIZEDTEXT("en-US", "RAM");
    UA_Server_addDataSourceVariableNode(server, currentNodeId, LevelId,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), currentName,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), vAttr,
                                        levelDataSource, NULL, NULL);
}
							  
UA_Boolean running = true;
static void stopHandler(int sig) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "received ctrl-c");
    running = false;
}

int main() {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_ServerConfig *config = UA_ServerConfig_new_default();
    UA_Server *server = UA_Server_new(config);

    addRAMDataSourceVariable(server);

    UA_StatusCode retval = UA_Server_run(server, &running);
    UA_Server_delete(server);
    UA_ServerConfig_delete(config);

    return (int) retval;
}
