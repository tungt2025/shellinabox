#include "custom_handlers.h"
#include "libhttp/http.h"

#include <stdlib.h>

// Khung handler upload mẫu
int uploadHttpHandler(HttpConnection *http, void *arg, const char *buf, int len) {
  // Kiểm tra shell đã login chưa (dựa vào session)
  Session *session = httpGetSession(http);
  if (!session || !session->user || !session->user[0]) {
    httpSendReply(http, 401, "Unauthorized", "Shell login required\n");
    return 0;
  }

  // Kiểm tra phương thức POST
  const char *method = httpGetRequestMethod(http);
  if (!method || strcmp(method, "POST") != 0) {
    httpSendReply(http, 405, "Method Not Allowed", "Only POST allowed\n");
    return 0;
  }

  // Kiểm tra Content-Type
  const char *contentType = httpGetHeader(http, "Content-Type");
  if (!contentType || strncmp(contentType, "multipart/form-data", 19) != 0) {
    httpSendReply(http, 400, "Bad Request", "Content-Type must be multipart/form-data\n");
    return 0;
  }

  // Tìm boundary
  const char *boundary = strstr(contentType, "boundary=");
  if (!boundary) {
    httpSendReply(http, 400, "Bad Request", "Missing boundary\n");
    return 0;
  }
  boundary += 9; // skip 'boundary='

  // Tìm vị trí bắt đầu dữ liệu file
  char boundaryStr[128];
  snprintf(boundaryStr, sizeof(boundaryStr), "--%s", boundary);
  char *fileStart = strstr((char *)buf, boundaryStr);
  if (!fileStart) {
    httpSendReply(http, 400, "Bad Request", "No file data found\n");
    return 0;
  }
  fileStart += strlen(boundaryStr);

  // Tìm header Content-Disposition
  char *disp = strstr(fileStart, "Content-Disposition:");
  if (!disp) {
    httpSendReply(http, 400, "Bad Request", "No Content-Disposition\n");
    return 0;
  }
  char *dataStart = strstr(disp, "\r\n\r\n");
  if (!dataStart) {
    httpSendReply(http, 400, "Bad Request", "Malformed multipart data\n");
    return 0;
  }
  dataStart += 4;

  // Tìm vị trí kết thúc file (boundary tiếp theo)
  char *fileEnd = strstr(dataStart, boundaryStr);
  if (!fileEnd) {
    fileEnd = (char *)buf + len;
  }
  size_t fileLen = fileEnd - dataStart;

  // Xác định thư mục upload
  const char *uploadDir = getenv("UPLOAD_DIR");
  if (!uploadDir || access(uploadDir, W_OK) != 0) {
    uploadDir = "/tmp";
  }

  // Sinh tên file tự động
  char filePath[256];
  snprintf(filePath, sizeof(filePath), "%s/uploaded_%ld_%d", uploadDir, time(NULL), rand());

  // Lưu file
  FILE *f = fopen(filePath, "wb");
  if (!f) {
    httpSendReply(http, 500, "Internal Server Error", "Cannot open file for writing\n");
    return 0;
  }
  fwrite(dataStart, 1, fileLen, f);
  fclose(f);

  char resp[512];
  snprintf(resp, sizeof(resp), "File uploaded successfully: %s\n", filePath);
  httpSendReply(http, 200, "OK", resp);
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
