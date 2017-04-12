/*
 * BtleCommWrapper.cpp
 *
 *  Created on: Mar 9, 2017
 *      Author: Zarnowski
 */

#include "BtleCommWrapperold.h"

#include <functional>
#include <exception>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include "BluetoothGuard.h"
#include "HciWrapper.hpp"
#include "ReadSyncBlock.h"

extern "C" {
    #include "libgatt/att.h"
    #include "libgatt/gatt.h"
    #include "libgatt/bluetooth.h"
    #include "libgatt/gattrib.h"
}

static const char* THREAD_NAME = "BtleCommWrapper";
//bulb BT
//static const char* CHAR_UUID = "0000ffe9-0000-1000-8000-00805f9b34fb";

//HM-10
static const char* CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";

static BtleCommWrapper_old* btleCommWrapper;

static gpointer threadMain(void *data) {
    GMainLoop* loop = static_cast<GMainLoop*>(data);
    g_main_loop_run(loop);
    printf("BTLE thread loop end\n");
    return nullptr;
}

BtleCommWrapper_old::BtleCommWrapper_old()
: eventLoop(g_main_loop_new(nullptr, false)),
  eventLoopThread(g_thread_new(THREAD_NAME, &threadMain, eventLoop)),
  btleChannel(nullptr),
  btleAttribute(nullptr),
  btleValueHandle(0),
  notificationReceived(false),
  state(bcwcsNone),
  notificationBuffer(make_shared<std::vector<uint8_t>>()) {

  BluetoothGuard::lockBluetooth(this);
    if(btleCommWrapper != nullptr) {
        throw std::logic_error("btleCommWrapper is already created!");
    }
    btleCommWrapper = this;
    g_mutex_init(&mutex);
//    printf("CREATED---\n");
}

BtleCommWrapper_old::~BtleCommWrapper_old() {
//  printf("DESTROYING -----\n");
    disconnect();
    g_main_loop_quit(eventLoop);
    g_thread_join(eventLoopThread); //this also do unref inside!
    g_main_loop_unref(eventLoop);
    g_mutex_clear(&mutex);

    btleCommWrapper = nullptr;
    BluetoothGuard::unlockBluetooth(this);
    printf("DESTROYED---\n");
}

void BtleCommWrapper_old::setState(BtleCommWrapperConnectionState state) {
    g_mutex_lock(&mutex);
    this->state = state;
    g_mutex_unlock(&mutex);
}

BtleCommWrapperConnectionState BtleCommWrapper_old::getState() {
  g_mutex_lock(&mutex);
  BtleCommWrapperConnectionState result = state;
  g_mutex_unlock(&mutex);
  return result;
}

bool BtleCommWrapper_old::isConnectingInProgress() {
    g_mutex_lock(&mutex);
    bool tmp = state == bcwcsConnecting;
    g_mutex_unlock(&mutex);
    return tmp;
}

void BtleCommWrapper_old::notificationEventsHandler(const uint8_t *pdu, uint16_t len, gpointer user_data)
{
    GAttrib *attrib = (GAttrib *)user_data;
    uint8_t *opdu;
    uint16_t i, olen = 0;
    size_t plen;

    BtleCommWrapper_old* wrapper = (BtleCommWrapper_old*)user_data;
    //uint16_t handle = att_get_u16(&pdu[1]);

    switch (pdu[0]) {
        case ATT_OP_HANDLE_NOTIFY:
            {
              printf("--> NOTIF\n>>");
                std::lock_guard<std::mutex> guard(wrapper->notificationMutex);
                for (i = 3; i < len; i++) {
                    wrapper->notificationBuffer->push_back(pdu[i]);
                    printf("%c", pdu[i]);
                }
                printf("<</n");
                wrapper->notificationReceived = true;
            }
            break;
        case ATT_OP_HANDLE_IND:
            for (i = 3; i < len; i++) {
                g_info("%02x ", pdu[i]);
            }
            g_info("\n");
            break;
        default:
            g_info("Invalid opcode\n");
            return;
    }

    if (pdu[0] == ATT_OP_HANDLE_NOTIFY)
        return;

    opdu = g_attrib_get_buffer(attrib, &plen);
    olen = enc_confirmation(opdu, plen);

    if (olen > 0)
        g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, NULL);
}

void BtleCommWrapper_old::connectCallback(GIOChannel *io, GError *err, gpointer user_data) {
  printf("%s, %p\n",__func__, io);
    if (err != nullptr) {
        g_warning("BTLE connection failed, reason: %s\n", err->message);
        btleCommWrapper->onConnectionFailed(io, err->code);

    } else {
        //printf("BTLE connection established\n");
        btleCommWrapper->onConnectionSuccess(io);
    }
}

bool BtleCommWrapper_old::isConnected() {
  //TODO: think if this is sufficient?
  g_mutex_lock(&mutex);
  bool result =  (btleAttribute != nullptr) && (btleChannel != nullptr) && (state == bcwcsConnected) &&
      (btleValueHandle != 0);
  g_mutex_unlock(&mutex);
  return result;
}

