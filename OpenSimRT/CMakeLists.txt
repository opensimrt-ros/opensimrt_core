# find OpenSim
include_directories(${OpenSim_INCLUDE_DIRS})
link_directories(${OpenSim_LIB_DIR})

include_directories(.)
add_subdirectory(Ros)
add_subdirectory(Common)
add_subdirectory(RealTime)
if(BUILD_VICON)
  add_subdirectory(Vicon)
endif()
if(BUILD_IMU)
	#add_subdirectory(IMU)
endif()
if(BUILD_UIMU)
	#add_subdirectory(UIMU)
endif()

add_subdirectory(Examp)
