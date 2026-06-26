#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>

#include "http_module.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"
#include "memory.h"

typedef struct
{
    char *data;
    size_t size;
} CurlBuffer;

static size_t curlWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realSize = size * nmemb;
    CurlBuffer *buf = (CurlBuffer *)userp;

    char *ptr = realloc(buf->data, buf->size + realSize + 1);
    if (ptr == NULL)
        return 0;

    buf->data = ptr;
    memcpy(buf->data + buf->size, contents, realSize);
    buf->size += realSize;
    buf->data[buf->size] = '\0';
    return realSize;
}

static void setNative(ObjModule *module, const char *name, NativeFn function)
{
    ObjString *key = copyString(name, (int)strlen(name));
    push(OBJ_VAL(key));
    ObjNative *native = newNative(function);
    push(OBJ_VAL(native));
    tableSet(&module->fields, key, OBJ_VAL(native));
    pop();
    pop();
}

static Value httpResponse(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;

    ObjClass *klass = newClass(copyString("Object", 6));
    push(OBJ_VAL(klass));
    ObjInstance *res = newInstance(klass);
    push(OBJ_VAL(res));

    ObjString *statusKey = copyString("status", 6);
    push(OBJ_VAL(statusKey));
    tableSet(&res->fields, statusKey, args[0]);
    pop();

    ObjString *bodyKey = copyString("body", 4);
    push(OBJ_VAL(bodyKey));
    tableSet(&res->fields, bodyKey, args[1]);
    pop();

    ObjString *typeKey = copyString("type", 4);
    push(OBJ_VAL(typeKey));
    tableSet(&res->fields, typeKey, OBJ_VAL(copyString("text/plain", 10)));
    pop();

    pop(); // res
    pop(); // klass
    return OBJ_VAL(res);
}

static ObjInstance *parseRequest(const char *buf, int bytes)
{
    // parse method and path from first line
    char method[16] = {0};
    char path[1024] = {0};
    sscanf(buf, "%15s %1023s", method, path);

    // find body — everything after \r\n\r\n
    const char *bodyStart = strstr(buf, "\r\n\r\n");
    const char *body = "";
    int bodyLen = 0;
    if (bodyStart != NULL)
    {
        bodyStart += 4;
        bodyLen = (int)(bytes - (bodyStart - buf));
        if (bodyLen > 0)
            body = bodyStart;
    }

    // build ObjInstance
    ObjClass *klass = newClass(copyString("Object", 6));
    push(OBJ_VAL(klass));
    ObjInstance *req = newInstance(klass);
    push(OBJ_VAL(req));

    // set method
    ObjString *methodKey = copyString("method", 6);
    push(OBJ_VAL(methodKey));
    tableSet(&req->fields, methodKey, OBJ_VAL(copyString(method, (int)strlen(method))));
    pop();

    // set path
    ObjString *pathKey = copyString("path", 4);
    push(OBJ_VAL(pathKey));
    tableSet(&req->fields, pathKey, OBJ_VAL(copyString(path, (int)strlen(path))));
    pop();

    // set body
    ObjString *bodyKey = copyString("body", 4);
    push(OBJ_VAL(bodyKey));
    tableSet(&req->fields, bodyKey, OBJ_VAL(copyString(body, bodyLen)));
    pop();

    pop(); // req
    pop(); // klass
    return req;
}

static Value callHandler(ObjClosure *handler, ObjInstance *req)
{
    push(OBJ_VAL(handler));
    push(OBJ_VAL(req));

    int frameBase = vm.frameCount;
    callFunction(handler, 1);
    InterpretResult result = run(frameBase);

    if (result != INTERPRET_OK)
        return NIL_VAL;

    return pop();
}

static void sendResponse(int clientFd, Value resVal)
{
    // default vals
    int status = 200;
    const char *statusText = "OK";
    const char *body = "";
    const char *contentType = "text/plain";

    if (IS_INSTANCE(resVal))
    {
        ObjInstance *res = AS_INSTANCE(resVal);

        // get status
        Value statusVal;
        ObjString *statusKey = copyString("status", 6);
        if (tableGet(&res->fields, statusKey, &statusVal) && IS_NUMBER(statusVal))
        {
            status = (int)AS_NUMBER(statusVal);
        }

        // get body
        Value bodyVal;
        ObjString *bodyKey = copyString("body", 4);
        if (tableGet(&res->fields, bodyKey, &bodyVal) && IS_STRING(bodyVal))
        {
            body = AS_CSTRING(bodyVal);
        }

        // get content type
        Value typeVal;
        ObjString *typeKey = copyString("type", 4);
        if (tableGet(&res->fields, typeKey, &typeVal) && IS_STRING(typeVal))
        {
            contentType = AS_CSTRING(typeVal);
        }
    }
    else
    {
        // handler didn't return a response object
        status = 500;
        statusText = "Internal Server Error";
        body = "handler must return http::response(...)";
    }

    // map status code to text
    if (status == 200)
        statusText = "OK";
    else if (status == 201)
        statusText = "Created";
    else if (status == 400)
        statusText = "Bad Request";
    else if (status == 404)
        statusText = "Not Found";
    else if (status == 500)
        statusText = "Internal Server Error";

    // build and send HTTP response
    char header[1024];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, statusText, contentType, strlen(body));

    send(clientFd, header, strlen(header), 0);
    send(clientFd, body, strlen(body), 0);
}

