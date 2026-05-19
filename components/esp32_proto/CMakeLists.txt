# CMake configuration for the 'esp32_proto' component.

# 设置路径
set(PROTOC_C "C:/msys64/mingw64/bin/protoc-c")
set(PROTO_DIR "${CMAKE_CURRENT_LIST_DIR}/proto")

# 将生成的文件放在组件目录下，而不是构建目录
set(OUTPUT_DIR "${CMAKE_CURRENT_LIST_DIR}/generated")

# 查找所有 .proto 文件
file(GLOB PROTO_FILES "${PROTO_DIR}/*.proto")

# 生成文件列表
set(GENERATED_SRCS "")
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(GEN_C "${OUTPUT_DIR}/${PROTO_NAME}.pb-c.c")
    set(GEN_H "${OUTPUT_DIR}/${PROTO_NAME}.pb-c.h")
    list(APPEND GENERATED_SRCS ${GEN_C})
    
    # 在配置时生成文件（而不是构建时）
    if(EXISTS "${PROTO_FILE}" AND NOT IDF_BUILD_PROCESS_IS_SCRIPTABLE)
        # 确保输出目录存在
        file(MAKE_DIRECTORY ${OUTPUT_DIR})
        
        execute_process(
            COMMAND ${PROTOC_C} 
                    --proto_path=${PROTO_DIR}
                    --c_out=${OUTPUT_DIR}
                    ${PROTO_FILE}
            RESULT_VARIABLE RESULT
            OUTPUT_VARIABLE OUTPUT
            ERROR_VARIABLE ERROR
        )
        if(RESULT EQUAL 0)
            message(STATUS "Generated: ${GEN_C}")
            message(STATUS "Generated: ${GEN_H}")
        else()
            message(WARNING "Failed to generate ${PROTO_NAME}.pb-c files")
            message(WARNING "Error: ${ERROR}")
        endif()
    endif()
endforeach()

# 注册组件
idf_component_register(
    SRCS 
        ${GENERATED_SRCS}
    REQUIRES 
        protobuf-c 
    INCLUDE_DIRS 
        "generated"
        "."
)