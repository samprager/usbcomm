//
//  usbdev.cpp
//
//
//  Created by Sam Prager on 6/5/2018.
//
// Class wrapper for FD2XX functions for host-to-host USB communication vs RS232 TTL
//

#include "usbcomm.hpp"

FT_STATUS getExtendedInfo(FT_HANDLE ftHandle);
static void dumpBuffer(unsigned char *buffer, int elements);

usbcomm::usbcomm(uint32_t port) : _rxbuf ( new struct rx_buf){
    int err = init(port);

    if (err){
        std::cout<<"Init failed"<<std::endl;
        _dev_ready = false;
    }else{
        _dev_ready = true;
    }
}

usbcomm::~usbcomm(){
    if (_ftHandle != NULL){
        _dev_mtx.lock();
        FT_Close(_ftHandle);
        _dev_mtx.unlock();
    }
}

int usbcomm::reset(uint32_t port){
    std::cout<<"Resetting Device..."<<std::endl;
    if (_ftHandle != NULL){
        _dev_mtx.lock();
        FT_Close(_ftHandle);
        _dev_mtx.unlock();
     }

     int err = init(port);

     if (err){
         std::cout<<"Reset failed"<<std::endl;
         _dev_ready = false;
     }else{
         _dev_ready = true;
     }
     return(err);
}
bool usbcomm::ready(){
    return _dev_ready;
}

int usbcomm::init(uint32_t port){
    uint32_t driverVersion = 0;
    uint32_t numDevs;
    FT_DEVICE_LIST_INFO_NODE *devInfo;
    _baudrate = 115200;
    FT_STATUS       ftStatus = FT_OK;
    int             portNum = 0; // First port

    _ftHandle = NULL;
    (void)FT_GetDriverVersion(NULL, &driverVersion);
	printf("Using D2XX version %08x\n", driverVersion);

    ftStatus = FT_CreateDeviceInfoList(&numDevs);

    if((numDevs>1)&&(port < numDevs)){
        portNum = port;
    }

    if ((ftStatus != FT_OK)||(numDevs==0))
    {
        printf("FT_CreateDeviceInfoList() failed, with error %d. NumDevs found = %d\n", (int)ftStatus,numDevs);
        printf("Use lsmod to check if ftdi_sio (and usbserial) are present.\n");
        printf("If so, unload them using rmmod, as they conflict with ftd2xx.\n");
        if (ftStatus == FT_OK) ftStatus = -1;
        goto exit;
    }
    printf("Found %d devices\n",numDevs);

    ftStatus = FT_Open(portNum, &_ftHandle);
    if (ftStatus != FT_OK)
    {
        printf("FT_Open(%d) failed, with error %d.\n", portNum, (int)ftStatus);
        printf("Use lsmod to check if ftdi_sio (and usbserial) are present.\n");
        printf("If so, unload them using rmmod, as they conflict with ftd2xx.\n");
        goto exit;
    }
    // get the device information list
    if (numDevs > 0)
    {
        // allocate storage for list based on numDevs
        devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*numDevs);
        // get the device information list
        ftStatus = FT_GetDeviceInfoList(devInfo,&numDevs);
        if (ftStatus == FT_OK)
        {
            for (int i = 0; i < numDevs; i++)
            {
                printf("Dev %d:\n",i);
                printf(" Flags=0x%x\n",devInfo[i].Flags);
                printf(" Type=0x%x\n",devInfo[i].Type);
                printf(" ID=0x%x\n",devInfo[i].ID);
                printf(" LocId=0x%x\n",devInfo[i].LocId);
                printf(" SerialNumber=%s\n",devInfo[i].SerialNumber);
                printf(" Description=%s\n",devInfo[i].Description);
                printf(" ftHandle=0x%lx\n",(unsigned long)devInfo[i].ftHandle);

            }
        }
    }
    free(devInfo);

    ftStatus = FT_ResetDevice(_ftHandle);

    if (ftStatus != FT_OK)
    {
        printf("Failure.  FT_ResetDevice returned %d.\n", (int)ftStatus);
        goto exit;
    }
    ftStatus = FT_SetDataCharacteristics(_ftHandle,
                                         FT_BITS_8,
                                         FT_STOP_BITS_1,
                                         FT_PARITY_NONE);
    if (ftStatus != FT_OK)
    {
        printf("Failure.  FT_SetDataCharacteristics returned %d.\n",(int)ftStatus);
        goto exit;
    }

    ftStatus = getExtendedInfo(_ftHandle);

    setbaud(_baudrate);

