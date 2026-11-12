TARGET ?= $(notdir $(shell pwd))
FILES = \
	Makefile \
	driver/Makefile \
	driver/scull.c \
	driver/scull.h \
	src/Makefile \
	src/scull.c

.PHONY: zip
zip: $(TARGET).zip

$(TARGET).zip: $(FILES)
	zip -r $@ $^
