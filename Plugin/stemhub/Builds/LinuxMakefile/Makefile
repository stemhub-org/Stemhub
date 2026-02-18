# Automatically generated makefile, created by the Projucer
# Don't edit this file! Your changes will be overwritten when you re-save the Projucer project!

# build with "V=1" for verbose builds
ifeq ($(V), 1)
V_AT =
else
V_AT = @
endif

# (this disables dependency generation if multiple architectures are set)
DEPFLAGS := $(if $(word 2, $(TARGET_ARCH)), , -MMD)

ifndef PKG_CONFIG
  PKG_CONFIG=pkg-config
endif

ifndef STRIP
  STRIP=strip
endif

ifndef AR
  AR=ar
endif

ifndef CONFIG
  CONFIG=Debug
endif

JUCE_ARCH_LABEL := $(shell uname -m)

ifeq ($(CONFIG),Debug)
  JUCE_BINDIR := build
  JUCE_LIBDIR := build
  JUCE_OBJDIR := build/intermediate/Debug
  JUCE_OUTDIR := build

  ifeq ($(TARGET_ARCH),)
    TARGET_ARCH := 
  endif

  JUCE_CPPFLAGS := $(DEPFLAGS) "-DLINUX=1" "-DDEBUG=1" "-D_DEBUG=1" "-DJUCE_PROJUCER_VERSION=0x8000c" "-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1" "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=1" "-DJucePlugin_Build_AU=1" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=1" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0" "-DJucePlugin_Enable_IAA=0" "-DJucePlugin_Enable_ARA=0" "-DJucePlugin_Name=\"stemhub\"" "-DJucePlugin_Desc=\"stemhub\"" "-DJucePlugin_Manufacturer=\"yourcompany\"" "-DJucePlugin_ManufacturerWebsite=\"www.yourcompany.com\"" "-DJucePlugin_ManufacturerEmail=\"\"" "-DJucePlugin_ManufacturerCode=0x4d616e75" "-DJucePlugin_PluginCode=0x57626372" "-DJucePlugin_IsSynth=0" "-DJucePlugin_WantsMidiInput=0" "-DJucePlugin_ProducesMidiOutput=0" "-DJucePlugin_IsMidiEffect=0" "-DJucePlugin_EditorRequiresKeyboardFocus=0" "-DJucePlugin_Version=1.0.0" "-DJucePlugin_VersionCode=0x10000" "-DJucePlugin_VersionString=\"1.0.0\"" "-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode" "-DJucePlugin_VSTCategory=kPlugCategEffect" "-DJucePlugin_Vst3Category=\"Fx\"" "-DJucePlugin_AUMainType='aufx'" "-DJucePlugin_AUSubType=JucePlugin_PluginCode" "-DJucePlugin_AUExportPrefix=stemhubAU" "-DJucePlugin_AUExportPrefixQuoted=\"stemhubAU\"" "-DJucePlugin_AUManufacturerCode=JucePlugin_ManufacturerCode" "-DJucePlugin_CFBundleIdentifier=com.yourcompany.stemhub" "-DJucePlugin_AAXIdentifier=com.yourcompany.stemhub" "-DJucePlugin_AAXManufacturerCode=JucePlugin_ManufacturerCode" "-DJucePlugin_AAXProductId=JucePlugin_PluginCode" "-DJucePlugin_AAXCategory=0" "-DJucePlugin_AAXDisableBypass=0" "-DJucePlugin_AAXDisableMultiMono=0" "-DJucePlugin_IAAType=0x61757278" "-DJucePlugin_IAASubType=JucePlugin_PluginCode" "-DJucePlugin_IAAName=\"yourcompany: stemhub\"" "-DJucePlugin_VSTNumMidiInputs=16" "-DJucePlugin_VSTNumMidiOutputs=16" "-DJucePlugin_ARAContentTypes=0" "-DJucePlugin_ARATransformationFlags=0" "-DJucePlugin_ARAFactoryID=\"com.yourcompany.stemhub.factory\"" "-DJucePlugin_ARADocumentArchiveID=\"com.yourcompany.stemhub.aradocumentarchive.1.0.0\"" "-DJucePlugin_ARACompatibleArchiveIDs=\"\"" "-DJUCE_STANDALONE_APPLICATION=JucePlugin_Build_Standalone" "-DJUCER_LINUX_MAKE_6D53C8B4=1" "-DJUCE_APP_VERSION=1.0.0" "-DJUCE_APP_VERSION_HEX=0x10000" -I$(HOME)/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK -I../../JuceLibraryCode $(CPPFLAGS)

  JUCE_CPPFLAGS_VST3 := 
  JUCE_CFLAGS_VST3 := -fPIC -fvisibility=hidden
  JUCE_LDFLAGS_VST3 := -shared -Wl,--no-undefined
  JUCE_VST3DIR := stemhub.vst3
  VST3_PLATFORM_ARCH := $(shell $(CXX) make_helpers/arch_detection.cpp 2>&1 | tr '\n' ' ' | sed "s/.*JUCE_ARCH \([a-zA-Z0-9_-]*\).*/\1/")
  JUCE_VST3SUBDIR := Contents/$(VST3_PLATFORM_ARCH)-linux
  JUCE_TARGET_VST3 := $(JUCE_VST3DIR)/$(JUCE_VST3SUBDIR)/stemhub.so
  JUCE_VST3DESTDIR := $(HOME)/.vst3
  JUCE_COPYCMD_VST3 := $(JUCE_OUTDIR)/$(JUCE_VST3DIR) $(JUCE_VST3DESTDIR)

  JUCE_CPPFLAGS_STANDALONE_PLUGIN := 
  JUCE_TARGET_STANDALONE_PLUGIN := stemhub

  JUCE_CPPFLAGS_SHARED_CODE :=  "-DJUCE_SHARED_CODE=1"
  JUCE_CFLAGS_SHARED_CODE := -fPIC -fvisibility=hidden
  JUCE_TARGET_SHARED_CODE := stemhub.a

  JUCE_CPPFLAGS_VST3_MANIFEST_HELPER := 
  JUCE_TARGET_VST3_MANIFEST_HELPER := juce_vst3_helper

  JUCE_CFLAGS += $(JUCE_CPPFLAGS) $(TARGET_ARCH) -fPIC -g -ggdb -O0 $(CFLAGS)
  JUCE_CXXFLAGS += $(JUCE_CFLAGS) -std=c++17 $(CXXFLAGS)
  JUCE_LDFLAGS += $(TARGET_ARCH) -L$(JUCE_BINDIR) -L$(JUCE_LIBDIR) -fvisibility=hidden $(LDFLAGS)

  CLEANCMD = rm -rf $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3) $(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER) $(JUCE_OBJDIR)
