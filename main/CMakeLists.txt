idf_component_register(SRCS
    "hello_world_ota_main.cpp"
    "ota.c"
    "wifi.cpp"
    INCLUDE_DIRS ".")

# idf_component_register(SRCS
#     "hello_world_ota_main.cpp"
#     "ota.c"
#     "wifi.cpp"
#     INCLUDE_DIRS "."
#     EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
