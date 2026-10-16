idf_component_register(
    SRCS 
        "AppMain.cpp"
        "Tile.cpp"
        "MasterTile.cpp"
        "TileComms.cpp"
        "Edge.cpp"
        "LEDController.cpp"
        "BasicTileComms.cpp"
        "MasterTileComms.cpp"
        "."
    INCLUDE_DIRS 
        "."
        
    REQUIRES
        esp_driver_gpio
        esp_timer
    )