static Value httpServe(int argCount, Value *args)
{
    // Takes port number and Rain handler closure.
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_CLOSURE(args[1]))
        return NIL_VAL;
    int port = (int)AS_NUMBER(args[0]);
    ObjClosure *handler = AS_CLOSURE(args[1]);

    // Create socket
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1)
        return NIL_VAL;
    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // bind and listen
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serverFd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        close(serverFd);
        return NIL_VAL;
    }

    if (listen(serverFd, 10) == -1)
    {
        close(serverFd);
        return NIL_VAL;
    }
    printf("Rain HTTP server on http://localhost:%d\n", port);

    while (1)
    {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &clientLen);
        if (clientFd == -1)
            continue;

        char buf[4096];
        memset(buf, 0, sizeof(buf));
        ssize_t bytes = recv(clientFd, buf, sizeof(buf) - 1, 0);

        if (bytes <= 0)
        {
            close(clientFd);
            continue;
        }

        ObjInstance *req = parseRequest(buf, (int)bytes);
        push(OBJ_VAL(req));

        Value res = callHandler(handler, req);
        sendResponse(clientFd, res);

        // parse method,path,bod
        pop();
        close(clientFd); // ← add this
    }

    close(serverFd);
    return NIL_VAL;
}

static Value httpJsonResponse(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]))
        return NIL_VAL;

    // stringify the value
    // we need json stringify here
    // for now accept string directly
    if (!IS_STRING(args[1]))
        return NIL_VAL;

    ObjClass *klass = newClass(copyString("Object", 6));
    push(OBJ_VAL(klass));
    ObjInstance *res = newInstance(klass);
    push(OBJ_VAL(res));

    ObjString *statusKey = copyString("status", 6);
    push(OBJ_VAL(statusKey));
    tableSet(&res->fields, statusKey, args[0]);
    pop();

    ObjString *bodyKey = copyString("body", 4);
    push(OBJ_VAL(bodyKey));
    tableSet(&res->fields, bodyKey, args[1]);
    pop();

    ObjString *typeKey = copyString("type", 4);
    push(OBJ_VAL(typeKey));
    tableSet(&res->fields, typeKey, OBJ_VAL(copyString("application/json", 16)));
    pop();

    pop(); // res
    pop(); // klass
    return OBJ_VAL(res);
}

