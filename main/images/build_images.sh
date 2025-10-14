#!/bin/bash

xxd -i signal-disconnected.bmp > images.c
xxd -i signal-1of4.bmp >> images.c
xxd -i signal-2of4.bmp >> images.c
xxd -i signal-3of4.bmp >> images.c
xxd -i signal-4of4.bmp >> images.c
xxd -i lock-buttons.bmp >> images.c
xxd -i scroll-lock.bmp >> images.c
xxd -i battery-0of6.bmp >> images.c
xxd -i battery-1of6.bmp >> images.c
xxd -i battery-2of6.bmp >> images.c
xxd -i battery-3of6.bmp >> images.c
xxd -i battery-4of6.bmp >> images.c
xxd -i battery-5of6.bmp >> images.c
xxd -i battery-6of6.bmp >> images.c
xxd -i battery-charging.bmp >> images.c