void BtleCommWrapper_old::onConnectionSuccess(GIOChannel* channel) {
  printf("%s, %p\n",__func__, channel);
    btleChannel = channel;  //store this object
    g_info("Discovering characteristic.\n");
    btleAttribute = g_attrib_new(channel);
    printf("1: %s btleAttribute=%p\n",__func__, btleAttribute);

    bt_uuid_t uuid;
    if (bt_string_to_uuid(&uuid, CHAR_UUID) < 0) {
        g_warning("BTLE Invalid UUID of characteristic.\n");
        setState(bcwcsFailedToConnect);
        return;
    }

    gatt_discover_char(btleAttribute, 0x0001, 0xffff, &uuid, BtleCommWrapper_old::discoverCharacteristicCallback, NULL);
}

void BtleCommWrapper_old::onConnectionFailed(GIOChannel* channel, gint errorCode) {
  printf("%s, %p\n",__func__, channel);
  switch(errorCode) {
    case 16:
      //resource busy error
      HciWrapper::destroyAllConnections();
      break;

    case 130:
      //connection aborted
      //TODO: retry with delay?
      break;

    case ATT_ECODE_TIMEOUT:
      //discovery char. time out
      break;

    default:
      //if any other error then stop process
      setState(bcwcsFailedToConnect);
      break;
  }
}

void BtleCommWrapper_old::discoverCharacteristicCallback(GSList *characteristics, uint8_t status, void *user_data) {
  printf("%s,%d\n",__func__, status);
    if (status != 0) {
        g_warning("Discover all characteristics failed\n");
        btleCommWrapper->onDiscoveryFailed(status);

    } else {
        struct gatt_char *characteristic = (struct gatt_char *) characteristics->data;
        btleCommWrapper->onDiscoverySuccess( characteristic->value_handle );
    }
}

void BtleCommWrapper_old::onDiscoverySuccess(uint16_t handle) {
  printf("%s,%d\n",__func__,handle);
    btleCommWrapper->btleValueHandle = handle;

    g_attrib_register(btleAttribute, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
            BtleCommWrapper_old::notificationEventsHandler, this, NULL);
    g_attrib_register(btleAttribute, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
            BtleCommWrapper_old::notificationEventsHandler, this, NULL);

    setState(bcwcsConnected);
}

void BtleCommWrapper_old::onDiscoveryFailed(gint errorCode) {
  printf("%s\n",__func__);
    btleCommWrapper->btleValueHandle = 0;

    //release attribute
//    g_attrib_unref(btleAttribute);
//    btleAttribute = nullptr;

    //release channel this will also set connectingInProgress to false
    onConnectionFailed(btleChannel, errorCode);
}

bool BtleCommWrapper_old::innerConnectTo(const string& address, int timeoutInMs) {
  int attempt = 0;
  gint64 startTime = g_get_monotonic_time();
  while(true) {
    attempt ++;
    printf("Connection attempt: %d\n", attempt);
    GError* error = nullptr;
    //NOTE: this channel will be stored to btleChannel in callback on success connection. Otherwise it will be destroyed
    //in onConnectionFailed, there is no need to assign gatt_connect to field btleChannel
    GIOChannel* tmp = gatt_connect("hci0", address.c_str(), "", "low", 0, 0, BtleCommWrapper_old::connectCallback, &error, NULL);
    if (tmp == nullptr) {
      g_warning("Failed to connect with error: %s", error->message);
      int err = error->code;
      g_error_free(error);

      switch(err) {
        case 16:
          HciWrapper::restartBTLE();
          usleep(3 * 1000000);
          break;

        case 130:
          //operation aborted, just wait and restart
          usleep(3 * 1000000);
          break;

        case 148:
          //Host is unreachable
          printf("--\n");
          return false;
      }

    } else {
      return true;
    }

    gint64 delta = g_get_monotonic_time() - startTime;
    if (delta > timeoutInMs) {
      printf("InnerConnect timeout\n");
      return false;
    }
  }

  return false;
}

bool BtleCommWrapper_old::connectTo(const string& address, int timeoutInMs) {
  if (isConnectingInProgress() == true) {
    g_warning("Already in connecting state\n");
    return false;
  }
  setState(bcwcsConnecting);

  if (innerConnectTo(address, timeoutInMs) == false) {
    setState(bcwcsFailedToConnect);
    return false;
  }

  //wait until we connect
  g_info("Waiting for connection");
  while (timeoutInMs > 0) {
    if (isConnectingInProgress() == false) {
      break;
    }
    usleep(30 * 1000);
    timeoutInMs -= 30;
  }

  if (timeoutInMs <= 0 || getState() == bcwcsFailedToConnect) {
    g_warning("Unable to connect to %s", address.c_str());
    disconnect();
  }

  return btleValueHandle != 0;
}

