/*
 * BluetoothGuard.h
 *
 *  Created on: Apr 3, 2017
 *      Author: Zarnowski
 */

#ifndef BluetoothGuard_hpp
#define BluetoothGuard_hpp

#include <mutex>

class BluetoothGuard {
  public:
    static void lockBluetooth(void* owner);
    static void unlockBluetooth(void* owner);
    static bool isBluetoothLocked(void* owner = nullptr);

  private:
    static std::mutex innerMutex;
    static void* owner;
};

#endif /* BluetoothGuard_hpp */
