idf_component_register(SRCS 
	"main.c" 
	"wifiInit.c" 
	"timeMgmt.c" 
	"mqtt.c"
	"counting.c"
	"showRoomState.c"
	"epaperInterface.cpp"
	"wakeup.c"
    EMBED_TXTFILES server_root_cert.pem INCLUDE_DIRS ".")
	set(COMPONENT_REQUIRES "epaper")
