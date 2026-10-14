idf_component_register(
    SRCS "src/main.cpp" "src/comms.cpp" "src/bt_transport.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES bt nvs_flash
)
