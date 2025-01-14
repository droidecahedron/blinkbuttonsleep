.. zephyr:code-sample:: nrf_system_off
   :name: System Off
   :relevant-api: sys_poweroff subsys_pm_device

   Use deep sleep on Nordic platforms.

Overview
********

**THIS REQUIRES AN NRF54L15DK, OR ADJUSTMENTS TO THE BIT DEFINES IN main.c**

This sample can be used for basic power measurement and as an example of
deep sleep on Nordic platforms and utilize the latch register to determine wakeup source.
https://docs.nordicsemi.com/bundle/ps_nrf54L15/page/gpio.html#ariaid-title15

It is overly verbose for educational purposes.

You can press switch 0-3 to see an LED blink the set number times and then go to sleep.



RAM Retention
=============

This sample can also demonstrate RAM retention.
By selecting ``CONFIG_APP_USE_RETAINED_MEM=y`` state related to number of boots,
number of times system off was entered, and total uptime since initial power-on
are retained in a checksummed data structure.
RAM is configured to keep the containing section powered while in system-off mode.

Requirements
************

This application uses nRF54L15 DK board for the demo.

Sample Output
=============

nRF54 core output
-----------------

.. code-block:: console

   *** Booting Zephyr OS build v2.3.0-rc1-204-g5f2eb85f728d  ***
   *** Using Zephyr OS v3.7.99-1f8f3dc29142 ***
   
   nrf54l15dk system off demo
   LATCH REGISTER FOR P0: 0
   LATCH REGISTER FOR P1: 512
   WAKEUP SRC: SW1
   Retained data not supported
   Entering system off; press any switch to restart
   *** Booting nRF Connect SDK v2.9.0-7787b2649840 ***
   *** Using Zephyr OS v3.7.99-1f8f3dc29142 ***
   
   nrf54l15dk system off demo
   LATCH REGISTER FOR P0: 0
   LATCH REGISTER FOR P1: 256
   WAKEUP SRC: SW2
   Retained data not supported
   Entering system off; press any switch to restart


Power Measurement
-----------------
.. image:: https://imgur.com/eMsx3zS.png
