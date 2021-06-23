Copyright (C) 2001, 2004, 2008 GE Intelligent Platforms, Inc.
International Copyright Secured.  All Rights Reserved.

This directory contains sample programs that provide examples on how to
use the driver and API with your application.

Three sample programs are included:  rfm2g_sender.c, rfm2g_receiver.c,
and rfm2g_map.c. 

The rfm2g_map.c program gives an example of using the RFM2gMapUserMemory()
and RFM2gUnMapUserMemory() functions.  This allows the user to read and
write the RFM card's memory area without having to call the read and write
functions.

The other two programs are intended to work together to demonstrate basic
data transfer and interrupt handshaking.  To use the programs together,
it is assumed that two computers are present, each having a Reflective
Memory card connected to the card in the other computer; and the RFM2g
device driver.

The rfm2g_sender.c program (running on Computer #1) does the following:

    Opens the RFM2g driver
    Writes a small buffer of data to Reflective Memory
    Sends an interrupt event to Computer #2
    Waits to receive an interrupt event from Computer #2
    Reads a buffer of data (written by Computer #2) from a different 
        Reflective Memory location
    Closes the RFM2g driver


The rfm2g_receiver.c program (running on Computer #2) does the following:

    Opens the RFM2g driver
    Waits to receive an interrupt event from Computer #1
    Reads the buffer of data (written by Computer #1) from Reflective Memory
    Writes the buffer of data to a different Reflective Memory location
    Sends an interrupt event to Computer #1
    Closes the RFM2g driver