exit:
    if (_ftHandle != NULL)
        if (ftStatus != FT_OK) {
            std::cout<<"Device closed"<<std::endl;
            FT_Close(_ftHandle);
        }
    return ftStatus;

}
int usbcomm::receive(std::string &msg,std::string &status, uint32_t nbytes, double timeout){
    FT_STATUS ftStatus = FT_OK;
    std::string readstatus;
    std::string writestatus;
    std::string resp="";

    msg = "";
    status="";

    uint32_t nattemps = 0;
    bool tran_pending = true;
    while(tran_pending){
        _transaction_mtx.lock();
        tran_pending = _transaction_pending;
        _transaction_mtx.unlock();
        nattemps++;
        if (tran_pending){
            if(nattemps>_max_attemps){
                status = "Error. Unable to obtain transaction lock. Maximum number of attemps exceeded";
                return(-1);
            }
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    _transaction_mtx.lock();
    _transaction_pending = true;
    _transaction_mtx.unlock();

    ftStatus = read(msg,readstatus,nbytes,timeout);
    if (readstatus=="timeout"){
        goto exit;
    }
    if (messagevalid(msg)){
        resp = makeresponse("okay");
    }
    else{
        resp = makeresponse("badmessage");
    }
    ftStatus = write(resp,writestatus);


    // while(resp != "okay"){
    //     ftStatus = read(msg,readstatus,nbytes,timeout);
    //     if (readstatus == "okay"){
    //         resp = "okay";
    //         if (msg.length()>0){
    //             ftStatus = write(resp,writestatus);
    //         }
    //     }
    //     else if(readstatus == "timeout"){
    //         status = readstatus;
    //         ftStatus = -1;
    //         break;
    //     }
    //     else{
    //         resp = "fail";
    //         if (msg.length()>0){
    //             ftStatus = write(resp,writestatus);
    //         }
    //         else {
    //             break;
    //         }
    //     }
    // }

exit:
    _transaction_mtx.lock();
    _transaction_pending = false;
    _transaction_mtx.unlock();

    std::cout<<"[receive] readstatus: "<<readstatus<<std::endl;
    std::cout<<"[receive] writestatus: "<<writestatus<<std::endl;

    status = readstatus;
    return(ftStatus);

}

int usbcomm::send(const std::string &msg,std::string &resp, double timeout){
    FT_STATUS ftStatus = FT_OK;
    std::string writestatus;
    std::string readstatus;
    std::string readmsg;
    resp="";

    uint32_t nattemps = 0;
    bool tran_pending = true;
    while(tran_pending){
        _transaction_mtx.lock();
        tran_pending = _transaction_pending;
        _transaction_mtx.unlock();
        nattemps++;
        if (tran_pending){
            if(nattemps>_max_attemps){
                resp = "Error. Unable to obtain transaction lock. Maximum number of attemps exceeded";
                return(-1);
            }
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    _transaction_mtx.lock();
    _transaction_pending = true;
    _transaction_mtx.unlock();

    std::string wmsg = makemessage(msg);
    ftStatus = write(wmsg,writestatus);
    ftStatus = read(resp,readstatus,0,timeout);
    if (readstatus == "timeout")
        resp = readstatus;
    else{
        if(!responsevalid(resp)){
            std::cout<<"[send] invalid response: "<<resp<<std::endl;
        }
    }
    std::cout<<"[send] writestatus: "<<writestatus<<std::endl;
    std::cout<<"[send] readstatus: "<<readstatus<<std::endl;

    // while(resp != "okay"){
    //     ftStatus = write(msg,writestatus);
    //     if (ftStatus!=FT_OK){
    //         resp = writestatus;
    //         break;
    //     }
    //     if (writestatus != "okay"){
    //         resp = "fail";
    //         break;
    //     }
    //     ftStatus = read(resp,readstatus,0,timeout);
    //     if (ftStatus != FT_OK){
    //         if(readstatus == "timeout"){
    //             resp = readstatus;
    //             break;
    //         }
    //         if (resp.length()==0){
    //             resp = "error";
    //             break;
    //         }
    //     }
    //     if ((readstatus =="okay") && (resp != "fail"))
    //         break;
    // }
    _transaction_mtx.lock();
    _transaction_pending = false;
    _transaction_mtx.unlock();

    return(ftStatus);

}

int usbcomm::read(std::string &msg,std::string &status, uint32_t nbytes, double timeout){

    FT_STATUS       ftStatus = FT_OK;
    uint32_t           bytesReceived = 0;
    uint32_t           bytesRead = 0;
    unsigned char  *readBuffer = NULL;
    int             queueChecks = 0;

    // Keep checking queue until D2XX has received all the bytes we wrote
 //		printf("D2XX receive-queue has ");
    bytesReceived = 0;
    uint32_t prev_brx = 0;
    double timewait = 0.0;

    msg = "";

    while (1)
    {
        _dev_mtx.lock();
        ftStatus = FT_GetQueueStatus(_ftHandle, &bytesReceived);
        _dev_mtx.unlock();
        if (ftStatus != FT_OK)
        {
            std::stringstream ss;
            ss << "Failure.  FT_GetQueueStatus returned "<<(int)ftStatus;
            status = ss.str();
            return(ftStatus);
        }

        if ((bytesReceived >= nbytes) && (nbytes>0))
            break;

        if ((nbytes==0) && (bytesReceived>0) && (prev_brx == bytesReceived))
          break;

        if ((timeout >= 0.0) && (timewait>= timeout)){
              ftStatus = -1;
              std::stringstream ss;
              ss<<"timeout";
              status = ss.str();
              return(ftStatus);
         }

        prev_brx = bytesReceived;

        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::this_thread::sleep_for(std::chrono::microseconds(10));

        timewait+=.01;
    }

    free(readBuffer); // Free previous iteration's buffer.

    readBuffer = (unsigned char *)calloc(bytesReceived, sizeof(unsigned char));

    // Then copy D2XX's buffer to ours.
    _dev_mtx.lock();
    ftStatus = FT_Read(_ftHandle, readBuffer, bytesReceived, &bytesRead);
    _dev_mtx.unlock();
    if (ftStatus != FT_OK)
    {
        std::stringstream ss;
        ss << "Failure.  FT_Read returned "<<(int)ftStatus;
        status = ss.str();
        return(ftStatus);
    }

    const char* temprbuf = reinterpret_cast<char*>(readBuffer);
    std::string readstr(temprbuf);
    msg = readstr;

    if (bytesRead != bytesReceived)
    {
        std::stringstream ss;
        ss << "Failure.  FT_Read only read "<<(int)bytesRead <<" of "<<(int)bytesReceived<<" bytes";
        status = ss.str();
        return(ftStatus);
    }


     // Check that queue hasn't gathered any additional unexpected bytes
     bytesReceived = 4242; // deliberately junk
     _dev_mtx.lock();
     ftStatus = FT_GetQueueStatus(_ftHandle, &bytesReceived);
     _dev_mtx.unlock();
     if (ftStatus != FT_OK)
     {
         std::stringstream ss;
         ss << "Failure. Flushin FT_GetQueueStatus returned "<<(int)ftStatus;
         status = ss.str();
         return(ftStatus);
     }

     if (bytesReceived != 0)
     {
         std::stringstream ss;
         ss << "Failure. "<<(int)bytesReceived <<"bytes in input queue -- expected none ";
         status = ss.str();
         return(ftStatus);
     }
     status = "okay";
     return(ftStatus);

}
int usbcomm::write(const std::string &msg, std::string &status)
{
    int retCode = -1; // Assume failure
    unsigned char  *writeBuffer;
    uint32_t           bytesToWrite = 0;
    uint32_t           bytesWritten = 0;
    FT_STATUS       ftStatus = FT_OK;

    bytesToWrite = msg.length();
    writeBuffer = reinterpret_cast<unsigned char*>(const_cast<char*>(msg.c_str()));


    _dev_mtx.lock();
    ftStatus = FT_Write(_ftHandle,writeBuffer,bytesToWrite,&bytesWritten);
    _dev_mtx.unlock();

    if (ftStatus != FT_OK)
    {
        std::stringstream ss;
        ss << "Failure.  FT_Write returned "<<(int)ftStatus;
        status = ss.str();
        return(ftStatus);
    }

    if (bytesWritten != bytesToWrite)
    {
        std::stringstream ss;
        ss << "Failure.  FT_Write only wrote "<<(int)bytesWritten <<" of "<<(int)bytesToWrite<<" bytes";
        status = ss.str();
        return(ftStatus);
    }

    status = "okay";
    return(ftStatus);
}

bool usbcomm::responsevalid(std::string &resp){
    bool valid = false;
    try{
        if(resp.compare(0,4,"RESP")==0){
            valid = true;
            resp = resp.substr(4);
        }
    } catch(const std::exception& e) {
        std::cout<<e.what();
        valid = false;
    }
    return(valid);
}
bool usbcomm::messagevalid(std::string &msg){
    bool valid = false;
    try{
        if(msg.compare(0,4,"MESG")==0){
            valid = true;
            msg = msg.substr(4);
        }
    } catch(const std::exception& e) {
        std::cout<<e.what();
        valid = false;
    }
    return(valid);
}
std::string usbcomm::makeresponse(const std::string & msg){
    std::string resp = "RESP" + msg;
    return(resp);
}
std::string usbcomm::makemessage(const std::string & msg){
    std::string resp = "MESG" + msg;
    return(resp);
}

int usbcomm::setbaud(unsigned long baudrate){
    FT_STATUS       ftStatus = FT_OK;
    _dev_mtx.lock();
    ftStatus = FT_SetBaudRate(_ftHandle, baudrate);
    _dev_mtx.unlock();
    if (ftStatus != FT_OK)
    {
        printf("Failure (ftHandle1).  FT_SetBaudRate(%d) returned %d.\n",(int)baudrate,(int)ftStatus);
        return(ftStatus);
    }
    _baudrate = baudrate;
    return FT_OK;
}

void usbcomm::getinfo(std::string &str){
    uint32_t numDevs;
    FT_DEVICE_LIST_INFO_NODE *devInfo;
    FT_STATUS       ftStatus = FT_OK;
    int             portNum = 0; // First port

    std::stringstream ss;

    ftStatus = FT_CreateDeviceInfoList(&numDevs);

    if ((ftStatus != FT_OK)||(numDevs==0))
    {
        ss << "No Devices found\n";
        str = ss.str();
        return;
    }
    ss << "Found " << numDevs<< "devices\n";

    // get the device information list
    if (numDevs > 0)
    {
        // allocate storage for list based on numDevs
        devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*numDevs);
        // get the device information list
        ftStatus = FT_GetDeviceInfoList(devInfo,&numDevs);
        if (ftStatus == FT_OK)
        {
            for (int i = 0; i < numDevs; i++)
            {
                ss<<"Dev "<<i<<":\n";
                ss<<" Flags="<<devInfo[i].Flags<<"\n";
                ss<<" Type="<<devInfo[i].Type<<"\n";
                ss<<" ID="<<devInfo[i].ID<<"\n";
                ss<<" LocId="<<devInfo[i].LocId<<"\n";
                ss<<" SerialNumber="<<devInfo[i].SerialNumber<<"\n";
                ss<<" Description="<<devInfo[i].Description<<"\n";
                ss<<" ftHandle="<<(unsigned long)devInfo[i].ftHandle<<"\n";

            }
        }
    }
    free(devInfo);

    str = ss.str();
}

FT_STATUS getExtendedInfo(FT_HANDLE ftHandle){

     // Get Program Data
     FT_STATUS ftStatus;
     FT_PROGRAM_DATA ftData;
     char ManufacturerBuf[32];
     char ManufacturerIdBuf[16];
     char DescriptionBuf[32];
     char SerialNumberBuf[16];
     ftData.Signature1 = 0x00000000;
     ftData.Signature2 = 0xffffffff;
     ftData.Version = 0x00000005; // EEPROM structure with FT232H extensions
     ftData.Manufacturer = ManufacturerBuf;
     ftData.ManufacturerId = ManufacturerIdBuf;
     ftData.Description = DescriptionBuf;
     ftData.SerialNumber = SerialNumberBuf;

     ftStatus = FT_EE_Read(ftHandle, &ftData);
     if (ftStatus == FT_OK)
     {
     printf("Signature 1: %08x \n", ftData.Signature1);
     printf("Signature 2: %08x \n", ftData.Signature2);
     printf("Version: %x08 \n", ftData.Version);
     printf("\nManufacturer: ");
     for (int i=0;i<32;i++)
     printf("%c",ftData.Manufacturer[i]);
     printf("\nManufacturer ID: ");
     for (int i=0;i<16;i++)
     printf("%c",ftData.ManufacturerId[i]);
     printf("\nDescription: ");
     for (int i=0;i<32;i++)
     printf("%c",ftData.Description[i]);
     printf("\nSerial NUmber: ");
     for (int i=0;i<16;i++)
     printf("%c",ftData.SerialNumber[i]);
     printf("\n\n");
     // FT_EE_Read OK, data is available in ftData
     }
     else
     {
     // FT_EE_Read FAILED!
     }

     DWORD EEUA_Size;
     ftStatus = FT_EE_UASize(ftHandle, &EEUA_Size);
     if (ftStatus == FT_OK)
     {
         // FT_EE_UASize OK
         // EEUA_Size contains the size, in bytes, of the EEUA
         printf("EEPROM Size: %i \n",EEUA_Size);
     } else
     {
     // FT_EE_UASize
         printf("Get EEPROM UA Size failed");
         return(ftStatus);
     }

     unsigned char eepromBuffer[32];
     DWORD eepromBytesRead;
     ftStatus = FT_EE_UARead(ftHandle, eepromBuffer, 32, &eepromBytesRead);
     if (ftStatus == FT_OK)
     { // FT_EE_UARead OK
     // User Area data stored in Buffer
     // Number of bytes read from EEUA stored in BytesRead
     printf("EEPROM User Area Data: \n");
     dumpBuffer(eepromBuffer, eepromBytesRead);
     }
     else {
          printf("FT_EE_UARead FAILED!");
     }

    return(ftStatus);

}

static void dumpBuffer(unsigned char *buffer, int elements)
{
	int j;

	for (j = 0; j < elements; j++)
	{
		if (j % 8 == 0)
		{
			if (j % 16 == 0)
				printf("\n%p: ", &buffer[j]);
			else
				printf("   "); // Separate two columns of eight bytes
		}
		printf("%02X ", (unsigned int)buffer[j]);
	}
	printf("\n\n");
}
