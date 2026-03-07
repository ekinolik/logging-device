#pragma once

#include <Arduino.h>
#include <SD.h>

bool ParseLastLine(const String& line, uint32_t& n, uint32_t& ms, uint32_t& chk);
bool VerifyChecksum(uint32_t n, uint32_t ms, uint32_t chk);
bool WriteAndVerify(uint32_t counter);
String GetLastLine(String filename, size_t window, String lastTail);