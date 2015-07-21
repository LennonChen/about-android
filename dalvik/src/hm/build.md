hm.jar build过程
========================================

Sources
----------------------------------------

#### Hm.java

```
package com.android.commands.hm;

public final class Hm {
    public static void main(String[] args) {
        System.out.println("Hello World!\n");
    }
}
```

#### Android.mk

```
# Copyright 2007 The Android Open Source Project
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)
LOCAL_MODULE := hm
include $(BUILD_JAVA_LIBRARY)
```

中间文件
----------------------------------------

.
├── classes

│   └── com

│       └── android

│           └── commands

│               └── hm

│                   └── Hm.class

├── classes.dex

├── classes-full-debug.jar

├── classes.jar

├── classes-jarjar.jar

├── classes-with-local.dex

├── emma_out

│   └── lib

│       └── classes-jarjar.jar

├── javalib.jar

└── noproguard.classes.jar

生成过程
----------------------------------------

### LOCAL_MODULE

path: build/core/base_rules.mk
```
.PHONY: $(LOCAL_MODULE)
$(LOCAL_MODULE): $(LOCAL_BUILT_MODULE) $(LOCAL_INSTALLED_MODULE)
```

LOCAL_MODULE = hm 依赖于 LOCAL_INSTALLED_MODULE

* LOCAL_INSTALLED_MODULE

out/target/product/cancro/system/framework/hm.jar

### LOCAL_INSTALLED_MODULE

path: build/core/base_rules.mk
```
$(LOCAL_INSTALLED_MODULE): $(LOCAL_BUILT_MODULE) | $(ACP)
    @echo "Install: $< --> $@"
    $(copy-file-to-new-target)
    $(PRIVATE_POST_INSTALL_CMD)
```

LOCAL_INSTALLED_MODULE 依赖于 LOCAL_BUILT_MODULE

* LOCAL_BUILT_MODULE

  out/target/product/cancro/obj/JAVA_LIBRARIES/hm_intermediates/javalib.jar

### LOCAL_BUILT_MODULE

path: build/core/java_library.mk
```
$(LOCAL_BUILT_MODULE) : $(common_javalib.jar) | $(ACP)
    $(call copy-file-to-target)
```

LOCAL_BUILT_MODULE 依赖于 common_javalib.jar

* common_javalib.jar

  out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/javalib.jar

### common_javalib.jar

path: build/core/java_library.mk
```
$(common_javalib.jar): PRIVATE_DEX_FILE := $(built_dex)
$(common_javalib.jar): $(built_dex) $(java_resource_sources)
    $(create-empty-package)
    $(add-dex-to-package)
    $(add-carried-java-resources)
ifneq ($(extra_jar_args),)
    $(add-java-resources-to-package)
endif
```

common_javalib.jar 依赖于 built_dex

* built_dex

  out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes.dex

##### create-empty-package

path: build/core/definitions.mk
```
# Create a mostly-empty .jar file that we'll add to later.
# The MacOS jar tool doesn't like creating empty jar files,
# so we need to give it something.
define create-empty-package
@mkdir -p $(dir $@)
$(hide) touch $(dir $@)/dummy
$(hide) (cd $(dir $@) && jar cf $(notdir $@) dummy)
$(hide) zip -qd $@ dummy
$(hide) rm $(dir $@)/dummy
endef
```

##### add-dex-to-package

path: build/core/definitions.mk
```
#TODO: update the manifest to point to the dex file
define add-dex-to-package
$(if $(filter classes.dex,$(notdir $(PRIVATE_DEX_FILE))),\
$(hide) zip -qj $@ $(PRIVATE_DEX_FILE),\
$(hide) _adtp_classes_dex=$(dir $(PRIVATE_DEX_FILE))classes.dex; \
cp $(PRIVATE_DEX_FILE) $$_adtp_classes_dex && \
zip -qj $@ $$_adtp_classes_dex && rm -f $$_adtp_classes_dex)
endef
```

### built_dex

path: build/core/java.mk
```
$(built_dex): $(built_dex_intermediate) | $(ACP)
    @echo "--> $(built_dex_intermediate)"
    @echo Copying: $@
    $(hide) $(ACP_VP) -fp $< $@
ifneq ($(GENERATE_DEX_DEBUG),)
    $(install-dex-debug)
endif
```

built_dex 依赖于 built_dex_intermediate

* built_dex_intermediate

  out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes-with-local.dex

### built_dex_intermediate

path: build/core/java.mk
```
$(built_dex_intermediate): $(full_classes_proguard_jar) $(DX)
    $(transform-classes.jar-to-dex)
```

built_dex_intermediate 依赖于 full_classes_proguard_jar

* full_classes_proguard_jar

  out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/noproguard.classes.jar

##### transform-classes.jar-to-dex

