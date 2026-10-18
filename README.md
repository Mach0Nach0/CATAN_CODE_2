# Project Name

Modular electronic CATAN board game 

## Overview

This is a modular electronic verison of the board game CATAN. Note, this project is still ongoing, so code may still have bugs. 
Each tile has its own ESP32, and communicates its resource type and edge connections to its neighbours and one master ESP.

## Network Structure

The tiles form a mesh network, with each hexagonal tile supporting
up to six direct neighbour connections—one along each edge.

Each connection uses a single-wire, half-duplex link, meaning data
can travel in either direction, but only one tile can transmit
over that link at a time.

Messages travel beyond immediate neighbours through multi-hop
forwarding. Tiles relay packets across the network using flooding,
with packet IDs to identify duplicates and a time-to-live (TTL)
value to limit how far packets propagate.

## Pictures

![Breadboard prototype wiring (Master tile and 2 normal tiles)](images/20261016_061911613_iOS_result.dng)
![Unsoldered custom PCBs](images/20261016_062016487_iOS_result.dng)
![PCB housing](images/20261016_062154518_iOS_result.dng)
![PCB housing with lid](images/20261016_062215070_iOS_result.dng)
![Tile to tile physical connection](images/20261016_062943588_iOS_result.dng)
![Tile to tile connectro PCB with 3D-printed housing](images/20261016_063025439_iOS_result.dng)
![House and road slots](images/20261016_063156741_iOS_result.dng)
![House and road insert prototypes](images/20261016_063151595_iOS_result.dng)

## Features

- Communication between neighbouring tiles
- Customisable lighting patterns across tiles
- Modular design

## Technologies and Components

- ESP32 C3 super mini
- C++
- ESP-IDF
- Addressable RGBW LEDs
- Pogo pins for modular connections

## How It Works

The master ESP floods the tile network with commands, specifying specific targets and commands to make lighting patterns and other functions. 
