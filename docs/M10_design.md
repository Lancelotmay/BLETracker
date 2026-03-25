<!--
Copyright (C) 2026 Lancelot MEI
SPDX-License-Identifier: GPL-2.0-only
-->

## app_M10 design

single thread
app -> ubx -> M10_hw

Buffer management (ring buffers)
static allocation, __aligned(32)
rx buffer: rx_buff_num x max rx message size
tx buffer: tx_buff_num x max tx message size

work items:
- when ISR trigger, decode rx message and update LNS
- when new config received, update M10 configuration
- GNSS state management (TBD)
- GNSS init sequence

system init:
    - interface init
    - BATT init
    - GNSS init
      - reset GNSS
      - disable all message
      - read version info
      - put GNSS in sleep
    - BLE init

triggers
- batt timer: put BATT reading to Workqueue: read ADC, send via BLE
- Start GNSS:
  - bluetooth get config from M10 setting profile (priviate) and notify main thread
  - main thread allocate buffer and send config to M10
  - enable 1pps interrupt
- 1pps interrrupt 
  - send signal to main thread
    - main  thread allocate buffer for PVT message
    - main thread poll PVT message;
    - main thread build and send PVT result to BLE
    - release buffer

   

UBX operations
- poll message:
  - build message payload
  - build message
  - send message
  - wait 10ms
  - check for response
  - decode response
- send message
- 
