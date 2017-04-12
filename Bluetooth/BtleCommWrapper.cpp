/*
 * BtleCom.cpp
 *
 *  Created on: Apr 10, 2017
 *      Author: Zarnowski
 */

#include "BtleCommWrapper.h"

#include <functional>
#include <exception>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include "BluetoothGuard.h"
#include "HciWrapper.hpp"
extern "C" {
  #include "libgatt/att.h"
  #include "libgatt/gatt.h"
  #include "libgatt/bluetooth.h"
  #include "libgatt/gattrib.h"
}
#include "ReadSyncBlock.h"

//HM-10
static const char* CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";

static const char* THREAD_NAME = "BtleCom";

//This class is NOT owning any of fields
class BtleCallbackData {
  public:
    BtleCommWrapper* btleCom;
    GIOChannel* btleChannel;
    GAttrib* btleAttribute;
    guint16 btleValueHandle;
    BtleCallbackData(BtleCommWrapper* btleCom, GIOChannel* btleChannel = nullptr, GAttrib* btleAttribute = nullptr,
        guint16 btleValueHandle = 0)
    : btleCom(btleCom), btleChannel(btleChannel), btleAttribute(btleAttribute), btleValueHandle(btleValueHandle) {

    }
};

static gpointer threadMain(void *data) {
  GMainLoop* loop = static_cast<GMainLoop*>(data);
  printf("BTLE thread start\n");
  g_main_loop_run(loop);
  printf("BTLE thread loop end\n");
  return nullptr;
}

static bool isItEnter(int i) {
  return i == 13;
}

BtleCommWrapper::BtleCommWrapper()
: state(cssNone),
  eventLoop(g_main_loop_new(nullptr, false)),
  eventLoopThread(g_thread_new(THREAD_NAME, &threadMain, eventLoop)),
  btleChannel(nullptr),
  btleAttribute(nullptr),
  btleValueHandle(0),
  notificationReceived(false),
  btleError(0),
  notificationBuffer(make_shared<std::vector<uint8_t>>()) {

  g_mutex_init(&mutex);
  BluetoothGuard::lockBluetooth(this);
}

BtleCommWrapper::~BtleCommWrapper() {
  disconnect();
  g_main_loop_quit(eventLoop);
  g_thread_join(eventLoopThread); //this also do unref inside!
  g_main_loop_unref(eventLoop);
  printf("BTLE Destroyed\n");
  BluetoothGuard::unlockBluetooth(this);
}

bool BtleCommWrapper::isConnectingInProgress() {
  g_mutex_lock(&mutex);
  bool tmp = (state != cssNone) && (state != cssConnect) && (state != cssFailedToConnect);
  g_mutex_unlock(&mutex);
  return tmp;
}

void BtleCommWrapper::setBtleError(int error) {
  g_mutex_lock(&mutex);
  this->btleError = error;
  g_mutex_unlock(&mutex);
}

bool BtleCommWrapper::isBtleError() {
  g_mutex_lock(&mutex);
  bool result = btleError != 0;
  g_mutex_unlock(&mutex);
  return result;
}

void BtleCommWrapper::setState(ConnectionStatusState state) {
  g_mutex_lock(&mutex);
  this->state = state;
  g_mutex_unlock(&mutex);
}

ConnectionStatusState BtleCommWrapper::getState() {
  g_mutex_lock(&mutex);
  ConnectionStatusState result = state;
  g_mutex_unlock(&mutex);
  return result;
}

bool BtleCommWrapper::isConnected() {
  //TODO: think if this is sufficient?
  g_mutex_lock(&mutex);
  bool result = (btleAttribute != nullptr) && (btleChannel != nullptr) && (state == cssConnectionEstablished)
      && (btleValueHandle != 0);
  g_mutex_unlock(&mutex);
  return result;
}

