#include "legato.h"
#include "interfaces.h"
#include "le_mrc_interface.h"
#include "le_atClient_interface.h"
#include "le_data_interface.h"

int send_cmd
(
	le_atClient_DeviceRef_t devRef, const char* cmd 
)
{

	le_atClient_CmdRef_t cmdRef; 
	cmdRef = le_atClient_Create(); 
	le_result_t result; 

	result = le_atClient_SetDevice(cmdRef, devRef); 

	result = le_atClient_SetCommand(cmdRef, cmd); 
	
	char buffer[LE_ATDEFS_RESPONSE_MAX_BYTES]; 
	result = le_atClient_SetFinalResponse(cmdRef, "OK|ERROR"); 
	
	result = le_atClient_Send(cmdRef); 	
	
	result = le_atClient_GetFinalResponse(cmdRef, buffer, LE_ATDEFS_RESPONSE_MAX_BYTES); 
	if(result == LE_OK){
	       	LE_INFO("FINAL RESPONSE : %s ", buffer); 
	}
	else
		return -1; 
	
	return 1; 
}

//configure a data connection 
void data_init
(
 	le_atClient_DeviceRef_t devref
)
{
	//set preffered technology to cellular 
	le_data_Technology_t tec = LE_DATA_CELLULAR; 
	le_data_SetTechnologyRank(1, tec); 
	
	le_mrc_Rat_t rat; 

	le_mrc_GetRadioAccessTechInUse(&rat); 

	//if LTE is not the used RAT 	
	if(rat != LE_MRC_RAT_LTE){
		//set the radio access technology to LTE only 
		send_cmd(devref, "AT!SELRAT=06"); 
		//set the radio module to use NB-IoT
		//to use LTE-M we would set SELCIOT = 2
		send_cmd(devref, "AT!SELCIOT=4"); 	
	}
	
	le_data_RequestObjRef_t req; 

	do{
		req = le_data_Request(); 
	}while(req == NULL); 
}

void unbind_device
(
 	le_atClient_DeviceRef_t deviceref
)
{
	le_result_t result; 
	result = le_atClient_Stop(deviceref); 

	switch(result){
		case LE_OK: 
			LE_INFO("UNBOUND THE DEVICE SUCCESSFULLY"); 
			break; 
		case LE_FAULT: 
		default: 
			LE_INFO("SOMETHING WENT WRONG WHILE TRYING TO UNBIND THE DEVICE"); 
			break;
	}
}

COMPONENT_INIT
{
	LE_INFO("==STARTING APPLICATION =="); 
	//bind the device to the AT port 
	int fd = open("/dev/ttyAT", O_RDWR | O_NOCTTY | O_NONBLOCK); 
	//if opens return a negative number, it failed 
	LE_ASSERT(fd >= 0);  
	
	le_atClient_DeviceRef_t deviceref = le_atClient_Start(fd);
	
	//make sure we use the LTE network and then connect
	data_init(deviceref); 
	
	//unbind the device from the AT port 
	unbind_device(deviceref); 

	LE_INFO("EVERYTHING WENT WELL!"); 
}
