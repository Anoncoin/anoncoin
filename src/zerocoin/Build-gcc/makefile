################################################################################
# Automatically-generated file. Do not edit!
################################################################################
# TODO: I edited these makefiles anyway, even though the correct procedure is to
#       edit CMakeLists.txt.  When I have time to understand CMake, I should go
#       back and do this the right way.

-include ../makefile.init

RM := rm

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
endif

 -include ../makefile.defs

.DEFAULT_GOAL := libzerocoin

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: libzerocoin test tutorial ufo

# Tool invocations
libzerocoin: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Unix archive utility'
	rm -f libzerocoin.a
	ar cqs libzerocoin.a $(OBJS) $(USER_OBJS)
	@echo 'Invoking: Cross G++ Linker'
	g++ -L/opt/local/lib -shared -o "libzerocoin.so" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

test: $(OBJS) $(USER_OBJS) 
	@echo 'Building target: $@'
	@echo 'Invoking: Cross G++ Linker'
	g++ -L/opt/local/lib -o "tests" ../Tests.cpp $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '  

paramgen: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross G++ Linker'
	g++ -L/opt/local/lib -o "paramgen" ../paramgen.cpp $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '  

benchmark: $(OBJS) $(USER_OBJS) 
	@echo 'Building target: $@'
	@echo 'Invoking: Cross G++ Linker'
	g++ -L/opt/local/lib -o "benchmark" ../Benchmark.cpp $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '  

tutorial: $(OBJS) $(USER_OBJS) 
	@echo 'Building target: $@'
	@echo 'Invoking: Cross G++ Linker'
	g++ -L/opt/local/lib -o "tutorial" ../Tutorial.cpp $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '  

ufo: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross G++ Linker'
	g++ -L/opt/local/lib -o "ufo" ../PrintUFOCandidate.cpp $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) -f $(C++_DEPS)$(OBJS)$(C_DEPS)$(CC_DEPS)$(LIBRARIES)$(CPP_DEPS)$(CXX_DEPS)$(C_UPPER_DEPS) libzerocoin.so libzerocoin.a tests benchmark paramgen tutorial ufo
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

 -include ../makefile.targets
