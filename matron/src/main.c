#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "args.h"
#include "device.h"
#include "device_list.h"
#include "device_hid.h"
#include "device_monitor.h"
#include "device_monome.h"
#include "device_midi.h"
#include "events.h"
#include "battery.h"
#include "gpio.h"
#include "hello.h"
#include "i2c.h"
#include "input.h"
#include "osc.h"
#include "metro.h"
#include "screen.h"
#include "stat.h"
#include "clock.h"

#include "oracle.h"
#include "weaver.h"

void print_version(void);

void cleanup(void) {
    dev_monitor_deinit();
    osc_deinit();
    o_deinit();
    w_deinit();
    gpio_deinit();
    i2c_deinit();
    screen_deinit();
    battery_deinit();
    stat_deinit();

    fprintf(stderr, "matron shutdown complete\n");
    exit(0);
}

int main(int argc, char **argv) {
    args_parse(argc, argv);

    print_version();

    events_init(); // <-- must come first!
    screen_init();

    metros_init();
    gpio_init();
    battery_init();
    stat_init();
    i2c_init();
    osc_init();
    clock_init();

    o_init(); // oracle (audio)

    w_init(); // weaver (scripting)
    dev_list_init();
    dev_monitor_init();

    // now is a good time to set our cleanup
    atexit(cleanup);
    // start reading input to interpreter
    input_init();
    // i/o subsystems are ready; run user startup routine
    w_startup();
    // scan for connected input devices
    dev_monitor_scan();

    // blocks until quit
    event_loop();
}

void print_version(void) {
    printf("MATRON\n");
    printf("norns version: %d.%d.%d\n",
           VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    printf("git hash: %s\n\n", VERSION_HASH);
}
