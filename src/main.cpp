#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mongoose.h"

char empty_string[2] = "";

static void *callback(enum mg_event event,
                      struct mg_connection *conn,
                      const struct mg_request_info *request_info) {
  if (event == MG_NEW_REQUEST) {
    // Echo requested URI back to the client
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n\r\n");
    char q[1024];
    q[1023] = '\0';
    printf("query string: %s\n", request_info->query_string);

    int ql = 0;
    if (request_info->query_string) {
      ql = mg_get_var(request_info->query_string, 
		      strlen(request_info->query_string), 
		      "q", q, 1023);
    }

    printf("%p, %d\n", q, ql);

    mg_printf(conn, "q: %s, ql: %d", q ? q : "", ql);
    return empty_string;  // Mark as processed
  } else {
    return NULL;
  }
}

int main(void) {
  struct mg_context *ctx;
  const char *options[] = {"listening_ports", "6767", NULL};

  ctx = mg_start(&callback, NULL, options);
  while (1) {
    // Never stop
    sleep(100);
  }
  mg_stop(ctx);

  return 0;
}