NextAction BtleCommWrapper::handleBtleError() {
  g_mutex_lock(&mutex);
  int errorCode = btleError;
  btleError = 0;
  g_mutex_unlock(&mutex);

  NextAction result = naContinue;

  printf("%s: error %d\n", __func__, errorCode);

  switch (errorCode) {
    case 0:
      //no error
      result = naContinue;
      break;

    case 16:
      //resource busy
      //Since we are going to destroy whole BTLE, fallback to connect state
      deleteBtleAttrib();
      deleteBtleChannel();
      setState(cssNone);

      HciWrapper::restartBTLE();
      usleep(3 * 1000000);
      result = naRepeat;
      break;

    case 130:
      //operation aborted, just wait and restart
      usleep(3 * 1000000);
      result = naRepeat;
      break;

    case 148:
      //Host is unreachable
      result = naFatal;
      break;

    default:
      result = naRepeat;
      usleep(1 * 1000000);
      break;
  }
  printf("%s: nextAction:%d\n", __func__, result);
  return result;
}

void BtleCommWrapper::deleteBtleAttrib() {
  g_mutex_lock(&mutex);
  if (btleAttribute != nullptr) {
    g_attrib_unref(btleAttribute);
    printf("1: %s btleAttribute=null\n", __func__);
    btleAttribute = nullptr;
  }
  g_mutex_unlock(&mutex);
}

void BtleCommWrapper::deleteBtleChannel() {
  g_mutex_lock(&mutex);
  if (btleChannel != nullptr) {
    GError* error = nullptr;
    g_io_channel_shutdown(btleChannel, true, &error);
    if (error != nullptr) {
      g_warning("Failed to shutdown channel with error: %s", error->message);
      g_error_free(error);
    }
    g_io_channel_unref(btleChannel);
    btleChannel = nullptr;
    printf("1: %s btleChannel=null\n", __func__);
  }
  g_mutex_unlock(&mutex);
}

void BtleCommWrapper::connectCallback(GIOChannel *io, GError *err, gpointer user_data) {
  printf("%s, %p\n", __func__, io);
  BtleCommWrapper* btleCom = static_cast<BtleCommWrapper*>(user_data);
  if (btleCom == nullptr) {
    //uuu bad!
    printf("%s: inner bad!\n", __func__);
    return;
  }

  if (err != nullptr) {
    g_warning("BTLE connection failed, reason: %s\n", err->message);

    if (io == btleCom->btleChannel) {
      btleCom->deleteBtleChannel();
    }

    btleCom->setBtleError(err->code);
    btleCom->setState(cssConnect);

  } else {
    printf("BTLE connected to host\n");
    btleCom->setState(cssDiscover);
  }
}

void BtleCommWrapper::discoverCharacteristicCallback(GSList *characteristics, uint8_t status, void *user_data) {
  printf("%s,%d\n", __func__, status);
  BtleCallbackData* callbackData = static_cast<BtleCallbackData*>(user_data);

  if (callbackData->btleCom == nullptr) {
    //uuu bad!
    printf("%s: inner bad!\n", __func__);
    return;
  }

  g_mutex_lock(&callbackData->btleCom->mutex);
  if ((callbackData->btleAttribute != callbackData->btleCom->btleAttribute) ||
      (callbackData->btleChannel != callbackData->btleCom->btleChannel)) {
    //Zombie
    printf("%s: Zombie callback, ignored!\n", __func__);
    g_mutex_unlock(&callbackData->btleCom->mutex);
    delete callbackData;
    return;
  }
  g_mutex_unlock(&callbackData->btleCom->mutex);

  if (status != 0) {
    g_warning("Discover all characteristics failed\n");
    callbackData->btleCom->setBtleError(status);
    callbackData->btleCom->setState(cssDiscover);

  } else {
    struct gatt_char *characteristic = (struct gatt_char *) characteristics->data;
    g_mutex_lock(&callbackData->btleCom->mutex);
    callbackData->btleCom->btleValueHandle = characteristic->value_handle;

    g_attrib_register(callbackData->btleAttribute, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
        BtleCommWrapper::notificationEventsHandler, callbackData->btleCom, NULL);
    g_attrib_register(callbackData->btleAttribute, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
        BtleCommWrapper::notificationEventsHandler, callbackData->btleCom, NULL);
    g_mutex_unlock(&callbackData->btleCom->mutex);

    callbackData->btleCom->setState(cssConnectionEstablished);
  }

  delete callbackData;
}