path: build/core/definitions.mk
```
#TODO: use a smaller -Xmx value for most libraries;
#      only core.jar and framework.jar need a heap this big.
# Avoid the memory arguments on Windows, dx fails to load for some reason with them.
define transform-classes.jar-to-dex
@echo "target Dex: $(PRIVATE_MODULE)"
@mkdir -p $(dir $@)
$(hide) $(DX_VP) \
    $(if $(findstring windows,$(HOST_OS)),,-JXms16M -JXmx2048M) \
    --dex --output=$@ \
    $(incremental_dex) \
    $(if $(NO_OPTIMIZE_DX), \
        --no-optimize) \
    $(if $(GENERATE_DEX_DEBUG), \
        --debug --verbose \
            --dump-to=$(@:.dex=.lst) \
                --dump-width=1000) \
    $(PRIVATE_DX_FLAGS) \
    $<
endef
```

### full_classes_proguard_jar

path: build/core/java.mk
```
$(full_classes_proguard_jar) : $(full_classes_jar) | $(ACP)
    @echo Copying: $@
    $(hide) $(ACP_VP) -fp $< $@
```

full_classes_proguard_jar 依赖于 full_classes_jar

* full_classes_jar

  out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes.jar

### full_classes_jar

path: build/core/java.mk
```
# Keep a copy of the jar just before proguard processing.
$(full_classes_jar): $(full_classes_emma_jar) | $(ACP)
    @echo Copying: $@
    $(hide) $(ACP_VP) -fp $< $@
```

full_classes_jar 依赖于 full_classes_emma_jar

* full_classes_emma_jar

  out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/emma_out/lib/classes-jarjar.jar

### full_classes_emma_jar

path: build/core/java.mk
```
$(full_classes_emma_jar): $(full_classes_jarjar_jar) | $(ACP)
    @echo Copying: $@
    $(copy-file-to-target)
```

full_classes_emma_jar 依赖于 full_classes_jarjar_jar

* full_classes_jarjar_jar

  out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes-jarjar.jar

### full_classes_jarjar_jar

path: build/core/java.mk
```
$(full_classes_jarjar_jar): $(full_classes_compiled_jar) | $(ACP)
    @echo Copying: $@
    $(hide) $(ACP_VP) -fp $< $@
```

full_classes_jarjar_jar 依赖于 full_classes_compiled_jar

* full_classes_compiled_jar

  out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes-full-debug.jar

### full_classes_compiled_jar

path: build/core/java.mk
```
$(full_classes_compiled_jar): $(java_sources) $(java_resource_sources) $(full_java_lib_deps) \
        $(jar_manifest_file) $(layers_file) $(RenderScript_file_stamp) \
        $(proto_java_sources_file_stamp) $(LOCAL_ADDITIONAL_DEPENDENCIES)
        $(transform-java-to-classes.jar)
        @echo "check if internal resources are used by miui code at $(PRIVATE_MODULE) ..."
        @build/tools/check_internal_resource.sh $(PRIVATE_MODULE)
```

##### transform-java-to-classes.jar

path: build/core/definitions.mk
```
define transform-java-to-classes.jar
@echo "target Java: $(PRIVATE_MODULE) ($(PRIVATE_CLASS_INTERMEDIATES_DIR))"
$(call compile-java,$(TARGET_JAVAC),$(PRIVATE_BOOTCLASSPATH))
endef
```

##### compile-java

