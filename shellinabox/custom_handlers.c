#include "custom_handlers.h"
#include "libhttp/http.h"

#include "shellinabox/session.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "logging/logging.h"

// Khung handler upload mẫu
int uploadHttpHandler(HttpConnection *http, void *arg, const char *buf, int len) {
  info("[UPLOAD] Handler called, len=%d", len);
  // Lấy sessionKey từ query string
  URL *url = newURL(http, buf, len);
  const char *query = urlGetQuery(url);
  const char *sessionKey = NULL;
  if (query) {
    const char *p = strstr(query, "session=");
    if (p) {
      p += 8;
      const char *end = strchr(p, '&');
      size_t slen = end ? (size_t)(end - p) : strlen(p);
      static char keybuf[128];
      if (slen < sizeof(keybuf)) {
        strncpy(keybuf, p, slen);
        keybuf[slen] = 0;
        // Giải mã URL encoding
        char decoded[128];
        int di = 0;
        for (int si = 0; keybuf[si] && di < (int)sizeof(decoded) - 1; ++si) {
          if (keybuf[si] == '%' && keybuf[si+1] && keybuf[si+2]) {
            char hex[3] = { keybuf[si+1], keybuf[si+2], 0 };
            decoded[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
          } else if (keybuf[si] == '+') {
            decoded[di++] = ' ';
          } else {
            decoded[di++] = keybuf[si];
          }
        }
        decoded[di] = 0;
        sessionKey = decoded;
        info("[UPLOAD] Found sessionKey: %s", sessionKey);
      } else {
        error("[UPLOAD] Session key too long");
      }
    } else {
      error("[UPLOAD] No session= in query");
    }
  } else {
    error("[UPLOAD] No query string");
  }
  // Lấy session từ sessionKey
  extern char *cgiSessionKey;
  int sessionIsNew = 0;
  struct Session *session = NULL;
  if (sessionKey)
    session = findSession(sessionKey, cgiSessionKey, &sessionIsNew, http);
  if (!session || session->done || session->pty < 0 || session->pid <= 0) {
    error("[UPLOAD] Invalid or missing session (sessionKey=%s)", sessionKey ? sessionKey : "NULL");
      httpSendRawReply(http, 401, "Unauthorized", "Shell login required\n");
    return 0;
  }

  // Kiểm tra phương thức POST
  const char *method = httpGetMethod(http);
  if (!method || strcmp(method, "POST") != 0) {
    error("[UPLOAD] Invalid method: %s", method ? method : "NULL");
    if (url) deleteURL(url);
      httpSendRawReply(http, 405, "Method Not Allowed", "Only POST allowed\n");
    return 0;
  }

  // Kiểm tra Content-Type
  const HashMap *headers = httpGetHeaders(http);
  const char *contentType = getFromHashMap(headers, "content-type");
  if (!contentType || strncmp(contentType, "multipart/form-data", 19) != 0) {
    error("[UPLOAD] Invalid Content-Type: %s", contentType ? contentType : "NULL");
    if (url) deleteURL(url);
      httpSendRawReply(http, 400, "Bad Request", "Content-Type must be multipart/form-data\n");
    return 0;
  }
  if (url) deleteURL(url);

  // Tìm boundary
  const char *boundary = strstr(contentType, "boundary=");
  if (!boundary) {
    warn("[UPLOAD] Missing boundary in Content-Type: %s", contentType);
      httpSendRawReply(http, 400, "Bad Request", "Missing boundary\n");
    return 0;
  }
  boundary += 9; // skip 'boundary='

  // Tìm vị trí bắt đầu dữ liệu file
  char boundaryStr[128];
  snprintf(boundaryStr, sizeof(boundaryStr), "--%s", boundary);
  char *fileStart = strstr((char *)buf, boundaryStr);
  if (!fileStart) {
    warn("[UPLOAD] No file data found (boundaryStr=%s)", boundaryStr);
      httpSendRawReply(http, 400, "Bad Request", "No file data found\n");
    return 0;
  }
  fileStart += strlen(boundaryStr);

  // Tìm header Content-Disposition
  char *disp = strstr(fileStart, "Content-Disposition:");
  if (!disp) {
    warn("[UPLOAD] No Content-Disposition header");
      httpSendRawReply(http, 400, "Bad Request", "No Content-Disposition\n");
    return 0;
  }
  char *dataStart = strstr(disp, "\r\n\r\n");
  if (!dataStart) {
    warn("[UPLOAD] Malformed multipart data (no data start)");
      httpSendRawReply(http, 400, "Bad Request", "Malformed multipart data\n");
    return 0;
  }
  dataStart += 4;

  // Tìm vị trí kết thúc file (boundary tiếp theo)
  char *fileEnd = strstr(dataStart, boundaryStr);
  if (!fileEnd) {
    warn("[UPLOAD] No ending boundary found, using buffer end");
    fileEnd = (char *)buf + len;
  }
  size_t fileLen = fileEnd - dataStart;

  // Xác định thư mục upload
  const char *uploadDir = getenv("UPLOAD_DIR");
  if (!uploadDir || access(uploadDir, W_OK) != 0) {
    warn("[UPLOAD] UPLOAD_DIR not set or not writable, fallback to /tmp");
    uploadDir = "/tmp";
  }

  // Sinh tên file tự động
  char filePath[256];
  snprintf(filePath, sizeof(filePath), "%s/uploaded_%ld_%d", uploadDir, time(NULL), rand());
  info("[UPLOAD] Saving file to %s, size=%zu", filePath, fileLen);

  // Lưu file
  FILE *f = fopen(filePath, "wb");
  if (!f) {
    error("[UPLOAD] Cannot open file for writing: %s", filePath);
      httpSendRawReply(http, 500, "Internal Server Error", "Cannot open file for writing\n");
    return 0;
  }
  size_t written = fwrite(dataStart, 1, fileLen, f);
  fclose(f);
  if (written != fileLen) {
    error("[UPLOAD] File write incomplete: %zu/%zu bytes", written, fileLen);
      httpSendRawReply(http, 500, "Internal Server Error", "File write incomplete\n");
    return 0;
  }

  char resp[512];
  snprintf(resp, sizeof(resp), "File uploaded successfully: %s\n", filePath);
  info("[UPLOAD] Success: %s", filePath);
    httpSendRawReply(http, 200, "OK", "File uploaded successfully: %s\n", filePath);
  return 0;
}

// Mảng các handler mở rộng
CustomHttpHandler customHandlers[] = {
  { "/upload", uploadHttpHandler, NULL },
  // Thêm handler mới tại đây
  { NULL, NULL, NULL }
};

// Đăng ký tất cả handler vào server
void registerCustomHandlers(Server *server) {
  for (int i = 0; customHandlers[i].path != NULL; i++) {
    serverRegisterHttpHandler(server, customHandlers[i].path, customHandlers[i].handler, customHandlers[i].arg);
  }
}

// Giải phóng handler nếu cần (hiện tại không cần, placeholder cho mở rộng)
void freeCustomHandlers(void) {
  // Nếu sau này cấp phát động, giải phóng ở đây
}
