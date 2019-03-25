#include "legato.h"
#include "interfaces.h"
#include "le_data_interface.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> 
#include <ifaddrs.h>

#define ADDR "46.236.76.67"
#define PORT 2023
#define SA struct sockaddr
#define ENABLED 1
#define NOT_ENABLED 0
#define STARTED 1
#define STOPPED 0
#define UNKNOWN 2
#define FIXED 1
#define FAILED 0
#define DEVICE_ID "not_defined"

struct gnss{
	uint8_t enabled; 
	uint8_t started; 
	uint8_t ttff; 
};

struct position{
	double lat;
        double lon;
        double accuracy;
	long timestamp; 
}; 

//this function may change as I change to the FX30 supporting LTE
static void data_init
(
 	void
)
{
	//set preferred technology to cellular
	le_data_Technology_t tec = LE_DATA_CELLULAR;
        le_data_SetTechnologyRank(1, tec);
	
	//connect
	le_data_Request();

}

static void configureGNSS
(
	struct gnss* state
)
{
	le_result_t result; 

	LE_INFO("ENABLING GNSS..."); 
	result = le_gnss_Enable();
	switch(result){
		case LE_DUPLICATE: 
			LE_INFO("GNSS STATE ALREADY ACTIVE"); 
			state->enabled = ENABLED; 
			break; 
		case LE_NOT_PERMITTED: 
			LE_INFO("GNSS STATE UNITIALIZED"); 
			state->enabled = NOT_ENABLED; 
			break; 
		case LE_OK: 
			LE_INFO("GNSS STATE READY"); 
			state->enabled = ENABLED; 
			break;
		default : 
			LE_INFO("UNKNOWN GNSS STATE"); 
			state->enabled = UNKNOWN; 
			break; 	
	}

	LE_INFO("STARTING GNSS..."); 
	result = le_gnss_Start(); 

	switch(result){
		case LE_OK: 
			LE_INFO("GNSS STATE ACTIVE"); 
			state->started = STARTED; 
			break; 
		case LE_NOT_PERMITTED: 
			LE_INFO("GNSS STATE DISABLED OR UNINITIALIZED"); 
			state->started = STOPPED; 
			break; 
		case LE_DUPLICATE: 
			LE_INFO("GNSS STATE ALREADY ACTIVE"); 
			state->started = STARTED;  
			break;
		default: 
			LE_INFO("UNKNOWN GNSS STATE"); 
			state->started = UNKNOWN; 
			break; 	

	}

}

static void getFix
(
	struct gnss *state
)
{
	LE_INFO("WAIT 60 SECONDS FOR A 3D FIX");
        sleep(60);

        uint32_t ttff;

        LE_INFO("CHECKING TIME TO FIRST FIX");
        le_result_t result = le_gnss_GetTtff(&ttff);

        switch(result){

                case LE_BUSY:
                        LE_INFO("THE POSITION IS NOT FIXED AND TTFF CAN'T BE MEASURED");
                        state->ttff = FAILED; 
			break;
                case LE_NOT_PERMITTED:
                        LE_INFO("GNSS DEVICE NOT ENABLED OR NOT STARTED");
                        state->ttff = FAILED;
			break;
                case LE_OK:
                        LE_INFO("SUCCEDED");
                        LE_INFO("TIME FOR FIRST FIX: %d", ttff);
                        state->ttff = FIXED; 
			break;
                case LE_FAULT:
			LE_INFO("FAILED TO GET FIX"); 
			state->ttff = FAILED;
			break;
                default:
                        LE_INFO("UNKNOWN ERROR");
			state->ttff = UNKNOWN; 
                        break;
        }
	
}

