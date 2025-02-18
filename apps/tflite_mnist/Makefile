#
# Realtek Semiconductor Corp.
#

CROSS_COMPILE ?= arm-linux-

CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar
RANLIB := $(CROSS_COMPILE)ranlib
O ?= $(shell pwd)


exe-y = rtk_tflite_mnist

all: $(O)/$(exe-y)

$(O)/$(exe-y):
	$(CXX) $(CFLAGS) $(LIBTFLITE_INC) -I. $(LDFLAGS) \
		example_tflite_mnist.cc \
		models/keras_mnist_conv_model_data.cc \
		testdata/input_int8_test_data.cc \
		testdata/label_int8_test_data.cc \
		$(LIBTFLITE_LIB) -lstdc++ -lm -o $@

clean:
	-rm -f $(O)/*
