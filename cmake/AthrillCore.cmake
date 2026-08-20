include_guard(GLOBAL)

get_filename_component(ATHRILL_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(ATHRILL_SRC_DIR "${ATHRILL_ROOT}/src")
set(ATHRILL_APL_DIR "${ATHRILL_ROOT}/apl")

foreach(_athrill_required_path
        "${ATHRILL_SRC_DIR}/inc/std_types.h"
        "${ATHRILL_SRC_DIR}/main/main.c"
        "${ATHRILL_SRC_DIR}/cpu/cpu.h")
    if(NOT EXISTS "${_athrill_required_path}")
        message(FATAL_ERROR
            "Invalid Athrill source tree: required path not found: "
            "${_athrill_required_path}")
    endif()
endforeach()

set(ATHRILL_BASE_INCLUDE_DIRS
    "${ATHRILL_SRC_DIR}/inc"
    "${ATHRILL_SRC_DIR}/lib"
)

set(ATHRILL_WINDOWS_COMPAT_INCLUDE_DIR
    "${ATHRILL_SRC_DIR}/platform/windows/include")

set(ATHRILL_MAIN_SOURCES
    "${ATHRILL_SRC_DIR}/main/cpuemu.c"
    "${ATHRILL_SRC_DIR}/main/option/option.c"
    "${ATHRILL_SRC_DIR}/debugger/executor/cpu_control/dbg_cpu_callback.c"
    "${ATHRILL_SRC_DIR}/debugger/executor/cpu_control/dbg_cpu_control.c"
    "${ATHRILL_SRC_DIR}/debugger/executor/cpu_control/dbg_cpu_thread_control.c"
)

set(ATHRILL_CUI_SOURCES
    "${ATHRILL_SRC_DIR}/lib/cui/cui_ops.c"
    "${ATHRILL_SRC_DIR}/lib/cui/stdio/cui_ops_stdio.c"
    "${ATHRILL_SRC_DIR}/lib/cui/udp/cui_ops_udp.c"
    "${ATHRILL_SRC_DIR}/debugger/interaction/front/parser/dbg_parser_config.c"
    "${ATHRILL_SRC_DIR}/debugger/interaction/front/parser/dbg_parser.c"
    "${ATHRILL_SRC_DIR}/debugger/interaction/front/parser/concrete_parser/dbg_std_parser.c"
    "${ATHRILL_SRC_DIR}/debugger/executor/concrete_executor/dbg_std_executor.c"
    "${ATHRILL_SRC_DIR}/debugger/executor/concrete_executor/util/dbg_print_data_type.c"
)

set(ATHRILL_DEVICE_COMMON_SOURCES
    "${ATHRILL_SRC_DIR}/device/peripheral/serial/fifo/serial_fifo.c"
)

set(ATHRILL_DEVICE_THREAD_SOURCES
    "${ATHRILL_SRC_DIR}/device/peripheral/athrill_mpthread.c"
)

set(ATHRILL_DEVICE_EXDEV_SOURCES
    "${ATHRILL_SRC_DIR}/device/peripheral/athrill_device.c"
    "${ATHRILL_SRC_DIR}/device/peripheral/athrill_syscall_device.c"
)

set(ATHRILL_BUS_SOURCES
    "${ATHRILL_SRC_DIR}/bus/bus.c"
)

set(ATHRILL_LOADER_SOURCES
    "${ATHRILL_SRC_DIR}/lib/symbol_ops.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/elf_section.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/elf_dwarf_line.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/elf_dwarf_loc.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/elf_dwarf_info.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/elf_dwarf_abbrev.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/elf_dwarf_util.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/elf_dwarf_info_ops.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/file_address_mapping.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/data_type/elf_dwarf_data_type.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/data_type/elf_dwarf_base_type.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/data_type/elf_dwarf_typedef_type.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/data_type/elf_dwarf_pointer_type.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/data_type/elf_dwarf_struct_type.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/data_type/elf_dwarf_enum_type.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/data_type/elf_dwarf_array_type.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/data_type/elf_dwarf_variable_type.c"
    "${ATHRILL_SRC_DIR}/lib/dwarf/data_type/elf_dwarf_subprogram_type.c"
)

set(ATHRILL_MPU_SOURCES
    "${ATHRILL_SRC_DIR}/device/mpu/mpu.c"
    "${ATHRILL_SRC_DIR}/device/mpu/mpu_malloc.c"
    "${ATHRILL_SRC_DIR}/device/mpu/loader/loader.c"
)

set(ATHRILL_STD_COMMON_SOURCES
    "${ATHRILL_SRC_DIR}/lib/hash.c"
    "${ATHRILL_SRC_DIR}/lib/token.c"
    "${ATHRILL_SRC_DIR}/lib/file.c"
    "${ATHRILL_SRC_DIR}/lib/winsock_wrapper/winsock_wrapper.c"
    "${ATHRILL_SRC_DIR}/lib/udp/udp_comm.c"
)

set(ATHRILL_STD_POSIX_SOURCES
    "${ATHRILL_SRC_DIR}/lib/comm_buffer.c"
    "${ATHRILL_SRC_DIR}/lib/tcp/tcp_socket.c"
    "${ATHRILL_SRC_DIR}/lib/tcp/tcp_client.c"
    "${ATHRILL_SRC_DIR}/lib/tcp/tcp_connection.c"
    "${ATHRILL_SRC_DIR}/lib/tcp/tcp_server.c"
)

set(ATHRILL_MROS_SRC_DIR
    "${ATHRILL_SRC_DIR}/device/peripheral/mros-dev/mros-src")
set(ATHRILL_MROS_DEVICE_DIR
    "${ATHRILL_SRC_DIR}/device/peripheral/mros-dev/mros-athrill")
set(ATHRILL_MROS_ROS_VERSION "kinetic")

set(ATHRILL_MROS_INCLUDE_DIRS
    "${ATHRILL_MROS_SRC_DIR}/api"
    "${ATHRILL_MROS_SRC_DIR}/inc"
    "${ATHRILL_MROS_SRC_DIR}/os/target/os_asp"
    "${ATHRILL_MROS_SRC_DIR}/protocol/cimpl"
    "${ATHRILL_MROS_SRC_DIR}/node/cimpl"
    "${ATHRILL_MROS_SRC_DIR}/topic/cimpl"
    "${ATHRILL_MROS_SRC_DIR}/comm/cimpl/target/lwip"
    "${ATHRILL_MROS_SRC_DIR}/comm/cimpl"
    "${ATHRILL_MROS_SRC_DIR}/packet/cimpl"
    "${ATHRILL_MROS_SRC_DIR}/packet/template/version/${ATHRILL_MROS_ROS_VERSION}"
    "${ATHRILL_MROS_SRC_DIR}/packet/cimpl/version/${ATHRILL_MROS_ROS_VERSION}"
    "${ATHRILL_MROS_SRC_DIR}/transfer/cimpl"
    "${ATHRILL_MROS_DEVICE_DIR}/api"
    "${ATHRILL_MROS_DEVICE_DIR}/config"
    "${ATHRILL_MROS_DEVICE_DIR}/config/os/target/os_asp"
    "${ATHRILL_MROS_DEVICE_DIR}/target"
    "${ATHRILL_MROS_DEVICE_DIR}/target/os"
    "${ATHRILL_MROS_DEVICE_DIR}/device"
)

set(ATHRILL_MROS_SOURCES
    "${ATHRILL_MROS_SRC_DIR}/comm/cimpl/mros_comm_socket_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/comm/cimpl/mros_comm_tcp_client_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/comm/cimpl/mros_comm_tcp_client_factory_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/comm/cimpl/mros_comm_tcp_server_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/comm/cimpl/target/lwip/mros_comm_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/lib/mros_memory.c"
    "${ATHRILL_MROS_SRC_DIR}/lib/mros_wait_queue.c"
    "${ATHRILL_MROS_SRC_DIR}/lib/mros_name.c"
    "${ATHRILL_MROS_SRC_DIR}/node/cimpl/mros_node_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/os/mros_exclusive_area.c"
    "${ATHRILL_MROS_SRC_DIR}/os/target/os_asp/mros_os.c"
    "${ATHRILL_MROS_SRC_DIR}/packet/cimpl/version/${ATHRILL_MROS_ROS_VERSION}/mros_packet_decoder_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/packet/cimpl/version/${ATHRILL_MROS_ROS_VERSION}/mros_packet_encoder_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/protocol/cimpl/mros_protocol_client_rpc_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/protocol/cimpl/mros_protocol_operation_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/protocol/cimpl/mros_protocol_publish_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/protocol/cimpl/mros_protocol_server_proc_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/protocol/cimpl/mros_protocol_slave_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/protocol/cimpl/mros_protocol_subscribe_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/protocol/cimpl/mros_protocol_master_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/topic/cimpl/mros_topic_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/topic/cimpl/mros_topic_connector_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/topic/cimpl/mros_topic_connector_factory_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/transfer/cimpl/mros_topic_data_publisher_cimpl.c"
    "${ATHRILL_MROS_SRC_DIR}/transfer/cimpl/mros_topic_data_subscriber_cimpl.c"
    "${ATHRILL_MROS_DEVICE_DIR}/api/ros_cimpl.c"
    "${ATHRILL_MROS_DEVICE_DIR}/device/athrill_mros_device.c"
    "${ATHRILL_MROS_DEVICE_DIR}/config/mros_sys_config.c"
    "${ATHRILL_MROS_DEVICE_DIR}/config/os/target/os_asp/mros_os_config.c"
    "${ATHRILL_MROS_DEVICE_DIR}/target/os/os_asp.c"
    "${ATHRILL_MROS_DEVICE_DIR}/target/os/mros_exclusive_ops_linux.c"
    "${ATHRILL_MROS_DEVICE_DIR}/target/lwip/lwip_linux.c"
)

function(athrill_apply_common_settings target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR
            "athrill_apply_common_settings: target does not exist: ${target_name}")
    endif()

    target_include_directories("${target_name}" PRIVATE
        ${ATHRILL_BASE_INCLUDE_DIRS})
    if(MSVC)
        target_compile_features("${target_name}" PRIVATE c_std_11)
        target_compile_options("${target_name}" PRIVATE
            /utf-8
            /W4
            $<$<CONFIG:Release>:/O2>
        )
        target_compile_definitions("${target_name}" PRIVATE
            _CRT_SECURE_NO_WARNINGS
        )
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_features("${target_name}" PRIVATE c_std_99)
        set_target_properties("${target_name}" PROPERTIES C_EXTENSIONS ON)
        target_compile_options("${target_name}" PRIVATE
            -g
            -Wall
            -Wunknown-pragmas
            -Wimplicit-int
            -Wtrigraphs
            -O3
        )
    endif()

    if(APPLE)
        target_compile_definitions("${target_name}" PRIVATE
            OS_LINUX
            OS_MAC
            SUPRESS_DETECT_WARNING_MESSAGE
        )
    elseif(UNIX)
        target_compile_definitions("${target_name}" PRIVATE
            OS_LINUX
            SUPRESS_DETECT_WARNING_MESSAGE
        )
    elseif(NOT WIN32)
        message(FATAL_ERROR
            "Unsupported Athrill host platform: ${CMAKE_SYSTEM_NAME}")
    endif()

    if(WIN32)
        target_include_directories("${target_name}" PRIVATE
            "${ATHRILL_WINDOWS_COMPAT_INCLUDE_DIR}")
    endif()
endfunction()