static le_result_t getLocation
(
	struct position *pos
)
{
	int32_t lat = 0;
        int32_t lon = 0;
        int32_t accuracy = 0;
	le_result_t result; 
	le_gnss_SampleRef_t positionSampleRef = le_gnss_GetLastSampleRef();


	LE_INFO("==GETTING LOCATION==");
        result = le_gnss_GetLocation(positionSampleRef, &lat, &lon, &accuracy);
        switch(result){
                case LE_FAULT:
                	LE_INFO("FAILED TO GET LOCATION DATA");
                        break;
                case LE_OUT_OF_RANGE:
                        LE_INFO("AT LEAST ONE PARAMETERS IS INVALID");
                        break;
                case LE_OK:
                        LE_INFO("SUCCEDED TO GET LOCATION");
                        LE_INFO("Latitude: %d, Longitude: %d, Accuracy: %d", lat, lon, accuracy);
			//get epoch time in seconds
			pos->timestamp = le_clk_GetAbsoluteTime().sec;
			//we need to get 6 decimals
                        pos->lat = (float)lat / 1000000; 
			pos->lon = (float) lon / 1000000; 
			pos->accuracy = accuracy; 
			break;
                default:
                        break;
        }
	return result; 
}

static void socket_create_connect
(
 	int sockfd, int connected
)
{
	struct sockaddr_in servaddr; 

	//create a socket
	sockfd  = socket(AF_INET, SOCK_STREAM, 0); 
	if(sockfd == -1){
		LE_INFO("SOCKET CREATION FAILED"); 
	}
	else{
		LE_INFO("SOCKET SUCCESSFULLY CREATED"); 
		bzero(&servaddr, sizeof(servaddr)); 
		
		servaddr.sin_family = AF_INET; 
		servaddr.sin_addr.s_addr = inet_addr(ADDR); 
		servaddr.sin_port = htons(PORT); 
		LE_INFO("SERVER ADDRESS : %d", servaddr.sin_addr.s_addr); 
		LE_INFO("PORT: %d", PORT); 
		LE_INFO("TRYING TO CONNECT.."); 
		if(connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0){
			LE_INFO("CONNECTION WITH SERVER FAILED");
			connected = 0; 	
		}
		else{
			LE_INFO("CONNECTED TO SERVER"); 
			connected = 1; 
		}
	}
	
}

static void socket_send_data
(
	struct position pos, int sockfd
)
{
	//message mapping: 
	// { 	
	// 	"ID" : Device ID, 
	// 	"Data" : {
	// 		"Lat" : lat, 
	// 		"Lon" : lon, 
	// 		"Ts"  : Timestamp 
	// 	}
	//
	// }
	char *msg = (char*) malloc (200 * sizeof(char));
	char stringyfied[40]; 
	
	//may change this to use some implementation of stringbuilder / jsonbuilder 
	//but ok for now 
	strcat(msg, "{ \"ID\" : "); 
	strcat(msg, DEVICE_ID); 
	strcat(msg, ", \"Data\" : { \"Lat\" : "); 
       	snprintf(stringyfied, 40, "%f", pos.lat); 
	strcat(msg, stringyfied); 
	strcat(msg, ", \"Lon\" : "); 
	snprintf(stringyfied, 40, "%f", pos.lon); 
	strcat(msg, stringyfied); 
	strcat(msg, ", \"Ts\" : "); 
	snprintf(stringyfied, 40, "%ld", pos.timestamp); 
	strcat(msg, stringyfied); 
	strcat(msg, "} }"); 
	
	write(sockfd, msg, sizeof(msg)); 
	free(msg); 
}

COMPONENT_INIT
{

	int sockfd = 0, connected = 0;  
	struct gnss state = {0}; 
	struct position newPosition = {0}; 
	le_result_t result; 

	//connect to network
	data_init(); 

	//start with enabling and starting gnss
	configureGNSS(&state);
	
	if(state.enabled == ENABLED && state.started == STARTED){
		LE_INFO("TRYING TO GET FIX..."); 
		sleep(60); 
		getFix(&state);
		if(state.ttff){
			do{
				result = getLocation(&newPosition); 
				sleep(30); 
			}while(result != LE_OK);
			
			socket_create_connect(sockfd, connected); 
			if(connected){
				socket_send_data(newPosition, sockfd); 
			}		
			close(sockfd); 
			//TODO: send location somewhere 
		}
		else 
		{
			//TODO: implement some kind of loop to get a new fix if we dont get one
			LE_INFO("NO FIX, WAITING FOR 120 SECONDS");				
			sleep(120); 
		}
	}

	LE_INFO("STOPPING GNSS SERVICE");
        le_gnss_Stop();
	LE_INFO("TIMESTAMP : %ld", le_clk_GetAbsoluteTime().sec);

	
}

