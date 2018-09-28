# SPMProject18

Compile 

pthread
``g++ tt_farm.cpp -o farm -std=c++11 -O3 -lm -pg -pthread -L/usr/X11R6/lib -ljpeg -lX11``

Fast flow
``g++ -I . -o ff_farm -std=c++11 ff_farm.cpp -O3 -lm -pthread -L/usr/X11R6/lib -ljpeg -lX11``
 
 
 RUN `./ff_farm img.jpg output/ logo.jpg 130 2 1000` 
 
 Arguments `(130:images ,2:Nw , 1000:10 msecs delay)`

