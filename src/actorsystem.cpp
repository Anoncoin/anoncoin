#include "actorsystem.h"

#include <string>
#include <iostream>

#include "caf/all.hpp"

using std::endl;
using std::string;

using namespace caf;

behavior mirror(event_based_actor* self) {
// return the (initial) actor behavior
return {
    // a handler for messages containing a single string
    // that replies with a string
    [=](const string& what) -> string {
    // prints "Hello World!" via aout (thread-safe cout wrapper)
    aout(self) << what << endl;
    // reply "!dlroW olleH"
    return string(what.rbegin(), what.rend());
    }
};
}

void hello_world(event_based_actor* self, const actor& buddy) {
// send "Hello World!" to our buddy ...
self->request(buddy, std::chrono::seconds(10), "Hello World!").then(
    // ... wait up to 10s for a response ...
    [=](const string& what) {
    // ... and print it
    aout(self) << what << endl;
    }
);
}

void actorsystemMain() {
  actor_system_config cfg;
  actor_system system{cfg};
  auto mirror_actor = system.spawn(mirror);
  system.spawn(hello_world, mirror_actor);
  // System will wait until all actors are destroyed
}