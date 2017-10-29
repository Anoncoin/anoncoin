#include "actorsystem.h"

#include <string>
#include <iostream>

#include "caf/all.hpp"

using std::endl;
using std::string;

using namespace caf;

void print_on_exit(const actor& hdl, const std::string& name) {
  hdl->attach_functor([=](const error& reason) {
    std::cout << name << " exited: " << to_string(reason) << endl;
  });
}

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
self->request(buddy, std::chrono::seconds(10), "Make Anoncoin Great Again!").then(
    // ... wait up to 10s for a response ...
    [=](const string& what) {
    // ... and print it
    aout(self) << what << endl;
    }
);
}

void caf_main(actor_system& system) {
  scoped_actor self{system};
  aout(self) << "Starting up Actor System" << endl;
  auto mirror_actor = self->spawn(mirror);
  self->spawn(hello_world, mirror_actor);
  self->await_all_other_actors_done();
  aout(self) << "Awaiting all actors to die" << endl;
}

void actorsystemMain() {
  actor_system_config cfg;

  // TODO: Add network support
  //cfg.load<io::middleman>();

  actor_system system{cfg};
  caf_main(system);
}