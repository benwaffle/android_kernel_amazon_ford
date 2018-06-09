include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y				+= kd_sensorlist.o
ifeq ($(ARCH_MTK_PROJECT), tb27A1910)
obj-y				+= gc0312yuv_Sensor.o
else
  ifeq ($(ARCH_MTK_PROJECT), ford)
	ifeq ($(CONFIG_AUSTIN_PROJECT), y)
	  obj-y				+= sp2509mipiraw_Sensor.o
	  obj-y				+= gc2355mipiraw_huaquan_Sensor.o
	  obj-y				+= sp0a19raw_Sensor.o
	  #obj-y			+= gc0312yuv_blx_Sensor.o
	  obj-y				+= gc0312mipi_Sensor.o
	else
	  obj-y				+= gc0312yuv_Sensor.o
	  obj-y				+= hi704yuv_Sensor.o
	endif
  else
   ifeq ($(ARCH_MTK_PROJECT), tb27A1912)
     obj-y                         += gc0312yuv_Sensor.o
   else
     ifeq ($(ARCH_MTK_PROJECT), ford_proto)
       obj-y                         += gc0312yuv_Sensor.o
     else
    obj-y				+= ov2659yuv_Sensor.o
   endif
  endif
endif
endif

# $(CONFIG_MTK_PLATFORM)
$(warning "xilog project is:$(ARCH_MTK_PROJECT)")
ifeq ($(ARCH_MTK_PROJECT), tb8127p4)
	obj-y				+= ov5647_Sensor_p4.o
else
  	ifeq ($(ARCH_MTK_PROJECT), tb8127p4_25)
		obj-y				+= ov5647_Sensor_p4_25.o
	else
		ifeq ($(ARCH_MTK_PROJECT), tb8127p1_25)
			obj-y				+= ov5647_Sensor_p1_25.o
		else
 			ifeq ($(ARCH_MTK_PROJECT), tb27A1910)
				obj-y				+= gc2355mipiraw_Sensor.o
			else
  			ifeq ($(ARCH_MTK_PROJECT), ford)
	  			obj-y				+= gc2355mipiraw_Sensor.o
	  			obj-y				+= gc2356mipiraw_Sensor.o
		  	else
                            ifeq ($(ARCH_MTK_PROJECT), tb27A1912)
                                obj-y                           += gc2355mipiraw_Sensor.o
                            else
                              ifeq ($(ARCH_MTK_PROJECT), ford_proto)
                                obj-y                           += gc2355mipiraw_Sensor.o
                              else
				 obj-y				+= ov5647_Sensor.o
			    endif
                        endif
		  endif
		endif
	endif
endif
endif

