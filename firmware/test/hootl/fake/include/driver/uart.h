#pragma once
#include <stdint.h>
#include "hal/uart_types.h"
typedef int uart_port_t;
typedef struct
{
    int baud_rate;
} uart_config_t;
