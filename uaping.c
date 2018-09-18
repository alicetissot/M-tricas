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


static UA_StatusCode readLatencia(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                         const UA_NodeId *nodeId, void *nodeContext, UA_Boolean sourceTimeStamp,
                                         const UA_NumericRange *range, UA_DataValue *dataValue) {


	// ping packet structure
	struct ping_pkt
	{
	    struct icmphdr hdr;
	    char msg[PING_PKT_S-sizeof(struct icmphdr)];
	};

	// Calculating the Check Sum
	unsigned short checksum(void *b, int len)
	{    
	    unsigned short *buf = b;
	    unsigned int sum=0;
	    unsigned short result;

	    for ( sum = 0; len > 1; len -= 2 )
		sum += *buf++;
	    if ( len == 1 )
		sum += *(unsigned char*)buf;
	    sum = (sum >> 16) + (sum & 0xFFFF);
	    sum += (sum >> 16);
	    result = ~sum;
	    return result;
	}


	// Interrupt handler
	void intHandler(int dummy)
	{
	    pingloop=0;
	}

	// Performs a DNS lookup 
	char *dns_lookup(char *addr_host, struct sockaddr_in *addr_con)
	{
	    printf("\nResolving DNS..\n");
	    struct hostent *host_entity;
	    char *ip=(char*)malloc(NI_MAXHOST*sizeof(char));
	    int i;

	    if ((host_entity = gethostbyname(addr_host)) == NULL)
	    {
		// No ip found for hostname
		return NULL;
	    }
	    
	    //filling up address structure
	    strcpy(ip, inet_ntoa(*(struct in_addr *)host_entity->h_addr));

	    (*addr_con).sin_family = host_entity->h_addrtype;
	    (*addr_con).sin_port = htons (PORT_NO);
	    (*addr_con).sin_addr.s_addr  = *(long*)host_entity->h_addr;

	    return ip;
	    
	}

	// Resolves the reverse lookup of the hostname
	char* reverse_dns_lookup(char *ip_addr)
	{
	    struct sockaddr_in temp_addr;    
	    socklen_t len;
	    char buf[NI_MAXHOST], *ret_buf;

	    temp_addr.sin_family = AF_INET;
	    temp_addr.sin_addr.s_addr = inet_addr(ip_addr);
	    len = sizeof(struct sockaddr_in);

	    if (getnameinfo((struct sockaddr *) &temp_addr, len, buf, sizeof(buf), NULL, 0, NI_NAMEREQD)) 
	    {
		printf("Could not resolve reverse lookup of hostname\n");
		return NULL;
	    }
	    ret_buf = (char*)malloc((strlen(buf) +1)*sizeof(char) );
	    strcpy(ret_buf, buf);
	    return ret_buf;
	}

	// make a ping request
	void send_ping(int ping_sockfd, struct sockaddr_in *ping_addr, char *ping_dom, char *ping_ip, char *rev_host)
	{
	    int ttl_val=64, msg_count=0, i, addr_len, flag=1, msg_received_count=0;
	    
	    struct ping_pkt pckt;
	    struct sockaddr_in r_addr;
	    struct timespec time_start, time_end, tfs, tfe;
	    long double rtt_msec=0, total_msec=0;
	    struct timeval tv_out;
	    tv_out.tv_sec = RECV_TIMEOUT;
	    tv_out.tv_usec = 0;

	    clock_gettime(CLOCK_MONOTONIC, &tfs);

	    
	    // set socket options at ip to TTL and value to 64,
	    // change to what you want by setting ttl_val
	    if (setsockopt(ping_sockfd, SOL_IP, IP_TTL, &ttl_val, sizeof(ttl_val)) != 0)
	    {
		printf("\nSetting socket options to TTL failed!\n");
		return;
	    }

	    else
	    {
		printf("\nSocket set to TTL..\n");
	    }

	    // setting timeout of recv setting
	    setsockopt(ping_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_out, sizeof tv_out);

	    // send icmp packet in an infinite loop
	    while(pingloop)
	    {
		// flag is whether packet was sent or not
		flag=1;
	     
		//filling packet
		bzero(&pckt, sizeof(pckt));
		
		pckt.hdr.type = ICMP_ECHO;
		pckt.hdr.un.echo.id = getpid();
		
		for ( i = 0; i < sizeof(pckt.msg)-1; i++ ) 
		pckt.msg[i] = i+'0';
		
		pckt.msg[i] = 0;
		pckt.hdr.un.echo.sequence = msg_count++;
		pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));


		usleep(PING_SLEEP_RATE);

		//send packet
		clock_gettime(CLOCK_MONOTONIC, &time_start);
		if ( sendto(ping_sockfd, &pckt, sizeof(pckt), 0, (struct sockaddr*) ping_addr, sizeof(*ping_addr)) <= 0)
		{
		    printf("\nPacket Sending Failed!\n");
		    flag=0;
		}

		//receive packet
		addr_len=sizeof(r_addr);

		if ( recvfrom(ping_sockfd, &pckt, sizeof(pckt), 0, (struct sockaddr*)&r_addr, &addr_len) <= 0 && msg_count>1) 
		{
		    printf("\nPacket receive failed!\n");
		}

		else
		{
		    clock_gettime(CLOCK_MONOTONIC, &time_end);
		    
		    double timeElapsed = ((double)(time_end.tv_nsec - time_start.tv_nsec))/1000000.0; 
			rtt_msec = (time_end.tv_sec- time_start.tv_sec) * 1000.0 + timeElapsed;
		    
		    // if packet was not sent, don't receive
		    if(flag)
		    {
		        if(!(pckt.hdr.type ==69 && pckt.hdr.code==0)) 
		        {
		            printf("Error..Packet received with ICMP type %d code %d\n", pckt.hdr.type, pckt.hdr.code);
		        }
		        else
		        {
		            printf("%d bytes from %s (h: %s) (%s) msg_seq=%d ttl=%d rtt = %Lf ms.\n",PING_PKT_S, ping_dom, rev_host, ping_ip, msg_count,ttl_val, rtt_msec);

		            msg_received_count++;
		        }
		    }
		}    
	    }
	    clock_gettime(CLOCK_MONOTONIC, &tfe);
	    double timeElapsed = ((double)(tfe.tv_nsec - tfs.tv_nsec))/1000000.0;
	    
	    total_msec = (tfe.tv_sec-tfs.tv_sec)*1000.0+ timeElapsed;
		           
	    printf("\n===%s ping statistics===\n", ping_ip);
	    printf("\n%d packets sent, %d packets received, %f percent packet loss. Total time: %Lf ms.\n\n", msg_count, msg_received_count,((msg_count - msg_received_count)/msg_count) * 100.0,total_msec); 
	}

	// Driver Code
	int main(int argc, char *argv[])
	{
	    int sockfd;
	    char *ip_addr, *reverse_hostname;
	    struct sockaddr_in addr_con;
	    int addrlen = sizeof(addr_con);
	    char net_buf[NI_MAXHOST];

	    if(argc!=2)
	    {
		printf("\nFormat %s <address>\n", argv[0]);
		return 0;
	    }

	    ip_addr = dns_lookup(argv[1], &addr_con);
	    if(ip_addr==NULL)
	    {
		printf("\nDNS lookup failed! Could not resolve hostname!\n");
		return 0;
	    }

	    reverse_hostname = reverse_dns_lookup(ip_addr);
	    printf("\nTrying to connect to '%s' IP: %s\n", argv[1], ip_addr);
	    printf("\nReverse Lookup domain: %s",reverse_hostname);

	    //socket()
	    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	    if(sockfd<0)
	    {
		printf("\nSocket file descriptor not received!!\n");
		return 0;
	    }
	    else
		printf("\nSocket file descriptor %d received\n", sockfd);

	    signal(SIGINT, intHandler);//catching interrupt

	    //send pings continuously
	    send_ping(sockfd, &addr_con, reverse_hostname,ip_addr, argv[1]);
	    
	    return 0;
	}
	 

    UA_Int16 level = val;
    UA_Variant_setScalarCopy(&dataValue->value, &level,
                             &UA_TYPES[UA_TYPES_UINT16]);
    dataValue->hasValue = true;
    return UA_STATUSCODE_GOOD;
}


