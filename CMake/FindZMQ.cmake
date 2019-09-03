find_path(ZMQ_INCLUDE_DIR zmq.h PATH_SUFFIXES zmq)
find_library(ZMQ_LIBRARY zmq)
mark_as_advanced(ZMQ_INCLUDE_DIR ZMQ_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZMQ DEFAULT_MSG
	ZMQ_INCLUDE_DIR ZMQ_LIBRARY)

if(ZMQ_FOUND AND NOT TARGET ZMQ)
  add_library(zmq::zmq UNKNOWN IMPORTED)
  set_target_properties(zmq::zmq PROPERTIES
    IMPORTED_LOCATION "${ZMQ_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ZMQ_INCLUDE_DIR}"
  )
endif()
