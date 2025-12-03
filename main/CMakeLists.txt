idf_component_register(
    SRCS 
        "main.c"
        "dht22.c"
        "ssd1306.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        driver
        esp_timer
)