path: build/core/definitions.mk
```
# Common definition to invoke javac on the host and target.
#
# Some historical notes:
# - below we write the list of java files to java-source-list to avoid argument
#   list length problems with Cygwin
# - we filter out duplicate java file names because eclipse's compiler
#   doesn't like them.
#
# $(1): javac
# $(2): bootclasspath
define compile-java
$(hide) rm -f $@
$(hide) rm -rf $(PRIVATE_CLASS_INTERMEDIATES_DIR)
$(hide) mkdir -p $(dir $@)
$(hide) mkdir -p $(PRIVATE_CLASS_INTERMEDIATES_DIR)
$(call unzip-jar-files,$(PRIVATE_STATIC_JAVA_LIBRARIES_VP),$(PRIVATE_CLASS_INTERMEDIATES_DIR))
$(call dump-words-to-file,$(PRIVATE_JAVA_SOURCES_VP),$(PRIVATE_CLASS_INTERMEDIATES_DIR)/java-source-list)
$(hide) if [ -d "$(PRIVATE_SOURCE_INTERMEDIATES_DIR)" ]; then \
            find $(PRIVATE_SOURCE_INTERMEDIATES_DIR) -name '*.java' >> $(PRIVATE_CLASS_INTERMEDIATES_DIR)/java-source-list; \
fi
$(hide) tr ' ' '\n' < $(PRIVATE_CLASS_INTERMEDIATES_DIR)/java-source-list \
    | sort -u > $(PRIVATE_CLASS_INTERMEDIATES_DIR)/java-source-list-uniq
$(hide) if [ -s $(PRIVATE_CLASS_INTERMEDIATES_DIR)/java-source-list-uniq ] ; then \
    $(1) -encoding UTF-8 \
    $(strip $(PRIVATE_JAVAC_DEBUG_FLAGS)) \
    $(if $(findstring true,$(PRIVATE_WARNINGS_ENABLE)),$(xlint_unchecked),) \
    $(2) \
    $(addprefix -classpath ,$(strip \
        $(call normalize-path-list,$(PRIVATE_ALL_JAVA_LIBRARIES_VP)))) \
    $(if $(findstring true,$(PRIVATE_WARNINGS_ENABLE)),$(xlint_unchecked),) \
    -extdirs "" -d $(PRIVATE_CLASS_INTERMEDIATES_DIR) \
    $(PRIVATE_JAVACFLAGS) \
    \@$(PRIVATE_CLASS_INTERMEDIATES_DIR)/java-source-list-uniq \
    || ( rm -rf $(PRIVATE_CLASS_INTERMEDIATES_DIR) ; exit 41 ) \
fi
$(if $(PRIVATE_JAVA_LAYERS_FILE), $(hide) build/tools/java-layers.py \
    $(PRIVATE_JAVA_LAYERS_FILE) \@$(PRIVATE_CLASS_INTERMEDIATES_DIR)/java-source-list-uniq,)
$(hide) rm -f $(PRIVATE_CLASS_INTERMEDIATES_DIR)/java-source-list
$(hide) rm -f $(PRIVATE_CLASS_INTERMEDIATES_DIR)/java-source-list-uniq
$(if $(PRIVATE_JAR_EXCLUDE_FILES), $(hide) find $(PRIVATE_CLASS_INTERMEDIATES_DIR) \
    -name $(word 1, $(PRIVATE_JAR_EXCLUDE_FILES)) \
    $(addprefix -o -name , $(wordlist 2, 999, $(PRIVATE_JAR_EXCLUDE_FILES))) \
    | xargs rm -rf)
$(if $(PRIVATE_JAR_PACKAGES), $(hide) find $(PRIVATE_CLASS_INTERMEDIATES_DIR) -mindepth 1 -type d \
    $(foreach pkg, $(PRIVATE_JAR_PACKAGES), \
        -not -path $(PRIVATE_CLASS_INTERMEDIATES_DIR)/$(subst .,/,$(pkg))) \
    | xargs rm -rf)
$(hide) jar $(if $(strip $(PRIVATE_JAR_MANIFEST)),-cfm,-cf) \
    $@ $(PRIVATE_JAR_MANIFEST) -C $(PRIVATE_CLASS_INTERMEDIATES_DIR) .
endef
```

执行命令类似如下所示：

```
echo "target Java: hm (out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes)"
rm -f out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes-full-debug.jar
rm -rf out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes
mkdir -p out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/
mkdir -p out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes

for f in ; do if [ ! -f $f ]; then echo Missing file $f; exit 1; fi; unzip -qo $f -d out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes; done ;rm -rf out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes/META-INF
rm -f out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes/java-source-list

echo -n 'frameworks/base/cmds/hm/src/com/android/commands/hm/Hm.java ' >> out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes/java-source-list

if [ -d "out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/src" ]; then find out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/src -name '*.java' >> out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes/java-source-list; fi

tr ' ' '\n' < out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes/java-source-list | sort -u > out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes/java-source-list-uniq

if [ -s out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes/java-source-list-uniq ] ; then javac -J-Xmx512M -target 1.5 -Xmaxerrs 9999999 -encoding UTF-8 -g  -bootclasspath out/target/common/obj/JAVA_LIBRARIES/core_intermediates/classes.jar -classpath out/target/common/obj/JAVA_LIBRARIES/core_intermediates/classes.jar:out/target/common/obj/JAVA_LIBRARIES/core-junit_intermediates/classes.jar:out/target/common/obj/JAVA_LIBRARIES/ext_intermediates/classes.jar:out/target/common/obj/JAVA_LIBRARIES/framework_intermediates/classes.jar:out/target/common/obj/JAVA_LIBRARIES/framework2_intermediates/classes.jar:out/target/common/obj/JAVA_LIBRARIES/miuiframework_intermediates/classes.jar  -extdirs "" -d out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes  \@out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes/java-source-list-uniq || ( rm -rf out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes ; exit 41 ) fi

rm -f out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes/java-source-list
rm -f out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes/java-source-list-uniq
jar -cf out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes-full-debug.jar  -C out/target/common/obj/JAVA_LIBRARIES/hm_intermediates/classes .
```
