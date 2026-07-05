#include "auth.h"
#include "config.h"
#include <cstring>

bool requireAdmin(AsyncWebServerRequest* request) {
  if (strlen(ADMIN_PASSWORD) == 0) return true;
  if (request->authenticate("admin", ADMIN_PASSWORD)) return true;
  request->requestAuthentication();
  return false;
}
