#ifndef CUSTOM_HANDLERS_H
#define CUSTOM_HANDLERS_H

#include "libhttp/http.h"
#include <stddef.h>

// Định nghĩa struct cho HTTP handler mở rộng
typedef int (*HttpHandlerFunc)(HttpConnection *, void *, const char *, int);
typedef struct {
  const char *path;
  HttpHandlerFunc handler;
  void *arg;
} CustomHttpHandler;

// Khung handler upload mẫu

int uploadHttpHandler(HttpConnection *http, void *arg, const char *buf, int len);
extern CustomHttpHandler customHandlers[];

// Đăng ký tất cả handler vào server
void registerCustomHandlers(Server *server);
// Giải phóng handler nếu cần
void freeCustomHandlers(void);

#endif // CUSTOM_HANDLERS_H
