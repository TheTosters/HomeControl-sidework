/*
 * ReadSyncBlock.cpp
 *
 *  Created on: Apr 11, 2017
 *      Author: Zarnowski
 */

#include "ReadSyncBlock.h"

ReadSyncBlock::ReadSyncBlock(GMutex* mutex)
: result(nullptr), resultSize(0), mutex(mutex), ready(false) {

}

ReadSyncBlock::~ReadSyncBlock() {
    delete[] result;
    result = nullptr;
    mutex = nullptr;
}

void ReadSyncBlock::setReady() {
    g_mutex_lock(mutex);
    ready = true;
    g_mutex_unlock(mutex);
}

bool ReadSyncBlock::isReady() {
    g_mutex_lock(mutex);
    bool tmp = ready;
    g_mutex_unlock(mutex);
    return tmp;
}
