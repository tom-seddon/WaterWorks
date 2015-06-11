MKDIR:=mkdir.exe
CP:=cp.exe

.PHONY:all
all:
	$(error Specify target)

.PHONY:bin
bin:
	$(MKDIR) bin
	$(CP) .build/waterworks__Win32__Release/waterworks.exe bin/
