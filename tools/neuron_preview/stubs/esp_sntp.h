#pragma once
enum { SNTP_SYNC_STATUS_RESET, SNTP_SYNC_STATUS_COMPLETED };
inline int sntp_get_sync_status() { return SNTP_SYNC_STATUS_RESET; }
inline void configTzTime(const char*, const char*, const char*) {}
