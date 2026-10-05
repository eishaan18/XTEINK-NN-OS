#pragma once
#include <cstdio>
#define LOG_ERR(tag, ...) (fprintf(stderr, "[ERR][%s] ", tag), fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define LOG_INF(tag, ...) (fprintf(stderr, "[INF][%s] ", tag), fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define LOG_DBG(tag, ...) ((void)0)
