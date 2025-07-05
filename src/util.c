#include <string.h>

#include "util.h"

// Simple function to get MIME type from file extension
const char *get_mime_type(const char *path) {
  const char *dot = strrchr(path, '.');
  if (!dot || dot == path) return "application/octet-stream";
  if (strcmp(dot, ".html") == 0) return "text/html";
  if (strcmp(dot, ".css") == 0) return "text/css";
  if (strcmp(dot, ".js") == 0) return "application/javascript";
  if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
  if (strcmp(dot, ".jpeg") == 0) return "image/jpeg";
  if (strcmp(dot, ".png") == 0) return "image/png";
  return "application/octet-stream";
} 