void BtleCommWrapper::notificationEventsHandler(const uint8_t *pdu, uint16_t len, gpointer user_data) {
  GAttrib *attrib = (GAttrib *) user_data;
  uint8_t *opdu;
  uint16_t i, olen = 0;
  size_t plen;

  BtleCommWrapper* wrapper = (BtleCommWrapper*) user_data;
  //uint16_t handle = att_get_u16(&pdu[1]);

  switch (pdu[0]) {
    case ATT_OP_HANDLE_NOTIFY: {
      std::lock_guard<std::mutex> guard(wrapper->notificationMutex);
      for (i = 3; i < len; i++) {
        wrapper->notificationBuffer->push_back(pdu[i]);
//        printf("%c", pdu[i]);
      }
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

  if (pdu[0] == ATT_OP_HANDLE_NOTIFY) {
    return;
  }

  opdu = g_attrib_get_buffer(attrib, &plen);
  olen = enc_confirmation(opdu, plen);

  if (olen > 0) {
    g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, NULL);
  }
}

void BtleCommWrapper::writeValueCallback(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data) {
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

gboolean BtleCommWrapper::channelWatch(GIOChannel* source, GIOCondition condition, gpointer data) {
  printf("In channel error -> disconnected state\n");
  BtleCommWrapper* btleCom = static_cast<BtleCommWrapper*>(data);
  btleCom->disconnect();
  return false;
}

void BtleCommWrapper::executeConnect(const string& address) {
  printf("%s\n",__func__);

  GError* error = nullptr;
  gpointer data = static_cast<gpointer>(this);

  btleChannel = gatt_connect("hci0", address.c_str(), "", "low", 0, 0, BtleCommWrapper::connectCallback, &error, data);
  if (btleChannel == nullptr) {
    g_warning("Failed to connect with error: %s", error->message);
    setBtleError(error->code);
    g_error_free(error);
    setState(cssNone);

  } else {
    //watch this channel
    g_io_add_watch(btleChannel, static_cast<GIOCondition>(G_IO_ERR | G_IO_HUP), BtleCommWrapper::channelWatch, this);
    setBtleError(0);
    setState(cssConnect);
  }
}

void BtleCommWrapper::executeDiscovery(int timeoutInMs, gint64 startTime) {
  g_info("Discovering characteristic.\n");
  if (btleChannel != nullptr) {
    btleAttribute = g_attrib_new(btleChannel);
    printf("1: %s btleAttribute=%p\n", __func__, btleAttribute);

    bt_uuid_t uuid;
    if (bt_string_to_uuid(&uuid, CHAR_UUID) < 0) {
      g_warning("BTLE Invalid UUID of characteristic.\n");
      setState(cssFailedToConnect);
      return;
    }

    BtleCallbackData* data = new BtleCallbackData(this, btleChannel, btleAttribute);
    gatt_discover_char(btleAttribute, 0x0001, 0xffff, &uuid, BtleCommWrapper::discoverCharacteristicCallback, data);
    setState(cssDiscover);

  } else {
    printf("%s: internal error, btleChannel == null", __func__);
  }
}

void BtleCommWrapper::disconnect() {
  deleteBtleAttrib();
  deleteBtleChannel();
  setState(cssNone);
  setBtleError(0);
  printf("--DISCONNECTED\n");
}

bool BtleCommWrapper::connectTo(const string& address, gint64 timeoutInMs) {
  printf("%s\n",__func__);
  if (isConnectingInProgress() == true) {
    g_warning("Already in connecting state\n");
    return false;
  }

  timeoutInMs *= 1000;

  gint64 startTime = g_get_monotonic_time();
  int attempt = 0;
  while (g_get_monotonic_time() - startTime < timeoutInMs) {
    attempt++;
    printf("Connection attempt: %d\n", attempt);

    switch( getState() ) {
      case cssNone:
        executeConnect(address);
        break;

      case cssDiscover:
        executeDiscovery(timeoutInMs, startTime);
        break;

      case cssConnectionEstablished:
        printf("%s: Shouldn't happen!\n", __func__);
        goto exitFromWhile;

      case cssFailedToConnect:
        goto exitFromWhile;

      default:
        printf("Illegal state %d\n", getState());
        break;
    }

    //immediate error handling
    NextAction nextAction = handleBtleError();
    if (nextAction == naRepeat) {
      printf("Next action repeat\n");
      continue;

    } else if (nextAction == naFatal) {
      printf("Next action fatal\n");
      break;
    }

    //wait until some callbacks
    ConnectionStatusState enterState = getState();
    printf("Waiting for callback or error\n");

    while (true) {
      gint64 curTime = g_get_monotonic_time();
      if( curTime - startTime > timeoutInMs) {
        printf("Timeout: %lld, %lld, %lld\n",curTime - startTime, curTime, startTime);
        break;
      }

      ConnectionStatusState curState = getState();
      if ((curState != enterState) || isBtleError()) {
        printf("Break sleep, curState=%d, enterState=%d, isError=%d\n", curState, enterState, isBtleError());
        break;
      }
      usleep(30 * 1000);
    }

    //post callback error handling
    if (handleBtleError() == naFatal) {
      break;
    }

    //exit if connection established
    if (getState() == cssConnectionEstablished) {
      break;
    }
  }

  exitFromWhile:
  bool result = isConnected();
  if (result == false) {
    deleteBtleAttrib();
    deleteBtleChannel();
    setState(cssNone);
    setBtleError(0);
    g_warning("Unable to connect to %s", address.c_str());
  } else {
    printf(" ---- Connection success\n");
  }
  return result;
}

bool BtleCommWrapper::send(const string& dataToSend, int timeoutInMs) {
  if (isConnected() == false) {
    g_warning("Not connected!");
    return false;
  }

  bool result = false;
  gsize plen = dataToSend.length();
  uint8_t *value = (uint8_t *) g_try_malloc0(plen + 1);
  strcpy((char*) value, dataToSend.c_str());
  if (plen != 0) {
    ReadSyncBlock data(&this->mutex);

    gatt_write_char(btleAttribute, btleValueHandle, value, plen, BtleCommWrapper::writeValueCallback, &data);

    const int sleepTimeMs = 30;
    gint64 startTime = g_get_monotonic_time();
    timeoutInMs *= 1000;

    while (true) {
      if (data.isReady() == true) {
        break;
      }
      gint64 curTime = g_get_monotonic_time();
      if (curTime - startTime > timeoutInMs) {
        printf("Send Timeout: %lld, %lld, %lld\n", curTime - startTime, curTime, startTime);
        break;
      }
      usleep(sleepTimeMs * 1000);
    }
    g_free(value);
    result = data.resultSize == 0;
  }

  return result;
}

string BtleCommWrapper::readLine(int timeoutInMs) {
  if (isConnected() == false) {
    printf("readLine: not connected, ignored!");
    return "";
  }

  string result = "";

  const int sleepTimeMs = 30;
  gint64 startTime = g_get_monotonic_time();
  timeoutInMs *= 1000;

  while (true) {
    usleep(sleepTimeMs * 1000);
    gint64 curTime = g_get_monotonic_time();
    if (curTime - startTime > timeoutInMs) {
      printf("Read Timeout: %lld, %lld, %lld\n", curTime - startTime, curTime, startTime);
      break;
    }

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
