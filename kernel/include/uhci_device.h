
// uhci.h
#ifndef UHCI_DEVICE_H
#define UHCI_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "kernel.h"
#include "uhci.h"

void uhci_dev_handle_td(struct uhci_device* device, struct transfer_descriptor* td);

#endif

