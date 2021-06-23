README.txt GE Intelligent Platforms, Inc.

Copyright (C) 2002, 2005-2014 GE Intelligent Platforms, Inc.
International Copyright Secured.  All Rights Reserved.


This is the GE Intelligent Platforms, Inc. RFM2g Reflective
Memory Device Driver for Linux:

(3.x kernel. Ubuntu 14.04 64bit, Fedora 20 64bit)
(2.4 kernel, Red Hat(tm) 7.2, 7.3, 8.0, and 9.0)
(2.6 kernel, Red Hat(tm) Enterprise Linux 5)
(2.6 kernel, Fedora Core 2, 3, 4, 6, 8, and 12)
(X86-64 kernel, Fedora Core 5 and Fedora 8)
(2.4 kernel, Red Haggis Linux LSP-7436 for PPC platforms)


The following feature set is implemented in this release:

    o   Open
    o   Close
    o   Read a buffer from reflective memory
    o   Write a buffer to reflective memory
    o   Peek a byte, word, or longword from reflective memory
    o   Poke a byte, word, or longword to reflective memory
    o   Read or write using the RFM2g board's DMA engine
    o   Send an interrupt event to another RFM2g node
    o   Get or set the status of the RFM2g board's Status LED
    o   Get or set the driver's debug flag settings
    o   Synchronous event notification, which allows the user to block
          (with timeout) on the arrival of an interrupt event
    o   Asynchronous event notification, which allows a user-specified
          callback function to execute upon the arrival of an interrupt event


See rfm2g/include/rfm2g_api.h for the API function prototypes.

To use the RFM2g driver with your application, you must include rfm2g_api.h,
and link to the RFM2g API and POSIX Threads libraries.  For examples,
change to the rfm2g/samples directory and examine the files Makefile_s,
Makefile_r, Makefile_m, rfm2g_sender.c, rfm2g_receiver.c, and rfm2g_map.c.


See install.txt on the distribution media for installation instructions.