endif

ifeq ($(CONFIG),Release)
  JUCE_BINDIR := build
  JUCE_LIBDIR := build
  JUCE_OBJDIR := build/intermediate/Release
  JUCE_OUTDIR := build

  ifeq ($(TARGET_ARCH),)
    TARGET_ARCH := 
  endif

  JUCE_CPPFLAGS := $(DEPFLAGS) "-DLINUX=1" "-DNDEBUG=1" "-DJUCE_PROJUCER_VERSION=0x8000c" "-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1" "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=1" "-DJucePlugin_Build_AU=1" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=1" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0" "-DJucePlugin_Enable_IAA=0" "-DJucePlugin_Enable_ARA=0" "-DJucePlugin_Name=\"stemhub\"" "-DJucePlugin_Desc=\"stemhub\"" "-DJucePlugin_Manufacturer=\"yourcompany\"" "-DJucePlugin_ManufacturerWebsite=\"www.yourcompany.com\"" "-DJucePlugin_ManufacturerEmail=\"\"" "-DJucePlugin_ManufacturerCode=0x4d616e75" "-DJucePlugin_PluginCode=0x57626372" "-DJucePlugin_IsSynth=0" "-DJucePlugin_WantsMidiInput=0" "-DJucePlugin_ProducesMidiOutput=0" "-DJucePlugin_IsMidiEffect=0" "-DJucePlugin_EditorRequiresKeyboardFocus=0" "-DJucePlugin_Version=1.0.0" "-DJucePlugin_VersionCode=0x10000" "-DJucePlugin_VersionString=\"1.0.0\"" "-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode" "-DJucePlugin_VSTCategory=kPlugCategEffect" "-DJucePlugin_Vst3Category=\"Fx\"" "-DJucePlugin_AUMainType='aufx'" "-DJucePlugin_AUSubType=JucePlugin_PluginCode" "-DJucePlugin_AUExportPrefix=stemhubAU" "-DJucePlugin_AUExportPrefixQuoted=\"stemhubAU\"" "-DJucePlugin_AUManufacturerCode=JucePlugin_ManufacturerCode" "-DJucePlugin_CFBundleIdentifier=com.yourcompany.stemhub" "-DJucePlugin_AAXIdentifier=com.yourcompany.stemhub" "-DJucePlugin_AAXManufacturerCode=JucePlugin_ManufacturerCode" "-DJucePlugin_AAXProductId=JucePlugin_PluginCode" "-DJucePlugin_AAXCategory=0" "-DJucePlugin_AAXDisableBypass=0" "-DJucePlugin_AAXDisableMultiMono=0" "-DJucePlugin_IAAType=0x61757278" "-DJucePlugin_IAASubType=JucePlugin_PluginCode" "-DJucePlugin_IAAName=\"yourcompany: stemhub\"" "-DJucePlugin_VSTNumMidiInputs=16" "-DJucePlugin_VSTNumMidiOutputs=16" "-DJucePlugin_ARAContentTypes=0" "-DJucePlugin_ARATransformationFlags=0" "-DJucePlugin_ARAFactoryID=\"com.yourcompany.stemhub.factory\"" "-DJucePlugin_ARADocumentArchiveID=\"com.yourcompany.stemhub.aradocumentarchive.1.0.0\"" "-DJucePlugin_ARACompatibleArchiveIDs=\"\"" "-DJUCE_STANDALONE_APPLICATION=JucePlugin_Build_Standalone" "-DJUCER_LINUX_MAKE_6D53C8B4=1" "-DJUCE_APP_VERSION=1.0.0" "-DJUCE_APP_VERSION_HEX=0x10000" -I$(HOME)/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK -I../../JuceLibraryCode $(CPPFLAGS)

  JUCE_CPPFLAGS_VST3 := 
  JUCE_CFLAGS_VST3 := -fPIC -fvisibility=hidden
  JUCE_LDFLAGS_VST3 := -shared -Wl,--no-undefined
  JUCE_VST3DIR := stemhub.vst3
  VST3_PLATFORM_ARCH := $(shell $(CXX) make_helpers/arch_detection.cpp 2>&1 | tr '\n' ' ' | sed "s/.*JUCE_ARCH \([a-zA-Z0-9_-]*\).*/\1/")
  JUCE_VST3SUBDIR := Contents/$(VST3_PLATFORM_ARCH)-linux
  JUCE_TARGET_VST3 := $(JUCE_VST3DIR)/$(JUCE_VST3SUBDIR)/stemhub.so
  JUCE_VST3DESTDIR := $(HOME)/.vst3
  JUCE_COPYCMD_VST3 := $(JUCE_OUTDIR)/$(JUCE_VST3DIR) $(JUCE_VST3DESTDIR)

  JUCE_CPPFLAGS_STANDALONE_PLUGIN := 
  JUCE_TARGET_STANDALONE_PLUGIN := stemhub

  JUCE_CPPFLAGS_SHARED_CODE :=  "-DJUCE_SHARED_CODE=1"
  JUCE_CFLAGS_SHARED_CODE := -fPIC -fvisibility=hidden
  JUCE_TARGET_SHARED_CODE := stemhub.a

  JUCE_CPPFLAGS_VST3_MANIFEST_HELPER := 
  JUCE_TARGET_VST3_MANIFEST_HELPER := juce_vst3_helper

  JUCE_CFLAGS += $(JUCE_CPPFLAGS) $(TARGET_ARCH) -fPIC -O3 $(CFLAGS)
  JUCE_CXXFLAGS += $(JUCE_CFLAGS) -std=c++17 $(CXXFLAGS)
  JUCE_LDFLAGS += $(TARGET_ARCH) -L$(JUCE_BINDIR) -L$(JUCE_LIBDIR) -fvisibility=hidden $(LDFLAGS)

  CLEANCMD = rm -rf $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3) $(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER) $(JUCE_OBJDIR)