void BtleCommWrapper_old::disconnect() {
  //printf("1: %s %p\n",__func__, btleAttribute );
    if (btleAttribute != nullptr) {
        g_attrib_unref(btleAttribute);
        printf("1: %s btleAttribute=null\n",__func__);
        btleAttribute = nullptr;
    }
    //printf("2: %s %p\n",__func__, btleChannel );
    if (btleChannel != nullptr) {
        GError* error = nullptr;
        g_io_channel_shutdown(btleChannel, true, &error);
        if (error != nullptr) {
          g_warning("Failed to shutdown channel with error: %s", error->message);
          g_error_free(error);
        }
        g_io_channel_unref(btleChannel);
        btleChannel = nullptr;
    }
    setState(bcwcsNone);
    //printf("3: %s\n",__func__);
}

void BtleCommWrapper_old::writeValueCallback(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data) {
    ReadSyncBlock* result = static_cast<ReadSyncBlock*>(user_data);

    result->resultSize = status;

    if (status != 0) {
        g_warning("Characteristic Write Request failed: %s\n", att_ecode2str(status));
        result->setReady();
        return;
    }

    if (!dec_write_resp(pdu, plen) && !dec_exec_write_resp(pdu, plen)) {
        g_warning("Protocol error [write]\n");
        result->resultSize = 1;
        result->setReady();
        return;
    }

    result->setReady();
}

bool BtleCommWrapper_old::send(const string& dataToSend, int timeoutInMs) {
    if (isConnected() == false)  {
        g_warning("Not connected!");
        return false;
    }

    bool result = false;
    gsize plen =  dataToSend.length();
    uint8_t *value = (uint8_t *)g_try_malloc0(plen+1);
    strcpy((char*)value, dataToSend.c_str());
    if (plen != 0) {
        ReadSyncBlock data(&this->mutex);

        gatt_write_char(btleAttribute, btleValueHandle, value, plen, BtleCommWrapper_old::writeValueCallback, &data);
        const int sleepTimeMs = 30;
        while (true) {
            if (data.isReady() == true || timeoutInMs <= 0) {
                break;
            }
            usleep(sleepTimeMs * 1000);
            timeoutInMs -= sleepTimeMs;
        }
        g_free(value);
        result = data.resultSize == 0;
    }
    return result;
}

void BtleCommWrapper_old::readValueCallback(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data) {
    ReadSyncBlock* result = static_cast<ReadSyncBlock*>(user_data);

    if (plen <= 0 || status != 0) {
        g_warning("Characteristic value/descriptor read failed: %s\n", att_ecode2str(status));
        result->setReady();
        return;
    }

    uint8_t* value = new uint8_t[plen];
    size_t vlen = sizeof(value);
    result->resultSize = dec_read_resp(pdu, plen, value, vlen);
    result->result = value; //we taking ownership of this memory

    if (vlen <= 0) {
        g_warning("Protocol error, vLen <= 0\n");
    }
    result->setReady();
}

bool isItEnter(int i) {
  return i == 13;
}

string BtleCommWrapper_old::readLineFromNotificationBuffer(int timeoutInMs) {
  string result = "";
  const int sleepTimeMs = 30;
  while (timeoutInMs > 0) {
    usleep(sleepTimeMs * 1000);
    timeoutInMs -= sleepTimeMs;
    { //critical section
      std::lock_guard<std::mutex> guard(notificationMutex);
      if (notificationReceived == false) {
        continue;
      }
      std::vector<uint8_t>::iterator it = std::find_if(notificationBuffer->begin(), notificationBuffer->end(),
          isItEnter);
      if (it == notificationBuffer->end()) {
        continue;
      }
      it++;
      uint8_t* startAddress = &((*notificationBuffer)[0]);
      size_t size = it - notificationBuffer->begin();
      while (startAddress[size - 1] == '\r') {
        size--;
      }
      if (size > 0) {
        result.append((const char*) startAddress, size);
      }
      notificationBuffer->erase(notificationBuffer->begin(), it);
      break;
    }
  }
  return result;
}

string BtleCommWrapper_old::readLine(int timeoutInMs, bool useNotifications) {
  if (isConnected() == false) {
    printf("readLine: not connected, ignored!");
    return "";
  }
  if (useNotifications == true) {
    return readLineFromNotificationBuffer(timeoutInMs);
  }
  ReadSyncBlock data(&this->mutex);

  //TODO: How to cancel this process in case of timeout?
  gatt_read_char(btleAttribute, btleValueHandle, BtleCommWrapper_old::readValueCallback, &data);
  const int sleepTimeMs = 30;
  while (true) {
    if (data.isReady() == true || timeoutInMs <= 0) {
      break;
    }
    usleep(sleepTimeMs * 1000);
    timeoutInMs -= sleepTimeMs;
  }
  return data.resultSize == 0 ? "" : string((char*) data.result, 0, data.resultSize);
}
