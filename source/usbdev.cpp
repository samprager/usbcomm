//
//  usbdev.cpp
//
//
//  Created by Sam Prager on 6/5/2018.
//
// Class wrapper for FD2XX functions for host-to-host USB communication vs RS232 TTL
//

#include "usbdev.hpp"

FT_STATUS getExtendedInfo(FT_HANDLE ftHandle);
static void dumpBuffer(unsigned char *buffer, int elements);

usbdev::usbdev(uint32_t port) : _rthread(), _lwthread(), _rxbuf(new struct rx_buf){
    int err = init(port);

    if (err){
        std::cout<<"Init failed"<<std::endl;
        _dev_ready = false;
    }else{
        _dev_ready = true;
    }
}

usbdev::~usbdev(){
    stop_listening();
    if (_rthread) _rthread.reset();
    if (_ftHandle != NULL){
        _dev_mtx.lock();
        FT_Close(_ftHandle);
        _dev_mtx.unlock();
    }
}

int usbdev::reset(uint32_t port){
    std::cout<<"Resetting Device..."<<std::endl;
    if (ready()) stop_listening();
    if (_rthread) _rthread.reset();
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
bool usbdev::ready(){
    return _dev_ready;
}

int usbdev::init(uint32_t port){
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

int usbdev::read(std::string &msg, uint32_t nbytes, uint32_t max_checks){
    FT_STATUS       ftStatus = FT_OK;
    uint32_t           bytesReceived = 0;
    uint32_t           bytesRead = 0;
    unsigned char  *readBuffer = NULL;
    int             queueChecks = 0;

    // Keep checking queue until D2XX has received all the bytes we wrote
 //		printf("D2XX receive-queue has ");
    bytesReceived = 0;
    uint32_t prev_brx = 0;
    for (queueChecks = 0; queueChecks < max_checks; queueChecks++)
    {
        if (queueChecks % 128 == 0)
        {
            // Periodically display number of bytes D2XX has received
            printf("%s%d",queueChecks == 0 ? " " : ", ",(int)bytesReceived);
        }
        _dev_mtx.lock();
        ftStatus = FT_GetQueueStatus(_ftHandle, &bytesReceived);
        _dev_mtx.unlock();
        if (ftStatus != FT_OK)
        {
            printf("\nFailure.  FT_GetQueueStatus returned %d\n",(int)ftStatus);
            return(ftStatus);
        }

        if (bytesReceived >= nbytes)
            break;

        // if ((bytesReceived >= nbytes) && (nbytes>0))
        //     break;
        //
        // if ((nbytes==0) && (bytesReceived>0) && (prev_brx == bytesReceived))
        //   break;

        prev_brx = bytesReceived;
    }
    printf("\nDev has %d bytes in queue\n", (int)bytesReceived);

    // Even if D2XX has the wrong number of bytes, create our
    // own buffer so we can read and display them.

    free(readBuffer); // Free previous iteration's buffer.

    readBuffer = (unsigned char *)calloc(bytesReceived, sizeof(unsigned char));

    if (readBuffer == NULL)
    {
        printf("Failed to allocate %d bytes\n", bytesReceived);
        return(ftStatus);
    }
    // Then copy D2XX's buffer to ours.
    _dev_mtx.lock();
    ftStatus = FT_Read(_ftHandle, readBuffer, bytesReceived, &bytesRead);
    _dev_mtx.unlock();
    if (ftStatus != FT_OK)
    {
        printf("Failure.  FT_Read returned %d\n", (int)ftStatus);
        return(ftStatus);
    }

    if (bytesRead != bytesReceived)
    {
        printf("Failure.  FT_Read only read %d (of %d) bytes\n",(int)bytesRead,(int)bytesReceived);
    }


    const char* temprbuf = reinterpret_cast<char*>(readBuffer);
    std::string readstr(temprbuf);
    std::cout<<"\ndev read: "<<readstr<<std::endl;
    dumpBuffer(readBuffer, bytesReceived);

    msg = readstr;

     // Check that queue hasn't gathered any additional unexpected bytes
     bytesReceived = 4242; // deliberately junk
     _dev_mtx.lock();
     ftStatus = FT_GetQueueStatus(_ftHandle, &bytesReceived);
     _dev_mtx.unlock();
     if (ftStatus != FT_OK)
     {
         printf("Failure.  FT_GetQueueStatus returned %d\n",(int)ftStatus);
         return(ftStatus);
     }

     if (bytesReceived != 0)
     {
         printf("Failure.  %d bytes in input queue -- expected none\n",(int)bytesReceived);
         return(ftStatus);
     }
     return(ftStatus);
}

int usbdev::write(const std::string &msg, bool wait_resp)
{
    int retCode = -1; // Assume failure
    unsigned char  *writeBuffer;
    uint32_t           bytesToWrite = 0;
    uint32_t           bytesWritten = 0;
    FT_STATUS       ftStatus = FT_OK;

    bytesToWrite = msg.length();
    writeBuffer = reinterpret_cast<unsigned char*>(const_cast<char*>(msg.c_str()));

    // starting a new write transaction and expect a response
    if (wait_resp){

        int attempts = 0;
        bool tran_pending = false;
        while(attempts<_max_attemps){
            _transaction_mtx.lock();
            tran_pending = _transaction_pending;
            _transaction_mtx.unlock();
            if(!tran_pending)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        if (tran_pending){
            std::cout<<"[write] Error: Unable to obtain transaction lock" <<std::endl;
            return(-1);
        }
        _transaction_mtx.lock();
        _transaction_pending = true;
        _transaction_mtx.unlock();
    }
    _dev_mtx.lock();
    ftStatus = FT_Write(_ftHandle,writeBuffer,bytesToWrite,&bytesWritten);
    _dev_mtx.unlock();
    if (ftStatus != FT_OK)
    {
        printf("Failure.  FT_Write returned %d\n", (int)ftStatus);
        if (wait_resp){
          _transaction_mtx.lock();
          _transaction_pending = false;
          _transaction_mtx.unlock();
        }
        return(ftStatus);
    }

    if (bytesWritten != bytesToWrite)
    {

        printf("Failure.  FT_Write wrote %d bytes instead of %d.\n",(int)bytesWritten,(int)bytesToWrite);
        if (wait_resp){
          _transaction_mtx.lock();
          _transaction_pending = false;
          _transaction_mtx.unlock();
        }
        return(ftStatus);
    }

    const char* tempwbuf = reinterpret_cast<char*>(writeBuffer);
    std::string writestr(tempwbuf);
    std::cout<<"\ndev wrote: "<<writestr<<std::endl;
    dumpBuffer(writeBuffer, bytesToWrite);

    // if we need to wait for a response/handshake
    if (wait_resp){
        bool keep_listening;

        // save and set _stop_listening to false so that listen thread actually executes
        _listen_mtx.lock();
        keep_listening = ((!_stop_listening) and _listening);
        _listen_mtx.unlock();

        // temporarily stop listener thread
        stop_listening();

        _listen_mtx.lock();
        _stop_listening = false;
        _listen_mtx.unlock();

        listen(std::bind(&usbdev::resp_callback, this,std::placeholders::_1),_default_response.length(),100);

        // listen(resp_callback, _default_response.length(),1,1000);
        // restart listener thread if it was already running
        if(keep_listening)
            start_listening();
    }

    return(ftStatus);
}
// use negative timeout to run indefinitely. timeout is in milliseconds
int usbdev::listen(std::function<int(uint32_t)> callback, uint32_t nbytes, double timeout)
{
    FT_STATUS       ftStatus = FT_OK;
    uint32_t           bytesReceived = 0;
    uint32_t           bytesRead = 0;
    unsigned char  *readBuffer = NULL;
    int             queueChecks = 0;

    // Keep checking queue until D2XX has received all the bytes we wrote
    //		printf("D2XX receive-queue has ");
    bytesReceived = 0;
    queueChecks = 0;

    _listen_mtx.lock();
    _stop_listening = false;
    _listening = true;
    _listen_mtx.unlock();

    bool stop_listen = false;

    double timewait = 0.0;

    uint32_t prev_brx = 0;
    while(!stop_listen)
    {
        if (queueChecks % 128 == 0)
        {
            // Periodically display number of bytes D2XX has received
            printf("%s%d",queueChecks == 0 ? " " : ", ",(int)bytesReceived);
        }
        _dev_mtx.lock();
        ftStatus = FT_GetQueueStatus(_ftHandle, &bytesReceived);
        _dev_mtx.unlock();
        if (ftStatus != FT_OK)
        {
            printf("\nFailure.  FT_GetQueueStatus returned %d\n",(int)ftStatus);
            break;
        }

        if ((bytesReceived >= nbytes) && (nbytes>0))
            break;

        if ((nbytes==0) && (bytesReceived>0) && (prev_brx == bytesReceived))
          break;

        prev_brx = bytesReceived;

        if ((timeout >= 0.0) && (timewait>= timeout)){
            ftStatus = -1;
            std::cout<<"\n[listener] Error: Timeout"<<std::endl;
            break;
        }

        queueChecks++;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        timewait += 10.0;

        _listen_mtx.lock();
        stop_listen = _stop_listening;
        _listen_mtx.unlock();
    }
    _listen_mtx.lock();
    _listening = false;
    _listen_mtx.unlock();

    if ((ftStatus == FT_OK)&&(!stop_listen)){
        printf("\nDev has %d bytes in queue\n", (int)bytesReceived);
        if(bytesReceived>=nbytes){
            ftStatus = callback(bytesReceived);
            return ftStatus;
          }
    }
    _transaction_mtx.lock();
    _transaction_pending = false;
    _transaction_mtx.unlock();

    return(ftStatus);
}

bool usbdev::listening(){
    bool islistening;
    _listen_mtx.lock();
    islistening = _listening;
    _listen_mtx.unlock();
    return (islistening);
}

void usbdev::stop_listening(){
    _listen_mtx.lock();
    _stop_listening = true;
    _listen_mtx.unlock();
    if(_rthread){
        if(_rthread->joinable())
            _rthread->join();
    }
}

void usbdev::start_listening(){
    if(!ready())
        return;

    bool islistening;
    _listen_mtx.lock();
    if(!_listening)
        _start_listening = true;
    _stop_listening = false;
    _listen_mtx.unlock();
    if (!_lwthread){
        _lwthread.reset(new std::thread(&usbdev::listen_watchdog, this));
        std::cout<<"[start_listening] Adding listener watchdog thread"<<std::endl;
    }
    // _rthread->detach();
}

void usbdev::listen_watchdog(){
    bool islistening = false;
    bool start_request = false;
    while (1){
        _listen_mtx.lock();
        if(!_stop_listening){
            start_request = _start_listening;
            islistening = _listening;
            if(_start_listening) _start_listening = false;
        }
        _listen_mtx.unlock();
        if(start_request){
            start_listening_thread();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void usbdev::start_listening_thread(){
    bool islistening;
    _listen_mtx.lock();
    islistening = _listening;
    _stop_listening = false;
    _listen_mtx.unlock();
    if (islistening){
        std::cout<<"[start_listening] listener thread already active"<<std::endl;
        return;
    }
    if(_rthread){
        if(_rthread->joinable()) {
            std::cout<<"[start_listening] joining old listener thread"<<std::endl;
            _rthread->join();
        }
    }
    _rthread.reset(new std::thread(&usbdev::listen, this, std::bind(&usbdev::listener_callback,this,std::placeholders::_1), 0,-1.0));
    std::cout<<"[start_listening_thread] Adding listener thread"<<std::endl;
    // _rthread->detach();
}

int usbdev::listener_callback(uint32_t bytesReceived){
    FT_STATUS       ftStatus = FT_OK;

    std::cout<<"Listener Callback Called: "<<bytesReceived<<" in read queue"<<std::endl;

    // starting a new read transaction and send a response
    int attempts = 0;
    bool tran_pending = true;
    while(attempts<_max_attemps){
        _transaction_mtx.lock();
        tran_pending = _transaction_pending;
        _transaction_mtx.unlock();
        if(!tran_pending)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        attempts++;
    }
    if (tran_pending){
        std::cout<<"[listener_callback] Error: Unable to obtain transaction lock" <<std::endl;
        return(-1);
    }
    _transaction_mtx.lock();
    _transaction_pending = true;
    _transaction_mtx.unlock();

    std::string msg;
    ftStatus= read(msg,bytesReceived);

    _rxbuf->mtx.lock();
    _rxbuf->data.push(msg);
    _rxbuf->mtx.unlock();

    std::cout<<"Read: " <<msg<<"("<<_rxbuf->data.front()<<")"<<std::endl;
    _rxbuf->data.pop();

    std::string resp = _default_response;

    ftStatus=write(resp, false);

    _transaction_mtx.lock();
    _transaction_pending = false;
    _transaction_mtx.unlock();

    _listen_mtx.lock();
    bool stop_listen = _stop_listening;
    _listen_mtx.unlock();
    if(!stop_listen){
        start_listening();
    }
    _listen_mtx.unlock();

    return(ftStatus);
}

int usbdev::resp_callback(uint32_t bytesReceived){
    FT_STATUS       ftStatus = FT_OK;

    std::cout<<"Response Callback Called: "<<bytesReceived<<" in read queue"<<std::endl;

    std::string resp;

    ftStatus= read(resp,bytesReceived);

    if(ftStatus == FT_OK)
        std::cout<<"Response Received: " <<resp<<" ("<<bytesReceived<<" bytes)"<<std::endl;
    else
        std::cout<<"[resp_callback]: Error. read() returned "<<ftStatus<<std::endl;

    _transaction_mtx.lock();
    _transaction_pending = false;
    _transaction_mtx.unlock();

    return(ftStatus);
}

int usbdev::setbaud(unsigned long baudrate){
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

void usbdev::getinfo(std::string &str){
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