endif

OBJECTS_ALL := \

OBJECTS_VST3 := \

OBJECTS_STANDALONE_PLUGIN := \

OBJECTS_SHARED_CODE := \
  $(JUCE_OBJDIR)/PluginProcessor_a059e380.o \
  $(JUCE_OBJDIR)/PluginEditor_94d4fb09.o \

OBJECTS_VST3_MANIFEST_HELPER := \
  $(JUCE_OBJDIR)/juce_VST3ManifestHelper_d62bf7a7.o \

.PHONY: clean all strip VST3 Standalone VST3_MANIFEST_HELPER

all : VST3 Standalone VST3_MANIFEST_HELPER

VST3 : $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3)
Standalone : $(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN)
VST3_MANIFEST_HELPER : $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER)


$(JUCE_OUTDIR)/$(JUCE_TARGET_VST3) : $(OBJECTS_VST3) $(JUCE_OBJDIR)/execinfo.cmd $(RESOURCES) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER)
	@echo Linking "stemhub - VST3"
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	-$(V_AT)mkdir -p $(JUCE_LIBDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)/$(JUCE_VST3DIR)/$(JUCE_VST3SUBDIR)
	$(V_AT)$(CXX) -o $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3) $(OBJECTS_VST3) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_LDFLAGS) $(shell cat $(JUCE_OBJDIR)/execinfo.cmd) $(JUCE_LDFLAGS_VST3) $(RESOURCES) $(TARGET_ARCH)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)/$(JUCE_VST3DIR)/Contents/Resources
	-$(V_AT)rm -f $(JUCE_OUTDIR)/$(JUCE_VST3DIR)/Contents/moduleinfo.json
	$(V_AT) $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER) > $(JUCE_OUTDIR)/$(JUCE_VST3DIR)/Contents/Resources/moduleinfo.json
	-$(V_AT)[ ! "$(JUCE_VST3DESTDIR)" ] || (mkdir -p $(JUCE_VST3DESTDIR) && cp -R $(JUCE_COPYCMD_VST3))

