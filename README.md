# Cgf.CameraControl.CameraController

This project provides an arduino compatible application that allows to control a [Bescor 101](https://www.bescor.com/product-page/mp1ac) Pan/Tilt Head and a [Sony Lanc](http://www.boehmel.de/lanc) enabled camera.

## Quick Start

Due to the strict timing required by the Sony Lanc Protocol this application does ask for new commands in the communication pause during the Lanc transmissions. For Lanc communication the [LibLanc](https://www.arduino.cc/reference/en/libraries/liblanc/) library which was specifically written for this applicataion.

To provide information for this arduino sketch a serial interface must be provided, that delivers requested information in a timely manner. For an example see [Cgf.CameraControl.CameraHost](../Cgf.CameraControl.CameraHost)
