# Final Project: Distributed systems: paradigms and models
```Context```
The problem is to apply a "marker" of the same dimension onto the images provided by the user.To achieve this, I use the CIMG library to process the images. 

```Aims```
The project is to provide both a sequential and parallel implementation of the problem using the pthread version and the fastflow library (https://github.com/fastflow) and to compare the cost and performance models.

Compile 

pthread
``g++ tt_farm.cpp -o farm -std=c++11 -O3 -lm -pg -pthread -L/usr/X11R6/lib -ljpeg -lX11``

Fast flow
``g++ -I . -o ff_farm -std=c++11 ff_farm.cpp -O3 -lm -pthread -L/usr/X11R6/lib -ljpeg -lX11``
 
 RUN `./ff_farm img.jpg output/ logo.jpg 130 2 1000` 