$(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN) : $(OBJECTS_STANDALONE_PLUGIN) $(JUCE_OBJDIR)/execinfo.cmd $(RESOURCES) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE)
	@echo Linking "stemhub - Standalone Plugin"
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	-$(V_AT)mkdir -p $(JUCE_LIBDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)
	$(V_AT)$(CXX) -o $(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN) $(OBJECTS_STANDALONE_PLUGIN) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_LDFLAGS) $(shell cat $(JUCE_OBJDIR)/execinfo.cmd) $(JUCE_LDFLAGS_STANDALONE_PLUGIN) $(RESOURCES) $(TARGET_ARCH)

$(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) : $(OBJECTS_SHARED_CODE) $(JUCE_OBJDIR)/execinfo.cmd $(RESOURCES)
	@echo Linking "stemhub - Shared Code"
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	-$(V_AT)mkdir -p $(JUCE_LIBDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)
	$(V_AT)$(AR) -rcs $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(OBJECTS_SHARED_CODE)

$(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER) : $(OBJECTS_VST3_MANIFEST_HELPER) $(JUCE_OBJDIR)/execinfo.cmd $(RESOURCES) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_OBJDIR)/cxxfs.cmd
	@echo Linking "stemhub - VST3 Manifest Helper"
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	-$(V_AT)mkdir -p $(JUCE_LIBDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)
	$(V_AT)$(CXX) -o $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER) $(OBJECTS_VST3_MANIFEST_HELPER) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_LDFLAGS) $(shell cat $(JUCE_OBJDIR)/execinfo.cmd) $(shell cat $(JUCE_OBJDIR)/cxxfs.cmd) $(RESOURCES) $(TARGET_ARCH)

$(JUCE_OBJDIR)/PluginProcessor_a059e380.o: ../../Source/PluginProcessor.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PluginProcessor.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PluginEditor_94d4fb09.o: ../../Source/PluginEditor.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PluginEditor.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/juce_VST3ManifestHelper_d62bf7a7.o: ~/JUCE/modules/juce_audio_plugin_client/VST3/juce_VST3ManifestHelper.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling juce_VST3ManifestHelper.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_VST3_MANIFEST_HELPER) $(JUCE_CFLAGS_VST3_MANIFEST_HELPER) -o "$@" -c "$<"

$(JUCE_OBJDIR)/execinfo.cmd:
	-$(V_AT)mkdir -p $(@D)
	-@if [ -z "$(V_AT)" ]; then echo "Checking if we need to link libexecinfo"; fi
	$(V_AT)printf "int main() { return 0; }" | $(CXX) -x c++ -o $(@D)/execinfo.x -lexecinfo - >/dev/null 2>&1 && printf -- "-lexecinfo" > "$@" || touch "$@"

$(JUCE_OBJDIR)/cxxfs.cmd:
	-$(V_AT)mkdir -p $(@D)
	-@if [ -z "$(V_AT)" ]; then echo "Checking if we need to link stdc++fs"; fi
	$(V_AT)printf "int main() { return 0; }" | $(CXX) -x c++ -o $(@D)/cxxfs.x -lstdc++fs - >/dev/null 2>&1 && printf -- "-lstdc++fs" > "$@" || touch "$@"

clean:
	@echo Cleaning stemhub
	$(V_AT)$(CLEANCMD)

strip:
	@echo Stripping stemhub
	-$(V_AT)$(STRIP) --strip-unneeded $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3)
	-$(V_AT)$(STRIP) --strip-unneeded $(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN)
	-$(V_AT)$(STRIP) --strip-unneeded $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER)

-include $(OBJECTS_VST3:%.o=%.d)
-include $(OBJECTS_STANDALONE_PLUGIN:%.o=%.d)
-include $(OBJECTS_SHARED_CODE:%.o=%.d)
-include $(OBJECTS_VST3_MANIFEST_HELPER:%.o=%.d)