static Value httpServeFile(int argCount, Value *args)
{
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;

    int status = (int)AS_NUMBER(args[0]);
    const char *path = AS_CSTRING(args[1]);

    // read file
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        // file not found — return 404
        ObjClass *klass = newClass(copyString("Object", 6));
        push(OBJ_VAL(klass));
        ObjInstance *res = newInstance(klass);
        push(OBJ_VAL(res));

        ObjString *sk = copyString("status", 6);
        push(OBJ_VAL(sk));
        tableSet(&res->fields, sk, NUMBER_VAL(404));
        pop();

        ObjString *bk = copyString("body", 4);
        push(OBJ_VAL(bk));
        tableSet(&res->fields, bk, OBJ_VAL(copyString("file not found", 14)));
        pop();

        ObjString *tk = copyString("type", 4);
        push(OBJ_VAL(tk));
        tableSet(&res->fields, tk, OBJ_VAL(copyString("text/plain", 10)));
        pop();

        pop(); // res
        pop(); // klass
        return OBJ_VAL(res);
    }

    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(size + 1);
    if (buffer == NULL)
    {
        fclose(file);
        return NIL_VAL;
    }

    size_t bytesRead = fread(buffer, sizeof(char), size, file);
    buffer[bytesRead] = '\0';
    fclose(file);

    // detect mime type from extension
    const char *mime = "text/plain";
    const char *dot = strrchr(path, '.');
    if (dot != NULL)
    {
        dot++;
        if (strcmp(dot, "html") == 0 || strcmp(dot, "htm") == 0)
            mime = "text/html";
        else if (strcmp(dot, "css") == 0)
            mime = "text/css";
        else if (strcmp(dot, "js") == 0)
            mime = "application/javascript";
        else if (strcmp(dot, "json") == 0)
            mime = "application/json";
        else if (strcmp(dot, "png") == 0)
            mime = "image/png";
        else if (strcmp(dot, "jpg") == 0 || strcmp(dot, "jpeg") == 0)
            mime = "image/jpeg";
        else if (strcmp(dot, "gif") == 0)
            mime = "image/gif";
        else if (strcmp(dot, "ico") == 0)
            mime = "image/x-icon";
        else if (strcmp(dot, "svg") == 0)
            mime = "image/svg+xml";
        else if (strcmp(dot, "pdf") == 0)
            mime = "application/pdf";
    }

    // build response
    ObjClass *klass = newClass(copyString("Object", 6));
    push(OBJ_VAL(klass));
    ObjInstance *res = newInstance(klass);
    push(OBJ_VAL(res));

    ObjString *sk = copyString("status", 6);
    push(OBJ_VAL(sk));
    tableSet(&res->fields, sk, NUMBER_VAL(status));
    pop();

    ObjString *bk = copyString("body", 4);
    push(OBJ_VAL(bk));
    tableSet(&res->fields, bk, OBJ_VAL(copyString(buffer, (int)bytesRead)));
    pop();

    ObjString *tk = copyString("type", 4);
    push(OBJ_VAL(tk));
    tableSet(&res->fields, tk, OBJ_VAL(copyString(mime, (int)strlen(mime))));
    pop();

    free(buffer);
    pop(); // res
    pop(); // klass
    return OBJ_VAL(res);
}

static Value httpGet(int argCount, Value *args)
{
    if (argCount != 1 || !IS_STRING(args[0]))
        return NIL_VAL;
    const char *url = AS_CSTRING(args[0]);

    CURL *curl = curl_easy_init();
    if (!curl)
        return NIL_VAL;

    CurlBuffer buf;
    buf.data = malloc(1);
    buf.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);

    long statusCode = 200;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        free(buf.data);
        return NIL_VAL;
    }

    // build response ObjInstance
    ObjClass *klass = newClass(copyString("Object", 6));
    push(OBJ_VAL(klass));
    ObjInstance *response = newInstance(klass);
    push(OBJ_VAL(response));

    ObjString *statusKey = copyString("status", 6);
    push(OBJ_VAL(statusKey));
    tableSet(&response->fields, statusKey, NUMBER_VAL((double)statusCode));
    pop();

    ObjString *bodyKey = copyString("body", 4);
    push(OBJ_VAL(bodyKey));
    tableSet(&response->fields, bodyKey, OBJ_VAL(copyString(buf.data, (int)buf.size)));
    pop();

    free(buf.data);
    pop(); // response
    pop(); // klass
    return OBJ_VAL(response);
}

static Value httpPost(int argCount, Value *args)
{
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;
    const char *url = AS_CSTRING(args[0]);
    const char *body = AS_CSTRING(args[1]);

    CURL *curl = curl_easy_init();
    if (!curl)
        return NIL_VAL;

    CurlBuffer buf;
    buf.data = malloc(1);
    buf.size = 0;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);

    long statusCode = 200;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        free(buf.data);
        return NIL_VAL;
    }

    ObjClass *klass = newClass(copyString("Object", 6));
    push(OBJ_VAL(klass));
    ObjInstance *response = newInstance(klass);
    push(OBJ_VAL(response));

    ObjString *statusKey = copyString("status", 6);
    push(OBJ_VAL(statusKey));
    tableSet(&response->fields, statusKey, NUMBER_VAL((double)statusCode));
    pop();

    ObjString *bodyKey = copyString("body", 4);
    push(OBJ_VAL(bodyKey));
    tableSet(&response->fields, bodyKey, OBJ_VAL(copyString(buf.data, (int)buf.size)));
    pop();

    free(buf.data);
    pop(); // response
    pop(); // klass
    return OBJ_VAL(response);
}

ObjModule *
initHttpModule(void)
{
    ObjString *name = copyString("http", 4);
    push(OBJ_VAL(name));
    ObjModule *module = newModule(name);
    push(OBJ_VAL(module));

    setNative(module, "serve", httpServe);
    setNative(module, "response", httpResponse);
    setNative(module, "jsonResponse", httpJsonResponse);
    setNative(module, "serveFile", httpServeFile);
    setNative(module, "get", httpGet);
    setNative(module, "post", httpPost);

    pop(); // module
    pop(); // name
    return module;
}
