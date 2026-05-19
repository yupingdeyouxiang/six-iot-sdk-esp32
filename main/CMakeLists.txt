idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(SRCS "six_iot_demo.c" 
					INCLUDE_DIRS "."
					REQUIRES esp32_main driver 
					)