static UA_StatusCode writeLatencia(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                      const UA_NodeId *nodeId, void *nodeContext, const UA_NumericRange *range,
                                      const UA_DataValue *data) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Manually changing sensor value NOT implemented!");
    return UA_STATUSCODE_BADINTERNALERROR;
}

static void addLatenciaDataSourceVariable(UA_Server *server) {

UA_NodeId LevelId; /* get the nodeid assigned by the server */
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    oAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Latencia");
    UA_Server_addObjectNode(server, UA_NODEID_NULL,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                            UA_QUALIFIEDNAME(1, "Latencia"), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            oAttr, NULL, &LevelId);

    UA_VariableAttributes mnAttr = UA_VariableAttributes_default;
    UA_String manufacturerName = UA_STRING("64");
    UA_Variant_setScalar(&mnAttr.value, &manufacturerName, &UA_TYPES[UA_TYPES_STRING]);
    mnAttr.displayName = UA_LOCALIZEDTEXT("en-US", "ttl");
    UA_Server_addVariableNode(server, UA_NODEID_NULL, LevelId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "ttl"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), mnAttr, NULL, NULL);

    UA_VariableAttributes modelAttr = UA_VariableAttributes_default;
    UA_String modelName = UA_STRING("???");
    UA_Variant_setScalar(&modelAttr.value, &modelName, &UA_TYPES[UA_TYPES_STRING]);
    modelAttr.displayName = UA_LOCALIZEDTEXT("en-US", "msg_seq");
    UA_Server_addVariableNode(server, UA_NODEID_NULL, LevelId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "msg_seq"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), modelAttr, NULL, NULL);

    UA_VariableAttributes tagAttr = UA_VariableAttributes_default;
    UA_String tag = UA_STRING("???");
    UA_Variant_setScalar(&tagAttr.value, &tag, &UA_TYPES[UA_TYPES_STRING]);
    tagAttr.displayName = UA_LOCALIZEDTEXT("en-US", "IP");
    UA_Server_addVariableNode(server, UA_NODEID_NULL, LevelId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "IP"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), tagAttr, NULL, NULL);
							  
							  
							  
    UA_NodeId currentNodeId = UA_NODEID_STRING(1, "rtt");
    UA_QualifiedName currentName = UA_QUALIFIEDNAME(1, "rtt");
	
    UA_DataSource levelDataSource;
    UA_VariableAttributes vAttr = UA_VariableAttributes_default;
    levelDataSource.read = readLatencia;
    levelDataSource.write = writeLatencia;
    UA_Variant_setScalar(&vAttr.value, &levelDataSource, &UA_TYPES[UA_TYPES_STRING]);
    vAttr.displayName = UA_LOCALIZEDTEXT("en-US", "rtt");
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

    addLatenciaDataSourceVariable(server);

    UA_StatusCode retval = UA_Server_run(server, &running);
    UA_Server_delete(server);
    UA_ServerConfig_delete(config);

    return (int) retval;
}
