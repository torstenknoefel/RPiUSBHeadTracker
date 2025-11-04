#include "crow.h"
int main_web() {

crow::SimpleApp app;

CROW_ROUTE(app, "/")
([]() {
  return "Hello, world!";
});
app.port(18080).multithreaded().run();
}