/*
 * BluetoothGuard.cpp
 *
 *  Created on: Apr 3, 2017
 *      Author: Zarnowski
 */

#include <unistd.h>
#include "BluetoothGuard.h"

std::mutex BluetoothGuard::innerMutex;
void* BluetoothGuard::owner = nullptr;

void BluetoothGuard::lockBluetooth(void* owner) {
  while(true) {
    //critical section
    std::unique_lock<std::mutex> lock(BluetoothGuard::innerMutex);
    if (BluetoothGuard::owner != nullptr) {
      lock.unlock();
      usleep(50 * 1000);

    } else {
      BluetoothGuard::owner = owner;
      return;
    }
  }
}

void BluetoothGuard::unlockBluetooth(void* owner) {
  std::unique_lock<std::mutex> lock(BluetoothGuard::innerMutex);
  if (owner != BluetoothGuard::owner) {
    printf("Wrong owner of bluetooth guard, expected %p but got %p", BluetoothGuard::owner, owner);
    exit(-1); //this is fatal
  }
  BluetoothGuard::owner = nullptr;
}

bool BluetoothGuard::isBluetoothLocked(void* owner) {
  std::unique_lock<std::mutex> lock(BluetoothGuard::innerMutex);
  return owner == nullptr ? BluetoothGuard::owner != nullptr : BluetoothGuard::owner == owner;
}
