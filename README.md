### imgcat

![](./images/imgcat.png)

### vidcat

![](./images/vidcat.gif)

### Controls

```
# both programs
p P - change mapping algorithm
wasd/arrow keys - pan
, z - zoom in
. x - zoom out
0 - reset pan / zoom
1 - use two characters for each pixel
l - show debug info
./*cat.o <file>
<media generator> | ./*cat.o
```

```
# imgcat specific
./imgcat.o <file> -p # sample the entire image while starting
./imgcat.o <file> -b # benchmark the color mapping algorithms
```

```
# vidcat specific
v b - skip 5s forward / backward
space - pause / unpause
f - auto zoom options
```

### Building

These are the commands I use, mileage may vary

I no longer target Windows but my libraries were written to support them at one point

```
g++ imgcat.cpp -Ithird_party -Iconsole console/console.linux.cpp console/advancedConsole.cpp -lncursesw -O2 -g -o imgcat.o
g++ vidcat.cpp -Iconsole console/console.linux.cpp console/advancedConsole.cpp -lopencv_core -lopencv_videoio -lopencv_imgproc -lncursesw -I/usr/include/opencv4 -L/usr/lib/x86_64-linux-gnu/ -O2 -g -o vidcat.o
```