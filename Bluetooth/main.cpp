#include <stdlib.h>
#include <errno.h>
#include <curses.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include "HciWrapper.hpp"
extern "C" {
    #include "libgatt/gatt.h"
    #include "libgatt/bluetooth.h"
    #include "libgatt/gattrib.h"
}
#include "BtleCommWrapperold.h"
#include "BtleCommWrapper.h"

#define HCI_STATE_NONE       0
#define HCI_STATE_OPEN       2
#define HCI_STATE_SCANNING   3
#define HCI_STATE_FILTERING  4

struct hci_state {
  int device_id;
  int device_handle;
  struct hci_filter original_filter;
  int state;
  int has_error;
  char error_message[1024];
} hci_state;

BTLEDevice deviceToUse("", "");

class HCITest : public HciWrapperListener {
    public:
        virtual ~HCITest();
        virtual void onScanStart() override;
        virtual void onScanStop() override;
        virtual void onNewDeviceFound(const BTLEDevice& device) override;
};

HCITest::~HCITest() {

}

void HCITest::onScanStart() {
    printf("------------- START SCAN\n");
}
void HCITest::onScanStop() {
    printf("------------- STOP SCAN\n");
}
void HCITest::onNewDeviceFound(const BTLEDevice& device) {
    printf("------------- Device address:%s, name:%s\n", device.address.c_str(), device.name.c_str());
}


void btleCommunicationTest() {
    BtleCommWrapper_old* comm = new BtleCommWrapper_old();
    //BTBulb
//    if (comm->connectTo("D0:B5:C2:B2:1A:E1") == true) {
//        comm->send("56FFFF0000F0AA");
//       // string s = comm->readLine(1000);
//       // printf("read: %s\n", s.c_str());
//        usleep(2000000);
//        comm->disconnect();
//    }

    //HM-10
    if (comm->connectTo("5C:F8:21:F9:93:74", 3000) == true) {    //"5C:F8:21:F9:80:BD"
        char buf[15];
        //sprintf(buf, "CTM0002%c",13);
        //string ss(buf);
        //comm->send(ss);
        //sprintf(buf, "!!!CPM02%c",13);
        sprintf(buf, "RTH0000%c",13);
        string ss(buf);
        printf("SEND:%s\n", ss.c_str());
        comm->send(ss);
//            string s = comm->readLine(4000);
//            printf("%d:read: %s\n", t, s.c_str());
        usleep(20000000);
        comm->disconnect();
    }

    printf("done!");
    delete comm;
}

void hciWrapperTests() {
    //Wrapper tests
    HCITest hciTest;
    HciWrapper* hciWrapper = new HciWrapper(hciTest);
    if (hciWrapper->startScan() == false) {
        hciWrapper->dumpError();
    }
    hciWrapper->scanLoop();
    hciWrapper->stopScan();
    vector<BTLEDevice> devices = hciWrapper->getFoundDevices();
    for(auto iter = devices.begin(); iter != devices.end(); iter ++) {
        printf("device: %s, address:%s\n", iter->name.c_str(), iter->address.c_str());
        if (iter->name == "HMSoft") {
            deviceToUse = *iter;
        }
    }
    delete hciWrapper;
}

void btleCommunicationTest2() {
  BtleCommWrapper_old* comm = new BtleCommWrapper_old();

  while (true) {
    //HM-10
    if (comm->connectTo(deviceToUse.address, 4000) == true) {    //"5C:F8:21:F9:80:BD"
      printf("SEND request\n");
      comm->send("!!!!#RTH1\r");
      string resp = comm->readLine(3000, true);
      printf("resp: %s\n", resp.c_str());
      fflush(stdout);
      //            string s = comm->readLine(4000);
      //            printf("%d:read: %s\n", t, s.c_str());
      usleep(5000000);
      comm->disconnect();
    }
    printf("Waiting...\n");
    usleep(5000000);
  }
  printf("done!");
  delete comm;
}

void btleCommunicationTest3() {
  BtleCommWrapper* comm = new BtleCommWrapper();

  while (true) {
    //HM-10
    if (comm->connectTo("5C:F8:21:F9:80:BD"/*deviceToUse.address*/, 4000) == true) {    //"5C:F8:21:F9:80:BD"
      printf("SEND request\n");
      comm->send("!!!!#RTH1\r");
      string resp = comm->readLine(3000);
      printf("resp: %s\n", resp.c_str());
      fflush(stdout);
      comm->disconnect();
    }
    printf("Waiting...\n");
    usleep(5000000);
  }
  printf("done!");
  delete comm;
}

int main(void) {
    hciWrapperTests();
//    btleCommunicationTest2();
    btleCommunicationTest3();
    return 0;
}
