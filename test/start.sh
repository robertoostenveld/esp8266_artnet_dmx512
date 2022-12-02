#!/bin/bash

javac -Xlint:unchecked VisualizeDMX.java
java VisualizeDMX
rm *.class
