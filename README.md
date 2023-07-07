### Seminar Room Group 10

To run this project please use the following command

`idf.py -p ${PORT} -b 460800 flash monitor`

The above port PORT should be configured depending on the serial port that the usb of the micro-controller is connected on.

An example would be ```/dev/cu.usbserial-015E0000```

This PORT can be found when running ```ls /dev/cu.*```

There are no additional dependencies to be added in order to run this project.