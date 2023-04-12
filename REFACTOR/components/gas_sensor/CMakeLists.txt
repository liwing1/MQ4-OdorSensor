set(component_srcs)

idf_component_register(SRCS "gas_sensor.c" "${component_srcs}"
                       PRIV_REQUIRES driver
                       INCLUDE_DIRS ".")
