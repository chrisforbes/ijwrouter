#ifdef WIN32
#	pragma warning( disable: 4103 )
#	pragma pack( push, 1 )
#	ifndef PACKED_STRUCT
#		define PACKED_STRUCT
#	endif
#else
#	ifndef PACKED_STRUCT
#		define PACKED_STRUCT __attribute__(( packed ))
#	endif
#endif
