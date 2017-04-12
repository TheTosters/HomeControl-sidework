/*
 * BtleCom.h
 *
 *  Created on: Apr 10, 2017
 *      Author: Zarnowski
 */

#ifndef BtleCom_hpp
#define BtleCom_hpp

extern "C" {
    #include "glib-2.0/glib.h"
    #include "libgatt/gattrib.h"
}
#include <string>
#include <vector>
#include <memory>
#include <mutex>

using namespace std;

enum ConnectionStatusState {
  cssNone,

  cssConnect,
  cssConnected,

  cssDiscover,
  cssDiscovered,

  cssFailedToConnect,
  cssConnectionEstablished,
};

enum NextAction {
  naContinue,
  naRepeat,
  naFatal
};

class BtleCommWrapper {
  public:
    BtleCommWrapper();
    virtual ~BtleCommWrapper();
    bool connectTo(const string& address, gint64 timeoutInMs);
    bool isConnected();
    void disconnect();
    bool send(const string& data, int timeoutInMs = 3000);
    string readLine(int timeoutInMs);
  private:
    ConnectionStatusState state;
    GMainLoop* eventLoop;
    GThread* eventLoopThread;
    GIOChannel* btleChannel;
    GAttrib* btleAttribute;
    guint16 btleValueHandle;
    std::mutex notificationMutex;
    bool notificationReceived;
    GMutex mutex;
    int btleError;
    std::shared_ptr<std::vector<uint8_t>> notificationBuffer;

    void setBtleError(int error);
    bool isBtleError();
    NextAction handleBtleError();

    void deleteBtleChannel();
    void deleteBtleAttrib();

    bool isConnectingInProgress();
    ConnectionStatusState getState();
    void setState(ConnectionStatusState state);

    static void connectCallback(GIOChannel *io, GError *err, gpointer user_data);
    static void discoverCharacteristicCallback(GSList *characteristics, uint8_t status, void *user_data);
    static void notificationEventsHandler(const uint8_t *pdu, uint16_t len, gpointer user_data);
    static gboolean channelWatch(GIOChannel* source, GIOCondition condition, gpointer data);
    static void writeValueCallback(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data);

    void executeConnect(const string& address);
    void executeDiscovery(int timeoutInMs, gint64 startTime);

};

#endif /* BtleCom_hpp */
