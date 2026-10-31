LOCAL_PATH := $(call my-dir)
GAME_SRC := ../../../../../src

include $(CLEAR_VARS)
LOCAL_MODULE    := circuit-grid
LOCAL_SRC_FILES := \
    $(GAME_SRC)/World.cpp \
    $(GAME_SRC)/Simulation.cpp \
    $(GAME_SRC)/CellRenderer.cpp \
    $(GAME_SRC)/MobileGame.cpp \
    $(GAME_SRC)/main_mobile.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(GAME_SRC)
LOCAL_CPPFLAGS  := -std=c++20 -Wall -O2
LOCAL_LDLIBS    := -landroid -llog
LOCAL_SHARED_LIBRARIES := sfml-system sfml-window sfml-graphics sfml-activity
LOCAL_WHOLE_STATIC_LIBRARIES := sfml-main
include $(BUILD_SHARED_LIBRARY)

# SFML installs itself into $NDK/sources/third_party/sfml
$(call import-module,third_party/sfml)
